/*
 * gulkan
 * Copyright 2020 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef GULKAN_DESCRIPTOR_POOL_H_
#define GULKAN_DESCRIPTOR_POOL_H_

#include "gulkan-descriptor-set.h"

G_BEGIN_DECLS

#define GULKAN_TYPE_DESCRIPTOR_POOL gulkan_descriptor_pool_get_type ()
G_DECLARE_FINAL_TYPE (GulkanDescriptorPool,
                      gulkan_descriptor_pool,
                      GULKAN,
                      DESCRIPTOR_POOL,
                      GObject)

#define GULKAN_DESCRIPTOR_POOL_NEW(a, b, c)                                    \
  gulkan_descriptor_pool_new (a, b, G_N_ELEMENTS (b), c)

GulkanDescriptorPool *
gulkan_descriptor_pool_new (GulkanContext                      *context,
                            const VkDescriptorSetLayoutBinding *bindings,
                            uint32_t                            set_size,
                            uint32_t                            max_sets);

GulkanDescriptorSet *
gulkan_descriptor_pool_create_set (GulkanDescriptorPool *self);

VkPipelineLayout
gulkan_descriptor_pool_get_pipeline_layout (GulkanDescriptorPool *self);

G_END_DECLS

#endif /* GULKAN_DESCRIPTOR_POOL_H_ */
