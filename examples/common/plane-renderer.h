/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef PLANE_RENDERER_H_
#define PLANE_RENDERER_H_

#include <glib-object.h>
#include <stdint.h>

#include <gulkan.h>

#define PLANE_TYPE_RENDERER plane_renderer_get_type ()
G_DECLARE_FINAL_TYPE (PlaneRenderer,
                      plane_renderer,
                      PLANE,
                      RENDERER,
                      GulkanSwapchainRenderer)

struct _PlaneRendererClass
{
  GulkanSwapchainRendererClass parent;
};

PlaneRenderer *
plane_renderer_new (void);

PlaneRenderer *
plane_renderer_new_from_context (GulkanContext *context);

gboolean
plane_renderer_initialize (PlaneRenderer *self, GulkanTexture *texture);

gboolean
plane_renderer_update_texture (PlaneRenderer *self, GulkanTexture *texture);

#endif /* PLANE_RENDERER_H_ */
