/*
 * gulkan
 * Copyright 2020 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * Author: Remco Kranenburg <remco@burgsoft.nl>
 * SPDX-License-Identifier: MIT
 */

#define _GNU_SOURCE
#include "common/plane-example.h"
#include <math.h>
#include <pango/pangocairo.h>

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

static void
draw_gradient_quad (cairo_t *cr, double w, double h)
{
  cairo_pattern_t *pat = cairo_pattern_create_linear (0.0, 0.0, 0.0, h);
  cairo_pattern_add_color_stop_rgba (pat, 1, 0, 0, 0, 1);
  cairo_pattern_add_color_stop_rgba (pat, 0, 1, 1, 1, 1);
  cairo_rectangle (cr, 0, 0, w, h);
  cairo_set_source (cr, pat);
  cairo_fill (cr);
  cairo_pattern_destroy (pat);
}

static void
draw_gradient_circle (cairo_t *cr, double w, double h)
{
  double r0;
  if (w < h)
    r0 = w / 10.0;
  else
    r0 = h / 10.0;

  double radius = r0 * 3.0;
  double r1 = r0 * 5.0;

  double center_x = w / 2.0;
  double center_y = h / 2.0;

  double cx0 = center_x - r0 / 2.0;
  double cy0 = center_y - r0;
  double cx1 = center_x - r0;
  double cy1 = center_y - r0;

  PangoLayout          *layout = pango_cairo_create_layout (cr);
  PangoFontDescription *description
    = pango_font_description_from_string ("Sans 32");
  pango_layout_set_font_description (layout, description);
  pango_font_description_free (description);

  int    width, height;
  double x_offset = 0.0;

  cairo_pattern_t *pat = cairo_pattern_create_radial (cx0, cy0, r0, cx1, cy1,
                                                      r1);
  cairo_pattern_add_color_stop_rgba (pat, 0, 1, 1, 1, 1);
  cairo_pattern_add_color_stop_rgba (pat, 1, 0, 0, 0, 1);
  cairo_set_source (cr, pat);
  cairo_arc (cr, center_x, center_y, radius, 0, 2 * M_PI);
  cairo_fill (cr);
  cairo_pattern_destroy (pat);

  // draw red text

  cairo_move_to (cr, center_x + x_offset, center_y);
  cairo_set_source_rgb (cr, 0.8, 0.3, 0.3);

  const gchar *text = "أحمر😊";
  pango_layout_set_text (layout, text, -1);
  pango_layout_get_size (layout, &width, &height);
  x_offset += (double) width / PANGO_SCALE;

  pango_cairo_update_layout (cr, layout);
  pango_cairo_show_layout (cr, layout);

  // draw green text

  cairo_move_to (cr, center_x + x_offset, center_y);
  cairo_set_source_rgb (cr, 0.3, 0.8, 0.3);

  pango_layout_set_text (layout, "绿色的🤣", -1);
  pango_layout_get_size (layout, &width, &height);
  x_offset += (double) width / PANGO_SCALE;

  pango_cairo_update_layout (cr, layout);
  pango_cairo_show_layout (cr, layout);

  // draw blue text

  cairo_move_to (cr, center_x + x_offset, center_y);
  cairo_set_source_rgb (cr, 0.3, 0.3, 0.8);

  pango_layout_set_text (layout, "नीला🙏", -1);
  pango_layout_get_size (layout, &width, &height);
  x_offset += (double) width / PANGO_SCALE;

  pango_cairo_update_layout (cr, layout);
  pango_cairo_show_layout (cr, layout);

  g_object_unref (layout);
}

static void
draw_cairo (cairo_t *cr, double w, double h)
{
  draw_gradient_quad (cr, w, h);
  draw_gradient_circle (cr, w, h);
}

static cairo_surface_t *
create_cairo_surface (unsigned char *image, int w, int h)
{
  cairo_surface_t *surface
    = cairo_image_surface_create_for_data (image, CAIRO_FORMAT_ARGB32, w, h,
                                           w * 4);

  cairo_t *cr = cairo_create (surface);

  cairo_rectangle (cr, 0, 0, w, h);
  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_fill (cr);

  draw_cairo (cr, w, h);

  cairo_destroy (cr);

  return surface;
}

static GulkanTexture *
_init_texture (PlaneExample *example, GulkanContext *context, GdkPixbuf *pixbuf)
{
  (void) pixbuf;
  (void) example;

  int w = gdk_pixbuf_get_width (pixbuf);
  int h = gdk_pixbuf_get_height (pixbuf);

  unsigned char   *image = g_malloc (sizeof (unsigned char)
                                     * (unsigned long) (w * h) * 4);
  cairo_surface_t *surface = create_cairo_surface (image, w, h);

  if (surface == NULL)
    {
      fprintf (stderr, "Could not create cairo surface.\n");
      return NULL;
    }

  GulkanTexture *texture = gulkan_texture_new_from_cairo_surface (
    context, surface, VK_FORMAT_B8G8R8A8_SRGB,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  g_free (image);

  return texture;
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
