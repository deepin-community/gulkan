/*
 * gulkan
 * Copyright 2021 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef GULKAN_DESCRIPTOR_SET_H_
#define GULKAN_DESCRIPTOR_SET_H_

#include <glib-object.h>
#include <vulkan/vulkan.h>

#include "gulkan-texture.h"
#include "gulkan-uniform-buffer.h"

G_BEGIN_DECLS

#define GULKAN_TYPE_DESCRIPTOR_SET gulkan_descriptor_set_get_type ()
G_DECLARE_FINAL_TYPE (GulkanDescriptorSet,
                      gulkan_descriptor_set,
                      GULKAN,
                      DESCRIPTOR_SET,
                      GObject)

GulkanDescriptorSet *
gulkan_descriptor_set_new (GulkanContext   *context,
                           VkDescriptorSet  handle,
                           VkDescriptorPool pool,
                           uint32_t         size);

void
gulkan_descriptor_set_bind (GulkanDescriptorSet *self,
                            VkPipelineLayout     layout,
                            VkCommandBuffer      cmd_buffer);

void
gulkan_descriptor_set_update_buffer (GulkanDescriptorSet *self,
                                     guint                index,
                                     GulkanUniformBuffer *buffer);

void
gulkan_descriptor_set_update_buffer_at (GulkanDescriptorSet *self,
                                        guint                index,
                                        guint                binding,
                                        GulkanUniformBuffer *buffer);

void
gulkan_descriptor_set_update_texture (GulkanDescriptorSet *self,
                                      guint                index,
                                      GulkanTexture       *texture);

void
gulkan_descriptor_set_update_texture_at (GulkanDescriptorSet *self,
                                         guint                index,
                                         guint                binding,
                                         GulkanTexture       *texture);

void
gulkan_descriptor_set_update_view_sampler (GulkanDescriptorSet *self,
                                           guint                index,
                                           VkImageView          view,
                                           VkSampler            sampler);

G_END_DECLS

#endif /* GULKAN_DESCRIPTOR_SET_H_ */
