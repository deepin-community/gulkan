/*
 * gulkan
 * Copyright 2022 Lubosz Sarnecki
 * SPDX-License-Identifier: MIT
 */

#ifndef GULKAN_WINDOW_H_
#define GULKAN_WINDOW_H_

#if !defined(GULKAN_INSIDE) && !defined(GULKAN_COMPILATION)
#error "Only <gulkan.h> can be included directly."
#endif

#include <glib-object.h>
#include <linux/input-event-codes.h>
#include <vulkan/vulkan.h>
#include <xkbcommon/xkbcommon.h>

G_BEGIN_DECLS

#define GULKAN_TYPE_WINDOW gulkan_window_get_type ()
G_DECLARE_DERIVABLE_TYPE (GulkanWindow, gulkan_window, GULKAN, WINDOW, GObject)

typedef struct _GulkanContext GulkanContext;

typedef struct
{
  VkExtent2D extent;
} GulkanConfigureEvent;

typedef struct
{
  VkOffset2D offset;
} GulkanPositionEvent;

typedef struct
{
  xkb_keysym_t key;
  gboolean     is_pressed;
} GulkanKeyEvent;

typedef struct
{
  uint32_t button;
  gboolean is_pressed;
} GulkanButtonEvent;

typedef struct
{
  uint32_t axis;
  int32_t  value;
} GulkanAxisEvent;

struct _GulkanWindowClass
{
  GObjectClass parent;

  gboolean (*initialize) (GulkanWindow *self,
                          VkExtent2D    extent,
                          const char   *title);

  VkResult (*create_surface) (GulkanWindow *self,
                              VkInstance    instance,
                              VkSurfaceKHR *surface);

  GSList *(*required_extensions) (void);

  void (*poll_events) (GulkanWindow *self);

  void (*toggle_fullscreen) (GulkanWindow *self);

  gboolean (*has_support) (GulkanWindow *self, GulkanContext *context);

  gboolean (*can_run) (GulkanWindow *self);
};

GulkanWindow *
gulkan_window_new (VkExtent2D extent, const char *title);

VkResult
gulkan_window_create_surface (GulkanWindow *self,
                              VkInstance    instance,
                              VkSurfaceKHR *surface);

GSList *
gulkan_window_required_extensions (GulkanWindow *self);

void
gulkan_window_poll_events (GulkanWindow *self);

void
gulkan_window_toggle_fullscreen (GulkanWindow *self);

gboolean
gulkan_window_has_support (GulkanWindow *self, GulkanContext *context);

gboolean
gulkan_window_can_run (GulkanWindow *self);

G_END_DECLS

#endif /* GULKAN_WINDOW_H_ */
