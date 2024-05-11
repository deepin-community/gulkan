/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "gulkan-vertex-buffer.h"

typedef struct
{
  size_t         stride;
  size_t         size;
  size_t         offset;
  const uint8_t *bytes;
} GulkanVertexAttribute;

typedef struct
{
  VkBuffer     *buffers;
  VkDeviceSize *offsets;
} GulkanVertexBindingCache;

struct _GulkanVertexBuffer
{
  GObject parent;

  GulkanDevice *device;

  VkPrimitiveTopology topology;

  GulkanBuffer *buffer;
  GulkanBuffer *index_buffer;

  VkIndexType index_type;

  uint32_t count;

  GArray *array;

  GSList *attributes;

  GulkanVertexBindingCache binding_cache;
};

G_DEFINE_TYPE (GulkanVertexBuffer, gulkan_vertex_buffer, G_TYPE_OBJECT)

static void
gulkan_vertex_buffer_finalize (GObject *gobject);

static void
gulkan_vertex_buffer_class_init (GulkanVertexBufferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gulkan_vertex_buffer_finalize;
}

static void
gulkan_vertex_buffer_init (GulkanVertexBuffer *self)
{
  self->device = NULL;
  self->count = 0;
  self->array = g_array_new (FALSE, FALSE, sizeof (float));
  self->attributes = NULL;
  self->index_type = VK_INDEX_TYPE_UINT16;
}

GulkanVertexBuffer *
gulkan_vertex_buffer_new (GulkanDevice *device, VkPrimitiveTopology topology)
{
  GulkanVertexBuffer *self = (GulkanVertexBuffer *)
    g_object_new (GULKAN_TYPE_VERTEX_BUFFER, 0);
  self->topology = topology;
  self->device = g_object_ref (device);
  return self;
}

static VkDeviceSize
_get_attributes_size (GulkanVertexBuffer *self)
{
  VkDeviceSize size = 0;

  for (GSList *l = self->attributes; l; l = l->next)
    {
      GulkanVertexAttribute *attribute = l->data;
      size += attribute->size;
    }

  return size;
}

void
gulkan_vertex_buffer_add_attribute (GulkanVertexBuffer *self,
                                    size_t              stride,
                                    size_t              size,
                                    size_t              offset,
                                    const uint8_t      *bytes)
{
  g_assert (bytes != NULL);

  GulkanVertexAttribute *attribute = g_malloc (sizeof (GulkanVertexAttribute));
  attribute->size = size;
  attribute->stride = stride;
  attribute->offset = offset;
  attribute->bytes = bytes;
  self->attributes = g_slist_append (self->attributes, attribute);
}

GulkanBuffer *
gulkan_vertex_buffer_get_index_buffer (GulkanVertexBuffer *self)
{
  return self->index_buffer;
}

gboolean
gulkan_vertex_buffer_upload (GulkanVertexBuffer *self)
{
  VkDeviceSize offset = 0;
  VkDeviceSize whole_size = _get_attributes_size (self);

  self->buffer = gulkan_buffer_new (self->device, whole_size,
                                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if (!self->buffer)
    return FALSE;

  uint32_t binding_count = g_slist_length (self->attributes);
  self->binding_cache.buffers = g_malloc (sizeof (VkBuffer) * binding_count);
  self->binding_cache.offsets = g_malloc (sizeof (VkDeviceSize)
                                          * binding_count);

  VkBuffer       buffer = gulkan_buffer_get_handle (self->buffer);
  VkDeviceMemory device_memory = gulkan_buffer_get_memory_handle (self->buffer);

  uint8_t *map;

  VkDevice device = gulkan_device_get_handle (self->device);
  VkResult res = vkMapMemory (device, device_memory, offset, whole_size, 0,
                              (void *) &map);

  for (guint i = 0; i < binding_count; i++)
    {
      GSList                *entry = g_slist_nth (self->attributes, i);
      GulkanVertexAttribute *attribute = entry->data;

      vk_check_error ("vkMapMemory", res, FALSE);

      memcpy (&map[offset], attribute->bytes + attribute->offset,
              attribute->size);

      self->binding_cache.buffers[i] = buffer;
      self->binding_cache.offsets[i] = offset;

      offset += attribute->size;
    }

  vkUnmapMemory (device, device_memory);
  vk_check_error ("vkUnmapMemory", res, FALSE);

  return TRUE;
}

static void
gulkan_vertex_buffer_finalize (GObject *gobject)
{
  GulkanVertexBuffer *self = GULKAN_VERTEX_BUFFER (gobject);

  g_array_free (self->array, TRUE);

  if (!self->device)
    return;

  g_clear_object (&self->buffer);
  g_clear_object (&self->index_buffer);

  g_slist_free_full (self->attributes, g_free);

  g_clear_object (&self->device);
}

void
gulkan_vertex_buffer_draw (GulkanVertexBuffer *self, VkCommandBuffer cmd_buffer)
{
  VkDeviceSize offsets[1] = {0};
  VkBuffer     buffer = gulkan_buffer_get_handle (self->buffer);
  vkCmdBindVertexBuffers (cmd_buffer, 0, 1, &buffer, &offsets[0]);
  vkCmdDraw (cmd_buffer, self->count, 1, 0, 0);
}

void
gulkan_vertex_buffer_draw_indexed (GulkanVertexBuffer *self,
                                   VkCommandBuffer     cmd_buffer)
{
  VkDeviceSize offsets[1] = {0};
  VkBuffer     buffer = gulkan_buffer_get_handle (self->buffer);
  VkBuffer     index_buffer = gulkan_buffer_get_handle (self->index_buffer);
  vkCmdBindVertexBuffers (cmd_buffer, 0, 1, &buffer, &offsets[0]);
  vkCmdBindIndexBuffer (cmd_buffer, index_buffer, 0, self->index_type);
  vkCmdDrawIndexed (cmd_buffer, self->count, 1, 0, 0, 0);
}

void
gulkan_vertex_buffer_bind_with_offsets (GulkanVertexBuffer *self,
                                        VkCommandBuffer     cmd_buffer)
{
  vkCmdBindVertexBuffers (cmd_buffer, 0, g_slist_length (self->attributes),
                          self->binding_cache.buffers,
                          self->binding_cache.offsets);
}

void
gulkan_vertex_buffer_reset (GulkanVertexBuffer *self)
{
  g_array_free (self->array, TRUE);

  self->array = g_array_new (FALSE, FALSE, sizeof (float));
  self->count = 0;
}

static void
_append_float (GArray *array, float v)
{
  g_array_append_val (array, v);
}

static void
gulkan_vertex_buffer_append_vec2f (GulkanVertexBuffer *self, float u, float v)
{
  _append_float (self->array, u);
  _append_float (self->array, v);
}

static void
gulkan_vertex_buffer_append_vec3 (GulkanVertexBuffer *self, graphene_vec3_t *v)
{
  _append_float (self->array, graphene_vec3_get_x (v));
  _append_float (self->array, graphene_vec3_get_y (v));
  _append_float (self->array, graphene_vec3_get_z (v));
}

static void
gulkan_vertex_buffer_append_vec4 (GulkanVertexBuffer *self, graphene_vec4_t *v)
{
  _append_float (self->array, graphene_vec4_get_x (v));
  _append_float (self->array, graphene_vec4_get_y (v));
  _append_float (self->array, graphene_vec4_get_z (v));
}

void
gulkan_vertex_buffer_append_with_color (GulkanVertexBuffer *self,
                                        graphene_vec4_t    *vec,
                                        graphene_vec3_t    *color)
{
  gulkan_vertex_buffer_append_vec4 (self, vec);
  gulkan_vertex_buffer_append_vec3 (self, color);

  self->count++;
}

void
gulkan_vertex_buffer_append_position_uv (GulkanVertexBuffer *self,
                                         graphene_vec4_t    *vec,
                                         float               u,
                                         float               v)
{
  gulkan_vertex_buffer_append_vec4 (self, vec);
  gulkan_vertex_buffer_append_vec2f (self, u, v);

  self->count++;
}

gboolean
gulkan_vertex_buffer_alloc_array (GulkanVertexBuffer *self)
{
  self->buffer
    = gulkan_buffer_new_from_data (self->device, self->array->data,
                                   self->array->len * sizeof (float),
                                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                     | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  return self->buffer != NULL;
}

gboolean
gulkan_vertex_buffer_alloc_data (GulkanVertexBuffer *self,
                                 const void         *data,
                                 VkDeviceSize        size)
{
  self->buffer
    = gulkan_buffer_new_from_data (self->device, data, size,
                                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                     | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  return self->buffer != NULL;
}

VkDeviceSize
gulkan_vertex_buffer_get_index_type_size (VkIndexType type)
{
  VkDeviceSize element_size = 0;
  switch (type)
    {
      case VK_INDEX_TYPE_UINT16:
        element_size = sizeof (uint16_t);
        break;
      case VK_INDEX_TYPE_UINT32:
        element_size = sizeof (uint32_t);
        break;
      case VK_INDEX_TYPE_UINT8_EXT:
        element_size = sizeof (uint8_t);
        break;
      default:
        g_warning ("Unknown index type %d", type);
        g_assert (FALSE);
    }
  return element_size;
}

gboolean
gulkan_vertex_buffer_alloc_index_data (GulkanVertexBuffer *self,
                                       const void         *data,
                                       VkIndexType         type,
                                       size_t              element_count)
{

  VkDeviceSize element_size = gulkan_vertex_buffer_get_index_type_size (type);
  self->index_type = type;

  self->index_buffer
    = gulkan_buffer_new_from_data (self->device, data,
                                   element_size * element_count,
                                   VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                     | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  self->count = (uint32_t) element_count;

  return self->index_buffer != NULL;
}

gboolean
gulkan_vertex_buffer_alloc_empty (GulkanVertexBuffer *self, uint32_t multiplier)
{
  if (self->array->len == 0)
    {
      g_printerr ("Vertex array is empty.\n");
      return FALSE;
    }

  self->buffer = gulkan_buffer_new (self->device,
                                    sizeof (float) * self->array->len
                                      * multiplier,
                                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  return self->buffer != NULL;
}

gboolean
gulkan_vertex_buffer_map_array (GulkanVertexBuffer *self)
{
  if (self->buffer == VK_NULL_HANDLE || self->array->len == 0)
    {
      g_printerr ("Invalid vertex buffer array.\n");
      if (self->buffer == VK_NULL_HANDLE)
        g_printerr ("Buffer is NULL_HANDLE\n");
      if (self->array->len == 0)
        g_printerr ("Array has len 0\n");
      return FALSE;
    }

  if (!gulkan_buffer_upload (self->buffer, self->array->data,
                             self->array->len * sizeof (float)))
    {
      g_printerr ("Could not map memory\n");
      return FALSE;
    }
  return TRUE;
}

gboolean
gulkan_vertex_buffer_is_initialized (GulkanVertexBuffer *self)
{
  return self->buffer != VK_NULL_HANDLE;
}

VkVertexInputBindingDescription *
gulkan_vertex_buffer_create_binding_desc (GulkanVertexBuffer *self)
{
  uint32_t binding_count = g_slist_length (self->attributes);
  VkVertexInputBindingDescription *desc
    = g_malloc (sizeof (VkVertexInputBindingDescription) * binding_count);

  for (guint i = 0; i < binding_count; i++)
    {
      GSList                *entry = g_slist_nth (self->attributes, i);
      GulkanVertexAttribute *attribute = entry->data;

      desc[i] = (VkVertexInputBindingDescription){
        .binding = i,
        .stride = (uint32_t) (attribute->stride * sizeof (float)),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
      };
    }
  return desc;
}

static VkFormat
_get_format_for_stride (size_t stride)
{
  switch (stride)
    {
      case 1:
        return VK_FORMAT_R32_SFLOAT;
      case 2:
        return VK_FORMAT_R32G32_SFLOAT;
      case 3:
        return VK_FORMAT_R32G32B32_SFLOAT;
      case 4:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
      default:
        g_warning ("Unspecified format for stride of %ld.", stride);
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
}

VkVertexInputAttributeDescription *
gulkan_vertex_buffer_create_attrib_desc (GulkanVertexBuffer *self)
{
  uint32_t binding_count = g_slist_length (self->attributes);
  VkVertexInputAttributeDescription *desc
    = g_malloc (sizeof (VkVertexInputAttributeDescription) * binding_count);

  for (guint i = 0; i < binding_count; i++)
    {
      GSList                *entry = g_slist_nth (self->attributes, i);
      GulkanVertexAttribute *attribute = entry->data;

      desc[i] = (VkVertexInputAttributeDescription){
        .location = i,
        .binding = i,
        .format = _get_format_for_stride (attribute->stride),
        .offset = 0,
      };
    }
  return desc;
}

uint32_t
gulkan_vertex_buffer_get_attrib_count (GulkanVertexBuffer *self)
{
  return g_slist_length (self->attributes);
}

VkPrimitiveTopology
gulkan_vertex_buffer_get_topology (GulkanVertexBuffer *self)
{
  return self->topology;
}
