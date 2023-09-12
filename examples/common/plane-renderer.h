/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef PLANE_RENDERER_H_
#define PLANE_RENDERER_H_

#include <stdint.h>
#include <glib-object.h>

#include <gulkan.h>

G_BEGIN_DECLS

#define PLANE_TYPE_RENDERER plane_renderer_get_type()
G_DECLARE_FINAL_TYPE (PlaneRenderer, plane_renderer,
                      PLANE, RENDERER, GulkanSwapchainRenderer)

struct _PlaneRendererClass
{
  GulkanSwapchainRendererClass parent;
};

PlaneRenderer *
plane_renderer_new (void);

PlaneRenderer *
plane_renderer_new_from_client (GulkanClient *client);

gboolean
plane_renderer_initialize (PlaneRenderer *self,
                           VkSurfaceKHR surface,
                           GulkanTexture *texture);

gboolean
plane_renderer_update_texture_descriptor (PlaneRenderer *self,
                                          GulkanTexture *texture);

G_END_DECLS

#endif /* PLANE_RENDERER_H_ */
