/*
 * gulkan
 * Copyright 2022 Lubosz Sarnecki
 * SPDX-License-Identifier: MIT
 */

#include "gulkan-window.h"

#if defined(GULKAN_HAVE_WAYLAND)
#include "gulkan-window-wayland.h"
#endif

#if defined(GULKAN_HAVE_XCB)
#include "gulkan-window-xcb.h"
#endif

typedef struct _GulkanWindowPrivate
{
  GObject parent;
} GulkanWindowPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GulkanWindow, gulkan_window, G_TYPE_OBJECT)

static gboolean
_initialize (GulkanWindow *self, VkExtent2D extent, const char *title)
{
  GulkanWindowClass *klass = GULKAN_WINDOW_GET_CLASS (self);
  if (klass->initialize == NULL)
    return FALSE;
  return klass->initialize (self, extent, title);
}

static GulkanWindow *
_new_auto (VkExtent2D extent, const char *title)
{
#if defined(GULKAN_HAVE_WAYLAND)
  {
    GType         t = GULKAN_TYPE_WINDOW_WAYLAND;
    GulkanWindow *self = GULKAN_WINDOW (g_object_new (t, 0));
    if (gulkan_window_can_run (self) && _initialize (self, extent, title))
      {
        return self;
      }
    g_object_unref (self);
  }
#endif

#if defined(GULKAN_HAVE_XCB)
  {
    GType         t = GULKAN_TYPE_WINDOW_XCB;
    GulkanWindow *self = GULKAN_WINDOW (g_object_new (t, 0));
    if (gulkan_window_can_run (self) && _initialize (self, extent, title))
      {
        return self;
      }
    g_object_unref (self);
  }
#endif
  g_printerr ("ERROR: Could find a supported window backend.\n");
  return NULL;
}

GulkanWindow *
gulkan_window_new (VkExtent2D extent, const char *title)
{
  GType        type;
  const gchar *api_env = g_getenv ("GULKAN_WINDOW");
  if (g_strcmp0 (api_env, "wayland") == 0)
    {
#if defined(GULKAN_HAVE_WAYLAND)
      type = GULKAN_TYPE_WINDOW_WAYLAND;
#else
      g_printerr ("Wayland backend is not built.\n");
      return NULL;
#endif
    }
  else if (g_strcmp0 (api_env, "xcb") == 0)
    {
#if defined(GULKAN_HAVE_XCB)
      type = GULKAN_TYPE_WINDOW_XCB;
#else
      g_printerr ("XCB backend is not built.\n");
      return NULL;
#endif
    }
  else
    {
      return _new_auto (extent, title);
    }

  GulkanWindow *self = GULKAN_WINDOW (g_object_new (type, 0));
  if (gulkan_window_can_run (self) && _initialize (self, extent, title))
    {
      return self;
    }
  g_object_unref (self);
  g_printerr ("ERROR: Could find init selected backend '%s'.\n", api_env);
  return NULL;
}

static void
gulkan_window_init (GulkanWindow *self)
{
  (void) self;
}

static void
_finalize (GObject *gobject)
{
  GulkanWindow        *self = GULKAN_WINDOW (gobject);
  GulkanWindowPrivate *priv = gulkan_window_get_instance_private (self);
  (void) priv;
  G_OBJECT_CLASS (gulkan_window_parent_class)->finalize (gobject);
}

static void
gulkan_window_class_init (GulkanWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = _finalize;
}

VkResult
gulkan_window_create_surface (GulkanWindow *self,
                              VkInstance    instance,
                              VkSurfaceKHR *surface)
{
  GulkanWindowClass *klass = GULKAN_WINDOW_GET_CLASS (self);
  if (klass->create_surface == NULL)
    return VK_ERROR_UNKNOWN;
  return klass->create_surface (self, instance, surface);
}

GSList *
gulkan_window_required_extensions (GulkanWindow *self)
{
  GulkanWindowClass *klass = GULKAN_WINDOW_GET_CLASS (self);
  if (klass->required_extensions == NULL)
    return NULL;
  return klass->required_extensions ();
}

void
gulkan_window_poll_events (GulkanWindow *self)
{
  GulkanWindowClass *klass = GULKAN_WINDOW_GET_CLASS (self);
  if (klass->poll_events == NULL)
    return;
  klass->poll_events (self);
}

void
gulkan_window_toggle_fullscreen (GulkanWindow *self)
{
  GulkanWindowClass *klass = GULKAN_WINDOW_GET_CLASS (self);
  if (klass->toggle_fullscreen == NULL)
    return;
  klass->toggle_fullscreen (self);
}

gboolean
gulkan_window_has_support (GulkanWindow *self, GulkanContext *context)
{
  GulkanWindowClass *klass = GULKAN_WINDOW_GET_CLASS (self);
  if (klass->has_support == NULL)
    return FALSE;
  return klass->has_support (self, context);
}

gboolean
gulkan_window_can_run (GulkanWindow *self)
{
  GulkanWindowClass *klass = GULKAN_WINDOW_GET_CLASS (self);
  if (klass->can_run == NULL)
    return FALSE;
  return klass->can_run (self);
}
