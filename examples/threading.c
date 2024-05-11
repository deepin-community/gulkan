/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "common/plane-renderer.h"
#include <gulkan.h>

#define GULKAN_TYPE_EXAMPLE gulkan_example_get_type ()
G_DECLARE_FINAL_TYPE (Example, gulkan_example, GULKAN, EXAMPLE, GObject)

struct _Example
{
  GObject parent;

  GulkanTexture *texture;
  GulkanWindow  *window;
  GMainLoop     *loop;
  PlaneRenderer *renderer;
  gboolean       should_quit;
  GdkPixbuf     *pixbuf;
  GdkPixbuf     *dirty_pixbuf;
  float          factor;
  float          step;

  GMutex   render_mutex;
  GThread *render_thread;
  GThread *upload_thread;
};
G_DEFINE_TYPE (Example, gulkan_example, G_TYPE_OBJECT)

static void
gulkan_example_init (Example *self)
{
  self->should_quit = FALSE;
  self->factor = 1.0f;
  self->step = 0.5f;
  self->loop = g_main_loop_new (NULL, FALSE);
  self->render_thread = NULL;
  g_mutex_init (&self->render_mutex);
}

static void
_finalize (GObject *gobject)
{
  Example *self = GULKAN_EXAMPLE (gobject);

  g_mutex_clear (&self->render_mutex);
  g_main_loop_unref (self->loop);

  g_object_unref (self->dirty_pixbuf);
  g_object_unref (self->pixbuf);
  g_object_unref (self->texture);
  g_object_unref (self->renderer);

  G_OBJECT_CLASS (gulkan_example_parent_class)->finalize (gobject);

  // Wayland surface needs to be deinited after vk swapchain
  g_object_unref (self->window);
}

static void
gulkan_example_class_init (ExampleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = _finalize;
}

static GdkPixbuf *
load_gdk_pixbuf ()
{
  GError    *error = NULL;
  GdkPixbuf *pixbuf_no_alpha
    = gdk_pixbuf_new_from_resource ("/res/cat_srgb.jpg", &error);

  if (error != NULL)
    {
      g_printerr ("Unable to read file: %s\n", error->message);
      g_error_free (error);
      return NULL;
    }

  GdkPixbuf *pixbuf = gdk_pixbuf_add_alpha (pixbuf_no_alpha, FALSE, 0, 0, 0);
  g_object_unref (pixbuf_no_alpha);
  return pixbuf;
}

static void
_key_cb (GulkanWindow *window, GulkanKeyEvent *event, Example *self)
{
  if (!event->is_pressed)
    {
      return;
    }

  if (event->key == XKB_KEY_Escape)
    {
      self->should_quit = TRUE;
    }
  else if (event->key == XKB_KEY_f)
    {
      gulkan_window_toggle_fullscreen (window);
    }
}

static void
_reinit_texture (Example *self)
{
  /* let's reinit the texture */
  g_mutex_lock (&self->render_mutex);

  g_print ("Recreating texture.\n");

  g_object_unref (self->texture);

  GulkanContext *context
    = gulkan_renderer_get_context (GULKAN_RENDERER (self->renderer));
  self->texture
    = gulkan_texture_new_from_pixbuf (context, self->dirty_pixbuf,
                                      VK_FORMAT_R8G8B8A8_SRGB,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                      FALSE);

  plane_renderer_update_texture (self->renderer, self->texture);
  g_mutex_unlock (&self->render_mutex);
}

static void *
_reupload_thread (gpointer data)
{
  Example *self = (Example *) data;

  while (!self->should_quit)
    {
      gboolean recreate = FALSE;

      if (self->factor > 10.0f || self->factor <= 0.0f)
        {
          self->step = -self->step;
          recreate = TRUE;
        }

      self->factor += self->step;

      gdk_pixbuf_saturate_and_pixelate (self->pixbuf, self->dirty_pixbuf,
                                        self->factor, FALSE);

      if (recreate)
        _reinit_texture (self);
      else
        gulkan_texture_upload_pixbuf (self->texture, self->dirty_pixbuf,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

  return NULL;
}

static void *
_render_thread (gpointer _self)
{
  Example *self = (Example *) _self;

  while (!self->should_quit)
    {
      gulkan_window_poll_events (self->window);
      g_mutex_lock (&self->render_mutex);
      gulkan_renderer_draw (GULKAN_RENDERER (self->renderer));
      g_mutex_unlock (&self->render_mutex);
      g_usleep (1);
    }

  if (self->loop != NULL)
    g_main_loop_quit (self->loop);

  return NULL;
}

static void
_close_cb (GulkanWindow *window, Example *self)
{
  (void) window;
  g_main_loop_quit (self->loop);
}

static void
_init_threads (Example *self)
{
  GError *error = NULL;
  self->render_thread = g_thread_try_new ("render",
                                          (GThreadFunc) _render_thread, self,
                                          &error);
  if (error != NULL)
    {
      g_printerr ("Unable to start render thread: %s\n", error->message);
      g_error_free (error);
    }

  self->upload_thread = g_thread_try_new ("upload",
                                          (GThreadFunc) _reupload_thread, self,
                                          &error);
  if (error != NULL)
    {
      g_printerr ("Unable to start render thread: %s\n", error->message);
      g_error_free (error);
    }
}

static void
_configure_cb (GulkanWindow *window, GulkanConfigureEvent *event, Example *self)
{
  GulkanContext *context
    = gulkan_renderer_get_context (GULKAN_RENDERER (self->renderer));
  VkInstance instance = gulkan_context_get_instance_handle (context);

  VkSurfaceKHR surface;
  if (gulkan_window_create_surface (window, instance, &surface) != VK_SUCCESS)
    {
      g_printerr ("Creating surface failed.");
      return;
    }

  if (!gulkan_swapchain_renderer_resize (
        GULKAN_SWAPCHAIN_RENDERER (self->renderer), surface, event->extent))
    g_warning ("Resize failed.");
}

static gboolean
_init (Example *self)
{
  self->pixbuf = load_gdk_pixbuf ();
  if (self->pixbuf == NULL)
    return FALSE;

  self->dirty_pixbuf = gdk_pixbuf_copy (self->pixbuf);
  if (self->dirty_pixbuf == NULL)
    return FALSE;

  uint32_t width = (uint32_t) gdk_pixbuf_get_width (self->pixbuf);
  uint32_t height = (uint32_t) gdk_pixbuf_get_height (self->pixbuf);

  VkExtent2D extent = {width / 2, height / 2};
  self->window = gulkan_window_new (extent, "Threading Example");
  if (!self->window)
    {
      g_printerr ("Could not initialize window.\n");
      return FALSE;
    }

  GSList *instance_ext_list = gulkan_window_required_extensions (self->window);

  const gchar *device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };

  GSList *device_ext_list = NULL;
  for (uint64_t i = 0; i < G_N_ELEMENTS (device_extensions); i++)
    {
      char *device_ext = g_strdup (device_extensions[i]);
      device_ext_list = g_slist_append (device_ext_list, device_ext);
    }

  GulkanContext *context
    = gulkan_context_new_from_extensions (instance_ext_list, device_ext_list,
                                          VK_NULL_HANDLE);

  if (!gulkan_window_has_support (self->window, context))
    {
      g_printerr ("Window surface extension support check failed.\n");
      return FALSE;
    }

  g_slist_free (instance_ext_list);
  g_slist_free (device_ext_list);

  self->renderer = plane_renderer_new_from_context (context);
  g_object_unref (context);

  if (!self->renderer)
    {
      g_printerr ("Unable to initialize Vulkan!\n");
      return FALSE;
    }

  self->texture
    = gulkan_texture_new_from_pixbuf (context, self->pixbuf,
                                      VK_FORMAT_R8G8B8A8_SRGB,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                      FALSE);

  gulkan_renderer_set_extent (GULKAN_RENDERER (self->renderer), extent);

  if (!plane_renderer_initialize (self->renderer, self->texture))
    return FALSE;

  g_signal_connect (self->window, "configure", (GCallback) _configure_cb, self);
  g_signal_connect (self->window, "close", (GCallback) _close_cb, self);
  g_signal_connect (self->window, "key", (GCallback) _key_cb, self);

  _init_threads (self);

  return TRUE;
}

int
main ()
{
  Example *self = (Example *) g_object_new (GULKAN_TYPE_EXAMPLE, 0);
  /* Silence clang warning */
  g_assert (GULKAN_IS_EXAMPLE (self));
  if (!_init (self))
    return EXIT_FAILURE;

  g_main_loop_run (self->loop);

  g_thread_join (self->upload_thread);
  g_thread_join (self->render_thread);

  g_print ("We joined the render thread!\n");

  g_object_unref (self);

  return EXIT_SUCCESS;
}
