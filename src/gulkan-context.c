/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "gulkan-context.h"

typedef struct _GulkanContextPrivate
{
  GObject object;

  GulkanInstance *instance;
  GulkanDevice   *device;
} GulkanContextPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GulkanContext, gulkan_context, G_TYPE_OBJECT)

static void
gulkan_context_finalize (GObject *gobject);

static void
gulkan_context_class_init (GulkanContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gulkan_context_finalize;
}

static void
gulkan_context_init (GulkanContext *self)
{
  GulkanContextPrivate *priv = gulkan_context_get_instance_private (self);
  priv->instance = gulkan_instance_new ();
  priv->device = gulkan_device_new ();
}

static gboolean
_init_vulkan_full (GulkanContext   *self,
                   GSList          *instance_extensions,
                   GSList          *device_extensions,
                   VkPhysicalDevice physical_device);

static GulkanContext *
_context_new (GSList          *instance_ext_list,
              GSList          *device_ext_list,
              VkPhysicalDevice physical_device)
{
  GulkanContext *self = (GulkanContext *) g_object_new (GULKAN_TYPE_CONTEXT, 0);
  if (!_init_vulkan_full (self, instance_ext_list, device_ext_list,
                          physical_device))
    {
      g_object_unref (self);
      return NULL;
    }
  return self;
}

GulkanContext *
gulkan_context_new (void)
{
  return _context_new (NULL, NULL, VK_NULL_HANDLE);
}

/**
 * gulkan_context_new_from_extensions:
 * @instance_ext_list: (element-type utf8): a list of instance extensions
 * @device_ext_list: (element-type utf8): a list of device extensions
 * @physical_device: a #VkPhysicalDevice. Pass VK_NULL_HANDLE to let Gulkan
 * choose one.
 * Returns: (transfer full): a new #GulkanContext
 */
GulkanContext *
gulkan_context_new_from_extensions (GSList          *instance_ext_list,
                                    GSList          *device_ext_list,
                                    VkPhysicalDevice physical_device)
{
  return _context_new (instance_ext_list, device_ext_list, physical_device);
}

/**
 * gulkan_context_new_from_instance:
 * @instance: (transfer full): a #GulkanInstance
 * @device_ext_list: (element-type utf8): a list of device extensions
 * @physical_device: a #VkPhysicalDevice. Pass VK_NULL_HANDLE to let Gulkan
 * choose one.
 * Returns: (transfer full): a new #GulkanContext
 */
GulkanContext *
gulkan_context_new_from_instance (GulkanInstance  *instance,
                                  GSList          *device_ext_list,
                                  VkPhysicalDevice physical_device)
{
  GulkanContext *self = (GulkanContext *) g_object_new (GULKAN_TYPE_CONTEXT, 0);

  GulkanContextPrivate *priv = gulkan_context_get_instance_private (self);
  priv->instance = instance;

  if (!gulkan_device_create (priv->device, priv->instance, physical_device,
                             device_ext_list))
    {
      g_printerr ("Failed to create device.\n");
      return NULL;
    }
  return self;
}

/**
 * gulkan_context_new_from_vk:
 * @vk_instance: An externally created VkInstance
 * @vk_physical_device: An externally created VkPhysicalDevice
 * @vk_device: An externally created VkDevice
 * @graphics_queue_index: Graphics queue index
 * @transfer_queue_index: Transfer queue index
 *
 * Returns: (transfer full): a new #GulkanContext
 */
GulkanContext *
gulkan_context_new_from_vk (VkInstance       vk_instance,
                            VkPhysicalDevice vk_physical_device,
                            VkDevice         vk_device,
                            uint32_t         graphics_queue_index,
                            uint32_t         transfer_queue_index)
{
  GulkanContext *self = (GulkanContext *) g_object_new (GULKAN_TYPE_CONTEXT, 0);
  GulkanContextPrivate *priv = gulkan_context_get_instance_private (self);

  if (!gulkan_instance_create_from_vk (priv->instance, vk_instance))
    {
      g_printerr ("Failed to create instance.\n");
      return FALSE;
    }

  if (!gulkan_device_create_from_vk (priv->device, vk_physical_device,
                                     vk_device, graphics_queue_index,
                                     transfer_queue_index))
    {
      g_printerr ("Failed to create device.\n");
      return FALSE;
    }

  return self;
}

static void
gulkan_context_finalize (GObject *gobject)
{
  GulkanContext *self = GULKAN_CONTEXT (gobject);

  GulkanContextPrivate *priv = gulkan_context_get_instance_private (self);
  g_object_unref (priv->device);
  g_object_unref (priv->instance);
}

static gboolean
_init_vulkan_full (GulkanContext   *self,
                   GSList          *instance_extensions,
                   GSList          *device_extensions,
                   VkPhysicalDevice physical_device)
{
  GulkanContextPrivate *priv = gulkan_context_get_instance_private (self);
  if (!gulkan_instance_create (priv->instance, instance_extensions))
    {
      g_printerr ("Failed to create instance.\n");
      return FALSE;
    }

  if (!gulkan_device_create (priv->device, priv->instance, physical_device,
                             device_extensions))
    {
      g_printerr ("Failed to create device.\n");
      return FALSE;
    }

  return TRUE;
}

/**
 * gulkan_context_get_physical_device_handle:
 * @self: a #GulkanContext
 *
 * Returns: (transfer none): a #VkPhysicalDevice
 */
VkPhysicalDevice
gulkan_context_get_physical_device_handle (GulkanContext *self)
{
  GulkanContextPrivate *priv = gulkan_context_get_instance_private (self);
  return gulkan_device_get_physical_handle (priv->device);
}

/**
 * gulkan_context_get_device_handle:
 * @self: a #GulkanContext
 *
 * Returns: (transfer none): a #VkDevice
 */
VkDevice
gulkan_context_get_device_handle (GulkanContext *self)
{
  GulkanContextPrivate *priv = gulkan_context_get_instance_private (self);
  return gulkan_device_get_handle (priv->device);
}

/**
 * gulkan_context_get_device:
 * @self: a #GulkanContext
 *
 * Returns: (transfer none): the #GulkanDevice
 */
GulkanDevice *
gulkan_context_get_device (GulkanContext *self)
{
  GulkanContextPrivate *priv = gulkan_context_get_instance_private (self);
  return priv->device;
}

/**
 * gulkan_context_get_instance_handle:
 * @self: a #GulkanContext
 *
 * Returns: (transfer none): a #VkInstance
 */
VkInstance
gulkan_context_get_instance_handle (GulkanContext *self)
{
  GulkanContextPrivate *priv = gulkan_context_get_instance_private (self);
  return gulkan_instance_get_handle (priv->instance);
}

/**
 * gulkan_context_get_instance:
 * @self: a #GulkanContext
 *
 * Returns: (transfer none): the #GulkanInstance
 */
GulkanInstance *
gulkan_context_get_instance (GulkanContext *self)
{
  GulkanContextPrivate *priv = gulkan_context_get_instance_private (self);
  return priv->instance;
}

/**
 * gulkan_context_get_external_memory_instance_extensions:
 *
 * Returns: (transfer full) (element-type utf8): the list of external memory
 * instance extensions
 */
GSList *
gulkan_context_get_external_memory_instance_extensions (void)
{
  const gchar *instance_extensions[] = {
    VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
    VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    VK_KHR_SURFACE_EXTENSION_NAME,
  };

  GSList *instance_ext_list = NULL;
  for (uint64_t i = 0; i < G_N_ELEMENTS (instance_extensions); i++)
    {
      char *instance_ext = g_strdup (instance_extensions[i]);
      instance_ext_list = g_slist_append (instance_ext_list, instance_ext);
    }

  return instance_ext_list;
}

/**
 * gulkan_context_get_external_memory_device_extensions:
 *
 * Returns: (transfer full) (element-type utf8): the list of external memory
 * device extensions
 */
GSList *
gulkan_context_get_external_memory_device_extensions (void)
{
  const gchar *device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME,
    VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME,
    VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
    VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
    VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
    VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
  };

  GSList *device_ext_list = NULL;
  for (uint64_t i = 0; i < G_N_ELEMENTS (device_extensions); i++)
    {
      char *device_ext = g_strdup (device_extensions[i]);
      device_ext_list = g_slist_append (device_ext_list, device_ext);
    }

  return device_ext_list;
}
