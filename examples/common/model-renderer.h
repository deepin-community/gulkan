/*
 * gulkan
 * Copyright 2020 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef MODEL_RENDERER_H_
#define MODEL_RENDERER_H_

#include <glib-object.h>
#include <gulkan.h>

G_BEGIN_DECLS

#define MODEL_TYPE_RENDERER model_renderer_get_type()
G_DECLARE_DERIVABLE_TYPE (ModelRenderer, model_renderer,
                          MODEL, RENDERER, GulkanSwapchainRenderer)

struct _ModelRendererClass
{
  GulkanSwapchainRendererClass parent;

  void
  (*init_draw_cmd) (ModelRenderer  *self,
                    VkCommandBuffer cmd_buffer);
};

G_END_DECLS

typedef struct
{
  float mv_matrix[16];
  float mvp_matrix[16];
  float normal_matrix[12];
} Transformation;

typedef struct {
  const gchar *vert;
  const gchar *frag;
} ShaderResources;

gboolean
model_renderer_initialize (ModelRenderer    *self,
                           VkSurfaceKHR      surface,
                           VkClearColorValue clear_color,
                           ShaderResources  *resources);

void
model_renderer_update_ubo (ModelRenderer *self, gpointer ubo);

#endif /* MODEL_RENDERER_H_ */
