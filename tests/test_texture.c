/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "gulkan.h"

static GdkPixbuf *
load_gdk_pixbuf ()
{
  GError *error = NULL;
  GdkPixbuf * pixbuf_rgb =
    gdk_pixbuf_new_from_resource ("/res/cat.jpg", &error);

  g_assert_null (error);

  GdkPixbuf *pixbuf = gdk_pixbuf_add_alpha (pixbuf_rgb, FALSE, 0, 0, 0);
  g_object_unref (pixbuf_rgb);
  return pixbuf;
}

static void
_test_pixbuf ()
{
  GdkPixbuf * pixbuf = load_gdk_pixbuf ();
  g_assert_nonnull (pixbuf);
  g_object_unref (pixbuf);
}

static void
_test_pixbuf_texture ()
{
  GdkPixbuf * pixbuf = load_gdk_pixbuf ();
  g_assert_nonnull (pixbuf);

  GulkanClient *client = gulkan_client_new ();
  g_assert_nonnull (client);

  GulkanTexture *texture =
    gulkan_texture_new_from_pixbuf (client, pixbuf,
                                    VK_FORMAT_R8G8B8A8_UNORM,
                                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                    FALSE);
  g_assert_nonnull (texture);

  g_object_unref (texture);
  g_object_unref (client);

  g_object_unref (pixbuf);
}

int
main ()
{
  _test_pixbuf ();
  _test_pixbuf_texture ();

  return 0;
}


