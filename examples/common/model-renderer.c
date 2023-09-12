/*
 * gulkan
 * Copyright 2020 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "model-renderer.h"

typedef struct _ModelRendererPrivate
{
  GulkanSwapchainRenderer parent;

  GulkanUniformBuffer *transformation_ubo;
  GulkanDescriptorPool *descriptor_pool;

  VkPipeline pipeline;
  VkDescriptorSet descriptor_set;
} ModelRendererPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (ModelRenderer, model_renderer,
                            GULKAN_TYPE_SWAPCHAIN_RENDERER)

static void
model_renderer_init (ModelRenderer *self)
{
  (void) self;
}

static void
_finalize (GObject *gobject)
{
  ModelRenderer *self = MODEL_RENDERER (gobject);
  ModelRendererPrivate *priv = model_renderer_get_instance_private (self);

  GulkanClient *client = gulkan_renderer_get_client (GULKAN_RENDERER (self));
  VkDevice device = gulkan_client_get_device_handle (client);
  gulkan_device_wait_idle (gulkan_client_get_device (client));

  g_object_unref (priv->descriptor_pool);
  vkDestroyPipeline (device, priv->pipeline, NULL);
  g_object_unref (priv->transformation_ubo);

  G_OBJECT_CLASS (model_renderer_parent_class)->finalize (gobject);
}

static void
_init_draw_cmd (GulkanSwapchainRenderer *renderer,
                VkCommandBuffer          cmd_buffer)
{
  ModelRenderer *self = MODEL_RENDERER (renderer);
  ModelRendererPrivate *priv = model_renderer_get_instance_private (self);

  ModelRendererClass *klass = MODEL_RENDERER_GET_CLASS (self);
  if (klass->init_draw_cmd == NULL)
    return;

  vkCmdBindPipeline (cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                     priv->pipeline);

  VkPipelineLayout layout =
    gulkan_descriptor_pool_get_pipeline_layout (priv->descriptor_pool);

  vkCmdBindDescriptorSets (cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           layout, 0, 1, &priv->descriptor_set, 0, NULL);

  klass->init_draw_cmd (self, cmd_buffer);
}

static gboolean
_init_pipeline (GulkanSwapchainRenderer *renderer,
                gconstpointer            data)
{
  const ShaderResources *resources = (const ShaderResources*) data;

  ModelRenderer *self = MODEL_RENDERER (renderer);
  ModelRendererPrivate *priv = model_renderer_get_instance_private (self);

  GulkanClient *client = gulkan_renderer_get_client (GULKAN_RENDERER (self));
  VkDevice device = gulkan_client_get_device_handle (client);

  VkPipelineVertexInputStateCreateInfo vi_create_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = 3,
    .pVertexBindingDescriptions =
      (VkVertexInputBindingDescription[]){
        { .binding = 0,
          .stride = 3 * sizeof (float),
          .inputRate = VK_VERTEX_INPUT_RATE_VERTEX },
        { .binding = 1,
          .stride = 3 * sizeof (float),
          .inputRate = VK_VERTEX_INPUT_RATE_VERTEX },
        { .binding = 2,
          .stride = 3 * sizeof (float),
          .inputRate = VK_VERTEX_INPUT_RATE_VERTEX } },
    .vertexAttributeDescriptionCount = 3,
    .pVertexAttributeDescriptions =
      (VkVertexInputAttributeDescription[]){
        { .location = 0,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32_SFLOAT,
          .offset = 0 },
        { .location = 1,
          .binding = 1,
          .format = VK_FORMAT_R32G32B32_SFLOAT,
          .offset = 0 },
        { .location = 2,
          .binding = 2,
          .format = VK_FORMAT_R32G32B32_SFLOAT,
          .offset = 0 } }
  };

  VkShaderModule vs_module;
  if (!gulkan_renderer_create_shader_module (
        GULKAN_RENDERER (self), resources->vert, &vs_module))
    return FALSE;

  VkShaderModule fs_module;
  if (!gulkan_renderer_create_shader_module (
        GULKAN_RENDERER (self), resources->frag, &fs_module))
    return FALSE;

  VkPipelineLayout layout =
    gulkan_descriptor_pool_get_pipeline_layout (priv->descriptor_pool);

  GulkanSwapchainRenderer *scr = GULKAN_SWAPCHAIN_RENDERER (self);
  VkRenderPass pass = gulkan_swapchain_renderer_get_render_pass_handle (scr);

  VkGraphicsPipelineCreateInfo pipeline_info = {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .stageCount = 2,
    .pStages =
      (VkPipelineShaderStageCreateInfo[]){
        {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = vs_module,
          .pName = "main",
        },
        {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = fs_module,
          .pName = "main",
        },
      },
    .pVertexInputState = &vi_create_info,
    .pInputAssemblyState =
      &(VkPipelineInputAssemblyStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
      },
    .pViewportState = &(VkPipelineViewportStateCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1,
    },
    .pDynamicState =
      &(VkPipelineDynamicStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates =
          (VkDynamicState[]){
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
         },
     },
    .pRasterizationState =
      &(VkPipelineRasterizationStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0f,
      },
    .pMultisampleState =
      &(VkPipelineMultisampleStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      },
    .pColorBlendState =
      &(VkPipelineColorBlendStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .blendConstants = { 0.f, 0.f, 0.f, 0.f },
        .pAttachments =
          (VkPipelineColorBlendAttachmentState[]){
            { .colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT },
          } },
    .layout = layout,
    .renderPass = pass,
  };

  VkResult res = vkCreateGraphicsPipelines (
    device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &priv->pipeline);

  vkDestroyShaderModule (device, vs_module, NULL);
  vkDestroyShaderModule (device, fs_module, NULL);

  vk_check_error ("vkCreateGraphicsPipelines", res, FALSE);

  return TRUE;
}

static void
model_renderer_class_init (ModelRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = _finalize;

  GulkanSwapchainRendererClass *parent_class =
    GULKAN_SWAPCHAIN_RENDERER_CLASS (klass);
  parent_class->init_draw_cmd = _init_draw_cmd;
  parent_class->init_pipeline = _init_pipeline;
}


static void
_update_descriptor_sets (ModelRenderer *self)
{
  ModelRendererPrivate *priv = model_renderer_get_instance_private (self);

  GulkanClient *client = gulkan_renderer_get_client (GULKAN_RENDERER (self));
  VkDevice device = gulkan_client_get_device_handle (client);

  vkUpdateDescriptorSets (
    device, 1,
    (VkWriteDescriptorSet[]){
      { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = priv->descriptor_set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pTexelBufferView = NULL,
        .pBufferInfo =
          &(VkDescriptorBufferInfo){
            .buffer =
              gulkan_uniform_buffer_get_handle (priv->transformation_ubo),
            .offset = 0,
            .range = VK_WHOLE_SIZE,
          } } },
    0, NULL);
}

gboolean
model_renderer_initialize (ModelRenderer    *self,
                           VkSurfaceKHR      surface,
                           VkClearColorValue clear_color,
                           ShaderResources  *resources)
{
  ModelRendererPrivate *priv = model_renderer_get_instance_private (self);
  GulkanClient *client = gulkan_renderer_get_client (GULKAN_RENDERER (self));
  GulkanDevice *gulkan_device = gulkan_client_get_device (client);

  priv->transformation_ubo =
    gulkan_uniform_buffer_new (gulkan_device, sizeof (Transformation));
  if (!priv->transformation_ubo)
    return FALSE;

  VkDescriptorSetLayoutBinding bindings[] = {
    {
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      .pImmutableSamplers = NULL,
    },
  };

  uint32_t set_count = 1;
  VkDescriptorPoolSize pool_sizes[] = {
    {
      .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = set_count,
    },
  };

  VkDevice device = gulkan_device_get_handle (gulkan_device);
  priv->descriptor_pool = GULKAN_DESCRIPTOR_POOL_NEW (device, bindings,
                                                      pool_sizes, set_count);
  if (!priv->descriptor_pool)
    return FALSE;

  if (!gulkan_descriptor_pool_allocate_sets (priv->descriptor_pool, 1,
                                            &priv->descriptor_set))
    return FALSE;

  _update_descriptor_sets (self);

  if (!gulkan_swapchain_renderer_initialize (GULKAN_SWAPCHAIN_RENDERER (self),
                                             surface, clear_color,
                                             (gpointer) resources))
    return FALSE;

  return TRUE;
}

void
model_renderer_update_ubo (ModelRenderer *self, gpointer ubo)
{
  ModelRendererPrivate *priv = model_renderer_get_instance_private (self);
  gulkan_uniform_buffer_update (priv->transformation_ubo, ubo);
}
