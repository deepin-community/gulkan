/*
 * gulkan
 * Copyright 2022 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "gulkan-pipeline.h"
#include "gulkan-descriptor-pool.h"
#include "gulkan-render-pass.h"

struct _GulkanPipeline
{
  GObject parent;

  GulkanContext *context;

  VkPipeline handle;
};

G_DEFINE_TYPE (GulkanPipeline, gulkan_pipeline, G_TYPE_OBJECT)

static void
gulkan_pipeline_init (GulkanPipeline *self)
{
  self->context = NULL;
  self->handle = VK_NULL_HANDLE;
}

static gboolean
_init (GulkanPipeline       *self,
       GulkanDescriptorPool *descriptor_pool,
       GulkanRenderPass     *render_pass,
       GulkanPipelineConfig *config)
{
  GulkanDevice *device = gulkan_context_get_device (self->context);

  VkShaderModule vs = config->vertex_shader;
  if (vs == VK_NULL_HANDLE)
    {
      if (!gulkan_device_create_shader_module (device,
                                               config->vertex_shader_uri, &vs))
        return FALSE;
    }

  VkShaderModule fs = config->fragment_shader;
  if (fs == VK_NULL_HANDLE)
    {
      if (!gulkan_device_create_shader_module (device,
                                               config->fragment_shader_uri,
                                               &fs))
        return FALSE;
    }

  VkPipelineLayout layout
    = gulkan_descriptor_pool_get_pipeline_layout (descriptor_pool);

  VkPipelineViewportStateCreateInfo dynamic_viewport_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .scissorCount = 1,
  };
  VkPipelineDynamicStateCreateInfo dynamic_dynamic_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = 2,
    .pDynamicStates =
      (VkDynamicState[]){
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
     },
  };

  VkPipelineViewportStateCreateInfo static_viewport_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .pViewports = &(VkViewport) {
      .x = 0.0f,
      .y = config->flip_y ? (float) config->extent.height : 0.0f,
      .width = (float) config->extent.width,
      .height = config->flip_y ? - (float) config->extent.height : (float) config->extent.height,
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
    },
    .scissorCount = 1,
    .pScissors = &(VkRect2D) {
      .offset = {0, 0},
      .extent = config->extent,
    },
  };

  VkPipelineDynamicStateCreateInfo static_dynamic_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
  };

  VkPipelineViewportStateCreateInfo *viewport_info;
  VkPipelineDynamicStateCreateInfo  *dynamic_info;
  if (config->dynamic_viewport)
    {
      viewport_info = &dynamic_viewport_info;
      dynamic_info = &dynamic_dynamic_info;
    }
  else
    {
      viewport_info = &static_viewport_info;
      dynamic_info = &static_dynamic_info;
    }

  VkGraphicsPipelineCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .layout = layout,
    .pVertexInputState = &(VkPipelineVertexInputStateCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pVertexAttributeDescriptions = config->attribs,
      .vertexAttributeDescriptionCount = config->attrib_count,
      .pVertexBindingDescriptions = config->bindings,
      .vertexBindingDescriptionCount = config->binding_count,
    },
    .pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo) {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = config->topology,
      .primitiveRestartEnable = VK_FALSE,
    },
    .pRasterizationState = config->rasterization_state,
    .pMultisampleState = &(VkPipelineMultisampleStateCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = config->sample_count,
      .minSampleShading = 0.0f,
      .pSampleMask = &(uint32_t) { 0xFFFFFFFF },
      .alphaToCoverageEnable = VK_FALSE,
    },
    .pDepthStencilState = config->depth_stencil_state,
    .pViewportState = viewport_info,
    .pDynamicState = dynamic_info,
    .pColorBlendState = &(VkPipelineColorBlendStateCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .attachmentCount = 1,
      .blendConstants = {0,0,0,0},
      .pAttachments = config->blend_attachments,
    },
    .stageCount = 2,
    .pStages = (VkPipelineShaderStageCreateInfo []) {
      {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vs,
        .pName = "main",
      },
      {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fs,
        .pName = "main",
      },
    },
    .renderPass = gulkan_render_pass_get_handle (render_pass),
    .subpass = 0,
  };

  g_assert (info.pViewportState->sType
            == VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);

  VkDevice vk_device = gulkan_context_get_device_handle (self->context);
  VkResult res;
  res = vkCreateGraphicsPipelines (vk_device, VK_NULL_HANDLE, 1, &info, NULL,
                                   &self->handle);
  vk_check_error ("vkCreateGraphicsPipelines", res, FALSE);

  vkDestroyShaderModule (vk_device, vs, NULL);
  vkDestroyShaderModule (vk_device, fs, NULL);

  return TRUE;
}

GulkanPipeline *
gulkan_pipeline_new (GulkanContext        *context,
                     GulkanDescriptorPool *descriptor_pool,
                     GulkanRenderPass     *render_pass,
                     GulkanPipelineConfig *config)
{
  GulkanPipeline *self = (GulkanPipeline *) g_object_new (GULKAN_TYPE_PIPELINE,
                                                          0);
  self->context = g_object_ref (context);

  if (!_init (self, descriptor_pool, render_pass, config))
    {
      g_object_unref (self);
      return NULL;
    }

  return self;
}

static void
_finalize (GObject *gobject)
{
  GulkanPipeline *self = GULKAN_PIPELINE (gobject);
  g_clear_object (&self->context);
  G_OBJECT_CLASS (gulkan_pipeline_parent_class)->finalize (gobject);
}

static void
gulkan_pipeline_class_init (GulkanPipelineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = _finalize;
}

void
gulkan_pipeline_bind (GulkanPipeline *self, VkCommandBuffer cmd_buffer)
{
  vkCmdBindPipeline (cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, self->handle);
}
