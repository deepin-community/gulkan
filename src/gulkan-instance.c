/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "gulkan-instance.h"

#include <vulkan/vulkan.h>

gboolean
gulkan_has_error (VkResult res, const gchar *fun, const gchar *file, int line)
{
  if (res != VK_SUCCESS)
    {
      g_printerr ("ERROR: %s failed with %s (%d) in %s:%d\n", fun,
                  vk_result_string (res), res, file, line);
      return TRUE;
    }
  return FALSE;
}

#define ENUM_TO_STR(r)                                                         \
  case r:                                                                      \
    return #r

const gchar *
vk_result_string (VkResult code)
{
  switch (code)
    {
      ENUM_TO_STR (VK_SUCCESS);
      ENUM_TO_STR (VK_NOT_READY);
      ENUM_TO_STR (VK_TIMEOUT);
      ENUM_TO_STR (VK_EVENT_SET);
      ENUM_TO_STR (VK_EVENT_RESET);
      ENUM_TO_STR (VK_INCOMPLETE);
      ENUM_TO_STR (VK_ERROR_OUT_OF_HOST_MEMORY);
      ENUM_TO_STR (VK_ERROR_OUT_OF_DEVICE_MEMORY);
      ENUM_TO_STR (VK_ERROR_FRAGMENTED_POOL);
      ENUM_TO_STR (VK_ERROR_OUT_OF_POOL_MEMORY);
      ENUM_TO_STR (VK_ERROR_INITIALIZATION_FAILED);
      ENUM_TO_STR (VK_ERROR_DEVICE_LOST);
      ENUM_TO_STR (VK_ERROR_MEMORY_MAP_FAILED);
      ENUM_TO_STR (VK_ERROR_LAYER_NOT_PRESENT);
      ENUM_TO_STR (VK_ERROR_EXTENSION_NOT_PRESENT);
      ENUM_TO_STR (VK_ERROR_FEATURE_NOT_PRESENT);
      ENUM_TO_STR (VK_ERROR_INCOMPATIBLE_DRIVER);
      ENUM_TO_STR (VK_ERROR_TOO_MANY_OBJECTS);
      ENUM_TO_STR (VK_ERROR_FORMAT_NOT_SUPPORTED);
      ENUM_TO_STR (VK_ERROR_SURFACE_LOST_KHR);
      ENUM_TO_STR (VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
      ENUM_TO_STR (VK_SUBOPTIMAL_KHR);
      ENUM_TO_STR (VK_ERROR_OUT_OF_DATE_KHR);
      ENUM_TO_STR (VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
      ENUM_TO_STR (VK_ERROR_VALIDATION_FAILED_EXT);
      ENUM_TO_STR (VK_ERROR_INVALID_SHADER_NV);
      ENUM_TO_STR (VK_ERROR_INVALID_EXTERNAL_HANDLE);
      ENUM_TO_STR (VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
      default:
        return "UNKNOWN RESULT";
    }
}

const gchar *
vk_format_string (VkFormat format)
{
  switch (format)
    {
      ENUM_TO_STR (VK_FORMAT_UNDEFINED);
      ENUM_TO_STR (VK_FORMAT_R4G4_UNORM_PACK8);
      ENUM_TO_STR (VK_FORMAT_R4G4B4A4_UNORM_PACK16);
      ENUM_TO_STR (VK_FORMAT_B4G4R4A4_UNORM_PACK16);
      ENUM_TO_STR (VK_FORMAT_R5G6B5_UNORM_PACK16);
      ENUM_TO_STR (VK_FORMAT_B5G6R5_UNORM_PACK16);
      ENUM_TO_STR (VK_FORMAT_R5G5B5A1_UNORM_PACK16);
      ENUM_TO_STR (VK_FORMAT_B5G5R5A1_UNORM_PACK16);
      ENUM_TO_STR (VK_FORMAT_A1R5G5B5_UNORM_PACK16);
      ENUM_TO_STR (VK_FORMAT_R8_UNORM);
      ENUM_TO_STR (VK_FORMAT_R8_SNORM);
      ENUM_TO_STR (VK_FORMAT_R8_USCALED);
      ENUM_TO_STR (VK_FORMAT_R8_SSCALED);
      ENUM_TO_STR (VK_FORMAT_R8_UINT);
      ENUM_TO_STR (VK_FORMAT_R8_SINT);
      ENUM_TO_STR (VK_FORMAT_R8_SRGB);
      ENUM_TO_STR (VK_FORMAT_R8G8_UNORM);
      ENUM_TO_STR (VK_FORMAT_R8G8_SNORM);
      ENUM_TO_STR (VK_FORMAT_R8G8_USCALED);
      ENUM_TO_STR (VK_FORMAT_R8G8_SSCALED);
      ENUM_TO_STR (VK_FORMAT_R8G8_UINT);
      ENUM_TO_STR (VK_FORMAT_R8G8_SINT);
      ENUM_TO_STR (VK_FORMAT_R8G8_SRGB);
      ENUM_TO_STR (VK_FORMAT_R8G8B8_UNORM);
      ENUM_TO_STR (VK_FORMAT_R8G8B8_SNORM);
      ENUM_TO_STR (VK_FORMAT_R8G8B8_USCALED);
      ENUM_TO_STR (VK_FORMAT_R8G8B8_SSCALED);
      ENUM_TO_STR (VK_FORMAT_R8G8B8_UINT);
      ENUM_TO_STR (VK_FORMAT_R8G8B8_SINT);
      ENUM_TO_STR (VK_FORMAT_R8G8B8_SRGB);
      ENUM_TO_STR (VK_FORMAT_B8G8R8_UNORM);
      ENUM_TO_STR (VK_FORMAT_B8G8R8_SNORM);
      ENUM_TO_STR (VK_FORMAT_B8G8R8_USCALED);
      ENUM_TO_STR (VK_FORMAT_B8G8R8_SSCALED);
      ENUM_TO_STR (VK_FORMAT_B8G8R8_UINT);
      ENUM_TO_STR (VK_FORMAT_B8G8R8_SINT);
      ENUM_TO_STR (VK_FORMAT_B8G8R8_SRGB);
      ENUM_TO_STR (VK_FORMAT_R8G8B8A8_UNORM);
      ENUM_TO_STR (VK_FORMAT_R8G8B8A8_SNORM);
      ENUM_TO_STR (VK_FORMAT_R8G8B8A8_USCALED);
      ENUM_TO_STR (VK_FORMAT_R8G8B8A8_SSCALED);
      ENUM_TO_STR (VK_FORMAT_R8G8B8A8_UINT);
      ENUM_TO_STR (VK_FORMAT_R8G8B8A8_SINT);
      ENUM_TO_STR (VK_FORMAT_R8G8B8A8_SRGB);
      ENUM_TO_STR (VK_FORMAT_B8G8R8A8_UNORM);
      ENUM_TO_STR (VK_FORMAT_B8G8R8A8_SNORM);
      ENUM_TO_STR (VK_FORMAT_B8G8R8A8_USCALED);
      ENUM_TO_STR (VK_FORMAT_B8G8R8A8_SSCALED);
      ENUM_TO_STR (VK_FORMAT_B8G8R8A8_UINT);
      ENUM_TO_STR (VK_FORMAT_B8G8R8A8_SINT);
      ENUM_TO_STR (VK_FORMAT_B8G8R8A8_SRGB);
      ENUM_TO_STR (VK_FORMAT_A8B8G8R8_UNORM_PACK32);
      ENUM_TO_STR (VK_FORMAT_A8B8G8R8_SNORM_PACK32);
      ENUM_TO_STR (VK_FORMAT_A8B8G8R8_USCALED_PACK32);
      ENUM_TO_STR (VK_FORMAT_A8B8G8R8_SSCALED_PACK32);
      ENUM_TO_STR (VK_FORMAT_A8B8G8R8_UINT_PACK32);
      ENUM_TO_STR (VK_FORMAT_A8B8G8R8_SINT_PACK32);
      ENUM_TO_STR (VK_FORMAT_A8B8G8R8_SRGB_PACK32);
      ENUM_TO_STR (VK_FORMAT_A2R10G10B10_UNORM_PACK32);
      ENUM_TO_STR (VK_FORMAT_A2R10G10B10_SNORM_PACK32);
      ENUM_TO_STR (VK_FORMAT_A2R10G10B10_USCALED_PACK32);
      ENUM_TO_STR (VK_FORMAT_A2R10G10B10_SSCALED_PACK32);
      ENUM_TO_STR (VK_FORMAT_A2R10G10B10_UINT_PACK32);
      ENUM_TO_STR (VK_FORMAT_A2R10G10B10_SINT_PACK32);
      ENUM_TO_STR (VK_FORMAT_A2B10G10R10_UNORM_PACK32);
      ENUM_TO_STR (VK_FORMAT_A2B10G10R10_SNORM_PACK32);
      ENUM_TO_STR (VK_FORMAT_A2B10G10R10_USCALED_PACK32);
      ENUM_TO_STR (VK_FORMAT_A2B10G10R10_SSCALED_PACK32);
      ENUM_TO_STR (VK_FORMAT_A2B10G10R10_UINT_PACK32);
      ENUM_TO_STR (VK_FORMAT_A2B10G10R10_SINT_PACK32);
      ENUM_TO_STR (VK_FORMAT_R16_UNORM);
      ENUM_TO_STR (VK_FORMAT_R16_SNORM);
      ENUM_TO_STR (VK_FORMAT_R16_USCALED);
      ENUM_TO_STR (VK_FORMAT_R16_SSCALED);
      ENUM_TO_STR (VK_FORMAT_R16_UINT);
      ENUM_TO_STR (VK_FORMAT_R16_SINT);
      ENUM_TO_STR (VK_FORMAT_R16_SFLOAT);
      ENUM_TO_STR (VK_FORMAT_R16G16_UNORM);
      ENUM_TO_STR (VK_FORMAT_R16G16_SNORM);
      ENUM_TO_STR (VK_FORMAT_R16G16_USCALED);
      ENUM_TO_STR (VK_FORMAT_R16G16_SSCALED);
      ENUM_TO_STR (VK_FORMAT_R16G16_UINT);
      ENUM_TO_STR (VK_FORMAT_R16G16_SINT);
      ENUM_TO_STR (VK_FORMAT_R16G16_SFLOAT);
      ENUM_TO_STR (VK_FORMAT_R16G16B16_UNORM);
      ENUM_TO_STR (VK_FORMAT_R16G16B16_SNORM);
      ENUM_TO_STR (VK_FORMAT_R16G16B16_USCALED);
      ENUM_TO_STR (VK_FORMAT_R16G16B16_SSCALED);
      ENUM_TO_STR (VK_FORMAT_R16G16B16_UINT);
      ENUM_TO_STR (VK_FORMAT_R16G16B16_SINT);
      ENUM_TO_STR (VK_FORMAT_R16G16B16_SFLOAT);
      ENUM_TO_STR (VK_FORMAT_R16G16B16A16_UNORM);
      ENUM_TO_STR (VK_FORMAT_R16G16B16A16_SNORM);
      ENUM_TO_STR (VK_FORMAT_R16G16B16A16_USCALED);
      ENUM_TO_STR (VK_FORMAT_R16G16B16A16_SSCALED);
      ENUM_TO_STR (VK_FORMAT_R16G16B16A16_UINT);
      ENUM_TO_STR (VK_FORMAT_R16G16B16A16_SINT);
      ENUM_TO_STR (VK_FORMAT_R16G16B16A16_SFLOAT);
      ENUM_TO_STR (VK_FORMAT_R32_UINT);
      ENUM_TO_STR (VK_FORMAT_R32_SINT);
      ENUM_TO_STR (VK_FORMAT_R32_SFLOAT);
      ENUM_TO_STR (VK_FORMAT_R32G32_UINT);
      ENUM_TO_STR (VK_FORMAT_R32G32_SINT);
      ENUM_TO_STR (VK_FORMAT_R32G32_SFLOAT);
      ENUM_TO_STR (VK_FORMAT_R32G32B32_UINT);
      ENUM_TO_STR (VK_FORMAT_R32G32B32_SINT);
      ENUM_TO_STR (VK_FORMAT_R32G32B32_SFLOAT);
      ENUM_TO_STR (VK_FORMAT_R32G32B32A32_UINT);
      ENUM_TO_STR (VK_FORMAT_R32G32B32A32_SINT);
      ENUM_TO_STR (VK_FORMAT_R32G32B32A32_SFLOAT);
      ENUM_TO_STR (VK_FORMAT_R64_UINT);
      ENUM_TO_STR (VK_FORMAT_R64_SINT);
      ENUM_TO_STR (VK_FORMAT_R64_SFLOAT);
      ENUM_TO_STR (VK_FORMAT_R64G64_UINT);
      ENUM_TO_STR (VK_FORMAT_R64G64_SINT);
      ENUM_TO_STR (VK_FORMAT_R64G64_SFLOAT);
      ENUM_TO_STR (VK_FORMAT_R64G64B64_UINT);
      ENUM_TO_STR (VK_FORMAT_R64G64B64_SINT);
      ENUM_TO_STR (VK_FORMAT_R64G64B64_SFLOAT);
      ENUM_TO_STR (VK_FORMAT_R64G64B64A64_UINT);
      ENUM_TO_STR (VK_FORMAT_R64G64B64A64_SINT);
      ENUM_TO_STR (VK_FORMAT_R64G64B64A64_SFLOAT);
      ENUM_TO_STR (VK_FORMAT_B10G11R11_UFLOAT_PACK32);
      ENUM_TO_STR (VK_FORMAT_E5B9G9R9_UFLOAT_PACK32);
      ENUM_TO_STR (VK_FORMAT_D16_UNORM);
      ENUM_TO_STR (VK_FORMAT_X8_D24_UNORM_PACK32);
      ENUM_TO_STR (VK_FORMAT_D32_SFLOAT);
      ENUM_TO_STR (VK_FORMAT_S8_UINT);
      ENUM_TO_STR (VK_FORMAT_D16_UNORM_S8_UINT);
      ENUM_TO_STR (VK_FORMAT_D24_UNORM_S8_UINT);
      ENUM_TO_STR (VK_FORMAT_D32_SFLOAT_S8_UINT);
      ENUM_TO_STR (VK_FORMAT_BC1_RGB_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_BC1_RGB_SRGB_BLOCK);
      ENUM_TO_STR (VK_FORMAT_BC1_RGBA_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_BC1_RGBA_SRGB_BLOCK);
      ENUM_TO_STR (VK_FORMAT_BC2_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_BC2_SRGB_BLOCK);
      ENUM_TO_STR (VK_FORMAT_BC3_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_BC3_SRGB_BLOCK);
      ENUM_TO_STR (VK_FORMAT_BC4_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_BC4_SNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_BC5_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_BC5_SNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_BC6H_UFLOAT_BLOCK);
      ENUM_TO_STR (VK_FORMAT_BC6H_SFLOAT_BLOCK);
      ENUM_TO_STR (VK_FORMAT_BC7_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_BC7_SRGB_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK);
      ENUM_TO_STR (VK_FORMAT_EAC_R11_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_EAC_R11_SNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_EAC_R11G11_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_EAC_R11G11_SNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_5x4_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_5x4_SRGB_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_5x5_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_5x5_SRGB_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_6x5_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_6x5_SRGB_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_6x6_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_6x6_SRGB_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_8x5_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_8x5_SRGB_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_8x6_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_8x6_SRGB_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_8x8_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_8x8_SRGB_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_10x5_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_10x5_SRGB_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_10x6_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_10x6_SRGB_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_10x8_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_10x8_SRGB_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_10x10_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_10x10_SRGB_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_12x10_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_12x10_SRGB_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_12x12_UNORM_BLOCK);
      ENUM_TO_STR (VK_FORMAT_ASTC_12x12_SRGB_BLOCK);
      ENUM_TO_STR (VK_FORMAT_G8B8G8R8_422_UNORM);
      ENUM_TO_STR (VK_FORMAT_B8G8R8G8_422_UNORM);
      ENUM_TO_STR (VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM);
      ENUM_TO_STR (VK_FORMAT_G8_B8R8_2PLANE_420_UNORM);
      ENUM_TO_STR (VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM);
      ENUM_TO_STR (VK_FORMAT_G8_B8R8_2PLANE_422_UNORM);
      ENUM_TO_STR (VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM);
      ENUM_TO_STR (VK_FORMAT_R10X6_UNORM_PACK16);
      ENUM_TO_STR (VK_FORMAT_R10X6G10X6_UNORM_2PACK16);
      ENUM_TO_STR (VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16);
      ENUM_TO_STR (VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16);
      ENUM_TO_STR (VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16);
      ENUM_TO_STR (VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16);
      ENUM_TO_STR (VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16);
      ENUM_TO_STR (VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16);
      ENUM_TO_STR (VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16);
      ENUM_TO_STR (VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16);
      ENUM_TO_STR (VK_FORMAT_R12X4_UNORM_PACK16);
      ENUM_TO_STR (VK_FORMAT_R12X4G12X4_UNORM_2PACK16);
      ENUM_TO_STR (VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16);
      ENUM_TO_STR (VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16);
      ENUM_TO_STR (VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16);
      ENUM_TO_STR (VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16);
      ENUM_TO_STR (VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16);
      ENUM_TO_STR (VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16);
      ENUM_TO_STR (VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16);
      ENUM_TO_STR (VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16);
      ENUM_TO_STR (VK_FORMAT_G16B16G16R16_422_UNORM);
      ENUM_TO_STR (VK_FORMAT_B16G16R16G16_422_UNORM);
      ENUM_TO_STR (VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM);
      ENUM_TO_STR (VK_FORMAT_G16_B16R16_2PLANE_420_UNORM);
      ENUM_TO_STR (VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM);
      ENUM_TO_STR (VK_FORMAT_G16_B16R16_2PLANE_422_UNORM);
      ENUM_TO_STR (VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM);
      ENUM_TO_STR (VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG);
      ENUM_TO_STR (VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG);
      ENUM_TO_STR (VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG);
      ENUM_TO_STR (VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG);
      ENUM_TO_STR (VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG);
      ENUM_TO_STR (VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG);
      ENUM_TO_STR (VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG);
      ENUM_TO_STR (VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG);
      default:
        return "Unknown";
    }
}

/**
 * GulkanInstance:
 *
 * A #GulkanInstance-struct which represents the Vulkan instance.
 */
struct _GulkanInstance
{
  GObjectClass parent_class;

  VkInstance instance;
};

G_DEFINE_TYPE (GulkanInstance, gulkan_instance, G_TYPE_OBJECT)

static void
gulkan_instance_init (GulkanInstance *self)
{
  self->instance = VK_NULL_HANDLE;
}

GulkanInstance *
gulkan_instance_new (void)
{
  return (GulkanInstance *) g_object_new (GULKAN_TYPE_INSTANCE, 0);
}

static void
gulkan_instance_finalize (GObject *gobject)
{
  GulkanInstance *self = GULKAN_INSTANCE (gobject);
  vkDestroyInstance (self->instance, NULL);
}

static void
gulkan_instance_class_init (GulkanInstanceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gulkan_instance_finalize;
}

static gboolean
_init_instance_extensions (GSList   *required_extensions,
                           char    **enabled_extensions,
                           uint32_t *out_num_enabled)
{
  uint32_t num_enabled = 0;
  uint32_t num_extensions = 0;
  VkResult res = vkEnumerateInstanceExtensionProperties (NULL, &num_extensions,
                                                         NULL);
  vk_check_error ("vkEnumerateInstanceExtensionProperties", res, FALSE);

  if (num_extensions == 0)
    {
      g_warning ("Could not find any extensions.\n");
      return FALSE;
    }

  VkExtensionProperties *extension_props
    = g_malloc (sizeof (VkExtensionProperties) * num_extensions);

  res = vkEnumerateInstanceExtensionProperties (NULL, &num_extensions,
                                                extension_props);
  vk_check_error ("vkEnumerateInstanceExtensionProperties", res, FALSE);

  for (size_t i = 0; i < g_slist_length (required_extensions); i++)
    {
      gboolean found = FALSE;
      for (uint32_t j = 0; j < num_extensions; j++)
        {
          GSList *name = g_slist_nth (required_extensions, (guint) i);
          if (strcmp ((gchar *) name->data, extension_props[j].extensionName)
              == 0)
            {
              found = TRUE;
              enabled_extensions[num_enabled++] = g_strdup (extension_props[j]
                                                              .extensionName);
              break;
            }
        }

      if (!found)
        {
          GSList *name = g_slist_nth (required_extensions, (guint) i);
          g_printerr ("Vulkan missing requested extension '%s'.\n",
                      (gchar *) name->data);
        }
    }

  if (num_enabled != (uint32_t) g_slist_length (required_extensions))
    return FALSE;

  *out_num_enabled = num_enabled;

  g_free (extension_props);

  return TRUE;
}

/**
 * gulkan_instance_create:
 * @self: a #GulkanInstance
 * @required_extensions: (element-type utf8): A list of extensions to enable
 *
 * Returns: %TRUE on success
 */
gboolean
gulkan_instance_create (GulkanInstance *self, GSList *required_extensions)
{
  uint32_t num_enabled_layers = 0;
  char   **enabled_layers = NULL;
  char   **enabled_extensions = g_malloc (sizeof (char *)
                                          * g_slist_length (required_extensions));

  uint32_t num_enabled_extensions = 0;

  if (!_init_instance_extensions (required_extensions, enabled_extensions,
                                  &num_enabled_extensions))
    return FALSE;

  if (num_enabled_extensions > 0)
    {
      g_debug ("Requesting instance extensions:");
      for (uint32_t i = 0; i < num_enabled_extensions; i++)
        g_debug ("%s", enabled_extensions[i]);
    }

  VkInstanceCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &(VkApplicationInfo) {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "gulkan",
      .applicationVersion = 1,
      .engineVersion = 1,
      .apiVersion = VK_MAKE_VERSION (1, 1, 0),
    },
    .enabledExtensionCount = num_enabled_extensions,
    .ppEnabledExtensionNames = (const char* const*) enabled_extensions,
    .enabledLayerCount = num_enabled_layers,
    .ppEnabledLayerNames = (const char* const*) enabled_layers,
  };

  VkResult res = vkCreateInstance (&info, NULL, &self->instance);
  vk_check_error ("vkCreateInstance", res, FALSE);

  for (uint32_t i = 0; i < num_enabled_extensions; i++)
    g_free (enabled_extensions[i]);
  g_free (enabled_extensions);

  return TRUE;
}

/**
 * gulkan_instance_create_from_vk:
 * @self: a #GulkanInstance
 * @vk_instance: An externally created VkInstance
 *
 * Returns: %TRUE on success
 */
gboolean
gulkan_instance_create_from_vk (GulkanInstance *self, VkInstance vk_instance)
{
  if (self->instance != VK_NULL_HANDLE)
    return FALSE;

  self->instance = vk_instance;
  return TRUE;
}

/**
 * gulkan_instance_get_handle:
 * @self: a #GulkanInstance
 *
 * Returns: (transfer none): a #VkInstance
 */
VkInstance
gulkan_instance_get_handle (GulkanInstance *self)
{
  return self->instance;
}
