/*
 * gulkan
 * Copyright 2020 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "plane-example.h"

#include "plane-renderer.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

typedef struct _PlaneExamplePrivate
{
  GObject parent;

  GLFWwindow *window;
  GMainLoop *loop;
  PlaneRenderer *renderer;
  gboolean should_quit;
  GdkPixbuf *pixbuf;
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
  PlaneExample *self = PLANE_EXAMPLE (gobject);
  PlaneExamplePrivate *priv = plane_example_get_instance_private (self);

  g_clear_object (&priv->texture);
  g_main_loop_unref (priv->loop);
  g_object_unref (priv->pixbuf);
  g_object_unref (priv->renderer);
  glfwDestroyWindow (priv->window);
  glfwTerminate ();

  G_OBJECT_CLASS (plane_example_parent_class)->finalize (gobject);
}

static void
plane_example_class_init (PlaneExampleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = _finalize;
}

static GdkPixbuf *
_load_pixbuf (const gchar* uri)
{
  GError *error = NULL;
  GdkPixbuf * pixbuf_no_alpha =
    gdk_pixbuf_new_from_resource (uri, &error);

  if (error != NULL) {
    g_printerr ("Unable to read file: %s\n", error->message);
    g_error_free (error);
    return NULL;
  } else {
    GdkPixbuf *pixbuf = gdk_pixbuf_add_alpha (pixbuf_no_alpha, FALSE, 0, 0, 0);
    g_object_unref (pixbuf_no_alpha);
    return pixbuf;
  }
}

static void
_key_cb (GLFWwindow* window, int key,
         int scancode, int action, int mods)
{
  (void) scancode;
  (void) mods;

  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
      PlaneExample *self = (PlaneExample*) glfwGetWindowUserPointer (window);
      PlaneExamplePrivate *priv = plane_example_get_instance_private (self);
      priv->should_quit = TRUE;
    }
}

static void
_init_glfw (PlaneExample *self, int width, int height)
{
  PlaneExamplePrivate *priv = plane_example_get_instance_private (self);

  glfwInit();

  glfwWindowHint (GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint (GLFW_RESIZABLE, GLFW_FALSE);
  priv->window = glfwCreateWindow (width, height, "Gulkan", NULL, NULL);
  glfwSetKeyCallback (priv->window, _key_cb);
  glfwSetWindowUserPointer (priv->window, self);
}

static gboolean
_draw_cb (gpointer data)
{
  PlaneExample *self = (PlaneExample*) data;
  PlaneExamplePrivate *priv = plane_example_get_instance_private (self);

  glfwPollEvents ();
  if (glfwWindowShouldClose (priv->window) || priv->should_quit)
    {
      g_main_loop_quit (priv->loop);
      return FALSE;
    }

  gulkan_renderer_draw (GULKAN_RENDERER (priv->renderer));

  GulkanClient *client =
    gulkan_renderer_get_client (GULKAN_RENDERER (priv->renderer));
  gulkan_device_wait_idle (gulkan_client_get_device (client));

  return TRUE;
}

gboolean
plane_example_initialize (PlaneExample *self,
                          const gchar  *pixbuf_uri,
                          GSList       *instance_ext_list,
                          GSList       *device_ext_list)
{
  PlaneExamplePrivate *priv = plane_example_get_instance_private (self);

  priv->pixbuf = _load_pixbuf (pixbuf_uri);
  if (priv->pixbuf == NULL)
    return FALSE;

  gint width = gdk_pixbuf_get_width (priv->pixbuf);
  gint height = gdk_pixbuf_get_height (priv->pixbuf);

  _init_glfw (self, width / 2, height / 2);

  uint32_t num_glfw_extensions = 0;
  const char** glfw_extensions;
  glfw_extensions = glfwGetRequiredInstanceExtensions (&num_glfw_extensions);

  for (uint32_t i = 0; i < num_glfw_extensions; i++)
    {
      char *instance_ext = g_strdup (glfw_extensions[i]);
      instance_ext_list = g_slist_append (instance_ext_list, instance_ext);
    }

  const gchar *device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };

  for (uint64_t i = 0; i < G_N_ELEMENTS (device_extensions); i++)
    {
      char *device_ext = g_strdup (device_extensions[i]);
      device_ext_list = g_slist_append (device_ext_list, device_ext);
    }

  GulkanClient *client = gulkan_client_new_from_extensions (instance_ext_list,
                                                            device_ext_list);
  priv->renderer = plane_renderer_new_from_client (client);
  if (!priv->renderer)
    {
      g_printerr ("Unable to init plane renderer!\n");
      return FALSE;
    }

  g_slist_free (instance_ext_list);
  g_slist_free (device_ext_list);

  VkInstance instance = gulkan_client_get_instance_handle (client);
  VkSurfaceKHR surface;
  VkResult res = glfwCreateWindowSurface (instance,
                                          priv->window, NULL,
                                         &surface);
  vk_check_error ("glfwCreateWindowSurface", res, FALSE);

  g_object_unref (client);

  PlaneExampleClass *klass = PLANE_EXAMPLE_GET_CLASS (self);
  if (klass->init_texture == NULL)
      return FALSE;

  priv->texture = klass->init_texture (self, client, priv->pixbuf);
  if (!priv->texture)
    return FALSE;

  if (!plane_renderer_initialize (priv->renderer,
                                  surface,
                                  priv->texture))
    return FALSE;
  return TRUE;
}

void
plane_example_run (PlaneExample *self)
{
  PlaneExamplePrivate *priv = plane_example_get_instance_private (self);
  g_timeout_add (1, _draw_cb, self);
  g_main_loop_run (priv->loop);
}
