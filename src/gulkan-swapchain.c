/*
 * gulkan
 * Copyright 2020 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "gulkan-swapchain.h"

struct _GulkanSwapchain
{
  GObject parent;

  GulkanContext *context;

  VkSwapchainKHR handle;
  VkSurfaceKHR   surface;
  VkExtent2D     extent;

  VkSurfaceFormatKHR surface_format;
  VkPresentModeKHR   present_mode;

  uint32_t size;
};

G_DEFINE_TYPE (GulkanSwapchain, gulkan_swapchain, G_TYPE_OBJECT)

static void
_finalize (GObject *gobject)
{
  GulkanSwapchain *self = GULKAN_SWAPCHAIN (gobject);
  VkDevice         device = gulkan_context_get_device_handle (self->context);

  vkDestroySwapchainKHR (device, self->handle, NULL);

  VkInstance instance = gulkan_context_get_instance_handle (self->context);
  if (self->surface != VK_NULL_HANDLE)
    vkDestroySurfaceKHR (instance, self->surface, NULL);

  G_OBJECT_CLASS (gulkan_swapchain_parent_class)->finalize (gobject);
}

static void
gulkan_swapchain_class_init (GulkanSwapchainClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = _finalize;
}

static void
gulkan_swapchain_init (GulkanSwapchain *self)
{
  self->handle = VK_NULL_HANDLE;
  self->surface = VK_NULL_HANDLE;
  self->size = 0;
}

static gboolean
_find_surface_format (VkPhysicalDevice    device,
                      VkSurfaceKHR        surface,
                      VkFormat            request_format,
                      VkColorSpaceKHR     request_colorspace,
                      VkSurfaceFormatKHR *format)
{
  uint32_t            num_formats;
  VkSurfaceFormatKHR *formats = NULL;
  vkGetPhysicalDeviceSurfaceFormatsKHR (device, surface, &num_formats, NULL);

  if (num_formats != 0)
    {
      formats = g_malloc (sizeof (VkSurfaceFormatKHR) * num_formats);
      vkGetPhysicalDeviceSurfaceFormatsKHR (device, surface, &num_formats,
                                            formats);
    }
  else
    {
      g_printerr ("Could enumerate surface formats.\n");
      return FALSE;
    }

  for (uint32_t i = 0; i < num_formats; i++)
    if (formats[i].format == request_format
        && formats[i].colorSpace == request_colorspace)
      {
        format->format = formats[i].format;
        format->colorSpace = formats[i].colorSpace;
        g_free (formats);
        return TRUE;
      }

  g_free (formats);
  g_printerr ("Requested format not supported.\n");
  return FALSE;
}

static gboolean
_find_surface_present_mode (VkPhysicalDevice  device,
                            VkSurfaceKHR      surface,
                            VkPresentModeKHR  request_present_mode,
                            VkPresentModeKHR *present_mode)
{
  uint32_t          num_present_modes;
  VkPresentModeKHR *present_modes;
  vkGetPhysicalDeviceSurfacePresentModesKHR (device, surface,
                                             &num_present_modes, NULL);

  if (num_present_modes != 0)
    {
      present_modes = g_malloc (sizeof (VkPresentModeKHR) * num_present_modes);
      vkGetPhysicalDeviceSurfacePresentModesKHR (device, surface,
                                                 &num_present_modes,
                                                 present_modes);
    }
  else
    {
      g_printerr ("Could not enumerate present modes.\n");
      return FALSE;
    }

  for (uint32_t i = 0; i < num_present_modes; i++)
    if (present_modes[i] == request_present_mode)
      {
        *present_mode = present_modes[i];
        g_free (present_modes);
        return TRUE;
      }

  g_free (present_modes);
  g_printerr ("Requested present mode not supported.\n");
  return FALSE;
}

gboolean
gulkan_swapchain_reset_surface (GulkanSwapchain *self,
                                VkSurfaceKHR     surface,
                                VkExtent2D       extent)
{
  if (self->surface != VK_NULL_HANDLE)
    {
      VkInstance instance = gulkan_context_get_instance_handle (self->context);
      vkDestroySurfaceKHR (instance, self->surface, NULL);
    }
  self->surface = surface;
  self->extent = extent;

  GulkanDevice *gulkan_device = gulkan_context_get_device (self->context);

  GulkanQueue *gulkan_queue = gulkan_device_get_graphics_queue (gulkan_device);

  if (!gulkan_queue_supports_surface (gulkan_queue, surface))
    {
      g_printerr ("Device does not support surface.\n");
      return FALSE;
    }

  VkPhysicalDevice physical_device
    = gulkan_device_get_physical_handle (gulkan_device);

  VkSurfaceCapabilitiesKHR surface_caps;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR (physical_device, surface,
                                             &surface_caps);

  g_assert (surface_caps.supportedCompositeAlpha
            & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR);

  VkDevice device = gulkan_device_get_handle (gulkan_device);

  VkSwapchainCreateInfoKHR info = {
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface = self->surface,
    .minImageCount = surface_caps.minImageCount,
    .imageFormat = self->surface_format.format,
    .imageColorSpace = self->surface_format.colorSpace,
    .imageExtent = extent,
    .imageArrayLayers = 1,
    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 1,
    .pQueueFamilyIndices = (uint32_t[]){0},
    .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode = self->present_mode,
  };

  VkResult res = vkCreateSwapchainKHR (device, &info, NULL, &self->handle);
  vk_check_error ("vkCreateSwapchainKHR", res, FALSE);

  vkGetSwapchainImagesKHR (device, self->handle, &self->size, NULL);
  g_assert (self->size > 0);

  return TRUE;
}

static gboolean
_init (GulkanSwapchain *self,
       VkSurfaceKHR     surface,
       VkExtent2D       extent,
       VkPresentModeKHR request_present_mode,
       VkFormat         request_format,
       VkColorSpaceKHR  request_colorspace)
{
  GulkanDevice *gulkan_device = gulkan_context_get_device (self->context);

  GulkanQueue *queue = gulkan_device_get_graphics_queue (gulkan_device);
  if (!gulkan_queue_supports_surface (queue, surface))
    {
      g_printerr ("Device does not support surface.\n");
      return FALSE;
    }

  VkPhysicalDevice physical_device
    = gulkan_device_get_physical_handle (gulkan_device);

  if (!_find_surface_format (physical_device, surface, request_format,
                             request_colorspace, &self->surface_format))
    return FALSE;

  if (!_find_surface_present_mode (physical_device, surface,
                                   request_present_mode, &self->present_mode))
    return FALSE;

  if (!gulkan_swapchain_reset_surface (self, surface, extent))
    return FALSE;

  return TRUE;
}

/**
 * gulkan_swapchain_get_images:
 * @self: a #GulkanSwapchain
 * @swap_chain_images: (array zero-terminated=1) (nullable): an array of
 * #VkImage
 *
 */
void
gulkan_swapchain_get_images (GulkanSwapchain *self, VkImage *swap_chain_images)
{
  VkDevice device = gulkan_context_get_device_handle (self->context);

  vkGetSwapchainImagesKHR (device, self->handle, &self->size,
                           swap_chain_images);
}

GulkanSwapchain *
gulkan_swapchain_new (GulkanContext   *context,
                      VkSurfaceKHR     surface,
                      VkExtent2D       extent,
                      VkPresentModeKHR present_mode,
                      VkFormat         format,
                      VkColorSpaceKHR  colorspace)
{
  GulkanSwapchain *self = (GulkanSwapchain *)
    g_object_new (GULKAN_TYPE_SWAPCHAIN, 0);

  self->context = context;

  if (!_init (self, surface, extent, present_mode, format, colorspace))
    {
      g_object_unref (self);
      return NULL;
    }

  return self;
}

uint32_t
gulkan_swapchain_get_size (GulkanSwapchain *self)
{
  return self->size;
}

/**
 * gulkan_swapchain_get_format:
 * @self: a #GulkanSwapchain
 *
 * Returns: (transfer none): a #VkFormat
 */
VkFormat
gulkan_swapchain_get_format (GulkanSwapchain *self)
{
  return self->surface_format.format;
}

gboolean
gulkan_swapchain_acquire (GulkanSwapchain *self,
                          VkSemaphore      signal_semaphore,
                          uint32_t        *index)
{
  GulkanDevice *gulkan_device = gulkan_context_get_device (self->context);
  VkDevice      device = gulkan_device_get_handle (gulkan_device);

  VkResult res = vkAcquireNextImageKHR (device, self->handle, 1e9,
                                        signal_semaphore, VK_NULL_HANDLE,
                                        index);

  if (res != VK_SUBOPTIMAL_KHR)
    {
      vk_check_error ("vkAcquireNextImageKHR", res, FALSE);
    }

  return TRUE;
}

gboolean
gulkan_swapchain_present (GulkanSwapchain *self,
                          VkSemaphore     *wait_semaphore,
                          uint32_t         index)
{
  GulkanDevice *gulkan_device = gulkan_context_get_device (self->context);
  GulkanQueue  *gulkan_queue = gulkan_device_get_graphics_queue (gulkan_device);
  VkQueue       queue = gulkan_queue_get_handle (gulkan_queue);

  VkPresentInfoKHR info = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = wait_semaphore,
    .swapchainCount = 1,
    .pSwapchains = (VkSwapchainKHR[]){self->handle},
    .pImageIndices = (uint32_t[]){index},
    .pResults = NULL,
  };

  VkResult res = vkQueuePresentKHR (queue, &info);

  if (res != VK_SUBOPTIMAL_KHR)
    {
      vk_check_error ("vkQueuePresentKHR", res, FALSE);
    }

  vkQueueWaitIdle (queue);

  return TRUE;
}

/**
 * gulkan_swapchain_get_extent:
 * @self: a #GulkanSwapchain
 *
 * Returns: (transfer none): a #VkExtent2D
 */
VkExtent2D
gulkan_swapchain_get_extent (GulkanSwapchain *self)
{
  return self->extent;
}
