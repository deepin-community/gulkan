/*
 * gulkan
 * Copyright 2022 Lubosz Sarnecki
 * SPDX-License-Identifier: MIT
 */

#ifndef GULKAN_WINDOW_WAYLAND_H_
#define GULKAN_WINDOW_WAYLAND_H_

#if !defined(GULKAN_INSIDE) && !defined(GULKAN_COMPILATION)
#error "Only <gulkan.h> can be included directly."
#endif

#include "gulkan-window.h"
#include <glib-object.h>
#include <vulkan/vulkan.h>

G_BEGIN_DECLS

#define GULKAN_TYPE_WINDOW_WAYLAND gulkan_window_wayland_get_type ()
G_DECLARE_FINAL_TYPE (GulkanWindowWayland,
                      gulkan_window_wayland,
                      GULKAN,
                      WINDOW_WAYLAND,
                      GulkanWindow)

G_END_DECLS

#endif /* GULKAN_WINDOW_WAYLAND_H_ */
