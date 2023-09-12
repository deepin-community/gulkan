/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <vulkan/vulkan.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <graphene.h>

#include "plane-renderer.h"

typedef struct {
  float position[2];
  float uv[2];
} Vertex;

static const Vertex vertices[4] = {
  {{-1.f, -1.f}, {1.f, 0.f}},
  {{ 1.f, -1.f}, {0.f, 0.f}},
  {{ 1.f,  1.f}, {0.f, 1.f}},
  {{-1.f,  1.f}, {1.f, 1.f}}
};

static const uint16_t indices[6] = {0, 1, 2, 2, 3, 0};

struct _PlaneRenderer
{
  GulkanSwapchainRenderer parent;

  GulkanVertexBuffer *vb;
  GulkanDescriptorPool *descriptor_pool;

  VkPipeline pipeline;
  VkDescriptorSet descriptor_set;

  VkSampler sampler;
};

G_DEFINE_TYPE (PlaneRenderer, plane_renderer,
               GULKAN_TYPE_SWAPCHAIN_RENDERER)

static void
plane_renderer_init (PlaneRenderer *self)
{
  self->sampler = VK_NULL_HANDLE;
}

PlaneRenderer *
plane_renderer_new (void)
{
  GulkanClient *client = gulkan_client_new ();
  PlaneRenderer *self = plane_renderer_new_from_client (client);
  g_object_unref (client);
  return self;
}

PlaneRenderer *
plane_renderer_new_from_client (GulkanClient *client)
{
  PlaneRenderer *self =
    (PlaneRenderer*) g_object_new (PLANE_TYPE_RENDERER, 0);
  g_object_ref (client);
  gulkan_renderer_set_client (GULKAN_RENDERER (self), client);

  return self;
}

static void
_finalize (GObject *gobject)
{
  PlaneRenderer *self = PLANE_RENDERER (gobject);
  GulkanClient *client = gulkan_renderer_get_client (GULKAN_RENDERER (self));
  VkDevice device = gulkan_client_get_device_handle (client);
  gulkan_device_wait_idle (gulkan_client_get_device (client));

  /* Check if rendering was initialized */
  if (self->pipeline != VK_NULL_HANDLE)
    {
      g_object_unref (self->descriptor_pool);
      vkDestroySampler (device, self->sampler, NULL);
      g_object_unref (self->vb);
      vkDestroyPipeline (device, self->pipeline, NULL);
    }

  G_OBJECT_CLASS (plane_renderer_parent_class)->finalize (gobject);
}

static gboolean
_init_pipeline (GulkanSwapchainRenderer *renderer,
                gconstpointer            data)
{
  (void) data;
  PlaneRenderer *self = PLANE_RENDERER (renderer);
  GulkanClient *client = gulkan_renderer_get_client (GULKAN_RENDERER (self));
  VkDevice device = gulkan_client_get_device_handle (client);

  VkShaderModule frag_shader;
  if (!gulkan_renderer_create_shader_module (GULKAN_RENDERER (self),
                                             "/shaders/texture.frag.spv",
                                            &frag_shader))
    return FALSE;

  VkShaderModule vert_shader;
  if (!gulkan_renderer_create_shader_module (GULKAN_RENDERER (self),
                                             "/shaders/texture.vert.spv",
                                            &vert_shader))
    return FALSE;

  VkPipelineLayout pipeline_layout =
    gulkan_descriptor_pool_get_pipeline_layout (self->descriptor_pool);

  VkExtent2D extent = gulkan_renderer_get_extent (GULKAN_RENDERER (self));

  GulkanSwapchainRenderer *scr = GULKAN_SWAPCHAIN_RENDERER (self);
  VkRenderPass pass = gulkan_swapchain_renderer_get_render_pass_handle (scr);

  VkGraphicsPipelineCreateInfo pipeline_info = {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .stageCount = 2,
    .pStages = (VkPipelineShaderStageCreateInfo[]) {
      {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vert_shader,
        .pName = "main"
      },
      {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = frag_shader,
        .pName = "main"
      }
    },
    .pVertexInputState = &(VkPipelineVertexInputStateCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &(VkVertexInputBindingDescription) {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
      },
      .vertexAttributeDescriptionCount = 2,
      .pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[]) {
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
    },
    .pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    },
    .pViewportState = &(VkPipelineViewportStateCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = &(VkViewport) {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float) extent.width,
        .height = (float) extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
      },
      .scissorCount = 1,
      .pScissors = &(VkRect2D) {
        .offset = {0, 0},
        .extent = extent
      }
    },
    .pRasterizationState = &(VkPipelineRasterizationStateCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .lineWidth = 1.0f
    },
    .pMultisampleState = &(VkPipelineMultisampleStateCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    },
    .pColorBlendState = &(VkPipelineColorBlendStateCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &(VkPipelineColorBlendAttachmentState) {
        .colorWriteMask =
          VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
      },
     .blendConstants = {0.f, 0.f, 0.f, 0.f}
    },
    .layout = pipeline_layout,
    .renderPass = pass
  };

  VkResult res = vkCreateGraphicsPipelines (device, VK_NULL_HANDLE, 1,
                                           &pipeline_info, NULL,
                                           &self->pipeline);
  vk_check_error ("vkCreateGraphicsPipelines", res, FALSE);

  vkDestroyShaderModule (device, frag_shader, NULL);
  vkDestroyShaderModule (device, vert_shader, NULL);

  return TRUE;
}

static gboolean
_init_vertex_buffer (PlaneRenderer *self)
{
  GulkanClient *client = gulkan_renderer_get_client (GULKAN_RENDERER (self));
  GulkanDevice *device = gulkan_client_get_device (client);

  self->vb = gulkan_vertex_buffer_new ();
  if (!gulkan_vertex_buffer_alloc_data (self->vb, device, vertices,
                                        sizeof (vertices)))
    return FALSE;

  if (!gulkan_vertex_buffer_alloc_index_data (self->vb, device, indices,
                                              sizeof (uint16_t),
                                              G_N_ELEMENTS (indices)))
    return FALSE;

  return TRUE;
}

static void
_update_descriptor_set (PlaneRenderer *self,
                        VkImageView texture_image_view)
{
  GulkanClient *client = gulkan_renderer_get_client (GULKAN_RENDERER (self));
  GulkanDevice *gk_device = gulkan_client_get_device (client);
  VkDevice device = gulkan_device_get_handle (gk_device);

  VkWriteDescriptorSet *descriptor_writes = (VkWriteDescriptorSet [])
  {
    {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = self->descriptor_set,
      .dstBinding = 0,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImageInfo = &(VkDescriptorImageInfo) {
        .sampler = self->sampler,
        .imageView = texture_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
      },
      .pTexelBufferView = NULL
    }
  };

  vkUpdateDescriptorSets (device, 1, descriptor_writes, 0, NULL);
}

static gboolean
_init_texture_sampler (VkDevice device, VkSampler *sampler)
{
  VkSamplerCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .magFilter = VK_FILTER_LINEAR,
    .minFilter = VK_FILTER_LINEAR,
    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .anisotropyEnable = VK_TRUE,
    .maxAnisotropy = 16,
    .compareEnable = VK_FALSE,
    .compareOp = VK_COMPARE_OP_ALWAYS,
    .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    .unnormalizedCoordinates = VK_FALSE
  };

  VkResult res = vkCreateSampler (device, &info, NULL, sampler);
  vk_check_error ("vkCreateSampler", res, FALSE);

  return TRUE;
}

static void
_init_draw_cmd (GulkanSwapchainRenderer *renderer,
                VkCommandBuffer          cmd_buffer)
{
  PlaneRenderer *self = PLANE_RENDERER (renderer);

  vkCmdBindPipeline (cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                     self->pipeline);

  VkPipelineLayout pipeline_layout =
    gulkan_descriptor_pool_get_pipeline_layout (self->descriptor_pool);

  vkCmdBindDescriptorSets (cmd_buffer,
                           VK_PIPELINE_BIND_POINT_GRAPHICS,
                           pipeline_layout,
                           0, 1, &self->descriptor_set, 0, NULL);

  gulkan_vertex_buffer_draw_indexed (self->vb, cmd_buffer);
}

static void
plane_renderer_class_init (PlaneRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = _finalize;

  GulkanSwapchainRendererClass *parent_class =
    GULKAN_SWAPCHAIN_RENDERER_CLASS (klass);
  parent_class->init_draw_cmd = _init_draw_cmd;
  parent_class->init_pipeline = _init_pipeline;
}

gboolean
plane_renderer_update_texture_descriptor (PlaneRenderer *self,
                                          GulkanTexture *texture)
{
  VkImageView vk_image_view = gulkan_texture_get_image_view (texture);
  _update_descriptor_set (self, vk_image_view);

  if (!gulkan_swapchain_renderer_init_draw_cmd_buffers (GULKAN_SWAPCHAIN_RENDERER (self)))
    {
      g_warning ("Could not recreate command buffers.");
      return FALSE;
    }

  return TRUE;
}

gboolean
plane_renderer_initialize (PlaneRenderer *self,
                           VkSurfaceKHR   surface,
                           GulkanTexture *texture)
{
  GulkanClient *client = gulkan_renderer_get_client (GULKAN_RENDERER (self));
  VkDevice device = gulkan_client_get_device_handle (client);

  VkClearColorValue black = {
    .float32 = { 0.0f, 0.0f, 0.0f, 1.0f },
  };

  VkDescriptorSetLayoutBinding bindings[] = {
    {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    }
  };
  uint32_t set_count = 1;
  VkDescriptorPoolSize pool_sizes[] = {
    {
      .descriptorCount = set_count,
      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    }
  };

  self->descriptor_pool = GULKAN_DESCRIPTOR_POOL_NEW (device, bindings,
                                                      pool_sizes, set_count);
  if (!self->descriptor_pool)
    return FALSE;

  if (!gulkan_descriptor_pool_allocate_sets (self->descriptor_pool, 1,
                                            &self->descriptor_set))
    return FALSE;

  if (!_init_vertex_buffer (self))
    return FALSE;

  if (!_init_texture_sampler (device, &self->sampler))
    return FALSE;

  VkImageView vk_image_view = gulkan_texture_get_image_view (texture);
  _update_descriptor_set (self, vk_image_view);

  if (!gulkan_swapchain_renderer_initialize (GULKAN_SWAPCHAIN_RENDERER (self),
                                             surface, black, NULL))
   return FALSE;

  return TRUE;
}
