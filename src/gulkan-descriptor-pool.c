/*
 * gulkan
 * Copyright 2020 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "gulkan-descriptor-pool.h"
#include "gulkan-context.h"

struct _GulkanDescriptorPool
{
  GObject parent;

  GulkanContext *context;

  VkDescriptorPool      handle;
  VkDescriptorSetLayout layout;
  VkPipelineLayout      pipeline_layout;

  uint32_t set_size;
};

G_DEFINE_TYPE (GulkanDescriptorPool, gulkan_descriptor_pool, G_TYPE_OBJECT)

static void
gulkan_descriptor_pool_init (GulkanDescriptorPool *self)
{
  self->context = NULL;
  self->handle = VK_NULL_HANDLE;
  self->layout = VK_NULL_HANDLE;
  self->pipeline_layout = VK_NULL_HANDLE;
}

static void
_finalize (GObject *gobject)
{
  GulkanDescriptorPool *self = GULKAN_DESCRIPTOR_POOL (gobject);
  VkDevice device = gulkan_context_get_device_handle (self->context);

  if (self->handle != VK_NULL_HANDLE)
    vkDestroyDescriptorPool (device, self->handle, NULL);
  if (self->layout != VK_NULL_HANDLE)
    vkDestroyDescriptorSetLayout (device, self->layout, NULL);
  if (self->pipeline_layout != VK_NULL_HANDLE)
    vkDestroyPipelineLayout (device, self->pipeline_layout, NULL);

  g_clear_object (&self->context);
}

static void
gulkan_descriptor_pool_class_init (GulkanDescriptorPoolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = _finalize;
}

static gboolean
_init_layouts (GulkanDescriptorPool               *self,
               const VkDescriptorSetLayoutBinding *bindings)
{
  VkDescriptorSetLayoutCreateInfo descriptor_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = self->set_size,
    .pBindings = bindings,
  };

  VkDevice device = gulkan_context_get_device_handle (self->context);

  VkResult res;
  res = vkCreateDescriptorSetLayout (device, &descriptor_info, NULL,
                                     &self->layout);
  vk_check_error ("vkCreateDescriptorSetLayout", res, FALSE);

  VkPipelineLayoutCreateInfo pipeline_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1,
    .pSetLayouts = &self->layout,
  };

  res = vkCreatePipelineLayout (device, &pipeline_info, NULL,
                                &self->pipeline_layout);
  vk_check_error ("vkCreatePipelineLayout", res, FALSE);
  return TRUE;
}

static gboolean
_init (GulkanDescriptorPool       *self,
       const VkDescriptorPoolSize *pool_sizes,
       uint32_t                    max_sets)
{
  VkDescriptorPoolCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
    .maxSets = max_sets,
    .poolSizeCount = self->set_size,
    .pPoolSizes = pool_sizes,
  };

  VkDevice device = gulkan_context_get_device_handle (self->context);
  VkResult res = vkCreateDescriptorPool (device, &info, NULL, &self->handle);

  vk_check_error ("vkCreateDescriptorPool", res, FALSE);

  return TRUE;
}

/**
 * gulkan_descriptor_pool_new:
 * @context: a #GulkanContext handle
 * @bindings: (array length=binding_count) (element-type
 * VkDescriptorSetLayoutBinding): an array of #VkDescriptorSetLayoutBinding
 * @set_size: the number of #VkDescriptorSetLayoutBinding
 * @max_sets: the maximum number of descriptor sets that can be allocated
 *
 * Returns: (transfer full) (nullable): a new #GulkanDescriptorPool
 */
GulkanDescriptorPool *
gulkan_descriptor_pool_new (GulkanContext                      *context,
                            const VkDescriptorSetLayoutBinding *bindings,
                            uint32_t                            set_size,
                            uint32_t                            max_sets)
{
  g_assert (set_size > 0);

  GulkanDescriptorPool *self = (GulkanDescriptorPool *)
    g_object_new (GULKAN_TYPE_DESCRIPTOR_POOL, 0);
  self->context = g_object_ref (context);
  self->set_size = set_size;

  VkDescriptorPoolSize *pool_sizes = g_malloc (sizeof (VkDescriptorPoolSize)
                                               * self->set_size);

  for (uint32_t i = 0; i < self->set_size; i++)
    {
      VkDescriptorType type = bindings[i].descriptorType;
      pool_sizes[i] = (VkDescriptorPoolSize){
        .type = type,
        .descriptorCount = max_sets,
      };
    }

  if (!_init (self, pool_sizes, max_sets))
    {
      g_object_unref (self);
      return NULL;
    }

  g_free (pool_sizes);

  if (!_init_layouts (self, bindings))
    {
      g_object_unref (self);
      return NULL;
    }

  return self;
}

/**
 * gulkan_descriptor_pool_allocate_sets:
 * @self: a #VkDevice handle
 * @count: the number of #VkDescriptorSet
 * @sets: (array length=count) (element-type VkDescriptorSet): an array of
 * #VkDescriptorSet
 *
 * Returns: %TRUE if the sets have been allocated
 */
GulkanDescriptorSet *
gulkan_descriptor_pool_create_set (GulkanDescriptorPool *self)
{
  VkDescriptorSetAllocateInfo alloc_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = self->handle,
    .descriptorSetCount = 1,
    .pSetLayouts = &self->layout,
  };

  VkDescriptorSet handle;

  VkDevice device = gulkan_context_get_device_handle (self->context);
  VkResult res = vkAllocateDescriptorSets (device, &alloc_info, &handle);
  vk_check_error ("vkAllocateDescriptorSets", res, NULL);

  return gulkan_descriptor_set_new (self->context, handle, self->handle,
                                    self->set_size);
}

/**
 * gulkan_descriptor_pool_get_pipeline_layout:
 * @self: a #GulkanDescriptorPool
 *
 * Returns: (transfer none): a #VkPipelineLayout
 */
VkPipelineLayout
gulkan_descriptor_pool_get_pipeline_layout (GulkanDescriptorPool *self)
{
  return self->pipeline_layout;
}
