/*
 * gulkan
 * Copyright 2022 Lubosz Sarnecki
 * SPDX-License-Identifier: MIT
 */

#include "gulkan-window-wayland.h"

#include <errno.h>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "gulkan-context.h"
#include "xdg-shell-client-protocol.h"

struct _GulkanWindowWayland
{
  GulkanWindow parent;

  struct wl_display    *display;
  struct wl_compositor *compositor;
  struct wl_keyboard   *keyboard;
  struct wl_pointer    *pointer;
  struct wl_seat       *seat;
  struct wl_surface    *surface;

  struct xdg_wm_base  *wm_base;
  struct xdg_surface  *surface_xdg;
  struct xdg_toplevel *toplevel;

  struct xkb_context *xkb;
  struct xkb_keymap  *key_map;
  struct xkb_state   *key_state;

  gboolean is_fullscreen;
};

G_DEFINE_TYPE (GulkanWindowWayland, gulkan_window_wayland, GULKAN_TYPE_WINDOW)

enum
{
  CONFIGURE_EVENT,
  POINTER_POSITION_EVENT,
  POINTER_BUTTON_EVENT,
  POINTER_AXIS_EVENT,
  KEY_EVENT,
  CLOSE_EVENT,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

static void
gulkan_window_wayland_init (GulkanWindowWayland *self)
{
  self->is_fullscreen = FALSE;
  self->surface_xdg = NULL;
  self->toplevel = NULL;
  self->wm_base = NULL;
  self->keyboard = NULL;
  self->pointer = NULL;
  self->seat = NULL;
  self->surface = NULL;
  self->compositor = NULL;
  self->display = NULL;
  self->xkb = NULL;
  self->key_map = NULL;
  self->key_state = NULL;
}

static void
_finalize (GObject *gobject)
{
  GulkanWindowWayland *self = GULKAN_WINDOW_WAYLAND (gobject);
  if (self->surface_xdg)
    xdg_surface_destroy (self->surface_xdg);
  if (self->toplevel)
    xdg_toplevel_destroy (self->toplevel);
  if (self->wm_base)
    xdg_wm_base_destroy (self->wm_base);
  if (self->keyboard)
    wl_keyboard_destroy (self->keyboard);
  if (self->pointer)
    wl_pointer_destroy (self->pointer);
  if (self->seat)
    wl_seat_destroy (self->seat);
  if (self->surface)
    wl_surface_destroy (self->surface);
  if (self->compositor)
    wl_compositor_destroy (self->compositor);
  if (self->display)
    wl_display_disconnect (self->display);
  if (self->xkb)
    xkb_context_unref (self->xkb);
  if (self->key_map)
    xkb_keymap_unref (self->key_map);
  if (self->key_state)
    xkb_state_unref (self->key_state);
  G_OBJECT_CLASS (gulkan_window_wayland_parent_class)->finalize (gobject);
}

static void
_keyboard_keymap_cb (void               *data,
                     struct wl_keyboard *keyboard,
                     uint32_t            format,
                     int                 fd,
                     uint32_t            size)
{
  (void) keyboard;

  GulkanWindowWayland *self = GULKAN_WINDOW_WAYLAND (data);

  g_assert (format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1);

  char *map_shm = mmap (NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  g_assert (map_shm != MAP_FAILED);

  self->key_map = xkb_keymap_new_from_string (self->xkb, map_shm,
                                              XKB_KEYMAP_FORMAT_TEXT_V1,
                                              XKB_KEYMAP_COMPILE_NO_FLAGS);
  munmap (map_shm, size);
  close (fd);

  self->key_state = xkb_state_new (self->key_map);

  if (self->key_state)
    return;
}

static VkResult
_create_surface (GulkanWindow *window,
                 VkInstance    instance,
                 VkSurfaceKHR *surface)
{
  GulkanWindowWayland          *self = GULKAN_WINDOW_WAYLAND (window);
  VkWaylandSurfaceCreateInfoKHR info = {
    .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
    .display = self->display,
    .surface = self->surface,
  };
  return vkCreateWaylandSurfaceKHR (instance, &info, NULL, surface);
}

static void
_toggle_fullscreen (GulkanWindow *window)
{
  GulkanWindowWayland *self = GULKAN_WINDOW_WAYLAND (window);

  if (self->is_fullscreen)
    {
      xdg_toplevel_unset_fullscreen (self->toplevel);
      self->is_fullscreen = FALSE;
    }
  else
    {
      xdg_toplevel_set_fullscreen (self->toplevel, NULL);
      self->is_fullscreen = TRUE;
    }

  wl_surface_commit (self->surface);
}

static GSList *
_required_extensions ()
{
  const gchar *exts[] = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
  };

  GSList *list = NULL;
  for (uint32_t i = 0; i < G_N_ELEMENTS (exts); i++)
    {
      list = g_slist_append (list, g_strdup (exts[i]));
    }

  return list;
}

static gboolean
_has_support (GulkanWindow *window, GulkanContext *context)
{
  GulkanWindowWayland *self = GULKAN_WINDOW_WAYLAND (window);
  VkPhysicalDevice     physical_device
    = gulkan_context_get_physical_device_handle (context);
  return (gboolean)
    vkGetPhysicalDeviceWaylandPresentationSupportKHR (physical_device, 0,
                                                      self->display);
}

static void
_poll_events (GulkanWindow *window)
{
  GulkanWindowWayland *self = GULKAN_WINDOW_WAYLAND (window);

  while (wl_display_prepare_read (self->display) != 0)
    wl_display_dispatch_pending (self->display);
  if (wl_display_flush (self->display) < 0 && errno != EAGAIN)
    {
      wl_display_cancel_read (self->display);
      return;
    }

  struct pollfd fds[] = {
    {
      .fd = wl_display_get_fd (self->display),
      .events = POLLIN,
      .revents = 0,
    },
  };
  if (poll (fds, 1, 0) > 0)
    {
      wl_display_read_events (self->display);
      wl_display_dispatch_pending (self->display);
    }
  else
    {
      wl_display_cancel_read (self->display);
    }
}

static void
_pointer_motion_cb (void              *data,
                    struct wl_pointer *pointer,
                    uint32_t           time,
                    wl_fixed_t         x,
                    wl_fixed_t         y)
{
  (void) pointer;
  (void) time;

  GulkanWindowWayland *self = GULKAN_WINDOW_WAYLAND (data);

  GulkanPositionEvent event = {
    .offset = {
      .x = wl_fixed_to_int (x),
      .y = wl_fixed_to_int (y),
    },
  };
  g_signal_emit (self, signals[POINTER_POSITION_EVENT], 0, &event);
}

static void
_pointer_button_cb (void              *data,
                    struct wl_pointer *pointer,
                    uint32_t           serial,
                    uint32_t           time,
                    uint32_t           button,
                    uint32_t           state)
{
  (void) pointer;
  (void) time;
  (void) serial;

  GulkanWindowWayland *self = GULKAN_WINDOW_WAYLAND (data);

  GulkanButtonEvent event = {
    .button = button,
    .is_pressed = state == 1,
  };

  g_signal_emit (self, signals[POINTER_BUTTON_EVENT], 0, &event);
}

static void
_pointer_axis_cb (void              *data,
                  struct wl_pointer *pointer,
                  uint32_t           time,
                  uint32_t           axis,
                  wl_fixed_t         value)
{
  (void) pointer;
  (void) time;

  GulkanAxisEvent event = {
    .axis = axis,
    .value = wl_fixed_to_int (value),
  };

  GulkanWindowWayland *self = GULKAN_WINDOW_WAYLAND (data);
  g_signal_emit (self, signals[POINTER_AXIS_EVENT], 0, &event);
}

static void
_keyboard_key_cb (void               *data,
                  struct wl_keyboard *keyboard,
                  uint32_t            serial,
                  uint32_t            time,
                  uint32_t            key,
                  uint32_t            state)
{
  (void) keyboard;
  (void) time;
  (void) serial;

  GulkanWindowWayland *self = GULKAN_WINDOW_WAYLAND (data);

  xkb_keycode_t code = key + 8;

  xkb_keysym_t sym = xkb_state_key_get_one_sym (self->key_state, code);
  g_debug ("keyboard_key: key %d sym: %d state: %d\n", key, sym, state);
  GulkanKeyEvent event = {
    .key = sym,
    .is_pressed = state == 1,
  };

  g_signal_emit (self, signals[KEY_EVENT], 0, &event);
}

// Unused pointer callbacks
static void
_pointer_enter_cb (void              *data,
                   struct wl_pointer *pointer,
                   uint32_t           serial,
                   struct wl_surface *surface,
                   wl_fixed_t         surface_x,
                   wl_fixed_t         surface_y)
{
  (void) data;
  (void) pointer;
  (void) serial;
  (void) surface;
  (void) surface_x;
  (void) surface_y;
}

static void
_pointer_leave_cb (void              *data,
                   struct wl_pointer *pointer,
                   uint32_t           serial,
                   struct wl_surface *surface)
{
  (void) data;
  (void) pointer;
  (void) serial;
  (void) surface;
}

static const struct wl_pointer_listener pointer_listener = {
  .enter = _pointer_enter_cb,
  .leave = _pointer_leave_cb,
  .motion = _pointer_motion_cb,
  .button = _pointer_button_cb,
  .axis = _pointer_axis_cb,
};

// Unused keyboard callbacks. They need to be initialized, otherwise we will get
// errors like:
// listener function for opcode 0 of wl_pointer is NULL
// Aborted (core dumped)
static void
_keyboard_enter_cb (void               *data,
                    struct wl_keyboard *keyboard,
                    uint32_t            serial,
                    struct wl_surface  *surface,
                    struct wl_array    *keys)
{
  (void) data;
  (void) keyboard;
  (void) serial;
  (void) surface;
  (void) keys;
}

static void
_keyboard_leave_cb (void               *data,
                    struct wl_keyboard *keyboard,
                    uint32_t            serial,
                    struct wl_surface  *surface)
{
  (void) data;
  (void) keyboard;
  (void) serial;
  (void) surface;
}

static void
_keyboard_modifiers_cb (void               *data,
                        struct wl_keyboard *keyboard,
                        uint32_t            serial,
                        uint32_t            mods_depressed,
                        uint32_t            mods_latched,
                        uint32_t            mods_locked,
                        uint32_t            group)
{
  (void) data;
  (void) keyboard;
  (void) serial;
  (void) mods_depressed;
  (void) mods_latched;
  (void) mods_locked;
  (void) group;
}

static void
_keyboard_repeat_info_cb (void               *data,
                          struct wl_keyboard *keyboard,
                          int32_t             rate,
                          int32_t             delay)
{
  (void) data;
  (void) keyboard;
  (void) rate;
  (void) delay;
}

static const struct wl_keyboard_listener keyboard_listener = {
  .keymap = _keyboard_keymap_cb,
  .enter = _keyboard_enter_cb,
  .leave = _keyboard_leave_cb,
  .key = _keyboard_key_cb,
  .modifiers = _keyboard_modifiers_cb,
  .repeat_info = _keyboard_repeat_info_cb,
};

static void
_seat_capabilities_cb (void *data, struct wl_seat *seat, uint32_t caps)
{
  GulkanWindowWayland *self = GULKAN_WINDOW_WAYLAND (data);
  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !self->pointer)
    {
      self->pointer = wl_seat_get_pointer (seat);
      wl_pointer_add_listener (self->pointer, &pointer_listener, self);
    }
  else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && self->pointer)
    {
      wl_pointer_destroy (self->pointer);
      self->pointer = NULL;
    }

  if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !self->keyboard)
    {
      self->keyboard = wl_seat_get_keyboard (seat);
      wl_keyboard_add_listener (self->keyboard, &keyboard_listener, self);
    }
  else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && self->keyboard)
    {
      wl_keyboard_destroy (self->keyboard);
      self->keyboard = NULL;
    }
}

static const struct wl_seat_listener seat_listener = {
  .capabilities = _seat_capabilities_cb,
};

static void
_xdg_wm_base_ping_cb (void *data, struct xdg_wm_base *wm_base, uint32_t serial)
{
  (void) data;
  xdg_wm_base_pong (wm_base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
  _xdg_wm_base_ping_cb,
};

static void
_registry_global_cb (void               *data,
                     struct wl_registry *registry,
                     uint32_t            name,
                     const char         *interface,
                     uint32_t            version)
{
  GulkanWindowWayland *self = GULKAN_WINDOW_WAYLAND (data);
  g_debug ("wayland registry: Interface %s Version %d", interface, version);
  if (strcmp (interface, "wl_compositor") == 0)
    {
      self->compositor = (struct wl_compositor *)
        wl_registry_bind (registry, name, &wl_compositor_interface,
                          MIN (5, version));
    }
  else if (strcmp (interface, "wl_seat") == 0)
    {
      self->seat = (struct wl_seat *) wl_registry_bind (registry, name,
                                                        &wl_seat_interface, 1);
      wl_seat_add_listener (self->seat, &seat_listener, self);
    }
  else if (strcmp (interface, "xdg_wm_base") == 0)
    {
      self->wm_base = (struct xdg_wm_base *)
        wl_registry_bind (registry, name, &xdg_wm_base_interface, 1);
      xdg_wm_base_add_listener (self->wm_base, &wm_base_listener, self);
    }
}

static const struct wl_registry_listener registry_listener = {
  .global = _registry_global_cb,
};

static void
_xdg_toplevel_configure_cb (void                *data,
                            struct xdg_toplevel *toplevel,
                            int32_t              width,
                            int32_t              height,
                            struct wl_array     *states)
{
  (void) toplevel;
  (void) states;

  GulkanWindowWayland *self = GULKAN_WINDOW_WAYLAND (data);

  GulkanConfigureEvent event = {
    .extent = {.width = (uint32_t) width, .height = (uint32_t) height},
  };
  g_signal_emit (self, signals[CONFIGURE_EVENT], 0, &event);
}

static void
_xdg_toplevel_close_cb (void *data, struct xdg_toplevel *toplevel)
{
  (void) toplevel;

  GulkanWindowWayland *self = GULKAN_WINDOW_WAYLAND (data);
  g_signal_emit (self, signals[CLOSE_EVENT], 0);
}

static const struct xdg_toplevel_listener toplevel_listener = {
  .configure = _xdg_toplevel_configure_cb,
  .close = _xdg_toplevel_close_cb,
};

static void
_xdg_surface_configure_cb (void               *data,
                           struct xdg_surface *surface,
                           uint32_t            serial)
{
  (void) data;
  xdg_surface_ack_configure (surface, serial);
}

static const struct xdg_surface_listener surface_listener_xdg = {
  .configure = _xdg_surface_configure_cb,
};

static gboolean
_initialize (GulkanWindow *window, VkExtent2D extent, const char *title)
{
  // Wayland gets the extent from the swapchain and does not store it on the
  // surface. Even if we set xdg_toplevel_set_min_size or
  // xdg_surface_set_window_geometry
  (void) extent;

  GulkanWindowWayland *self = GULKAN_WINDOW_WAYLAND (window);

  struct wl_registry *registry = wl_display_get_registry (self->display);
  if (!registry)
    {
      g_printerr ("Could not get Wayland registry.\n");
      return FALSE;
    }

  wl_registry_add_listener (registry, &registry_listener, self);

  wl_display_roundtrip (self->display);

  if (!self->compositor || !self->wm_base || !self->seat)
    {
      g_printerr ("Could not bind Wayland protocols.\n");
      return FALSE;
    }

  wl_registry_destroy (registry);

  self->surface = wl_compositor_create_surface (self->compositor);

  self->surface_xdg = xdg_wm_base_get_xdg_surface (self->wm_base,
                                                   self->surface);

  xdg_surface_add_listener (self->surface_xdg, &surface_listener_xdg, self);
  self->toplevel = xdg_surface_get_toplevel (self->surface_xdg);

  xdg_toplevel_add_listener (self->toplevel, &toplevel_listener, self);

  xdg_toplevel_set_app_id (self->toplevel, "gulkan");
  xdg_toplevel_set_title (self->toplevel, title);

  wl_surface_commit (self->surface);

  // Keyboard stuff
  self->xkb = xkb_context_new (XKB_CONTEXT_NO_FLAGS);
  if (!self->xkb)
    return FALSE;

  return TRUE;
}

static gboolean
_can_run (GulkanWindow *window)
{
  GulkanWindowWayland *self = GULKAN_WINDOW_WAYLAND (window);
  self->display = wl_display_connect (NULL);
  if (!self->display)
    {
      g_printerr ("window-wayland: Could not connect to Wayland display.\n");
      return FALSE;
    }
  return TRUE;
}

static void
gulkan_window_wayland_class_init (GulkanWindowWaylandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = _finalize;

  GulkanWindowClass *parent_class = GULKAN_WINDOW_CLASS (klass);
  parent_class->initialize = _initialize;
  parent_class->create_surface = _create_surface;
  parent_class->required_extensions = _required_extensions;
  parent_class->poll_events = _poll_events;
  parent_class->toggle_fullscreen = _toggle_fullscreen;
  parent_class->has_support = _has_support;
  parent_class->can_run = _can_run;

  signals[CONFIGURE_EVENT] = g_signal_new ("configure",
                                           G_TYPE_FROM_CLASS (klass),
                                           G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                           NULL, G_TYPE_NONE, 1,
                                           G_TYPE_POINTER
                                             | G_SIGNAL_TYPE_STATIC_SCOPE);

  signals[POINTER_POSITION_EVENT]
    = g_signal_new ("pointer-position", G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1,
                    G_TYPE_POINTER | G_SIGNAL_TYPE_STATIC_SCOPE);

  signals[POINTER_BUTTON_EVENT] = g_signal_new ("pointer-button",
                                                G_TYPE_FROM_CLASS (klass),
                                                G_SIGNAL_RUN_LAST, 0, NULL,
                                                NULL, NULL, G_TYPE_NONE, 1,
                                                G_TYPE_POINTER
                                                  | G_SIGNAL_TYPE_STATIC_SCOPE);

  signals[POINTER_AXIS_EVENT] = g_signal_new ("pointer-axis",
                                              G_TYPE_FROM_CLASS (klass),
                                              G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                              NULL, G_TYPE_NONE, 1,
                                              G_TYPE_POINTER
                                                | G_SIGNAL_TYPE_STATIC_SCOPE);

  signals[KEY_EVENT] = g_signal_new ("key", G_TYPE_FROM_CLASS (klass),
                                     G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                                     G_TYPE_NONE, 1,
                                     G_TYPE_POINTER
                                       | G_SIGNAL_TYPE_STATIC_SCOPE);

  signals[CLOSE_EVENT] = g_signal_new ("close", G_TYPE_FROM_CLASS (klass),
                                       G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL,
                                       G_TYPE_NONE, 0);
}
