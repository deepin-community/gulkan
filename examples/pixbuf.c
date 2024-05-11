/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "common/plane-example.h"

struct _Example
{
  PlaneExample parent;
};
G_DEFINE_TYPE (Example, gulkan_example, PLANE_TYPE_EXAMPLE)

static void
gulkan_example_init (Example *self)
{
  (void) self;
}

static GulkanTexture *
_init_texture (PlaneExample *example, GulkanContext *context, GdkPixbuf *pixbuf)
{
  (void) example;
  return gulkan_texture_new_from_pixbuf (
    context, pixbuf, VK_FORMAT_R8G8B8A8_SRGB,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, TRUE);
}

static void
gulkan_example_class_init (ExampleClass *klass)
{
  PlaneExampleClass *parent_class = PLANE_EXAMPLE_CLASS (klass);
  parent_class->init_texture = _init_texture;
}

int
main ()
{
  Example *self = (Example *) g_object_new (GULKAN_TYPE_EXAMPLE, 0);
  if (!plane_example_initialize (PLANE_EXAMPLE (self), "/res/cat_srgb.jpg",
                                 NULL, NULL))
    return EXIT_FAILURE;

  plane_example_run (PLANE_EXAMPLE (self));

  g_object_unref (self);

  return EXIT_SUCCESS;
}
