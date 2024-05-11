/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "gulkan-device.h"
#include "gulkan-queue.h"

#include <gio/gio.h>

struct _GulkanDevice
{
  GObjectClass parent_class;

  VkDevice                   device;
  VkPhysicalDevice           physical_device;
  VkPhysicalDeviceProperties physical_props;

  VkPhysicalDeviceMemoryProperties memory_properties;

  GulkanQueue *graphics_queue;
  GulkanQueue *transfer_queue;

  PFN_vkGetMemoryFdKHR extVkGetMemoryFdKHR;
};

G_DEFINE_TYPE (GulkanDevice, gulkan_device, G_TYPE_OBJECT)

static void
gulkan_device_init (GulkanDevice *self)
{
  self->device = VK_NULL_HANDLE;
  self->physical_device = VK_NULL_HANDLE;
  self->transfer_queue = NULL;
  self->graphics_queue = NULL;
  self->extVkGetMemoryFdKHR = 0;
}

GulkanDevice *
gulkan_device_new (void)
{
  return (GulkanDevice *) g_object_new (GULKAN_TYPE_DEVICE, 0);
}

static void
_finalize (GObject *gobject)
{
  GulkanDevice *self = GULKAN_DEVICE (gobject);
  g_clear_object (&self->transfer_queue);
  g_clear_object (&self->graphics_queue);
  vkDestroyDevice (self->device, NULL);
  G_OBJECT_CLASS (gulkan_device_parent_class)->finalize (gobject);
}

static void
gulkan_device_class_init (GulkanDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = _finalize;
}

static gboolean
_find_physical_device (GulkanDevice    *self,
                       GulkanInstance  *instance,
                       VkPhysicalDevice requested_device)
{
  VkInstance vk_instance = gulkan_instance_get_handle (instance);
  uint32_t   num_devices = 0;
  VkResult   res = vkEnumeratePhysicalDevices (vk_instance, &num_devices, NULL);
  vk_check_error ("vkEnumeratePhysicalDevices", res, FALSE);

  if (num_devices == 0)
    {
      g_printerr ("No Vulkan devices found.\n");
      return FALSE;
    }

  VkPhysicalDevice *physical_devices = g_malloc (sizeof (VkPhysicalDevice)
                                                 * num_devices);
  res = vkEnumeratePhysicalDevices (vk_instance, &num_devices,
                                    physical_devices);
  vk_check_error ("vkEnumeratePhysicalDevices", res, FALSE);

  if (requested_device == VK_NULL_HANDLE)
    {
      /* No device requested. Using first one */
      self->physical_device = physical_devices[0];
    }
  else
    {
      /* Find requested device */
      self->physical_device = VK_NULL_HANDLE;
      for (uint32_t i = 0; i < num_devices; i++)
        if (physical_devices[i] == requested_device)
          {
            self->physical_device = requested_device;
            g_debug ("Using requested VkPhysicalDevice %p",
                     (void *) requested_device);
            break;
          }

      if (self->physical_device == VK_NULL_HANDLE)
        {
          g_printerr ("Failed to find requested VkPhysicalDevice, "
                      "falling back to the first one.\n");
          self->physical_device = physical_devices[0];
        }
    }

  g_free (physical_devices);

  return TRUE;
}

static gboolean
_get_physical_device_props (GulkanDevice *self)
{
  vkGetPhysicalDeviceMemoryProperties (self->physical_device,
                                       &self->memory_properties);

  vkGetPhysicalDeviceProperties (self->physical_device, &self->physical_props);

  return TRUE;
}

static gboolean
find_queue_index_for_flags (VkQueueFlagBits          flags,
                            VkQueueFlagBits          exclude_flags,
                            uint32_t                 num_queues,
                            VkQueueFamilyProperties *queue_family_props,
                            uint32_t                *out_index)
{
  uint32_t i = 0;
  for (i = 0; i < num_queues; i++)
    if (queue_family_props[i].queueFlags & flags
        && !(queue_family_props[i].queueFlags & exclude_flags))
      break;

  if (i >= num_queues)
    return FALSE;

  *out_index = i;
  return TRUE;
}

static gboolean
_find_queue_families (GulkanDevice *self,
                      uint32_t     *graphics_queue_index,
                      uint32_t     *transfer_queue_index)
{
  /* Find the first graphics queue */
  uint32_t num_queues = 0;
  vkGetPhysicalDeviceQueueFamilyProperties (self->physical_device, &num_queues,
                                            0);

  VkQueueFamilyProperties *queue_family_props
    = g_malloc (sizeof (VkQueueFamilyProperties) * num_queues);

  vkGetPhysicalDeviceQueueFamilyProperties (self->physical_device, &num_queues,
                                            queue_family_props);

  if (num_queues == 0)
    {
      g_printerr ("Failed to get queue properties.\n");
      return FALSE;
    }

  *graphics_queue_index = UINT32_MAX;
  if (!find_queue_index_for_flags (VK_QUEUE_GRAPHICS_BIT, (VkQueueFlagBits) 0,
                                   num_queues, queue_family_props,
                                   graphics_queue_index))
    {
      g_printerr ("No graphics queue found\n");
      return FALSE;
    }

  *transfer_queue_index = UINT32_MAX;
  if (find_queue_index_for_flags (VK_QUEUE_TRANSFER_BIT, VK_QUEUE_GRAPHICS_BIT,
                                  num_queues, queue_family_props,
                                  transfer_queue_index))
    {
      g_debug ("Got pure transfer queue");
    }
  else
    {
      g_debug ("No pure transfer queue found, trying all queues");
      if (find_queue_index_for_flags (VK_QUEUE_TRANSFER_BIT,
                                      (VkQueueFlagBits) 0, num_queues,
                                      queue_family_props, transfer_queue_index))
        {
          g_debug ("Got a transfer queue");
        }
      else
        {
          g_printerr ("No transfer queue found\n");
          return FALSE;
        }
    }

  g_free (queue_family_props);
  return TRUE;
}

static gboolean
_create_queues (GulkanDevice *self,
                uint32_t      graphics_queue_index,
                uint32_t      transfer_queue_index)
{
  self->graphics_queue = gulkan_queue_new (self, graphics_queue_index);
  self->transfer_queue = gulkan_queue_new (self, transfer_queue_index);
  return TRUE;
}

static gboolean
_get_device_extension_count (GulkanDevice *self, uint32_t *count)
{
  VkResult res = vkEnumerateDeviceExtensionProperties (self->physical_device,
                                                       NULL, count, NULL);
  vk_check_error ("vkEnumerateDeviceExtensionProperties", res, FALSE);
  return TRUE;
}

static gboolean
_init_device_extensions (GulkanDevice *self,
                         GSList       *required_extensions,
                         uint32_t      num_extensions,
                         char        **extension_names,
                         uint32_t     *out_num_enabled)
{
  uint32_t num_enabled = 0;

  /* Enable required device extensions */
  VkExtensionProperties *extension_props
    = g_malloc (sizeof (VkExtensionProperties) * num_extensions);

  memset (extension_props, 0, sizeof (VkExtensionProperties) * num_extensions);

  VkResult res = vkEnumerateDeviceExtensionProperties (self->physical_device,
                                                       NULL, &num_extensions,
                                                       extension_props);
  vk_check_error ("vkEnumerateDeviceExtensionProperties", res, FALSE);
  for (uint32_t i = 0; i < g_slist_length (required_extensions); i++)
    {
      gboolean found = FALSE;
      for (uint32_t j = 0; j < num_extensions; j++)
        {
          GSList *extension_name = g_slist_nth (required_extensions, i);
          if (strcmp ((gchar *) extension_name->data,
                      extension_props[j].extensionName)
              == 0)
            {
              found = TRUE;
              break;
            }
        }

      if (found)
        {
          GSList *extension_name = g_slist_nth (required_extensions, i);
          extension_names[num_enabled] = g_strdup ((char *)
                                                     extension_name->data);
          num_enabled++;
        }
    }

  *out_num_enabled = num_enabled;

  g_free (extension_props);

  return TRUE;
}

/**
 * gulkan_device_create:
 * @self: a #GulkanDevice
 * @instance: a #GulkanInstance
 * @device: a #VkPhysicalDevice
 * @extensions: (element-type utf8): A list of extensions to enable
 *
 * Returns: %TRUE on success
 */
gboolean
gulkan_device_create (GulkanDevice    *self,
                      GulkanInstance  *instance,
                      VkPhysicalDevice device,
                      GSList          *extensions)
{
  if (!_find_physical_device (self, instance, device))
    return FALSE;

  if (!_get_physical_device_props (self))
    return FALSE;

  uint32_t graphics_queue_index;
  uint32_t transfer_queue_index;
  if (!_find_queue_families (self, &graphics_queue_index,
                             &transfer_queue_index))
    return FALSE;

  if (!_create_queues (self, graphics_queue_index, transfer_queue_index))
    return FALSE;

  uint32_t num_extensions = 0;
  if (!_get_device_extension_count (self, &num_extensions))
    return FALSE;

  char   **extension_names = g_malloc (sizeof (char *) * num_extensions);
  uint32_t num_enabled = 0;

  if (num_extensions > 0)
    {
      if (!_init_device_extensions (self, extensions, num_extensions,
                                    extension_names, &num_enabled))
        return FALSE;
    }

  gboolean requested_multiview = FALSE;

  if (num_enabled > 0)
    {
      g_debug ("Requesting device extensions:");
      for (uint32_t i = 0; i < num_enabled; i++)
        {
          if (strcmp (extension_names[i], VK_KHR_MULTIVIEW_EXTENSION_NAME) == 0)
            {
              requested_multiview = TRUE;
            }
          g_debug ("%s", extension_names[i]);
        }
    }

  VkPhysicalDeviceFeatures physical_device_features;
  vkGetPhysicalDeviceFeatures (self->physical_device,
                               &physical_device_features);

  VkDeviceCreateInfo device_info =
    {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = 2,
      .pQueueCreateInfos =
          (VkDeviceQueueCreateInfo[]) {
            {
              .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
              .queueFamilyIndex = gulkan_queue_get_family_index(self->graphics_queue),
              .queueCount = 1,
              .pQueuePriorities = (const float[]) { 1.0f }
            },
            {
              .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
              .queueFamilyIndex = gulkan_queue_get_family_index (self->transfer_queue),
              .queueCount = 1,
              .pQueuePriorities = (const float[]) { 0.8f }
            },
          },
      .enabledExtensionCount = num_enabled,
      .ppEnabledExtensionNames = (const char* const*) extension_names,
      .pEnabledFeatures = &physical_device_features,
    };

  VkPhysicalDeviceMultiviewFeatures multiview_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
    .multiview = VK_TRUE,
    .multiviewGeometryShader = VK_FALSE,
    .multiviewTessellationShader = VK_FALSE,
  };

  if (requested_multiview)
    {
      device_info.pNext = (const void *) &multiview_features;
    }

  VkResult res = vkCreateDevice (self->physical_device, &device_info, NULL,
                                 &self->device);
  vk_check_error ("vkCreateDevice", res, FALSE);

  if (!gulkan_queue_initialize (self->graphics_queue))
    return FALSE;

  if (!gulkan_queue_initialize (self->transfer_queue))
    return FALSE;

  if (num_enabled > 0)
    {
      for (uint32_t i = 0; i < num_enabled; i++)
        g_free (extension_names[i]);
    }
  g_free (extension_names);

  return TRUE;
}

/**
 * gulkan_device_create_from_vk:
 * @self: a #GulkanDevice
 * @vk_physical_device: a #VkPhysicalDevice
 * @vk_device: a #VkDevice
 * @graphics_queue_index: The index of a graphics queue
 * @transfer_queue_index: The index of a transfer queue
 *
 * Returns: %TRUE on success
 */
gboolean
gulkan_device_create_from_vk (GulkanDevice    *self,
                              VkPhysicalDevice vk_physical_device,
                              VkDevice         vk_device,
                              uint32_t         graphics_queue_index,
                              uint32_t         transfer_queue_index)
{
  self->physical_device = vk_physical_device;
  self->device = vk_device;

  if (!_get_physical_device_props (self))
    return FALSE;

  if (!_create_queues (self, graphics_queue_index, transfer_queue_index))
    return FALSE;

  if (!gulkan_queue_initialize (self->graphics_queue))
    return FALSE;

  if (!gulkan_queue_initialize (self->transfer_queue))
    return FALSE;

  return TRUE;
}

gboolean
gulkan_device_memory_type_from_properties (GulkanDevice *self,
                                           uint32_t      memory_type_bits,
                                           VkMemoryPropertyFlags
                                                     memory_property_flags,
                                           uint32_t *type_index_out)
{
  for (uint32_t i = 0; i < self->memory_properties.memoryTypeCount; i++)
    {
      if ((memory_type_bits & 1) == 1)
        {
          if ((self->memory_properties.memoryTypes[i].propertyFlags
               & memory_property_flags)
              == memory_property_flags)
            {
              *type_index_out = i;
              return TRUE;
            }
        }
      memory_type_bits >>= 1;
    }

  /* Could not find matching memory type.*/
  return FALSE;
}

/**
 * gulkan_device_get_handle:
 * @self: a #GulkanDevice
 *
 * Returns: (transfer none): a #VkDevice
 */
VkDevice
gulkan_device_get_handle (GulkanDevice *self)
{
  return self->device;
}

/**
 * gulkan_device_get_physical_handle:
 * @self: a #GulkanDevice
 *
 * Returns: (transfer none): a #VkPhysicalDevice
 */
VkPhysicalDevice
gulkan_device_get_physical_handle (GulkanDevice *self)
{
  return self->physical_device;
}

/**
 * gulkan_device_get_graphics_queue:
 * @self: a #GulkanDevice
 *
 * Returns: (transfer none): the graphics queue
 */
GulkanQueue *
gulkan_device_get_graphics_queue (GulkanDevice *self)
{
  return self->graphics_queue;
}

/**
 * gulkan_device_get_transfer_queue:
 * @self: a #GulkanDevice
 *
 * Returns: (transfer none): the transfer queue
 */
GulkanQueue *
gulkan_device_get_transfer_queue (GulkanDevice *self)
{
  return self->transfer_queue;
}

gboolean
gulkan_device_get_memory_fd (GulkanDevice  *self,
                             VkDeviceMemory image_memory,
                             int           *fd)
{
  if (!self->extVkGetMemoryFdKHR)
    self->extVkGetMemoryFdKHR = (PFN_vkGetMemoryFdKHR)
      vkGetDeviceProcAddr (self->device, "vkGetMemoryFdKHR");

  if (!self->extVkGetMemoryFdKHR)
    {
      g_printerr ("Gulkan Device: Could not load vkGetMemoryFdKHR\n");
      return FALSE;
    }

  VkMemoryGetFdInfoKHR vkFDInfo = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
    .memory = image_memory,
    .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR,
  };

  if (self->extVkGetMemoryFdKHR (self->device, &vkFDInfo, fd) != VK_SUCCESS)
    {
      g_printerr ("Gulkan Device: Could not get file descriptor for memory!\n");
      g_object_unref (self);
      return FALSE;
    }
  return TRUE;
}

void
gulkan_device_wait_idle (GulkanDevice *self)
{
  if (self->device != VK_NULL_HANDLE)
    vkDeviceWaitIdle (self->device);
}

void
gulkan_device_print_memory_properties (GulkanDevice *self)
{
  VkPhysicalDeviceMemoryProperties *props = &self->memory_properties;

  g_print ("\n= VkPhysicalDeviceMemoryProperties =\n");
  for (uint32_t i = 0; i < props->memoryTypeCount; i++)
    {
      VkMemoryType *type = &props->memoryTypes[i];
      g_print ("\nVkMemoryType %d: heapIndex %d\n", i, type->heapIndex);

      if (type->propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        g_print ("+ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT\n");
      if (type->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        g_print ("+ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT\n");
      if (type->propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        g_print ("+ VK_MEMORY_PROPERTY_HOST_COHERENT_BIT\n");
      if (type->propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
        g_print ("+ VK_MEMORY_PROPERTY_HOST_CACHED_BIT\n");
      if (type->propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
        g_print ("+ VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT\n");
      if (type->propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT)
        g_print ("+ VK_MEMORY_PROPERTY_PROTECTED_BIT\n");
#ifdef VK_AMD_device_coherent_memory
      if (type->propertyFlags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD)
        g_print ("+ VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD\n");
      if (type->propertyFlags & VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD)
        g_print ("+ VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD\n");
#endif
    }

  for (uint32_t i = 0; i < props->memoryHeapCount; i++)
    {
      VkMemoryHeap *heap = &props->memoryHeaps[i];
      g_print ("\nVkMemoryHeap %d: size %ld MB\n", i, heap->size / 1024 / 1024);
      if (heap->flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
        g_print ("+ VK_MEMORY_HEAP_DEVICE_LOCAL_BIT\n");
      if (heap->flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT)
        g_print ("+ VK_MEMORY_HEAP_MULTI_INSTANCE_BIT\n");
    }

  g_print ("\n====================================\n");
}

void
gulkan_device_print_memory_budget (GulkanDevice *self)
{
#ifdef VK_EXT_memory_budget
  VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT,
  };

  VkPhysicalDeviceMemoryProperties2 props = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
    .pNext = &budget,
    .memoryProperties = self->memory_properties,
  };

  vkGetPhysicalDeviceMemoryProperties2 (self->physical_device, &props);

  for (uint32_t i = 0; i < self->memory_properties.memoryHeapCount; i++)
    g_print ("Heap %d: usage %.2f budget %.2f MB\n", i,
             (double) budget.heapUsage[i] / 1024.0 / 1024.0,
             (double) budget.heapBudget[i] / 1024.0 / 1024.0);
#else
  g_print ("VK_EXT_memory_budget not supported in the vulkan SDK gulkan was "
           "compiled with!\n");
#endif
}

/**
 * gulkan_device_get_heap_budget:
 * @self: a #GulkanDevice
 * @i: the the memory heap number
 *
 * Returns: (transfer none): a #VkDeviceSize
 */
VkDeviceSize
gulkan_device_get_heap_budget (GulkanDevice *self, uint32_t i)
{
#ifdef VK_EXT_memory_budget
  VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT,
  };

  VkPhysicalDeviceMemoryProperties2 props = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
    .pNext = &budget,
    .memoryProperties = self->memory_properties,
  };

  vkGetPhysicalDeviceMemoryProperties2 (self->physical_device, &props);

  return budget.heapBudget[i];
#else
  g_print ("VK_EXT_memory_budget not supported in the vulkan SDK gulkan was "
           "compiled with!\n");
  return 0;
#endif
}

/**
 * gulkan_device_get_physical_device_properties:
 * @self: a #GulkanDevice
 *
 * Returns: (transfer none): a #VkPhysicalDeviceProperties
 */
VkPhysicalDeviceProperties *
gulkan_device_get_physical_device_properties (GulkanDevice *self)
{
  return &self->physical_props;
}

static gboolean
_load_resource (const gchar *path, GBytes **res)
{
  GError *error = NULL;
  *res = g_resources_lookup_data (path, G_RESOURCE_LOOKUP_FLAGS_NONE, &error);

  if (error != NULL)
    {
      g_printerr ("Unable to read file: %s\n", error->message);
      g_error_free (error);
      return FALSE;
    }

  return TRUE;
}

/**
 * gulkan_device_create_shader_module:
 * @self: a #GulkanDevice
 * @resource_name: the name of the #GResource to use
 * @module: (out): the created #VkShaderModule
 *
 * Returns: %TRUE if the shader has been created
 */
gboolean
gulkan_device_create_shader_module (GulkanDevice   *self,
                                    const gchar    *resource_name,
                                    VkShaderModule *module)
{
  GBytes *bytes;
  if (!_load_resource (resource_name, &bytes))
    return FALSE;

  gsize           size = 0;
  const uint32_t *buffer = g_bytes_get_data (bytes, &size);

  VkShaderModuleCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = size,
    .pCode = buffer,
  };

  VkResult res = vkCreateShaderModule (self->device, &info, NULL, module);
  vk_check_error ("vkCreateShaderModule", res, FALSE);

  g_bytes_unref (bytes);

  return TRUE;
}
