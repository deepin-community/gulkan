/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

/*
 * Use syscalls to check whether two fds point to the same memory.
 * Else use drmPrimeFDToHandle from libdrm.
 */
#define USE_SYSCALLS

#ifdef USE_SYSCALLS
#define _GNU_SOURCE
#include <sys/syscall.h>
#include <unistd.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <xf86drm.h>
#endif

#include "gulkan-texture.h"

#include "gulkan-buffer.h"
#include "gulkan-cmd-buffer.h"
#include <vulkan/vulkan.h>

#include <drm_fourcc.h>

struct _GulkanTexture
{
  GObjectClass parent;

  GulkanContext *context;

  VkImage        image;
  VkDeviceMemory image_memory;
  VkImageView    image_view;

  guint mip_levels;

  VkExtent2D extent;

  VkFormat format;

  VkSampler sampler;
};

G_DEFINE_TYPE (GulkanTexture, gulkan_texture, G_TYPE_OBJECT)

typedef struct
{
  uint32_t           levels;
  uint8_t           *buffer;
  VkDeviceSize       size;
  VkBufferImageCopy *buffer_image_copies;
} GulkanMipMap;

struct DrmFormatTableEntry
{
  uint32_t drm_format;
  VkFormat vk_format;
};

// clang-format off
static struct DrmFormatTableEntry DRMVKFormatTable[] = {
  { .drm_format = DRM_FORMAT_ABGR8888, .vk_format = VK_FORMAT_R8G8B8A8_UNORM, },
  { .drm_format = DRM_FORMAT_ARGB8888, .vk_format = VK_FORMAT_B8G8R8A8_UNORM, },

  { .drm_format = DRM_FORMAT_BGRA8888, .vk_format = VK_FORMAT_A8B8G8R8_UNORM_PACK32, }, // TODO
  { .drm_format = DRM_FORMAT_RGBA8888, .vk_format = VK_FORMAT_A8B8G8R8_UNORM_PACK32, }, // TODO

  { .drm_format = DRM_FORMAT_XBGR8888, .vk_format = VK_FORMAT_R8G8B8A8_UNORM, },
  { .drm_format = DRM_FORMAT_XRGB8888, .vk_format = VK_FORMAT_B8G8R8A8_UNORM, },

  { .drm_format = DRM_FORMAT_RGBX8888, .vk_format = VK_FORMAT_A8B8G8R8_UNORM_PACK32, }, // TODO
  { .drm_format = DRM_FORMAT_BGRX8888, .vk_format = VK_FORMAT_A8B8G8R8_UNORM_PACK32, }, // TODO

  // { .DRMFormat = DRM_FORMAT_NV12, .vkFormat = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, },

  { .drm_format = DRM_FORMAT_INVALID, .vk_format = VK_FORMAT_UNDEFINED, },
};
// clang-format on

static inline VkFormat
drm_format_to_vulkan (uint32_t drm_format)
{
  for (int i = 0; DRMVKFormatTable[i].vk_format != VK_FORMAT_UNDEFINED; i++)
    {
      if (DRMVKFormatTable[i].drm_format == drm_format)
        {
          g_debug ("drm format number %d", i);
          return DRMVKFormatTable[i].vk_format;
        }
    }

  return VK_FORMAT_UNDEFINED;
}

static GulkanMipMap
_generate_mipmaps (GdkPixbuf *pixbuf);

static void
gulkan_texture_init (GulkanTexture *self)
{
  self->image = VK_NULL_HANDLE;
  self->image_memory = VK_NULL_HANDLE;
  self->image_view = VK_NULL_HANDLE;
  self->format = VK_FORMAT_UNDEFINED;
  self->mip_levels = 1;
  self->sampler = VK_NULL_HANDLE;
}

static void
_finalize (GObject *gobject)
{
  GulkanTexture *self = GULKAN_TEXTURE (gobject);
  VkDevice       device = gulkan_context_get_device_handle (self->context);

  vkDestroyImageView (device, self->image_view, NULL);
  vkDestroyImage (device, self->image, NULL);
  vkFreeMemory (device, self->image_memory, NULL);

  if (self->sampler != VK_NULL_HANDLE)
    vkDestroySampler (device, self->sampler, NULL);

  g_object_unref (self->context);

  G_OBJECT_CLASS (gulkan_texture_parent_class)->finalize (gobject);
}

static void
gulkan_texture_class_init (GulkanTextureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = _finalize;
}

static gboolean
_upload_pixels (GulkanTexture           *self,
                guchar                  *pixels,
                gsize                    size,
                const VkBufferImageCopy *regions,
                VkImageLayout            layout)
{
  GulkanDevice    *device = gulkan_context_get_device (self->context);
  GulkanQueue     *queue = gulkan_device_get_transfer_queue (device);
  GulkanCmdBuffer *cmd_buffer = gulkan_queue_request_cmd_buffer (queue);
  GMutex          *mutex = gulkan_queue_get_pool_mutex (queue);

  g_mutex_lock (mutex);
  gboolean ret = gulkan_cmd_buffer_begin_one_time (cmd_buffer);
  g_mutex_unlock (mutex);

  if (!ret)
    return FALSE;

  GulkanBuffer *staging_buffer
    = gulkan_buffer_new (device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

  if (!staging_buffer)
    return FALSE;

  if (!gulkan_buffer_upload (staging_buffer, pixels, size))
    return FALSE;

  gulkan_texture_record_transfer (self,
                                  gulkan_cmd_buffer_get_handle (cmd_buffer),
                                  VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  g_mutex_lock (mutex);
  vkCmdCopyBufferToImage (gulkan_cmd_buffer_get_handle (cmd_buffer),
                          gulkan_buffer_get_handle (staging_buffer),
                          self->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          self->mip_levels, regions);
  g_mutex_unlock (mutex);

  gulkan_texture_record_transfer (self,
                                  gulkan_cmd_buffer_get_handle (cmd_buffer),
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout);

  if (!gulkan_queue_end_submit (queue, cmd_buffer))
    return FALSE;

  g_object_unref (staging_buffer);
  gulkan_queue_free_cmd_buffer (queue, cmd_buffer);

  return TRUE;
}

GulkanTexture *
gulkan_texture_new_from_pixbuf (GulkanContext *context,
                                GdkPixbuf     *pixbuf,
                                VkFormat       format,
                                VkImageLayout  layout,
                                gboolean       create_mipmaps)
{
  VkExtent2D extent = {
    .width = (uint32_t) gdk_pixbuf_get_width (pixbuf),
    .height = (uint32_t) gdk_pixbuf_get_height (pixbuf),
  };

  GulkanTexture *self;

  if (create_mipmaps)
    {
      GulkanMipMap mipmap = _generate_mipmaps (pixbuf);

      self = gulkan_texture_new_mip_levels (context, extent, mipmap.levels,
                                            format);

      if (!_upload_pixels (self, mipmap.buffer, mipmap.size,
                           mipmap.buffer_image_copies, layout))
        {
          g_printerr ("ERROR: Could not upload pixels.\n");
          g_object_unref (self);
          self = NULL;
        }

      g_free (mipmap.buffer);
      g_free (mipmap.buffer_image_copies);
    }
  else
    {
      gsize   size = gdk_pixbuf_get_byte_length (pixbuf);
      guchar *pixels = gdk_pixbuf_get_pixels (pixbuf);
      self = gulkan_texture_new (context, extent, format);

      if (!gulkan_texture_upload_pixels (self, pixels, size, layout))
        {
          g_printerr ("ERROR: Could not upload pixels.\n");
          g_object_unref (self);
          self = NULL;
        }
    }

  return self;
}

GulkanTexture *
gulkan_texture_new_from_resource (GulkanContext *context,
                                  const char    *resource_path,
                                  VkImageLayout  layout,
                                  gboolean       create_mipmaps)
{
  GError    *error = NULL;
  GdkPixbuf *pixbuf_rgb = gdk_pixbuf_new_from_resource (resource_path, &error);
  if (error != NULL)
    {
      g_printerr ("Unable to read resource '%s': %s\n", resource_path,
                  error->message);
      g_error_free (error);
      return NULL;
    }

  GdkPixbuf *pixbuf = gdk_pixbuf_add_alpha (pixbuf_rgb, FALSE, 0, 0, 0);
  g_object_unref (pixbuf_rgb);
  if (pixbuf == NULL)
    {
      g_printerr ("Could initialize pixbuf from '%s'.\n", resource_path);
      return NULL;
    }

  GulkanTexture *texture
    = gulkan_texture_new_from_pixbuf (context, pixbuf, VK_FORMAT_R8G8B8A8_SRGB,
                                      layout, create_mipmaps);

  g_object_unref (pixbuf);

  return texture;
}

GulkanTexture *
gulkan_texture_new_from_cairo_surface (GulkanContext   *context,
                                       cairo_surface_t *surface,
                                       VkFormat         format,
                                       VkImageLayout    layout)
{
  VkExtent2D extent = {
    .width = (uint32_t) cairo_image_surface_get_width (surface),
    .height = (uint32_t) cairo_image_surface_get_height (surface),
  };

  guint stride = (guint) cairo_image_surface_get_stride (surface);

  guint size = stride * extent.height;

  guchar *pixels = cairo_image_surface_get_data (surface);

  GulkanTexture *self = gulkan_texture_new (context, extent, format);

  if (!gulkan_texture_upload_pixels (self, pixels, size, layout))
    {
      g_printerr ("ERROR: Could not upload pixels.\n");
      g_object_unref (self);
      self = NULL;
    }

  return self;
}

GulkanTexture *
gulkan_texture_new (GulkanContext *context, VkExtent2D extent, VkFormat format)
{
  return gulkan_texture_new_mip_levels (context, extent, 1, format);
}

GulkanTexture *
gulkan_texture_new_mip_levels (GulkanContext *context,
                               VkExtent2D     extent,
                               guint          mip_levels,
                               VkFormat       format)
{
  GulkanTexture *self = (GulkanTexture *) g_object_new (GULKAN_TYPE_TEXTURE, 0);

  self->extent = extent;
  self->context = g_object_ref (context);
  self->format = format;
  self->mip_levels = mip_levels;

  VkDevice vk_device = gulkan_context_get_device_handle (context);

  /* TODO: Check with vkGetPhysicalDeviceFormatProperties */
  VkImageTiling tiling;
  switch (format)
    {
      case VK_FORMAT_R8G8B8_SRGB:
      case VK_FORMAT_R8G8B8_UNORM:
        tiling = VK_IMAGE_TILING_LINEAR;
        break;
      case VK_FORMAT_R8G8B8A8_SRGB:
      case VK_FORMAT_R8G8B8A8_UNORM:
      case VK_FORMAT_B8G8R8A8_UNORM:
      case VK_FORMAT_B8G8R8A8_SRGB:
        tiling = VK_IMAGE_TILING_OPTIMAL;
        break;
      default:
        g_printerr ("Warning: No tiling for format %s (%d) specified.\n",
                    vk_format_string (format), format);
        tiling = VK_IMAGE_TILING_OPTIMAL;
    }

  VkImageCreateInfo image_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .extent.width = extent.width,
    .extent.height = extent.height,
    .extent.depth = 1,
    .mipLevels = mip_levels,
    .arrayLayers = 1,
    .format = format,
    .tiling = tiling,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
             | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    .flags = 0,
  };
  VkResult res;
  res = vkCreateImage (vk_device, &image_info, NULL, &self->image);
  vk_check_error ("vkCreateImage", res, NULL);

  VkMemoryRequirements memory_requirements;
  vkGetImageMemoryRequirements (vk_device, self->image, &memory_requirements);

  VkMemoryAllocateInfo memory_info = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = memory_requirements.size,
  };
  gulkan_device_memory_type_from_properties (gulkan_context_get_device (context),
                                             memory_requirements.memoryTypeBits,
                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                             &memory_info.memoryTypeIndex);
  res = vkAllocateMemory (vk_device, &memory_info, NULL, &self->image_memory);
  vk_check_error ("vkAllocateMemory", res, NULL);
  res = vkBindImageMemory (vk_device, self->image, self->image_memory, 0);
  vk_check_error ("vkBindImageMemory", res, NULL);

  VkImageViewCreateInfo image_view_info =
  {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .image = self->image,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = image_info.format,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = image_info.mipLevels,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
  };
  res = vkCreateImageView (vk_device, &image_view_info, NULL,
                           &self->image_view);
  vk_check_error ("vkCreateImageView", res, NULL);

  return self;
}

static GulkanMipMap
_generate_mipmaps (GdkPixbuf *pixbuf)
{
  int width = gdk_pixbuf_get_width (pixbuf);
  int height = gdk_pixbuf_get_height (pixbuf);

  GulkanMipMap mipmap = {
    .levels = 1,
    .size = sizeof (uint8_t) * (guint) width * (guint) height * 4 * 2,
  };

  mipmap.buffer = g_malloc (mipmap.size);

  /* Test how many levels we will be generating */
  int test_width = width;
  int test_height = height;
  while (test_width > 1 && test_height > 1)
    {
      test_width /= 2;
      test_height /= 2;
      mipmap.levels++;
    }

  mipmap.buffer_image_copies = g_malloc (sizeof (VkBufferImageCopy)
                                         * mipmap.levels);

  /* Original size */
  VkBufferImageCopy buffer_image_copy = {
    .imageSubresource = {
      .baseArrayLayer = 0,
      .layerCount = 1,
      .mipLevel = 0,
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    },
    .imageExtent = {
      .width = (guint) width,
      .height = (guint) height,
      .depth = 1
    },
  };

  memcpy (&mipmap.buffer_image_copies[0], &buffer_image_copy,
          sizeof (VkBufferImageCopy));

  uint8_t *current = mipmap.buffer;
  gsize    original_size = gdk_pixbuf_get_byte_length (pixbuf);
  memcpy (current, gdk_pixbuf_get_pixels (pixbuf), original_size);
  current += original_size;

  /* MIP levels */
  uint32_t   level = 1;
  int        mip_width = width;
  int        mip_height = height;
  GdkPixbuf *last_pixbuf = pixbuf;
  while (mip_width > 1 && mip_height > 1)
    {
      mip_width /= 2;
      mip_height /= 2;

      GdkPixbuf *level_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
                                                MAX (mip_width, 1),
                                                MAX (mip_height, 1));

      gdk_pixbuf_scale (last_pixbuf, level_pixbuf, 0, 0,
                        gdk_pixbuf_get_width (level_pixbuf),
                        gdk_pixbuf_get_height (level_pixbuf), 0.0, 0.0, 0.5,
                        0.5, GDK_INTERP_BILINEAR);

      if (last_pixbuf != pixbuf)
        g_object_unref (last_pixbuf);
      last_pixbuf = level_pixbuf;

      /* Copy pixels */
      memcpy (current, gdk_pixbuf_get_pixels (level_pixbuf),
              gdk_pixbuf_get_byte_length (level_pixbuf));

      /* Copy VkBufferImageCopy */
      buffer_image_copy.bufferOffset = (VkDeviceSize) (current - mipmap.buffer);
      buffer_image_copy.imageSubresource.mipLevel = level;
      buffer_image_copy.imageExtent.width = (guint)
        gdk_pixbuf_get_width (level_pixbuf);
      buffer_image_copy.imageExtent.height = (guint)
        gdk_pixbuf_get_height (level_pixbuf);
      memcpy (&mipmap.buffer_image_copies[level], &buffer_image_copy,
              sizeof (VkBufferImageCopy));

      level++;
      current += gdk_pixbuf_get_byte_length (level_pixbuf);
    }

  g_object_unref (last_pixbuf);

  return mipmap;
}

gboolean
gulkan_texture_upload_pixels (GulkanTexture *self,
                              guchar        *pixels,
                              gsize          size,
                              VkImageLayout  layout)
{
  if (self->mip_levels != 1)
    {
      g_warning ("Trying to upload one mip level to multi level texture.\n");
      return FALSE;
    }

  VkBufferImageCopy buffer_image_copy = {
    .imageSubresource = {
      .baseArrayLayer = 0,
      .layerCount = 1,
      .mipLevel = 0,
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    },
    .imageExtent = {
      .width = self->extent.width,
      .height = self->extent.height,
      .depth = 1,
    },
  };

  return _upload_pixels (self, pixels, size, &buffer_image_copy, layout);
}

gboolean
gulkan_texture_upload_pixels_region (GulkanTexture *self,
                                     guchar        *region_pixels,
                                     gsize          region_size,
                                     VkImageLayout  layout,
                                     VkOffset2D     offset,
                                     VkExtent2D     extent)
{
  if (self->mip_levels != 1)
    {
      g_warning ("Trying to upload one mip level to multi level texture.\n");
      return FALSE;
    }

  VkBufferImageCopy buffer_image_copy = {
    .bufferOffset = 0,
    .bufferRowLength = 0,
    .bufferImageHeight = 0,

    .imageSubresource = {
      .baseArrayLayer = 0,
      .layerCount = 1,
      .mipLevel = 0,
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    },
    .imageOffset = {
      .x = offset.x,
      .y = offset.y,
      .z = 0,
    },
    .imageExtent = {
      .width = extent.width,
      .height = extent.height,
      .depth = 1,
    },
  };

  return _upload_pixels (self, region_pixels, region_size, &buffer_image_copy,
                         layout);
}

gboolean
gulkan_texture_upload_pixbuf (GulkanTexture *self,
                              GdkPixbuf     *pixbuf,
                              VkImageLayout  layout)
{
  guchar *pixels = gdk_pixbuf_get_pixels (pixbuf);
  gsize   size = gdk_pixbuf_get_byte_length (pixbuf);

  return gulkan_texture_upload_pixels (self, pixels, size, layout);
}

gboolean
gulkan_texture_upload_cairo_surface (GulkanTexture   *self,
                                     cairo_surface_t *surface,
                                     VkImageLayout    layout)
{
  guchar *pixels = cairo_image_surface_get_data (surface);
  gsize   size = (gsize) cairo_image_surface_get_stride (surface)
               * (gsize) cairo_image_surface_get_height (surface);

  return gulkan_texture_upload_pixels (self, pixels, size, layout);
}

GulkanTexture *
gulkan_texture_new_from_dmabuf (GulkanContext *context,
                                int            fd,
                                VkExtent2D     extent,
                                VkFormat       format)
{
  GulkanTexture *self = (GulkanTexture *) g_object_new (GULKAN_TYPE_TEXTURE, 0);
  VkDevice       vk_device = gulkan_context_get_device_handle (context);

  self->extent = extent;
  self->context = g_object_ref (context);
  self->format = format;

  VkExternalMemoryImageCreateInfoKHR external_memory_image_create_info = {
    .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR,
    .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR
                   | VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
  };

  VkImageCreateInfo image_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .pNext = &external_memory_image_create_info,
    .imageType = VK_IMAGE_TYPE_2D,
    .extent = {
      .width = extent.width,
      .height = extent.height,
      .depth = 1,
    },
    .mipLevels = 1,
    .arrayLayers = 1,
    .format = format,
    .tiling = VK_IMAGE_TILING_LINEAR,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    /* DMA buffer only allowed to import as UNDEFINED */
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VkResult res;
  res = vkCreateImage (vk_device, &image_info, NULL, &self->image);
  vk_check_error ("vkCreateImage", res, NULL);

  VkMemoryDedicatedAllocateInfoKHR dedicated_memory_info = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
    .pNext = NULL,
    .image = self->image,
    .buffer = VK_NULL_HANDLE,
  };

  VkImportMemoryFdInfoKHR import_memory_info = {
    .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
    .pNext = &dedicated_memory_info,
    .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    .fd = fd,
  };

  VkMemoryRequirements memory_requirements;
  vkGetImageMemoryRequirements (vk_device, self->image, &memory_requirements);

  VkMemoryAllocateInfo memory_info = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .pNext = &import_memory_info,
    .allocationSize = memory_requirements.size,
  };

  gulkan_device_memory_type_from_properties (
    gulkan_context_get_device (context), memory_requirements.memoryTypeBits,
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memory_info.memoryTypeIndex);

  res = vkAllocateMemory (vk_device, &memory_info, NULL, &self->image_memory);
  vk_check_error ("vkAllocateMemory", res, NULL);

  res = vkBindImageMemory (vk_device, self->image, self->image_memory, 0);
  vk_check_error ("vkBindImageMemory", res, NULL);

  VkImageViewCreateInfo image_view_info =
  {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .flags = 0,
    .image = self->image,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = image_info.format,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
  };
  res = vkCreateImageView (vk_device, &image_view_info, NULL,
                           &self->image_view);
  vk_check_error ("vkCreateImageView", res, NULL);

  return self;
}

static gboolean
_import_fd_into_memory (GulkanTexture        *self,
                        int                   fd,
                        VkDeviceMemory       *memory,
                        VkMemoryRequirements *out_memory_requirements)
{
  VkResult res;
  VkDevice vk_device = gulkan_context_get_device_handle (self->context);

  static PFN_vkGetMemoryFdPropertiesKHR GetMemoryFdPropertiesKHR = NULL;
  if (GetMemoryFdPropertiesKHR == NULL)
    GetMemoryFdPropertiesKHR = (PFN_vkGetMemoryFdPropertiesKHR)
      vkGetDeviceProcAddr (vk_device, "vkGetMemoryFdPropertiesKHR");
  if (GetMemoryFdPropertiesKHR == NULL)
    {
      g_printerr ("vkGetMemoryFdPropertiesKHR not available");
      return FALSE;
    }

  VkMemoryFdPropertiesKHR fd_props = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR,
  };
  res
    = GetMemoryFdPropertiesKHR (vk_device,
                                VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
                                fd, &fd_props);
  vk_check_error ("vkGetMemoryFdPropertiesKHR", res, FALSE);

  VkImageMemoryRequirementsInfo2 req_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
    .pNext = NULL,
    .image = self->image,
  };
  VkMemoryDedicatedRequirements ded_req = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS,
  };
  VkMemoryRequirements2 memory_requirements = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
    .pNext = &ded_req,
  };
  vkGetImageMemoryRequirements2 (vk_device, &req_info, &memory_requirements);

  g_debug ("fd %d size %lu alignment %lu bits %d", fd,
           memory_requirements.memoryRequirements.size,
           memory_requirements.memoryRequirements.alignment,
           memory_requirements.memoryRequirements.memoryTypeBits);
  g_debug ("dedicated memory alloc preferred %d required %d",
           ded_req.prefersDedicatedAllocation,
           ded_req.requiresDedicatedAllocation);
  gboolean use_ded_mem = (gboolean) ded_req.prefersDedicatedAllocation
                         | (gboolean) ded_req.requiresDedicatedAllocation;

  g_debug ("%susing dedicated memory allocation", use_ded_mem ? "" : "NOT ");

  *out_memory_requirements = memory_requirements.memoryRequirements;

  uint32_t memory_type_index = 0;
  gulkan_device_memory_type_from_properties (
    gulkan_context_get_device (self->context),
    memory_requirements.memoryRequirements.memoryTypeBits,
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memory_type_index);

  VkMemoryDedicatedAllocateInfo ded_alloc_info = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
    .pNext = NULL,
    .image = self->image,
  };
  VkImportMemoryFdInfoKHR import_fd_info = {
    .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
    .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    .fd = fd,
    .pNext = use_ded_mem ? &ded_alloc_info : NULL,
  };
  VkMemoryAllocateInfo alloc_info = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .pNext = &import_fd_info,
    .allocationSize = memory_requirements.memoryRequirements.size,
    .memoryTypeIndex = memory_type_index,
  };
  res = vkAllocateMemory (vk_device, &alloc_info, NULL, memory);
  vk_check_error ("vkAllocateMemory", res, FALSE);

  return TRUE;
}

#ifdef USE_SYSCALLS
// Copied from <linux/kcmp.h>
#define KCMP_FILE 0
static gboolean
is_fd_same_memory (int fd1, int fd2)
{
  if (fd1 == fd2)
    return TRUE;

  pid_t pid = getpid ();

  // kcmp returns -1 for failures, 0 for equal, >0 for different
  if (syscall (SYS_kcmp, pid, pid, KCMP_FILE, fd1, fd2) > 0)
    return FALSE;
  else
    return TRUE;
}
#else
static gboolean
is_fd_same_memory (int fd1, int fd2)
{
  if (fd1 == fd2)
    return TRUE;

  const gchar *dev = g_getenv ("GULKAN_GBM_DEVICE");
  if (!dev)
    dev = "/dev/dri/renderD128";

  // TODO: support using the device of the caller
  int32_t device_fd = open (dev, O_RDWR);
  if (device_fd <= 0)
    {
      g_printerr ("Failed to open render node\n");
      return 0;
    }

  int ret;

  uint32_t fd1_handle = 0;
  ret = drmPrimeFDToHandle (device_fd, fd1, &fd1_handle);
  if (ret)
    {
      g_printerr ("drmPrimeFDToHandle failed: %s (%d)", strerror (errno), ret);
      return FALSE;
    }

  uint32_t fd2_handle = 0;
  ret = drmPrimeFDToHandle (device_fd, fd2, &fd2_handle);
  if (ret)
    {
      g_printerr ("drmPrimeFDToHandle failed: %s (%d)", strerror (errno), ret);
      return FALSE;
    }

  close (device_fd);

  g_debug ("fd %d -> prime handle %d, fd %d -> prime handle %d", fd1,
           fd1_handle, fd2, fd2_handle);
  return fd1_handle == fd2_handle;
}
#endif

GulkanTexture *
gulkan_texture_new_from_dmabuf_attribs (GulkanContext                 *context,
                                        struct GulkanDmabufAttributes *attribs)
{
  // TODO: gracefully handle the no drm modifier case (fallback?)

  if (attribs->n_planes > 2)
    {
      g_printerr ("dmabuf with more than 2 planes not supported\n");
      return NULL;
    }

  if (attribs->modifier == DRM_FORMAT_MOD_INVALID)
    {
      g_printerr ("drm modifier format is DRM_FORMAT_MOD_INVALID\n");
      return NULL;
    }

  if (attribs->n_planes > 1)
    {
      if (!is_fd_same_memory (attribs->fd[0], attribs->fd[1]))
        {
          g_printerr ("gulkan does not support importing distinct memory "
                      "planes");
          return NULL;
        }
    }

  GulkanTexture *self = (GulkanTexture *) g_object_new (GULKAN_TYPE_TEXTURE, 0);
  VkDevice       vk_device = gulkan_context_get_device_handle (context);
  VkPhysicalDevice vk_physical_device
    = gulkan_context_get_physical_device_handle (context);

  VkFormat vk_format = drm_format_to_vulkan (attribs->format);

  VkSharingMode sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
  VkImageTiling tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;

  VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT
                            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  // VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
  // VK_IMAGE_USAGE_STORAGE_BIT

  g_debug ("DRM format %d -> Vk format %d", attribs->format, vk_format);
  g_debug ("modifier: %lu", attribs->modifier);

  /*
   * TODO: we have multiple planes for drm modifier compression etc, but we do
   * not handle images for VK_IMAGE_CREATE_DISJOINT_BIT, which are ycbcr images
   * separated into multiple planes
   */
  VkImageCreateFlags image_flags = 0;

  self->extent = (VkExtent2D){
    .width = (uint32_t) attribs->width,
    .height = (uint32_t) attribs->height,
  };
  self->context = g_object_ref (context);
  self->format = vk_format;

  VkDrmFormatModifierPropertiesEXT supported_modifier_props = {0};

  // check that the drm modifier we got is supported by the vulkan driver
  {
    VkDrmFormatModifierPropertiesListEXT modifier_prop_list = {
      .sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
    };
    VkFormatProperties2 format_props = {
      .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
      .pNext = &modifier_prop_list,
    };
    vkGetPhysicalDeviceFormatProperties2 (vk_physical_device, vk_format,
                                          &format_props);

    modifier_prop_list.pDrmFormatModifierProperties
      = g_malloc (sizeof (VkDrmFormatModifierPropertiesEXT)
                  * modifier_prop_list.drmFormatModifierCount);
    vkGetPhysicalDeviceFormatProperties2 (vk_physical_device, vk_format,
                                          &format_props);

    g_debug ("%d Supported modifiers:",
             modifier_prop_list.drmFormatModifierCount);
    for (uint32_t i = 0; i < modifier_prop_list.drmFormatModifierCount; i++)
      {
        VkDrmFormatModifierPropertiesEXT *m = &modifier_prop_list
                                                 .pDrmFormatModifierProperties
                                                   [i];
        g_debug ("modifier %lu: planes %d tiling features %d,",
                 m->drmFormatModifier, m->drmFormatModifierPlaneCount,
                 m->drmFormatModifierTilingFeatures);

        if (m->drmFormatModifier == attribs->modifier)
          {
            supported_modifier_props = modifier_prop_list
                                         .pDrmFormatModifierProperties[i];
          }
      }

    g_free (modifier_prop_list.pDrmFormatModifierProperties);
    modifier_prop_list.pDrmFormatModifierProperties = NULL;

    VkPhysicalDeviceImageDrmFormatModifierInfoEXT modifier_format_info = {
      .sType
      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT,
      .drmFormatModifier = attribs->modifier,
      .sharingMode = sharing_mode,
    };
    VkPhysicalDeviceExternalImageFormatInfo external_image_format_info = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
      .pNext = &modifier_format_info,
      .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    VkPhysicalDeviceImageFormatInfo2 image_format_info = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
      .pNext = &external_image_format_info,
      .format = vk_format,
      .type = VK_IMAGE_TYPE_2D,
      .tiling = tiling,
      .usage = usage,
      .flags = image_flags,
    };

    VkExternalImageFormatProperties external_image_props = {
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
    };
    VkImageFormatProperties2 image_props = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
      .pNext = &external_image_props,
    };

    VkResult res
      = vkGetPhysicalDeviceImageFormatProperties2 (vk_physical_device,
                                                   &image_format_info,
                                                   &image_props);
    vk_check_error ("vkGetPhysicalDeviceImageFormatProperties2", res, NULL);

    g_debug ("external memory features %d",
             external_image_props.externalMemoryProperties
               .externalMemoryFeatures);

    if (!(external_image_props.externalMemoryProperties.externalMemoryFeatures
          & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT))
      {
        g_printerr ("external memory is not importable\n");
        return NULL;
      }
  }

  if (supported_modifier_props.drmFormatModifier != attribs->modifier)
    {
      g_printerr ("modifier %lu not supported", attribs->modifier);
      return NULL;
    }

  if (supported_modifier_props.drmFormatModifierTilingFeatures
      & VK_FORMAT_FEATURE_DISJOINT_BIT)
    {
      g_printerr ("disjointed image format not supported by gulkan");
      return NULL;
    }

  static PFN_vkGetImageDrmFormatModifierPropertiesEXT
    GetImageDrmFormatModifierPropertiesEXT
    = NULL;
  if (GetImageDrmFormatModifierPropertiesEXT == NULL)
    GetImageDrmFormatModifierPropertiesEXT
      = (PFN_vkGetImageDrmFormatModifierPropertiesEXT)
        vkGetDeviceProcAddr (vk_device,
                             "vkGetImageDrmFormatModifierPropertiesEXT");

  if (GetImageDrmFormatModifierPropertiesEXT == NULL)
    {
      g_printerr ("vkGetImageDrmFormatModifierPropertiesEXT not available");
      return NULL;
    }

  VkSubresourceLayout modifier_plane_layouts[GULKAN_DMABUF_MAX_PLANES] = {0};
  for (int i = 0; i < attribs->n_planes; ++i)
    {
      modifier_plane_layouts[i].offset = attribs->offset[i];
      modifier_plane_layouts[i].rowPitch = attribs->stride[i];
      modifier_plane_layouts[i].size = 0;
      g_debug ("Plane %d: offset %d, pitch %d, fd %d", i, attribs->offset[i],
               attribs->stride[i], attribs->fd[i]);
    }
  VkImageDrmFormatModifierExplicitCreateInfoEXT modifier_info = {
    .sType
    = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT,
    .pNext = NULL,
    .drmFormatModifier = attribs->modifier,
    .drmFormatModifierPlaneCount = (uint32_t) attribs->n_planes,
    .pPlaneLayouts = modifier_plane_layouts,
  };
  VkExternalMemoryImageCreateInfo external_image_create_info = {
    .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
    .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    .pNext = &modifier_info,
  };
  VkImageCreateInfo image_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .flags = image_flags,
    .pNext = &external_image_create_info,
    .imageType = VK_IMAGE_TYPE_2D,
    .extent = {
      .width = self->extent.width,
      .height = self->extent.height,
      .depth = 1,
    },
    .mipLevels = 1,
    .arrayLayers = 1,
    .format = vk_format,
    .tiling = tiling,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .usage = usage,
    .sharingMode = sharing_mode,
    /* DMA buffer only allowed to import as UNDEFINED */
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VkResult res = vkCreateImage (vk_device, &image_info, NULL, &self->image);
  vk_check_error ("vkCreateImage", res, NULL);

  static PFN_vkGetMemoryFdPropertiesKHR GetMemoryFdPropertiesKHR = NULL;
  if (GetMemoryFdPropertiesKHR == NULL)
    GetMemoryFdPropertiesKHR = (PFN_vkGetMemoryFdPropertiesKHR)
      vkGetDeviceProcAddr (vk_device, "vkGetMemoryFdPropertiesKHR");
  if (GetMemoryFdPropertiesKHR == NULL)
    {
      g_printerr ("vkGetMemoryFdPropertiesKHR not available");
      return NULL;
    }

  VkMemoryRequirements memory_req;

  int fd = dup (attribs->fd[0]);
  g_debug ("dup fd %d -> %d", attribs->fd[0], fd);
  if (!_import_fd_into_memory (self, fd, &self->image_memory, &memory_req))
    {
      g_printerr ("Failed to import fd %d into plane 0\n", attribs->fd[0]);
      close (fd);
      return NULL;
    }

#if 0
  /* when tiling is VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT whe should be
   * able to use VkBindImagePlaneMemoryInfo even when our image is not disjoint.
   * TODO: this causes a validation error
   */
  VkBindImagePlaneMemoryInfo bind_image_plane0_info = {
    .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO,
    .pNext = NULL,
    .planeAspect = VK_IMAGE_ASPECT_PLANE_0_BIT,
  };
  VkBindImagePlaneMemoryInfo bind_image_plane1_info = {
    .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO,
    .pNext = NULL,
    .planeAspect = VK_IMAGE_ASPECT_PLANE_1_BIT,
  };

  VkBindImageMemoryInfo bind_image_memory_infos[GULKAN_DMABUF_MAX_PLANES] = {
    {
      .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
      .pNext = &bind_image_plane0_info,
      .image = self->image,
      .memory = self->image_memory,
      .memoryOffset = attribs->offset[0],
    },
    {
      .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
      .pNext = &bind_image_plane1_info,
      .image = self->image,
      .memory = self->image_memory,
      .memoryOffset = attribs->offset[1],
    }
  };

  // TODO: the second bind with attribs->offset[1] causes a GPU hang on amd
  res = vkBindImageMemory2 (vk_device, attribs->n_planes, bind_image_memory_infos);
  vk_check_error ("vkBindImageMemory2", res, NULL);
#else
  // gamescope uses vkBindImageMemory instead of vkBindImageMemory2
  res = vkBindImageMemory (vk_device, self->image, self->image_memory, 0);
  vk_check_error ("vkBindImageMemory", res, NULL);
#endif

  VkImageViewCreateInfo image_view_info =  {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .flags = 0,
    .image = self->image,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = image_info.format,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
  };
  res = vkCreateImageView (vk_device, &image_view_info, NULL,
                           &self->image_view);
  vk_check_error ("vkCreateImageView", res, NULL);

  return self;
}

/**
 * gulkan_texture_new_export_fd:
 * @context: a #GulkanContext
 * @extent: Extent in pixels
 * @format: VkFormat of the texture
 * @layout: VkImageLayout of the texture
 * @size: (out): Return value of allocated size
 * @fd: (out): Return value for allocated fd
 *
 * Allocates a #GulkanTexture and exports it via external memory to an fd and
 * provides the size of the external memory.
 *
 * based on code from
 * https://github.com/lostgoat/ogl-samples/blob/master/tests/gl-450-culling.cpp
 * https://gitlab.com/beVR_nz/VulkanIPC_Demo/
 *
 * Returns: the initialized #GulkanTexture
 */
GulkanTexture *
gulkan_texture_new_export_fd (GulkanContext *context,
                              VkExtent2D     extent,
                              VkFormat       format,
                              VkImageLayout  layout,
                              gsize         *size,
                              int           *fd)
{
  GulkanTexture *self = (GulkanTexture *) g_object_new (GULKAN_TYPE_TEXTURE, 0);
  VkDevice       vk_device = gulkan_context_get_device_handle (context);

  self->extent = extent;
  self->context = g_object_ref (context);
  self->format = format;

  /* we can also export the memory of the image without using this struct but
   * the spec makes it sound like we should use it */
  VkExternalMemoryImageCreateInfo external_memory_image_create_info = {
    .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
    .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT
                   | VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
  };

  VkImageCreateInfo image_info =
  {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .pNext = &external_memory_image_create_info,
    .imageType = VK_IMAGE_TYPE_2D,
    .extent = {
      .width = extent.width,
      .height = extent.height,
      .depth = 1,
    },
    .mipLevels = 1,
    .arrayLayers = 1,
    .format = format,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .usage = VK_IMAGE_USAGE_SAMPLED_BIT |
             VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
             VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VkResult res;
  res = vkCreateImage (vk_device, &image_info, NULL, &self->image);
  vk_check_error ("vkCreateImage", res, NULL);

  VkMemoryDedicatedAllocateInfoKHR dedicated_memory_info = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
    .pNext = NULL,
    .image = self->image,
    .buffer = VK_NULL_HANDLE,
  };

  VkMemoryRequirements memory_reqs;
  vkGetImageMemoryRequirements (vk_device, self->image, &memory_reqs);

  *size = memory_reqs.size;

  VkMemoryAllocateInfo memory_info = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .pNext = &dedicated_memory_info,
    .allocationSize = memory_reqs.size,
    /* filled in later */
    .memoryTypeIndex = 0,
  };

  VkMemoryPropertyFlags full_memory_property_flags
    = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

  GulkanDevice *device = gulkan_context_get_device (context);

  gboolean full_flags_available
    = gulkan_device_memory_type_from_properties (device,
                                                 memory_reqs.memoryTypeBits,
                                                 full_memory_property_flags,
                                                 &memory_info.memoryTypeIndex);

  if (!full_flags_available)
    {
      VkMemoryPropertyFlags fallback_memory_property_flags
        = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

      if (!gulkan_device_memory_type_from_properties (
            device, memory_reqs.memoryTypeBits, fallback_memory_property_flags,
            &memory_info.memoryTypeIndex))
        {
          g_printerr ("VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT"
                      " memory flags not available.\n");
          return NULL;
        }
    }

  res = vkAllocateMemory (vk_device, &memory_info, NULL, &self->image_memory);
  vk_check_error ("vkAllocateMemory", res, NULL);

  res = vkBindImageMemory (vk_device, self->image, self->image_memory, 0);
  vk_check_error ("vkBindImageMemory", res, NULL);

  VkImageViewCreateInfo image_view_info =
  {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .flags = 0,
    .image = self->image,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = image_info.format,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
  };
  res = vkCreateImageView (vk_device, &image_view_info, NULL,
                           &self->image_view);
  vk_check_error ("vkCreateImageView", res, NULL);

  if (!gulkan_device_get_memory_fd (device, self->image_memory, fd))
    {
      g_printerr ("Could not get file descriptor for memory!\n");
      g_object_unref (self);
      return NULL;
    }

  GulkanQueue     *queue = gulkan_device_get_transfer_queue (device);
  GulkanCmdBuffer *cmd_buffer = gulkan_queue_request_cmd_buffer (queue);
  GMutex          *mutex = gulkan_queue_get_pool_mutex (queue);

  g_mutex_lock (mutex);
  gboolean ret = gulkan_cmd_buffer_begin_one_time (cmd_buffer);
  g_mutex_unlock (mutex);
  if (!ret)
    return FALSE;

  gulkan_texture_record_transfer (self,
                                  gulkan_cmd_buffer_get_handle (cmd_buffer),
                                  VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  gulkan_texture_record_transfer (self,
                                  gulkan_cmd_buffer_get_handle (cmd_buffer),
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout);

  if (!gulkan_queue_end_submit (queue, cmd_buffer))
    return FALSE;

  gulkan_queue_free_cmd_buffer (queue, cmd_buffer);

  return self;
}

static VkAccessFlags
_get_access_flags (VkImageLayout layout)
{
  switch (layout)
    {
      case VK_IMAGE_LAYOUT_UNDEFINED:
        return 0;
      case VK_IMAGE_LAYOUT_PREINITIALIZED:
        return VK_ACCESS_HOST_WRITE_BIT;
      case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return VK_ACCESS_TRANSFER_READ_BIT;
      case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return VK_ACCESS_TRANSFER_WRITE_BIT;
      case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return VK_ACCESS_SHADER_READ_BIT;
      default:
        g_warning ("Unhandled access mask case for layout %d.\n", layout);
    }
  return 0;
}

void
gulkan_texture_record_transfer (GulkanTexture  *self,
                                VkCommandBuffer cmd_buffer,
                                VkImageLayout   src_layout,
                                VkImageLayout   dst_layout)
{
  gulkan_texture_record_transfer_full (self, cmd_buffer,
                                       _get_access_flags (src_layout),
                                       _get_access_flags (dst_layout),
                                       src_layout, dst_layout,
                                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}

void
gulkan_texture_record_transfer_full (GulkanTexture       *self,
                                     VkCommandBuffer      cmd_buffer,
                                     VkAccessFlags        src_access_mask,
                                     VkAccessFlags        dst_access_mask,
                                     VkImageLayout        src_layout,
                                     VkImageLayout        dst_layout,
                                     VkPipelineStageFlags src_stage_mask,
                                     VkPipelineStageFlags dst_stage_mask)
{
  GulkanDevice *device = gulkan_context_get_device (self->context);
  GulkanQueue  *gulkan_queue = gulkan_device_get_transfer_queue (device);
  uint32_t      queue_index = gulkan_queue_get_family_index (gulkan_queue);
  GMutex       *mutex = gulkan_queue_get_pool_mutex (gulkan_queue);

  VkImageMemoryBarrier image_memory_barrier =
  {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = src_access_mask,
    .dstAccessMask = dst_access_mask,
    .oldLayout = src_layout,
    .newLayout = dst_layout,
    .image = self->image,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = self->mip_levels,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
    .srcQueueFamilyIndex = queue_index,
    .dstQueueFamilyIndex = queue_index,
  };

  g_mutex_lock (mutex);
  vkCmdPipelineBarrier (cmd_buffer, src_stage_mask, dst_stage_mask, 0, 0, NULL,
                        0, NULL, 1, &image_memory_barrier);
  g_mutex_unlock (mutex);
}

gboolean
gulkan_texture_transfer_layout_full (GulkanTexture       *self,
                                     VkAccessFlags        src_access_mask,
                                     VkAccessFlags        dst_access_mask,
                                     VkImageLayout        src_layout,
                                     VkImageLayout        dst_layout,
                                     VkPipelineStageFlags src_stage_mask,
                                     VkPipelineStageFlags dst_stage_mask)
{
  GulkanDevice    *device = gulkan_context_get_device (self->context);
  GulkanQueue     *queue = gulkan_device_get_transfer_queue (device);
  GulkanCmdBuffer *cmd_buffer = gulkan_queue_request_cmd_buffer (queue);
  GMutex          *mutex = gulkan_queue_get_pool_mutex (queue);
  g_mutex_lock (mutex);
  gboolean ret = gulkan_cmd_buffer_begin_one_time (cmd_buffer);
  g_mutex_unlock (mutex);
  if (!ret)
    return FALSE;

  gulkan_texture_record_transfer_full (self,
                                       gulkan_cmd_buffer_get_handle (cmd_buffer),
                                       src_access_mask, dst_access_mask,
                                       src_layout, dst_layout, src_stage_mask,
                                       dst_stage_mask);

  if (!gulkan_queue_end_submit (queue, cmd_buffer))
    return FALSE;

  gulkan_queue_free_cmd_buffer (queue, cmd_buffer);

  return TRUE;
}

gboolean
gulkan_texture_transfer_layout (GulkanTexture *self,
                                VkImageLayout  src_layout,
                                VkImageLayout  dst_layout)
{
  GulkanDevice    *device = gulkan_context_get_device (self->context);
  GulkanQueue     *queue = gulkan_device_get_transfer_queue (device);
  GulkanCmdBuffer *cmd_buffer = gulkan_queue_request_cmd_buffer (queue);
  GMutex          *mutex = gulkan_queue_get_pool_mutex (queue);
  g_mutex_lock (mutex);
  gboolean ret = gulkan_cmd_buffer_begin_one_time (cmd_buffer);
  g_mutex_unlock (mutex);

  if (!ret)
    return FALSE;

  gulkan_texture_record_transfer (self,
                                  gulkan_cmd_buffer_get_handle (cmd_buffer),
                                  src_layout, dst_layout);

  if (!gulkan_queue_end_submit (queue, cmd_buffer))
    return FALSE;

  gulkan_queue_free_cmd_buffer (queue, cmd_buffer);

  return TRUE;
}

/**
 * gulkan_texture_get_image_view:
 * @self: a #GulkanTexture
 *
 * Returns: (transfer none): a #VkImageView
 */
VkImageView
gulkan_texture_get_image_view (GulkanTexture *self)
{
  return self->image_view;
}

/**
 * gulkan_texture_get_image:
 * @self: a #GulkanTexture
 *
 * Returns: (transfer none): a #VkImage
 */
VkImage
gulkan_texture_get_image (GulkanTexture *self)
{
  return self->image;
}

/**
 * gulkan_texture_get_extent:
 * @self: a #GulkanTexture
 *
 * Returns: (transfer none): a #VkExtent2D
 */
VkExtent2D
gulkan_texture_get_extent (GulkanTexture *self)
{
  return self->extent;
}

/**
 * gulkan_texture_get_format:
 * @self: a #GulkanTexture
 *
 * Returns: (transfer none): a #VkFormat
 */
VkFormat
gulkan_texture_get_format (GulkanTexture *self)
{
  return self->format;
}

guint
gulkan_texture_get_mip_levels (GulkanTexture *self)
{
  return self->mip_levels;
}

VkSampler
gulkan_texture_get_sampler (GulkanTexture *self)
{
  return self->sampler;
}

void
gulkan_texture_set_sampler (GulkanTexture *self, VkSampler sampler)
{
  self->sampler = sampler;
}

gboolean
gulkan_texture_init_sampler (GulkanTexture       *self,
                             VkFilter             filter,
                             VkSamplerAddressMode address_mode)
{
  VkSamplerMipmapMode mipmap_mode = (filter == VK_FILTER_LINEAR)
                                      ? VK_SAMPLER_MIPMAP_MODE_LINEAR
                                      : VK_SAMPLER_MIPMAP_MODE_NEAREST;

  VkSamplerCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .magFilter = filter,
    .minFilter = filter,
    .mipmapMode = mipmap_mode,
    .addressModeU = address_mode,
    .addressModeV = address_mode,
    .addressModeW = address_mode,
    .anisotropyEnable = VK_TRUE,
    .maxAnisotropy = 16,
    .compareEnable = VK_FALSE,
    .compareOp = VK_COMPARE_OP_ALWAYS,
    .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    .unnormalizedCoordinates = VK_FALSE,
    .minLod = 0.0f,
    .maxLod = (float) self->mip_levels,
  };

  VkDevice device = gulkan_context_get_device_handle (self->context);
  VkResult res = vkCreateSampler (device, &info, NULL, &self->sampler);
  vk_check_error ("vkCreateSampler", res, FALSE);

  return TRUE;
}

VkDescriptorImageInfo *
gulkan_texture_get_descriptor_info (GulkanTexture *self)
{
  VkDescriptorImageInfo *info = g_malloc (sizeof (VkDescriptorImageInfo));
  *info = (VkDescriptorImageInfo){
    .sampler = self->sampler,
    .imageView = self->image_view,
    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
  return info;
}
