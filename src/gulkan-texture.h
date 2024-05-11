/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef GULKAN_TEXTURE_H_
#define GULKAN_TEXTURE_H_

#if !defined(GULKAN_INSIDE) && !defined(GULKAN_COMPILATION)
#error "Only <gulkan.h> can be included directly."
#endif

#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib-object.h>
#include <vulkan/vulkan.h>

#include "gulkan-buffer.h"
#include "gulkan-context.h"

G_BEGIN_DECLS

// equivalent to wlroots dmabuf.h
#define GULKAN_DMABUF_MAX_PLANES 4

/**
 * GulkanDmabufAttributes:
 * @width: Width
 * @height: Height
 * @format: DRM format
 * @modifier: dmabuf modifier
 * @n_planes: Number of planes
 * @offset: Memory offset of plane N
 * @stride: Stride of plane N
 * @fd: File descriptor of plane N. Can be different file descriptors pointing
 *   at the same memory (Multi plane image where the planes are laid out next
 *   to each other and are accessed with offset rather than different memory
 *   per plane).
 */
struct GulkanDmabufAttributes
{
  int32_t  width;
  int32_t  height;
  uint32_t format;
  uint64_t modifier;

  int      n_planes;
  uint32_t offset[GULKAN_DMABUF_MAX_PLANES];
  uint32_t stride[GULKAN_DMABUF_MAX_PLANES];
  int      fd[GULKAN_DMABUF_MAX_PLANES];
};

#define GULKAN_TYPE_TEXTURE gulkan_texture_get_type ()
G_DECLARE_FINAL_TYPE (GulkanTexture, gulkan_texture, GULKAN, TEXTURE, GObject)

GulkanTexture *
gulkan_texture_new_mip_levels (GulkanContext *context,
                               VkExtent2D     extent,
                               guint          mip_levels,
                               VkFormat       format);

GulkanTexture *
gulkan_texture_new (GulkanContext *context, VkExtent2D extent, VkFormat format);

GulkanTexture *
gulkan_texture_new_from_pixbuf (GulkanContext *context,
                                GdkPixbuf     *pixbuf,
                                VkFormat       format,
                                VkImageLayout  layout,
                                gboolean       create_mipmaps);

GulkanTexture *
gulkan_texture_new_from_resource (GulkanContext *context,
                                  const char    *resource_path,
                                  VkImageLayout  layout,
                                  gboolean       create_mipmaps);

GulkanTexture *
gulkan_texture_new_from_cairo_surface (GulkanContext   *context,
                                       cairo_surface_t *surface,
                                       VkFormat         format,
                                       VkImageLayout    layout);

GulkanTexture *
gulkan_texture_new_from_dmabuf (GulkanContext *context,
                                int            fd,
                                VkExtent2D     extent,
                                VkFormat       format);

GulkanTexture *
gulkan_texture_new_from_dmabuf_attribs (GulkanContext                 *context,
                                        struct GulkanDmabufAttributes *attribs);

GulkanTexture *
gulkan_texture_new_export_fd (GulkanContext *context,
                              VkExtent2D     extent,
                              VkFormat       format,
                              VkImageLayout  layout,
                              gsize         *size,
                              int           *fd);

void
gulkan_texture_record_transfer (GulkanTexture  *self,
                                VkCommandBuffer cmd_buffer,
                                VkImageLayout   src_layout,
                                VkImageLayout   dst_layout);

void
gulkan_texture_record_transfer_full (GulkanTexture       *self,
                                     VkCommandBuffer      cmd_buffer,
                                     VkAccessFlags        src_access_mask,
                                     VkAccessFlags        dst_access_mask,
                                     VkImageLayout        src_layout,
                                     VkImageLayout        dst_layout,
                                     VkPipelineStageFlags src_stage_mask,
                                     VkPipelineStageFlags dst_stage_mask);

gboolean
gulkan_texture_transfer_layout (GulkanTexture *self,
                                VkImageLayout  src_layout,
                                VkImageLayout  dst_layout);

gboolean
gulkan_texture_transfer_layout_full (GulkanTexture       *self,
                                     VkAccessFlags        src_access_mask,
                                     VkAccessFlags        dst_access_mask,
                                     VkImageLayout        src_layout,
                                     VkImageLayout        dst_layout,
                                     VkPipelineStageFlags src_stage_mask,
                                     VkPipelineStageFlags dst_stage_mask);

gboolean
gulkan_texture_upload_pixels (GulkanTexture *self,
                              guchar        *pixels,
                              gsize          size,
                              VkImageLayout  layout);

gboolean
gulkan_texture_upload_pixels_region (GulkanTexture *self,
                                     guchar        *region_pixels,
                                     gsize          region_size,
                                     VkImageLayout  layout,
                                     VkOffset2D     offset,
                                     VkExtent2D     extent);

gboolean
gulkan_texture_upload_cairo_surface (GulkanTexture   *self,
                                     cairo_surface_t *surface,
                                     VkImageLayout    layout);

gboolean
gulkan_texture_upload_pixbuf (GulkanTexture *self,
                              GdkPixbuf     *pixbuf,
                              VkImageLayout  layout);

VkImageView
gulkan_texture_get_image_view (GulkanTexture *self);

VkImage
gulkan_texture_get_image (GulkanTexture *self);

VkExtent2D
gulkan_texture_get_extent (GulkanTexture *self);

VkFormat
gulkan_texture_get_format (GulkanTexture *self);

guint
gulkan_texture_get_mip_levels (GulkanTexture *self);

VkSampler
gulkan_texture_get_sampler (GulkanTexture *self);

gboolean
gulkan_texture_init_sampler (GulkanTexture       *self,
                             VkFilter             filter,
                             VkSamplerAddressMode address_mode);

VkDescriptorImageInfo *
gulkan_texture_get_descriptor_info (GulkanTexture *self);

void
gulkan_texture_set_sampler (GulkanTexture *self, VkSampler sampler);

G_END_DECLS

#endif /* GULKAN_TEXTURE_H_ */
