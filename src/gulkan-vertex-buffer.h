/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef GULKAN_VERTEX_BUFFER_H_
#define GULKAN_VERTEX_BUFFER_H_

#if !defined(GULKAN_INSIDE) && !defined(GULKAN_COMPILATION)
#error "Only <gulkan.h> can be included directly."
#endif

#include <glib-object.h>
#include <graphene.h>
#include <vulkan/vulkan.h>

#include "gulkan-buffer.h"
#include "gulkan-device.h"

G_BEGIN_DECLS

#define GULKAN_TYPE_VERTEX_BUFFER gulkan_vertex_buffer_get_type ()
G_DECLARE_FINAL_TYPE (GulkanVertexBuffer,
                      gulkan_vertex_buffer,
                      GULKAN,
                      VERTEX_BUFFER,
                      GObject)

GulkanVertexBuffer *
gulkan_vertex_buffer_new (GulkanDevice *device, VkPrimitiveTopology topology);

void
gulkan_vertex_buffer_draw (GulkanVertexBuffer *self,
                           VkCommandBuffer     cmd_buffer);

void
gulkan_vertex_buffer_draw_indexed (GulkanVertexBuffer *self,
                                   VkCommandBuffer     cmd_buffer);

void
gulkan_vertex_buffer_reset (GulkanVertexBuffer *self);

gboolean
gulkan_vertex_buffer_map_array (GulkanVertexBuffer *self);

gboolean
gulkan_vertex_buffer_alloc_empty (GulkanVertexBuffer *self,
                                  uint32_t            multiplier);

gboolean
gulkan_vertex_buffer_alloc_array (GulkanVertexBuffer *self);

gboolean
gulkan_vertex_buffer_alloc_data (GulkanVertexBuffer *self,
                                 const void         *data,
                                 VkDeviceSize        size);

gboolean
gulkan_vertex_buffer_alloc_index_data (GulkanVertexBuffer *self,
                                       const void         *data,
                                       VkIndexType         type,
                                       size_t              element_count);

void
gulkan_vertex_buffer_append_position_uv (GulkanVertexBuffer *self,
                                         graphene_vec4_t    *vec,
                                         float               u,
                                         float               v);

void
gulkan_vertex_buffer_append_with_color (GulkanVertexBuffer *self,
                                        graphene_vec4_t    *vec,
                                        graphene_vec3_t    *color);

gboolean
gulkan_vertex_buffer_is_initialized (GulkanVertexBuffer *self);

void
gulkan_vertex_buffer_bind_with_offsets (GulkanVertexBuffer *self,
                                        VkCommandBuffer     cmd_buffer);

void
gulkan_vertex_buffer_add_attribute (GulkanVertexBuffer *self,
                                    size_t              stride,
                                    size_t              size,
                                    size_t              offset,
                                    const uint8_t      *bytes);

GulkanBuffer *
gulkan_vertex_buffer_get_index_buffer (GulkanVertexBuffer *self);

gboolean
gulkan_vertex_buffer_upload (GulkanVertexBuffer *self);

VkPrimitiveTopology
gulkan_vertex_buffer_get_topology (GulkanVertexBuffer *self);

uint32_t
gulkan_vertex_buffer_get_attrib_count (GulkanVertexBuffer *self);

VkVertexInputAttributeDescription *
gulkan_vertex_buffer_create_attrib_desc (GulkanVertexBuffer *self);

VkVertexInputBindingDescription *
gulkan_vertex_buffer_create_binding_desc (GulkanVertexBuffer *self);

VkDeviceSize
gulkan_vertex_buffer_get_index_type_size (VkIndexType type);

G_END_DECLS

#endif /* GULKAN_VERTEX_BUFFER_H_ */
