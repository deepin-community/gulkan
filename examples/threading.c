/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <gulkan.h>
#include "common/common.h"
#include "common/plane-renderer.h"

G_BEGIN_DECLS
#define GULKAN_TYPE_EXAMPLE gulkan_example_get_type ()
G_DECLARE_FINAL_TYPE (Example, gulkan_example, GULKAN,
                      EXAMPLE, GObject)
G_END_DECLS

struct _Example
{
  GObject parent;

  GulkanTexture *texture;
  GLFWwindow *window;
  GMainLoop *loop;
  PlaneRenderer *renderer;
  gboolean should_quit;
  GdkPixbuf *pixbuf;
  GdkPixbuf *dirty_pixbuf;
  float factor;
  float step;

  GMutex render_mutex;
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

  glfwDestroyWindow (self->window);
  glfwTerminate ();
  G_OBJECT_CLASS (gulkan_example_parent_class)->finalize (gobject);
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
  GError *error = NULL;
  GdkPixbuf * pixbuf_no_alpha =
    gdk_pixbuf_new_from_resource ("/res/cat_srgb.jpg", &error);

  if (error != NULL) {
    g_printerr ("Unable to read file: %s\n", error->message);
    g_error_free (error);
    return NULL;
  }

  GdkPixbuf *pixbuf = gdk_pixbuf_add_alpha (pixbuf_no_alpha, FALSE, 0, 0, 0);
  g_object_unref (pixbuf_no_alpha);
  return pixbuf;
}

static void
key_callback (GLFWwindow* window, int key,
              int scancode, int action, int mods)
{
  (void) scancode;
  (void) mods;

  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
      Example *self = (Example*) glfwGetWindowUserPointer (window);
      self->should_quit = TRUE;
    }
}

static void
init_glfw (Example *self, int width, int height)
{
  glfwInit();

  glfwWindowHint (GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint (GLFW_RESIZABLE, FALSE);

  self->window = glfwCreateWindow (width, height, "Vulkan Pixbuf", NULL, NULL);

  glfwSetKeyCallback (self->window, key_callback);

  glfwSetWindowUserPointer (self->window, self);
}

static gboolean
input_cb (gpointer data)
{
  Example *self = (Example*) data;

  glfwPollEvents ();
  if (glfwWindowShouldClose (self->window) || self->should_quit)
    return FALSE;

  return TRUE;
}

static void
_reinit_texture (Example *self)
{
  /* let's reinit the texture */
  g_mutex_lock (&self->render_mutex);

  g_print ("Recreating texture.\n");

  g_object_unref(self->texture);

  GulkanClient *client =
    gulkan_renderer_get_client(GULKAN_RENDERER(self->renderer));
  self->texture =
    gulkan_texture_new_from_pixbuf (client, self->dirty_pixbuf,
                                    VK_FORMAT_R8G8B8A8_SRGB,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    FALSE);

  plane_renderer_update_texture_descriptor (self->renderer, self->texture);
  g_mutex_unlock (&self->render_mutex);
}

static void*
_reupload_thread (gpointer data)
{
  Example *self = (Example*) data;

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

static void*
_render_thread (gpointer _self)
{
  Example *self = (Example*) _self;

  while (!self->should_quit)
    {
      g_mutex_lock (&self->render_mutex);
      gulkan_renderer_draw (GULKAN_RENDERER (self->renderer));
      g_mutex_unlock (&self->render_mutex);
      g_usleep (1);
    }

  if (self->loop != NULL)
    g_main_loop_quit (self->loop);

  return NULL;
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

  gint width = gdk_pixbuf_get_width (self->pixbuf);
  gint height = gdk_pixbuf_get_height (self->pixbuf);

  init_glfw (self, width / 2, height / 2);

  GulkanClient *client = gulkan_client_new_glfw ();
  self->renderer = plane_renderer_new_from_client (client);
  if (!self->renderer)
    {
      g_printerr ("Unable to initialize Vulkan!\n");
      return FALSE;
    }

  VkInstance instance = gulkan_client_get_instance_handle (client);
  VkSurfaceKHR surface;
  VkResult res = glfwCreateWindowSurface (instance, self->window, NULL,
                                         &surface);
  vk_check_error ("glfwCreateWindowSurface", res, FALSE);

  self->texture =
    gulkan_texture_new_from_pixbuf (client, self->pixbuf,
                                    VK_FORMAT_R8G8B8A8_SRGB,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    FALSE);

  if (!plane_renderer_initialize (self->renderer, surface, self->texture))
    return FALSE;

  g_timeout_add (7, input_cb, self);

  GError *error = NULL;
  self->render_thread = g_thread_try_new ("render",
                                          (GThreadFunc) _render_thread,
                                          self,
                                         &error);
  if (error != NULL)
    {
      g_printerr ("Unable to start render thread: %s\n", error->message);
      g_error_free (error);
    }

  self->upload_thread = g_thread_try_new ("upload",
                                          (GThreadFunc) _reupload_thread,
                                          self,
                                         &error);
  if (error != NULL)
    {
      g_printerr ("Unable to start render thread: %s\n", error->message);
      g_error_free (error);
    }

  g_object_unref (client);

  return TRUE;
}

int
main () {
  Example *self = (Example *) g_object_new (GULKAN_TYPE_EXAMPLE, 0);
  /* Silence clang warning */
  g_assert (GULKAN_IS_EXAMPLE (self));
  if (!_init (self))
    return EXIT_FAILURE;

  g_main_loop_run (self->loop);

  g_thread_join (self->upload_thread);
  g_thread_join (self->render_thread);

  g_print("We joined the render thread!\n");

  g_object_unref (self);

  return EXIT_SUCCESS;
}
