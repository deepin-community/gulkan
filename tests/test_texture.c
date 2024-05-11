/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "gulkan.h"

static void
_test_resource_texture ()
{
  GulkanContext *context = gulkan_context_new ();
  g_assert_nonnull (context);

  GulkanTexture *texture
    = gulkan_texture_new_from_resource (context, "/res/cat_srgb.jpg",
                                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                        TRUE);

  g_assert_nonnull (texture);

  g_object_unref (texture);
  g_object_unref (context);
}

static void
_test_raw_texture ()
{
  GError    *error = NULL;
  GdkPixbuf *pixbuf_rgb = gdk_pixbuf_new_from_resource ("/res/cat_srgb.jpg",
                                                        &error);

  g_assert_null (error);

  GdkPixbuf *pixbuf = gdk_pixbuf_add_alpha (pixbuf_rgb, FALSE, 0, 0, 0);
  g_object_unref (pixbuf_rgb);

  g_assert_nonnull (pixbuf);

  GulkanContext *context = gulkan_context_new ();
  g_assert_nonnull (context);

  guchar    *data = gdk_pixbuf_get_pixels (pixbuf);
  gsize      size = gdk_pixbuf_get_byte_length (pixbuf);
  VkExtent2D extent = {(uint32_t) gdk_pixbuf_get_width (pixbuf),
                       (uint32_t) gdk_pixbuf_get_height (pixbuf)};

  GulkanTexture *texture = gulkan_texture_new (context, extent,
                                               VK_FORMAT_R8G8B8A8_UNORM);
  g_assert_nonnull (texture);

  gboolean ret
    = gulkan_texture_upload_pixels (texture, data, size,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  g_assert_true (ret);

  g_object_unref (texture);
  g_object_unref (context);

  g_object_unref (pixbuf);
}

int
main ()
{
  _test_resource_texture ();
  _test_raw_texture ();

  return 0;
}
