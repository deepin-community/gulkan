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
 * Author: Manas Chaudhary <manaschaudhary2000@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>

#include <glib.h>

#include <gulkan.h>

#define GULKAN_TYPE_EXAMPLE gulkan_example_get_type ()
G_DECLARE_FINAL_TYPE (Example,
                      gulkan_example,
                      GULKAN,
                      EXAMPLE,
                      GulkanSwapchainRenderer)

// clang-format off
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

static const float normals[] = {
  // front
  +0.0f, +0.0f, +1.0f, // forward
  +0.0f, +0.0f, +1.0f, // forward
  +0.0f, +0.0f, +1.0f, // forward
  +0.0f, +0.0f, +1.0f, // forward
  // back
  +0.0f, +0.0f, -1.0f, // backward
  +0.0f, +0.0f, -1.0f, // backward
  +0.0f, +0.0f, -1.0f, // backward
  +0.0f, +0.0f, -1.0f, // backward
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

static const float tex_coords[] = {
  // front
  0.0f, 0.0f,
  1.0f, 0.0f,
  0.0f, 1.0f,
  1.0f, 1.0f,
  // back
  0.0f, 0.0f,
  1.0f, 0.0f,
  0.0f, 1.0f,
  1.0f, 1.0f,
  // right
  0.0f, 0.0f,
  1.0f, 0.0f,
  0.0f, 1.0f,
  1.0f, 1.0f,
  // left
  0.0f, 0.0f,
  1.0f, 0.0f,
  0.0f, 1.0f,
  1.0f, 1.0f,
  // top
  0.0f, 0.0f,
  1.0f, 0.0f,
  0.0f, 1.0f,
  1.0f, 1.0f,
  // bottom
  0.0f, 0.0f,
  1.0f, 0.0f,
  0.0f, 1.0f,
  1.0f, 1.0f,
};

static const float tangents[] = {
  // front
  1.0f, 0.0f, 0.0f,
  1.0f, 0.0f, 0.0f,
  1.0f, 0.0f, 0.0f,
  1.0f, 0.0f, 0.0f,
  // back
  -1.0f, 0.0f, 0.0f,
  -1.0f, 0.0f, 0.0f,
  -1.0f, 0.0f, 0.0f,
  -1.0f, 0.0f, 0.0f,
  // right
  0.0f, 0.0f, -1.0f,
  0.0f, 0.0f, -1.0f,
  0.0f, 0.0f, -1.0f,
  0.0f, 0.0f, -1.0f,
  // left
  0.0f, 0.0f, 1.0f,
  0.0f, 0.0f, 1.0f,
  0.0f, 0.0f, 1.0f,
  0.0f, 0.0f, 1.0f,
  // top
  1.0f, 0.0f, 0.0f,
  1.0f, 0.0f, 0.0f,
  1.0f, 0.0f, 0.0f,
  1.0f, 0.0f, 0.0f,
  // bottom
  1.0f, 0.0f, 0.0f,
  1.0f, 0.0f, 0.0f,
  1.0f, 0.0f, 0.0f,
  1.0f, 0.0f, 0.0f,
};
// clang-format on

typedef struct
{
  float mv_matrix[16];
  float mvp_matrix[16];
  float normal_matrix[12];
} Transformation;

static const gchar *diffuse_pixbuf_uri = "/res/wall_diffuse.png";
static const gchar *normal_pixbuf_uri = "/res/wall_normal.png";

static const gchar *vertex_shader_uri = "/shaders/normal-map.vert.spv";
static const gchar *fragment_shader_uri = "/shaders/normal-map.frag.spv";

static const VkClearColorValue background_color = {
  .float32 = {0.05f, 0.05f, 0.05f, 1.0f},
};

struct _Example
{
  GulkanSwapchainRenderer parent;

  GulkanVertexBuffer *vb;

  GMainLoop    *loop;
  GulkanWindow *window;
  gboolean      should_quit;

  GulkanTexture *diffuse_texture;
  GulkanTexture *normal_texture;

  VkExtent2D last_window_size;
  VkOffset2D last_window_position;

  GulkanUniformBuffer *transformation_ubo;

  GulkanPipeline       *pipeline;
  GulkanDescriptorSet  *descriptor_set;
  GulkanDescriptorPool *descriptor_pool;
};

G_DEFINE_TYPE (Example, gulkan_example, GULKAN_TYPE_SWAPCHAIN_RENDERER)

static void
_finalize (GObject *gobject)
{
  Example *self = GULKAN_EXAMPLE (gobject);
  g_main_loop_unref (self->loop);

  g_object_unref (self->vb);
  g_object_unref (self->descriptor_set);
  g_object_unref (self->transformation_ubo);

  GulkanContext *context = gulkan_renderer_get_context (GULKAN_RENDERER (self));
  g_object_unref (self->pipeline);

  gulkan_device_wait_idle (gulkan_context_get_device (context));

  G_OBJECT_CLASS (gulkan_example_parent_class)->finalize (gobject);

  // Wayland surface needs to be deinited after vk swapchain
  g_object_unref (self->window);
}

static void
_init_draw_cmd (GulkanSwapchainRenderer *renderer, VkCommandBuffer cmd_buffer)
{
  Example *self = GULKAN_EXAMPLE (renderer);

  gulkan_pipeline_bind (self->pipeline, cmd_buffer);

  VkPipelineLayout layout
    = gulkan_descriptor_pool_get_pipeline_layout (self->descriptor_pool);

  gulkan_descriptor_set_bind (self->descriptor_set, layout, cmd_buffer);

  gulkan_vertex_buffer_bind_with_offsets (self->vb, cmd_buffer);

  // Draw faces seperately
  uint32_t       offset = 0;
  const uint32_t face_elements = 4;
  for (uint32_t i = 0; i < 6; i++)
    {
      vkCmdDraw (cmd_buffer, face_elements, 1, offset, 0);
      offset += face_elements;
    }
}

static void
gulkan_example_init (Example *self)
{
  gulkan_renderer_set_extent (GULKAN_RENDERER (self),
                              (VkExtent2D){.width = 1280, .height = 720});
  self->should_quit = FALSE;
  self->loop = g_main_loop_new (NULL, FALSE);
  gulkan_swapchain_renderer_initialize (GULKAN_SWAPCHAIN_RENDERER (self),
                                        background_color, NULL);
}

static void
_update_uniform_buffer (Example *self)
{
  int64_t t = gulkan_renderer_get_msec_since_start (GULKAN_RENDERER (self));
  t /= 5;

  graphene_matrix_t mv_matrix;
  graphene_matrix_init_identity (&mv_matrix);

  graphene_matrix_rotate_x (&mv_matrix, 45.0f + (0.25f * (float) t));
  graphene_matrix_rotate_y (&mv_matrix, 45.0f - (0.5f * (float) t));
  graphene_matrix_rotate_z (&mv_matrix, 10.0f + (0.15f * (float) t));

  graphene_point3d_t pos = {0.0f, 0.0f, -8.0f};
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

  gulkan_uniform_buffer_update (self->transformation_ubo, (gpointer) &ubo);
}

static void
_key_cb (GulkanWindow *window, GulkanKeyEvent *event, Example *self)
{
  (void) self;

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
_configure_cb (GulkanWindow *window, GulkanConfigureEvent *event, Example *self)
{
  GulkanContext *context = gulkan_renderer_get_context (GULKAN_RENDERER (self));
  VkInstance     instance = gulkan_context_get_instance_handle (context);

  VkSurfaceKHR surface;
  if (gulkan_window_create_surface (window, instance, &surface) != VK_SUCCESS)
    {
      g_printerr ("Creating surface failed.");
      return;
    }

  if (!gulkan_swapchain_renderer_resize (GULKAN_SWAPCHAIN_RENDERER (self),
                                         surface, event->extent))
    g_warning ("Resize failed.");
}

static gboolean
_iterate (gpointer _self)
{
  Example *self = (Example *) _self;

  gulkan_window_poll_events (self->window);

  if (self->should_quit)
    {
      g_main_loop_quit (self->loop);
      return FALSE;
    }

  _update_uniform_buffer (self);

  return gulkan_renderer_draw (GULKAN_RENDERER (self));
}

static void
_close_cb (GulkanWindow *window, Example *self)
{
  (void) window;
  g_main_loop_quit (self->loop);
}

static gboolean
_init (Example *self)
{
  VkExtent2D extent = gulkan_renderer_get_extent (GULKAN_RENDERER (self));
  self->window = gulkan_window_new (extent, "Gulkan Cube");
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

  gulkan_renderer_set_context (GULKAN_RENDERER (self), context);

  GulkanDevice *gulkan_device = gulkan_context_get_device (context);

  self->vb = gulkan_vertex_buffer_new (gulkan_device,
                                       VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
  gulkan_vertex_buffer_add_attribute (self->vb, 3, sizeof (positions), 0,
                                      (uint8_t *) positions);
  gulkan_vertex_buffer_add_attribute (self->vb, 3, sizeof (normals), 0,
                                      (uint8_t *) normals);
  gulkan_vertex_buffer_add_attribute (self->vb, 2, sizeof (tex_coords), 0,
                                      (uint8_t *) tex_coords);
  gulkan_vertex_buffer_add_attribute (self->vb, 3, sizeof (tangents), 0,
                                      (uint8_t *) tangents);

  if (!gulkan_vertex_buffer_upload (self->vb))
    return FALSE;

  if (!self->vb)
    return FALSE;

  self->diffuse_texture
    = gulkan_texture_new_from_resource (context, diffuse_pixbuf_uri,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        TRUE);

  if (!self->diffuse_texture)
    return FALSE;

  self->normal_texture
    = gulkan_texture_new_from_resource (context, normal_pixbuf_uri,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        TRUE);

  if (!self->normal_texture)
    return FALSE;

  self->transformation_ubo
    = gulkan_uniform_buffer_new (gulkan_device, sizeof (Transformation));
  if (!self->transformation_ubo)
    return FALSE;

  if (!gulkan_texture_init_sampler (self->diffuse_texture, VK_FILTER_LINEAR,
                                    VK_SAMPLER_ADDRESS_MODE_REPEAT))
    return FALSE;

  if (!gulkan_texture_init_sampler (self->normal_texture, VK_FILTER_LINEAR,
                                    VK_SAMPLER_ADDRESS_MODE_REPEAT))
    return FALSE;

  VkDescriptorSetLayoutBinding bindings[] = {
    {
      .binding = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    },
    {
      .binding = 1,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    },
    {
      .binding = 2,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    },
  };
  self->descriptor_pool = GULKAN_DESCRIPTOR_POOL_NEW (context, bindings, 1);
  self->descriptor_set
    = gulkan_descriptor_pool_create_set (self->descriptor_pool);

  gulkan_descriptor_set_update_buffer (self->descriptor_set, 0,
                                       self->transformation_ubo);
  gulkan_descriptor_set_update_texture (self->descriptor_set, 1,
                                        self->diffuse_texture);
  gulkan_descriptor_set_update_texture (self->descriptor_set, 2,
                                        self->normal_texture);

  g_signal_connect (self->window, "configure", (GCallback) _configure_cb, self);
  g_signal_connect (self->window, "close", (GCallback) _close_cb, self);
  g_signal_connect (self->window, "key", (GCallback) _key_cb, self);

  return TRUE;
}

static gboolean
_init_pipeline (GulkanSwapchainRenderer *renderer, gconstpointer data)
{
  (void) data;
  Example *self = GULKAN_EXAMPLE (renderer);

  VkVertexInputBindingDescription *binding_desc
    = gulkan_vertex_buffer_create_binding_desc (self->vb);
  VkVertexInputAttributeDescription *attrib_desc
    = gulkan_vertex_buffer_create_attrib_desc (self->vb);

  GulkanPipelineConfig config = {
    .sample_count = VK_SAMPLE_COUNT_1_BIT,
    .vertex_shader_uri = vertex_shader_uri,
    .fragment_shader_uri = fragment_shader_uri,
    .topology = gulkan_vertex_buffer_get_topology (self->vb),
    .attribs = attrib_desc,
    .attrib_count = gulkan_vertex_buffer_get_attrib_count (self->vb),
    .bindings = binding_desc,
    .binding_count = gulkan_vertex_buffer_get_attrib_count (self->vb),
    .blend_attachments = (VkPipelineColorBlendAttachmentState[]){
      {
        .colorWriteMask =
          VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        },
      },
    .rasterization_state = &(VkPipelineRasterizationStateCreateInfo){
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
      .lineWidth = 1.0f,
    },
    .dynamic_viewport = TRUE,
  };

  GulkanRenderPass *pass = gulkan_swapchain_renderer_get_render_pass (renderer);
  GulkanContext *context = gulkan_renderer_get_context (GULKAN_RENDERER (self));
  self->pipeline = gulkan_pipeline_new (context, self->descriptor_pool, pass,
                                        &config);

  g_free (binding_desc);
  g_free (attrib_desc);

  return TRUE;
}

static void
gulkan_example_class_init (ExampleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = _finalize;

  GulkanSwapchainRendererClass *parent_class
    = GULKAN_SWAPCHAIN_RENDERER_CLASS (klass);
  parent_class->init_draw_cmd = _init_draw_cmd;
  parent_class->init_pipeline = _init_pipeline;
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
