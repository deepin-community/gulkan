/*
 * gulkan
 * Copyright 2020 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef PLANE_EXAMPLE_H_
#define PLANE_EXAMPLE_H_

#include <glib-object.h>

#include <gulkan.h>

G_BEGIN_DECLS

#define PLANE_TYPE_EXAMPLE plane_example_get_type()
G_DECLARE_DERIVABLE_TYPE (PlaneExample, plane_example,
                          PLANE, EXAMPLE, GObject)

struct _PlaneExampleClass
{
  GObjectClass parent;

  GulkanTexture *(*init_texture) (PlaneExample *self,
                                  GulkanClient *client,
                                  GdkPixbuf    *pixbuf);
};

gboolean
plane_example_initialize (PlaneExample *self,
                          const gchar  *pixbuf_uri,
                          GSList       *instance_ext_list,
                          GSList       *device_ext_list);

void
plane_example_run (PlaneExample *self);

#define GULKAN_TYPE_EXAMPLE gulkan_example_get_type ()
G_DECLARE_FINAL_TYPE (Example, gulkan_example, GULKAN,
                      EXAMPLE, PlaneExample)

G_END_DECLS

#endif /* PLANE_EXAMPLE_H_ */
