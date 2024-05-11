/*
 * gulkan
 * Copyright 2020 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef GULKAN_SWAPCHAIN_RENDERER_H_
#define GULKAN_SWAPCHAIN_RENDERER_H_

#include <glib-object.h>

#include "gulkan-render-pass.h"
#include "gulkan-renderer.h"

G_BEGIN_DECLS

#define GULKAN_TYPE_SWAPCHAIN_RENDERER gulkan_swapchain_renderer_get_type ()
G_DECLARE_DERIVABLE_TYPE (GulkanSwapchainRenderer,
                          gulkan_swapchain_renderer,
                          GULKAN,
                          SWAPCHAIN_RENDERER,
                          GulkanRenderer)

/**
 * GulkanSwapchainRendererClass:
 * @parent: Parent class
 * @init_draw_cmd: method to initialize a command buffer
 * @init_pipeline: method to initialize a pipeline
 */
struct _GulkanSwapchainRendererClass
{
  GulkanRendererClass parent;

  void (*init_draw_cmd) (GulkanSwapchainRenderer *self,
                         VkCommandBuffer          cmd_buffer);

  gboolean (*init_pipeline) (GulkanSwapchainRenderer *self, gconstpointer data);
};

GulkanRenderPass *
gulkan_swapchain_renderer_get_render_pass (GulkanSwapchainRenderer *self);

void
gulkan_swapchain_renderer_initialize (GulkanSwapchainRenderer *self,
                                      VkClearColorValue        clear_color,
                                      gconstpointer            pipeline_data);

gboolean
gulkan_swapchain_renderer_resize (GulkanSwapchainRenderer *self,
                                  VkSurfaceKHR             surface,
                                  VkExtent2D               extent);

gboolean
gulkan_swapchain_renderer_init_draw_cmd_buffers (GulkanSwapchainRenderer *self);

G_END_DECLS

#endif /* GULKAN_SWAPCHAIN_RENDERER_H_ */
