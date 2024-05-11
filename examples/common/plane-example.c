/*
 * gulkan
 * Copyright 2020 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "plane-example.h"

#include "common.h"
#include "plane-renderer.h"

typedef struct _PlaneExamplePrivate
{
  GObject parent;

  GulkanWindow  *window;
  GMainLoop     *loop;
  PlaneRenderer *renderer;
  gboolean       should_quit;
  GdkPixbuf     *pixbuf;
  GulkanTexture *texture;
} PlaneExamplePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (PlaneExample, plane_example, G_TYPE_OBJECT)

static void
plane_example_init (PlaneExample *self)
{
  PlaneExamplePrivate *priv = plane_example_get_instance_private (self);
  priv->should_quit = FALSE;
  priv->loop = g_main_loop_new (NULL, FALSE);
}

static void
_finalize (GObject *gobject)
{
  PlaneExample        *self = PLANE_EXAMPLE (gobject);
  PlaneExamplePrivate *priv = plane_example_get_instance_private (self);

  g_clear_object (&priv->texture);
  g_main_loop_unref (priv->loop);
  g_object_unref (priv->pixbuf);
  g_object_unref (priv->renderer);

  G_OBJECT_CLASS (plane_example_parent_class)->finalize (gobject);

  // Wayland surface needs to be deinited after vk swapchain
  g_object_unref (priv->window);
}

static void
plane_example_class_init (PlaneExampleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = _finalize;
}

static void
_key_cb (GulkanWindow *window, GulkanKeyEvent *event, PlaneExample *self)
{
  PlaneExamplePrivate *priv = plane_example_get_instance_private (self);

  if (!event->is_pressed)
    {
      return;
    }

  if (event->key == XKB_KEY_Escape)
    {
      priv->should_quit = TRUE;
    }
  else if (event->key == XKB_KEY_f)
    {
      gulkan_window_toggle_fullscreen (window);
    }
}

static void
_configure_cb (GulkanWindow         *window,
               GulkanConfigureEvent *event,
               PlaneExample         *self)
{
  PlaneExamplePrivate *priv = plane_example_get_instance_private (self);
  GulkanContext       *context
    = gulkan_renderer_get_context (GULKAN_RENDERER (priv->renderer));
  VkInstance instance = gulkan_context_get_instance_handle (context);

  VkSurfaceKHR surface;
  if (gulkan_window_create_surface (window, instance, &surface) != VK_SUCCESS)
    {
      g_printerr ("Creating surface failed.");
      return;
    }

  if (!gulkan_swapchain_renderer_resize (
        GULKAN_SWAPCHAIN_RENDERER (priv->renderer), surface, event->extent))
    g_warning ("Resize failed.");
}

static gboolean
_draw_cb (gpointer data)
{
  PlaneExample        *self = (PlaneExample *) data;
  PlaneExamplePrivate *priv = plane_example_get_instance_private (self);

  gulkan_window_poll_events (priv->window);

  if (priv->should_quit)
    {
      g_main_loop_quit (priv->loop);
      return FALSE;
    }

  gulkan_renderer_draw (GULKAN_RENDERER (priv->renderer));

  return TRUE;
}

static void
_close_cb (GulkanWindow *window, PlaneExample *self)
{
  (void) window;
  PlaneExamplePrivate *priv = plane_example_get_instance_private (self);
  g_main_loop_quit (priv->loop);
}

gboolean
plane_example_initialize (PlaneExample *self,
                          const gchar  *pixbuf_uri,
                          GSList       *instance_ext_list,
                          GSList       *device_ext_list)
{
  PlaneExamplePrivate *priv = plane_example_get_instance_private (self);

  priv->pixbuf = gdk_load_pixbuf_from_uri (pixbuf_uri);
  if (priv->pixbuf == NULL)
    return FALSE;

  uint32_t width = (uint32_t) gdk_pixbuf_get_width (priv->pixbuf);
  uint32_t height = (uint32_t) gdk_pixbuf_get_height (priv->pixbuf);

  VkExtent2D extent = {width / 2, height / 2};
  priv->window = gulkan_window_new (extent, "Gulkan");
  if (!priv->window)
    {
      g_printerr ("Could not initialize window.\n");
      return FALSE;
    }

  GSList *window_ext_list = gulkan_window_required_extensions (priv->window);

  instance_ext_list = g_slist_concat (instance_ext_list, window_ext_list);

  const gchar *device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };

  for (uint64_t i = 0; i < G_N_ELEMENTS (device_extensions); i++)
    {
      char *device_ext = g_strdup (device_extensions[i]);
      device_ext_list = g_slist_append (device_ext_list, device_ext);
    }

  GulkanContext *context
    = gulkan_context_new_from_extensions (instance_ext_list, device_ext_list,
                                          VK_NULL_HANDLE);

  if (!gulkan_window_has_support (priv->window, context))
    {
      g_printerr ("Window surface extension support check failed.\n");
      return FALSE;
    }

  g_slist_free (instance_ext_list);
  g_slist_free (device_ext_list);

  priv->renderer = plane_renderer_new_from_context (context);
  if (!priv->renderer)
    {
      g_printerr ("Unable to init plane renderer!\n");
      return FALSE;
    }

  g_object_unref (context);

  gulkan_renderer_set_extent (GULKAN_RENDERER (priv->renderer), extent);

  PlaneExampleClass *klass = PLANE_EXAMPLE_GET_CLASS (self);
  if (klass->init_texture == NULL)
    return FALSE;

  priv->texture = klass->init_texture (self, context, priv->pixbuf);
  if (!priv->texture)
    return FALSE;

  if (!plane_renderer_initialize (priv->renderer, priv->texture))
    return FALSE;

  g_signal_connect (priv->window, "configure", (GCallback) _configure_cb, self);
  g_signal_connect (priv->window, "close", (GCallback) _close_cb, self);
  g_signal_connect (priv->window, "key", (GCallback) _key_cb, self);

  return TRUE;
}

void
plane_example_run (PlaneExample *self)
{
  PlaneExamplePrivate *priv = plane_example_get_instance_private (self);
  g_timeout_add (1, _draw_cb, self);
  g_main_loop_run (priv->loop);
}
