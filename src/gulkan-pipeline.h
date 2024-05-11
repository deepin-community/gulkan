/*
 * gulkan
 * Copyright 2022 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef GULKAN_PIPELINE_H_
#define GULKAN_PIPELINE_H_

#include <glib-object.h>

#include "gulkan-context.h"
#include "gulkan-descriptor-pool.h"
#include "gulkan-render-pass.h"

G_BEGIN_DECLS

#define GULKAN_TYPE_PIPELINE gulkan_pipeline_get_type ()
G_DECLARE_FINAL_TYPE (GulkanPipeline,
                      gulkan_pipeline,
                      GULKAN,
                      PIPELINE,
                      GObject)

typedef struct __attribute__ ((__packed__))
{
  VkExtent2D                                    extent;
  VkSampleCountFlagBits                         sample_count;
  const char                                   *vertex_shader_uri;
  VkShaderModule                                vertex_shader;
  const char                                   *fragment_shader_uri;
  VkShaderModule                                fragment_shader;
  VkPrimitiveTopology                           topology;
  const VkVertexInputAttributeDescription      *attribs;
  uint32_t                                      attrib_count;
  const VkVertexInputBindingDescription        *bindings;
  uint32_t                                      binding_count;
  const VkPipelineDepthStencilStateCreateInfo  *depth_stencil_state;
  const VkPipelineColorBlendAttachmentState    *blend_attachments;
  const VkPipelineRasterizationStateCreateInfo *rasterization_state;
  gboolean                                      dynamic_viewport;
  gboolean                                      flip_y;
} GulkanPipelineConfig;

GulkanPipeline *
gulkan_pipeline_new (GulkanContext        *context,
                     GulkanDescriptorPool *descriptor_pool,
                     GulkanRenderPass     *render_pass,
                     GulkanPipelineConfig *config);

void
gulkan_pipeline_bind (GulkanPipeline *self, VkCommandBuffer cmd_buffer);

G_END_DECLS

#endif /* GULKAN_PIPELINE_H_ */
