/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <vulkan/vulkan.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <graphene.h>

#include "plane-renderer.h"

typedef struct
{
  float position[2];
  float uv[2];
} Vertex;

// clang-format off
static const Vertex vertices[4] = {
  {{-1.f, -1.f}, {1.f, 0.f}},
  {{ 1.f, -1.f}, {0.f, 0.f}},
  {{ 1.f,  1.f}, {0.f, 1.f}},
  {{-1.f,  1.f}, {1.f, 1.f}}
};
// clang-format on

static const uint16_t indices[6] = {0, 1, 2, 2, 3, 0};

struct _PlaneRenderer
{
  GulkanSwapchainRenderer parent;

  GulkanVertexBuffer   *vb;
  GulkanPipeline       *pipeline;
  GulkanDescriptorSet  *descriptor_set;
  GulkanDescriptorPool *descriptor_pool;
};

G_DEFINE_TYPE (PlaneRenderer, plane_renderer, GULKAN_TYPE_SWAPCHAIN_RENDERER)

static void
plane_renderer_init (PlaneRenderer *self)
{
  self->vb = NULL;
  self->descriptor_set = NULL;
  self->pipeline = VK_NULL_HANDLE;
  self->descriptor_set = VK_NULL_HANDLE;
}

PlaneRenderer *
plane_renderer_new (void)
{
  GulkanContext *context = gulkan_context_new ();
  PlaneRenderer *self = plane_renderer_new_from_context (context);
  g_object_unref (context);
  return self;
}

PlaneRenderer *
plane_renderer_new_from_context (GulkanContext *context)
{
  PlaneRenderer *self = (PlaneRenderer *) g_object_new (PLANE_TYPE_RENDERER, 0);
  g_object_ref (context);
  gulkan_renderer_set_context (GULKAN_RENDERER (self), context);

  return self;
}

static void
_finalize (GObject *gobject)
{
  PlaneRenderer *self = PLANE_RENDERER (gobject);
  GulkanContext *context = gulkan_renderer_get_context (GULKAN_RENDERER (self));
  gulkan_device_wait_idle (gulkan_context_get_device (context));

  /* Check if rendering was initialized */
  if (self->pipeline != VK_NULL_HANDLE)
    {
      g_object_unref (self->descriptor_set);
      g_object_unref (self->vb);
      g_object_unref (self->pipeline);
    }

  G_OBJECT_CLASS (plane_renderer_parent_class)->finalize (gobject);
}

static gboolean
_init_pipeline (GulkanSwapchainRenderer *renderer, gconstpointer data)
{
  (void) data;
  PlaneRenderer *self = PLANE_RENDERER (renderer);
  GulkanContext *context = gulkan_renderer_get_context (GULKAN_RENDERER (self));

  GulkanPipelineConfig config = {
    .sample_count = VK_SAMPLE_COUNT_1_BIT,
    .vertex_shader_uri = "/shaders/texture.vert.spv",
    .fragment_shader_uri = "/shaders/texture.frag.spv",
    .topology = gulkan_vertex_buffer_get_topology (self->vb),
    .attribs = (VkVertexInputAttributeDescription[]) {
        {
          .location = 0,
          .binding = 0,
          .format = VK_FORMAT_R32G32_SFLOAT,
          .offset = offsetof (Vertex, position)
        },
        {
          .location = 1,
          .binding = 0,
          .format = VK_FORMAT_R32G32_SFLOAT,
          .offset = offsetof (Vertex, uv)
        }
      },
    .attrib_count = 2,
    .bindings = &(VkVertexInputBindingDescription) {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
      },
    .binding_count = 1,
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
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .lineWidth = 1.0f
    },
    .dynamic_viewport = TRUE,
  };

  GulkanRenderPass *pass = gulkan_swapchain_renderer_get_render_pass (renderer);
  self->pipeline = gulkan_pipeline_new (context, self->descriptor_pool, pass,
                                        &config);

  return TRUE;
}

static gboolean
_init_vertex_buffer (PlaneRenderer *self)
{
  GulkanContext *context = gulkan_renderer_get_context (GULKAN_RENDERER (self));
  GulkanDevice  *device = gulkan_context_get_device (context);

  self->vb = gulkan_vertex_buffer_new (device,
                                       VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  if (!gulkan_vertex_buffer_alloc_data (self->vb, vertices, sizeof (vertices)))
    return FALSE;

  if (!gulkan_vertex_buffer_alloc_index_data (self->vb, indices,
                                              VK_INDEX_TYPE_UINT16,
                                              G_N_ELEMENTS (indices)))
    return FALSE;

  return TRUE;
}

static void
_init_draw_cmd (GulkanSwapchainRenderer *renderer, VkCommandBuffer cmd_buffer)
{
  PlaneRenderer *self = PLANE_RENDERER (renderer);

  gulkan_pipeline_bind (self->pipeline, cmd_buffer);

  VkPipelineLayout layout
    = gulkan_descriptor_pool_get_pipeline_layout (self->descriptor_pool);

  gulkan_descriptor_set_bind (self->descriptor_set, layout, cmd_buffer);

  gulkan_vertex_buffer_draw_indexed (self->vb, cmd_buffer);
}

static void
plane_renderer_class_init (PlaneRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = _finalize;

  GulkanSwapchainRendererClass *parent_class
    = GULKAN_SWAPCHAIN_RENDERER_CLASS (klass);
  parent_class->init_draw_cmd = _init_draw_cmd;
  parent_class->init_pipeline = _init_pipeline;
}

gboolean
plane_renderer_update_texture (PlaneRenderer *self, GulkanTexture *texture)
{
  if (!gulkan_texture_init_sampler (texture, VK_FILTER_LINEAR,
                                    VK_SAMPLER_ADDRESS_MODE_REPEAT))
    return FALSE;

  gulkan_descriptor_set_update_texture (self->descriptor_set, 0, texture);

  if (!gulkan_swapchain_renderer_init_draw_cmd_buffers (
        GULKAN_SWAPCHAIN_RENDERER (self)))
    {
      g_warning ("Could not recreate command buffers.");
      return FALSE;
    }

  return TRUE;
}

gboolean
plane_renderer_initialize (PlaneRenderer *self, GulkanTexture *texture)
{
  GulkanContext *context = gulkan_renderer_get_context (GULKAN_RENDERER (self));

  VkClearColorValue black = {
    .float32 = {0.0f, 0.0f, 0.0f, 1.0f},
  };

  if (!_init_vertex_buffer (self))
    return FALSE;

  VkDescriptorSetLayoutBinding bindings[] = {
    {
      .binding = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    },
  };
  self->descriptor_pool = GULKAN_DESCRIPTOR_POOL_NEW (context, bindings, 1);
  self->descriptor_set
    = gulkan_descriptor_pool_create_set (self->descriptor_pool);

  if (!gulkan_texture_init_sampler (texture, VK_FILTER_LINEAR,
                                    VK_SAMPLER_ADDRESS_MODE_REPEAT))
    return FALSE;

  gulkan_descriptor_set_update_texture (self->descriptor_set, 0, texture);

  gulkan_swapchain_renderer_initialize (GULKAN_SWAPCHAIN_RENDERER (self), black,
                                        NULL);

  return TRUE;
}
