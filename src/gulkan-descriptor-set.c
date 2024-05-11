/*
 * gulkan
 * Copyright 2021 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "gulkan-descriptor-set.h"

#include "gulkan-descriptor-pool.h"

struct _GulkanDescriptorSet
{
  GObject parent;

  GulkanContext *context;

  VkDescriptorSet  handle;
  VkDescriptorPool pool;

  gpointer *descriptors;
  uint32_t  size;
};

G_DEFINE_TYPE (GulkanDescriptorSet, gulkan_descriptor_set, G_TYPE_OBJECT)

static void
gulkan_descriptor_set_init (GulkanDescriptorSet *self)
{
  self->pool = NULL;
  self->handle = VK_NULL_HANDLE;
  self->descriptors = NULL;
  self->context = NULL;
  self->size = 0;
}

GulkanDescriptorSet *
gulkan_descriptor_set_new (GulkanContext   *context,
                           VkDescriptorSet  handle,
                           VkDescriptorPool pool,
                           uint32_t         size)
{
  g_assert (size > 0);

  GulkanDescriptorSet *self = (GulkanDescriptorSet *)
    g_object_new (GULKAN_TYPE_DESCRIPTOR_SET, 0);
  self->context = g_object_ref (context);
  self->handle = handle;
  self->pool = pool;
  self->size = size;
  self->descriptors = g_malloc (sizeof (gpointer) * size);
  for (size_t i = 0; i < self->size; i++)
    {
      self->descriptors[i] = NULL;
    }
  return self;
}

static void
_finalize (GObject *gobject)
{
  GulkanDescriptorSet *self = GULKAN_DESCRIPTOR_SET (gobject);
  for (size_t i = 0; i < self->size; i++)
    {
      g_clear_object (&self->descriptors[i]);
    }
  g_free (self->descriptors);

  VkDevice device = gulkan_context_get_device_handle (self->context);
  vkFreeDescriptorSets (device, self->pool, 1, &self->handle);

  g_clear_object (&self->context);
  G_OBJECT_CLASS (gulkan_descriptor_set_parent_class)->finalize (gobject);
}

static void
gulkan_descriptor_set_class_init (GulkanDescriptorSetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = _finalize;
}

void
gulkan_descriptor_set_bind (GulkanDescriptorSet *self,
                            VkPipelineLayout     layout,
                            VkCommandBuffer      cmd_buffer)
{
  vkCmdBindDescriptorSets (cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout,
                           0, 1, &self->handle, 0, NULL);
}

void
gulkan_descriptor_set_update_buffer_at (GulkanDescriptorSet *self,
                                        guint                index,
                                        guint                binding,
                                        GulkanUniformBuffer *buffer)
{
  g_assert (index < self->size);

  GulkanUniformBuffer *old = self->descriptors[index];

  if (buffer == old)
    {
      g_warning ("Updating already set uniform buffer at index %d", index);
      return;
    }

  if (old && GULKAN_IS_UNIFORM_BUFFER (old))
    {
      g_clear_object (&self->descriptors[index]);
    }

  self->descriptors[index] = g_object_ref (buffer);

  VkDescriptorBufferInfo *info
    = gulkan_uniform_buffer_get_descriptor_info (buffer);
  VkWriteDescriptorSet write_sets = {
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .dstSet = self->handle,
    .dstBinding = binding,
    .dstArrayElement = 0,
    .descriptorCount = 1,
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .pBufferInfo = info,
  };

  VkDevice device = gulkan_context_get_device_handle (self->context);
  vkUpdateDescriptorSets (device, 1, &write_sets, 0, NULL);

  g_free (info);
}

void
gulkan_descriptor_set_update_buffer (GulkanDescriptorSet *self,
                                     guint                index,
                                     GulkanUniformBuffer *buffer)
{
  gulkan_descriptor_set_update_buffer_at (self, index, index, buffer);
}

void
gulkan_descriptor_set_update_texture_at (GulkanDescriptorSet *self,
                                         guint                index,
                                         guint                binding,
                                         GulkanTexture       *texture)
{
  g_assert (index < self->size);

  GulkanTexture *old = self->descriptors[index];

  if (texture == old)
    {
      g_warning ("Updating already set texture at index %d", index);
      return;
    }

  if (old && GULKAN_IS_TEXTURE (old))
    {
      g_clear_object (&self->descriptors[index]);
    }

  self->descriptors[index] = g_object_ref (texture);

  VkDescriptorImageInfo *info = gulkan_texture_get_descriptor_info (texture);

  VkWriteDescriptorSet write_sets = {
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .dstSet = self->handle,
    .dstBinding = binding,
    .dstArrayElement = 0,
    .descriptorCount = 1,
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .pImageInfo = info,
  };

  VkDevice device = gulkan_context_get_device_handle (self->context);
  vkUpdateDescriptorSets (device, 1, &write_sets, 0, NULL);

  g_free (info);
}

void
gulkan_descriptor_set_update_texture (GulkanDescriptorSet *self,
                                      guint                index,
                                      GulkanTexture       *texture)
{
  gulkan_descriptor_set_update_texture_at (self, index, index, texture);
}

void
gulkan_descriptor_set_update_view_sampler (GulkanDescriptorSet *self,
                                           guint                index,
                                           VkImageView          view,
                                           VkSampler            sampler)
{
  VkWriteDescriptorSet *write_sets = (VkWriteDescriptorSet[]){
    {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = self->handle,
      .dstBinding = index,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImageInfo = &(VkDescriptorImageInfo){
        .sampler = sampler,
        .imageView = view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
     },
     .pTexelBufferView = NULL,
    },
  };

  VkDevice device = gulkan_context_get_device_handle (self->context);
  vkUpdateDescriptorSets (device, 1, write_sets, 0, NULL);
}
