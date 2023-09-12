/*
 * gulkan
 *
 * Roughly based on vkcube.
 *
 * Copyright 2012 Arvin Schnell <arvin.schnell@gmail.com>
 * Copyright 2012 Rob Clark <rob@ti.com>
 * Copyright 2015 Intel Corporation
 * Copyright 2020 Collabora Ltd
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>

#include <glib.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <gulkan.h>

#include "common/model-renderer.h"
#include "common/common.h"

G_BEGIN_DECLS

#define GULKAN_TYPE_EXAMPLE gulkan_example_get_type ()
G_DECLARE_FINAL_TYPE (Example, gulkan_example, GULKAN,
                      EXAMPLE, ModelRenderer)

G_END_DECLS

static const float positions[] = {
  // front
  -1.0f, -1.0f, +1.0f, // point blue
  +1.0f, -1.0f, +1.0f, // point magenta
  -1.0f, +1.0f, +1.0f, // point cyan
  +1.0f, +1.0f, +1.0f, // point white
  // back
  +1.0f, -1.0f, -1.0f, // point red
  -1.0f, -1.0f, -1.0f, // point black
  +1.0f, +1.0f, -1.0f, // point yellow
  -1.0f, +1.0f, -1.0f, // point green
  // right
  +1.0f, -1.0f, +1.0f, // point magenta
  +1.0f, -1.0f, -1.0f, // point red
  +1.0f, +1.0f, +1.0f, // point white
  +1.0f, +1.0f, -1.0f, // point yellow
  // left
  -1.0f, -1.0f, -1.0f, // point black
  -1.0f, -1.0f, +1.0f, // point blue
  -1.0f, +1.0f, -1.0f, // point green
  -1.0f, +1.0f, +1.0f, // point cyan
  // top
  -1.0f, +1.0f, +1.0f, // point cyan
  +1.0f, +1.0f, +1.0f, // point white
  -1.0f, +1.0f, -1.0f, // point green
  +1.0f, +1.0f, -1.0f, // point yellow
  // bottom
  -1.0f, -1.0f, -1.0f, // point black
  +1.0f, -1.0f, -1.0f, // point red
  -1.0f, -1.0f, +1.0f, // point blue
  +1.0f, -1.0f, +1.0f  // point magenta
};

static const float colors[] = {
  // front
  0.0f, 0.0f, 1.0f, // blue
  1.0f, 0.0f, 1.0f, // magenta
  0.0f, 1.0f, 1.0f, // cyan
  1.0f, 1.0f, 1.0f, // white
  // back
  1.0f, 0.0f, 0.0f, // red
  0.0f, 0.0f, 0.0f, // black
  1.0f, 1.0f, 0.0f, // yellow
  0.0f, 1.0f, 0.0f, // green
  // right
  1.0f, 0.0f, 1.0f, // magenta
  1.0f, 0.0f, 0.0f, // red
  1.0f, 1.0f, 1.0f, // white
  1.0f, 1.0f, 0.0f, // yellow
  // left
  0.0f, 0.0f, 0.0f, // black
  0.0f, 0.0f, 1.0f, // blue
  0.0f, 1.0f, 0.0f, // green
  0.0f, 1.0f, 1.0f, // cyan
  // top
  0.0f, 1.0f, 1.0f, // cyan
  1.0f, 1.0f, 1.0f, // white
  0.0f, 1.0f, 0.0f, // green
  1.0f, 1.0f, 0.0f, // yellow
  // bottom
  0.0f, 0.0f, 0.0f, // black
  1.0f, 0.0f, 0.0f, // red
  0.0f, 0.0f, 1.0f, // blue
  1.0f, 0.0f, 1.0f  // magenta
};

static const float normals[] = {
  // front
  +0.0f, +0.0f, +1.0f, // forward
  +0.0f, +0.0f, +1.0f, // forward
  +0.0f, +0.0f, +1.0f, // forward
  +0.0f, +0.0f, +1.0f, // forward
  // back
  +0.0f, +0.0f, -1.0f, // backbard
  +0.0f, +0.0f, -1.0f, // backbard
  +0.0f, +0.0f, -1.0f, // backbard
  +0.0f, +0.0f, -1.0f, // backbard
  // right
  +1.0f, +0.0f, +0.0f, // right
  +1.0f, +0.0f, +0.0f, // right
  +1.0f, +0.0f, +0.0f, // right
  +1.0f, +0.0f, +0.0f, // right
  // left
  -1.0f, +0.0f, +0.0f, // left
  -1.0f, +0.0f, +0.0f, // left
  -1.0f, +0.0f, +0.0f, // left
  -1.0f, +0.0f, +0.0f, // left
  // top
  +0.0f, +1.0f, +0.0f, // up
  +0.0f, +1.0f, +0.0f, // up
  +0.0f, +1.0f, +0.0f, // up
  +0.0f, +1.0f, +0.0f, // up
  // bottom
  +0.0f, -1.0f, +0.0f, // down
  +0.0f, -1.0f, +0.0f, // down
  +0.0f, -1.0f, +0.0f, // down
  +0.0f, -1.0f, +0.0f  // down
};

static const VkClearColorValue background_color = {
  .float32 = { 0.05f, 0.05f, 0.05f, 1.0f },
};

struct _Example
{
  ModelRenderer parent;

  GulkanVertexBuffer *vb;

  GMainLoop *loop;
  GLFWwindow *window;
  gboolean should_quit;

  VkExtent2D last_window_size;
  VkOffset2D last_window_position;
};

G_DEFINE_TYPE (Example, gulkan_example, MODEL_TYPE_RENDERER)

static void
_finalize (GObject *gobject)
{
  Example *self = GULKAN_EXAMPLE (gobject);
  g_main_loop_unref (self->loop);

  g_object_unref (self->vb);

  G_OBJECT_CLASS (gulkan_example_parent_class)->finalize (gobject);

  /*
   * GLFW needs to be destroyed after the surface,
   * which belongs to GulkanSwapchain.
   */
  glfwDestroyWindow (self->window);
  glfwTerminate ();
}

static void
gulkan_example_init (Example *self)
{
  gulkan_renderer_set_extent (GULKAN_RENDERER (self),
                              (VkExtent2D) { .width = 1280, .height = 720 });
  self->should_quit = FALSE;
  self->loop = g_main_loop_new (NULL, FALSE);
}

static void
_update_uniform_buffer (Example *self)
{
  int64_t t = gulkan_renderer_get_msec_since_start (GULKAN_RENDERER (self));
  t /= 5;

  graphene_matrix_t mv_matrix;
  graphene_matrix_init_identity (&mv_matrix);

  graphene_matrix_rotate_x (&mv_matrix, 45.0f + (0.25f * t));
  graphene_matrix_rotate_y (&mv_matrix, 45.0f - (0.5f * t));
  graphene_matrix_rotate_z (&mv_matrix, 10.0f + (0.15f * t));

  graphene_point3d_t pos = { 0.0f, 0.0f, -8.0f };
  graphene_matrix_translate (&mv_matrix, &pos);

  float aspect = gulkan_renderer_get_aspect (GULKAN_RENDERER (self));
  graphene_matrix_t p_matrix;
  graphene_matrix_init_perspective (&p_matrix, 45.0f, aspect, 0.1f, 10.f);

  graphene_matrix_t mvp_matrix;
  graphene_matrix_multiply (&mv_matrix, &p_matrix, &mvp_matrix);

  Transformation ubo;
  graphene_matrix_to_float (&mv_matrix, ubo.mv_matrix);
  graphene_matrix_to_float (&mvp_matrix, ubo.mvp_matrix);

  /* The mat3 normalMatrix is laid out as 3 vec4s. */
  memcpy (ubo.normal_matrix, ubo.mv_matrix, sizeof ubo.normal_matrix);

  model_renderer_update_ubo (MODEL_RENDERER (self), (gpointer) &ubo);
}

static void
_key_cb (GLFWwindow *window, int key, int scancode, int action, int mods)
{
  (void) scancode;
  (void) mods;

  Example *self = (Example *) glfwGetWindowUserPointer (window);
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
      self->should_quit = TRUE;
    }
  if (key == GLFW_KEY_F && action == GLFW_PRESS)
    {
      glfw_toggle_fullscreen (window,
                             &self->last_window_position,
                             &self->last_window_size);
    }
}

static void
_framebuffer_size_cb (GLFWwindow* window, int w, int h)
{
  (void) w;
  (void) h;
  Example *self = (Example *) glfwGetWindowUserPointer (window);
  GulkanClient *client = gulkan_renderer_get_client (GULKAN_RENDERER (self));

  VkSurfaceKHR surface;

  VkInstance instance = gulkan_client_get_instance_handle (client);
  VkResult res = glfwCreateWindowSurface (instance, self->window,
                                          NULL, &surface);
  if (res != VK_SUCCESS)
    {
      g_printerr ("Creating surface failed.");
      return;
    }

  if (!gulkan_swapchain_renderer_resize (GULKAN_SWAPCHAIN_RENDERER (self),
                                         surface))
    g_warning ("Resize failed.");
}

static gboolean
_init_glfw (Example *self)
{
  glfwInit ();

  glfwWindowHint (GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint (GLFW_RESIZABLE, TRUE);
  glfwWindowHint (GLFW_AUTO_ICONIFY, GLFW_FALSE);

  VkExtent2D extent = gulkan_renderer_get_extent (GULKAN_RENDERER (self));
  self->window =
    glfwCreateWindow ((int) extent.width, (int) extent.height,
                      "Gulkan Cube", NULL, NULL);

  if (!self->window)
    return FALSE;

  glfwSetKeyCallback (self->window, _key_cb);

  glfwSetWindowUserPointer (self->window, self);

  return TRUE;
}

static gboolean
_iterate (gpointer _self)
{
  Example *self = (Example *) _self;

  glfwPollEvents ();
  if (glfwWindowShouldClose (self->window) || self->should_quit)
    {
      g_main_loop_quit (self->loop);
      return FALSE;
    }

  _update_uniform_buffer (self);

  return gulkan_renderer_draw (GULKAN_RENDERER (self));
}

static gboolean
_init (Example *self)
{
  if (!_init_glfw (self))
    {
      g_printerr ("failed to initialize glfw\n");
      return FALSE;
    }

  GulkanClient *client = gulkan_client_new_glfw ();
  if (!client)
    {
      g_printerr ("Could not init gulkan.\n");
      return FALSE;
    }

  gulkan_renderer_set_client (GULKAN_RENDERER (self), client);

  GulkanInstance *gulkan_instance = gulkan_client_get_instance (client);
  GulkanDevice *gulkan_device = gulkan_client_get_device (client);

  VkInstance instance = gulkan_instance_get_handle (gulkan_instance);

  VkSurfaceKHR surface;
  if (glfwCreateWindowSurface (instance, self->window, NULL, &surface) !=
      VK_SUCCESS)
    {
      g_printerr ("Unable to create surface.\n");
      return FALSE;
    }

  self->vb = GULKAN_VERTEX_BUFFER_NEW_FROM_ATTRIBS (gulkan_device, positions,
                                                    colors, normals);

  if (!self->vb)
    return FALSE;

  ShaderResources resources = {
    "/shaders/cube.vert.spv",
    "/shaders/cube.frag.spv"
  };

  if (!model_renderer_initialize (MODEL_RENDERER (self),
                                  surface,
                                  background_color,
                                 &resources))
    return FALSE;

  glfwSetFramebufferSizeCallback(self->window, _framebuffer_size_cb);

  return TRUE;
}

static void
_init_draw_cmd (ModelRenderer  *renderer,
                VkCommandBuffer cmd_buffer)
{
  Example *self = GULKAN_EXAMPLE (renderer);

  gulkan_vertex_buffer_bind_with_offsets (self->vb, cmd_buffer);
  vkCmdDraw (cmd_buffer, 4, 1, 0, 0);
  vkCmdDraw (cmd_buffer, 4, 1, 4, 0);
  vkCmdDraw (cmd_buffer, 4, 1, 8, 0);
  vkCmdDraw (cmd_buffer, 4, 1, 12, 0);
  vkCmdDraw (cmd_buffer, 4, 1, 16, 0);
  vkCmdDraw (cmd_buffer, 4, 1, 20, 0);
}

static void
gulkan_example_class_init (ExampleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = _finalize;

  ModelRendererClass *parent_class = MODEL_RENDERER_CLASS (klass);
  parent_class->init_draw_cmd = _init_draw_cmd;
}

int
main ()
{
  Example *self = (Example *) g_object_new (GULKAN_TYPE_EXAMPLE, 0);
  /* Silence clang warning */
  g_assert (GULKAN_IS_EXAMPLE (self));
  if (!_init (self))
    {
      g_object_unref (self);
      return -1;
    }

  g_timeout_add (1, _iterate, self);
  g_main_loop_run (self->loop);

  g_object_unref (self);

  return 0;
}
