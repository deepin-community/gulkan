/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef GULKAN_CONTEXT_H_
#define GULKAN_CONTEXT_H_

#if !defined(GULKAN_INSIDE) && !defined(GULKAN_COMPILATION)
#error "Only <gulkan.h> can be included directly."
#endif

#include <glib-object.h>
#include <vulkan/vulkan.h>

#include "gulkan-device.h"
#include "gulkan-instance.h"

G_BEGIN_DECLS

#define GULKAN_TYPE_CONTEXT gulkan_context_get_type ()
G_DECLARE_DERIVABLE_TYPE (GulkanContext,
                          gulkan_context,
                          GULKAN,
                          CONTEXT,
                          GObject)

struct _GulkanContextClass
{
  GObjectClass parent_class;
};

GulkanContext *
gulkan_context_new (void);

GulkanContext *
gulkan_context_new_from_extensions (GSList          *instance_ext_list,
                                    GSList          *device_ext_list,
                                    VkPhysicalDevice physical_device);

GulkanContext *
gulkan_context_new_from_instance (GulkanInstance  *instance,
                                  GSList          *device_ext_list,
                                  VkPhysicalDevice physical_device);

GulkanContext *
gulkan_context_new_from_vk (VkInstance       vk_instance,
                            VkPhysicalDevice vk_physical_device,
                            VkDevice         vk_device,
                            uint32_t         graphics_queue_index,
                            uint32_t         transfer_queue_index);

VkPhysicalDevice
gulkan_context_get_physical_device_handle (GulkanContext *self);

VkDevice
gulkan_context_get_device_handle (GulkanContext *self);

GulkanDevice *
gulkan_context_get_device (GulkanContext *self);

VkInstance
gulkan_context_get_instance_handle (GulkanContext *self);

GulkanInstance *
gulkan_context_get_instance (GulkanContext *self);

GSList *
gulkan_context_get_external_memory_instance_extensions (void);

GSList *
gulkan_context_get_external_memory_device_extensions (void);

G_END_DECLS

#endif /* GULKAN_CONTEXT_H_ */
