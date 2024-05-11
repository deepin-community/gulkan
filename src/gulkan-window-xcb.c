/*
 * gulkan
 * Copyright 2022 Lubosz Sarnecki
 * SPDX-License-Identifier: MIT
 */

#include "gulkan-window-xcb.h"

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xcb.h>

#include "gulkan-context.h"

struct _GulkanWindowXcb
{
  GulkanWindow parent;

  xcb_connection_t  *connection;
  xcb_window_t       window;
  xcb_key_symbols_t *syms;
  xcb_screen_t      *screen;

  xcb_atom_t atom_wm_protocols;
  xcb_atom_t atom_wm_delete_window;

  VkExtent2D last_extent;
};

G_DEFINE_TYPE (GulkanWindowXcb, gulkan_window_xcb, GULKAN_TYPE_WINDOW)

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
gulkan_window_xcb_init (GulkanWindowXcb *self)
{
  self->connection = NULL;
  self->window = XCB_NONE;
  self->syms = NULL;
  self->screen = NULL;
}

static void
_finalize (GObject *gobject)
{
  GulkanWindowXcb *self = GULKAN_WINDOW_XCB (gobject);
  xcb_destroy_window (self->connection, self->window);
  xcb_disconnect (self->connection);
  xcb_key_symbols_free (self->syms);
  G_OBJECT_CLASS (gulkan_window_xcb_parent_class)->finalize (gobject);
}

static xcb_atom_t
_get_atom (GulkanWindowXcb *self, const char *name)
{
  xcb_intern_atom_cookie_t cookie;
  xcb_intern_atom_reply_t *reply;
  xcb_atom_t               atom;

  cookie = xcb_intern_atom (self->connection, 0, (uint16_t) strlen (name),
                            name);
  reply = xcb_intern_atom_reply (self->connection, cookie, NULL);
  if (reply)
    atom = reply->atom;
  else
    atom = XCB_NONE;

  free (reply);
  return atom;
}

static VkResult
_create_surface (GulkanWindow *window,
                 VkInstance    instance,
                 VkSurfaceKHR *surface)
{
  GulkanWindowXcb *self = GULKAN_WINDOW_XCB (window);

  VkXcbSurfaceCreateInfoKHR info = {
    .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
    .connection = self->connection,
    .window = self->window,
  };
  return vkCreateXcbSurfaceKHR (instance, &info, NULL, surface);
}

static xcb_intern_atom_cookie_t
_get_cookie (GulkanWindowXcb *self, const char *name)
{
  return xcb_intern_atom (self->connection, 0, (uint16_t) strlen (name), name);
}

static xcb_atom_t
_get_reply (GulkanWindowXcb *self, xcb_intern_atom_cookie_t cookie)
{
  xcb_generic_error_t     *error;
  xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply (self->connection,
                                                          cookie, &error);
  if (error)
    {
      g_printerr ("xcb_intern_atom_reply failed with %d\n", error->error_code);
    }
  return reply->atom;
}

#if !defined(_NET_WM_STATE_TOGGLE)
#define _NET_WM_STATE_TOGGLE 2
#endif

static void
_toggle_fullscreen (GulkanWindow *window)
{
  GulkanWindowXcb *self = GULKAN_WINDOW_XCB (window);

  xcb_intern_atom_cookie_t wm_state_ck = _get_cookie (self, "_NET_WM_STATE");
  xcb_intern_atom_cookie_t wm_state_fs_ck
    = _get_cookie (self, "_NET_WM_STATE_FULLSCREEN");

  xcb_client_message_event_t ev = {
    .response_type = XCB_CLIENT_MESSAGE,
    .type = _get_reply (self, wm_state_ck),
    .format = 32,
    .window = self->window,
    .data = {
      .data32 = {
        _NET_WM_STATE_TOGGLE,
        _get_reply (self, wm_state_fs_ck),
        XCB_ATOM_NONE,
        0,
        0,
      },
    },
  };

  xcb_send_event (self->connection, 1, self->window,
                  XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
                    | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
                  (const char *) &ev);
}

static GSList *
_required_extensions ()
{
  const gchar *exts[] = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_KHR_XCB_SURFACE_EXTENSION_NAME,
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
  GulkanWindowXcb *self = GULKAN_WINDOW_XCB (window);
  VkPhysicalDevice physical_device
    = gulkan_context_get_physical_device_handle (context);
  return (gboolean)
    vkGetPhysicalDeviceXcbPresentationSupportKHR (physical_device, 0,
                                                  self->connection,
                                                  self->screen->root_visual);
}

static gboolean
_can_run (GulkanWindow *window)
{
  GulkanWindowXcb *self = GULKAN_WINDOW_XCB (window);

  self->connection = xcb_connect (NULL, NULL);
  if (self->connection == NULL || xcb_connection_has_error (self->connection))
    {
      g_printerr ("window-xcb: Could not initialize connection.\n");
      return FALSE;
    }
  return TRUE;
}

static void
_update_window_title (GulkanWindowXcb *self, const char *title)
{
  xcb_change_property (self->connection, XCB_PROP_MODE_REPLACE, self->window,
                       XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                       (uint32_t) strlen (title), title);
}

static void
_handle_motion_notify (GulkanWindowXcb                 *self,
                       const xcb_motion_notify_event_t *xcb_event)
{
  GulkanPositionEvent event = {
    .offset = {
      .x = xcb_event->event_x,
      .y = xcb_event->event_y,
    },
  };
  g_signal_emit (self, signals[POINTER_POSITION_EVENT], 0, &event);
}

static void
_handle_client_message (GulkanWindowXcb                  *self,
                        const xcb_client_message_event_t *xcb_event)
{
  if (xcb_event->type == self->atom_wm_protocols
      && xcb_event->data.data32[0] == self->atom_wm_delete_window)
    {
      g_signal_emit (self, signals[CLOSE_EVENT], 0);
    }
}

// The XCB ancients forgot about these
#if !defined(XCB_BUTTON_INDEX_6)
#define XCB_BUTTON_INDEX_6 6
#endif
#if !defined(XCB_BUTTON_INDEX_7)
#define XCB_BUTTON_INDEX_7 7
#endif
#if !defined(XCB_BUTTON_INDEX_8)
#define XCB_BUTTON_INDEX_8 8
#endif
#if !defined(XCB_BUTTON_INDEX_9)
#define XCB_BUTTON_INDEX_9 9
#endif

static uint32_t
_x11_to_linux_button (xcb_button_t xcb_button)
{
  switch (xcb_button)
    {
      case XCB_BUTTON_INDEX_1:
        return BTN_LEFT;
      case XCB_BUTTON_INDEX_2:
        return BTN_MIDDLE;
      case XCB_BUTTON_INDEX_3:
        return BTN_RIGHT;
      // From here on there won't be enums (but dragons)
      case XCB_BUTTON_INDEX_8:
        return BTN_SIDE;
      case XCB_BUTTON_INDEX_9:
        return BTN_EXTRA;
      default:
        return UINT32_MAX;
    }
}

static void
_handle_button_press (GulkanWindowXcb                *self,
                      const xcb_button_press_event_t *xcb_event)
{
  switch (xcb_event->detail)
    {
      case XCB_BUTTON_INDEX_1:
      case XCB_BUTTON_INDEX_2:
      case XCB_BUTTON_INDEX_3:
      case XCB_BUTTON_INDEX_8:
      case XCB_BUTTON_INDEX_9:
        {
          GulkanButtonEvent event = {
            .button = _x11_to_linux_button (xcb_event->detail),
            .is_pressed = TRUE,
          };
          g_signal_emit (self, signals[POINTER_BUTTON_EVENT], 0, &event);
          break;
        }
      // When the ancients blessed us with XCB, mice had no wheels.
      case XCB_BUTTON_INDEX_4:
      case XCB_BUTTON_INDEX_5:
        {
          GulkanAxisEvent event = {
            .axis = 0,
            .value = xcb_event->detail == XCB_BUTTON_INDEX_4 ? -10 : 10,
          };
          g_signal_emit (self, signals[POINTER_AXIS_EVENT], 0, &event);
          break;
        }
      case XCB_BUTTON_INDEX_6:
      case XCB_BUTTON_INDEX_7:
        {
          GulkanAxisEvent event = {
            .axis = 1,
            .value = xcb_event->detail == XCB_BUTTON_INDEX_6 ? -10 : 10,
          };
          g_signal_emit (self, signals[POINTER_AXIS_EVENT], 0, &event);
          break;
        }
      default:
        break;
    }
}

static void
_handle_button_release (GulkanWindowXcb                  *self,
                        const xcb_button_release_event_t *xcb_event)
{
  switch (xcb_event->detail)
    {
      case XCB_BUTTON_INDEX_1:
      case XCB_BUTTON_INDEX_2:
      case XCB_BUTTON_INDEX_3:
      case XCB_BUTTON_INDEX_8:
      case XCB_BUTTON_INDEX_9:
        {
          GulkanButtonEvent event = {
            .button = _x11_to_linux_button (xcb_event->detail),
            .is_pressed = FALSE,
          };
          g_signal_emit (self, signals[POINTER_BUTTON_EVENT], 0, &event);
          break;
        }
      default:
        break;
    }
}

static void
_handle_key_press (GulkanWindowXcb             *self,
                   const xcb_key_press_event_t *xcb_event)
{
  GulkanKeyEvent event = {
    .key = xcb_key_symbols_get_keysym (self->syms, xcb_event->detail, 0),
    .is_pressed = TRUE,
  };
  g_signal_emit (self, signals[KEY_EVENT], 0, &event);
}

static void
_handle_key_release (GulkanWindowXcb               *self,
                     const xcb_key_release_event_t *xcb_event)
{
  GulkanKeyEvent event = {
    .key = xcb_key_symbols_get_keysym (self->syms, xcb_event->detail, 0),
    .is_pressed = FALSE,
  };
  g_signal_emit (self, signals[KEY_EVENT], 0, &event);
}

static void
_handle_configure (GulkanWindowXcb                    *self,
                   const xcb_configure_notify_event_t *xcb_event)
{
  // Don't repeat configure events when extent did not change.
  if (self->last_extent.width == xcb_event->width
      && self->last_extent.height == xcb_event->height)
    {
      return;
    }

  GulkanConfigureEvent event = {
    .extent = {
      .width = xcb_event->width,
      .height = xcb_event->height,
    },
  };
  self->last_extent = event.extent;

  g_signal_emit (self, signals[CONFIGURE_EVENT], 0, &event);
}

static void
_handle_event (GulkanWindowXcb *self, const xcb_generic_event_t *event)
{
  switch (event->response_type & 0x7f)
    {
      case XCB_CLIENT_MESSAGE:
        _handle_client_message (self,
                                (const xcb_client_message_event_t *) event);
        break;
      case XCB_MOTION_NOTIFY:
        _handle_motion_notify (self, (xcb_motion_notify_event_t *) event);
        break;
      case XCB_BUTTON_PRESS:
        _handle_button_press (self, (xcb_button_press_event_t *) event);
        break;
      case XCB_BUTTON_RELEASE:
        _handle_button_release (self, (xcb_button_release_event_t *) event);
        break;
      case XCB_KEY_PRESS:
        _handle_key_press (self, (xcb_key_press_event_t *) event);
        break;
      case XCB_KEY_RELEASE:
        _handle_key_release (self, (xcb_key_release_event_t *) event);
        break;
      case XCB_DESTROY_NOTIFY:
        g_signal_emit (self, signals[CLOSE_EVENT], 0);
        break;
      case XCB_CONFIGURE_NOTIFY:
        _handle_configure (self, (const xcb_configure_notify_event_t *) event);
        break;
      default:
        break;
    }
}

static void
_poll_events (GulkanWindow *window)
{
  GulkanWindowXcb     *self = GULKAN_WINDOW_XCB (window);
  xcb_generic_event_t *event;
  while ((event = xcb_poll_for_event (self->connection)))
    {
      _handle_event (self, event);
      free (event);
    }
}

static gboolean
_initialize (GulkanWindow *window, VkExtent2D extent, const char *title)
{
  GulkanWindowXcb *self = GULKAN_WINDOW_XCB (window);

  xcb_screen_iterator_t iter
    = xcb_setup_roots_iterator (xcb_get_setup (self->connection));

  self->screen = iter.data;
  self->syms = xcb_key_symbols_alloc (self->connection);

  self->window = xcb_generate_id (self->connection);

  uint32_t window_values[1] = {
    XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_RELEASE
      | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_STRUCTURE_NOTIFY
      | XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_PRESS
      | XCB_EVENT_MASK_BUTTON_RELEASE,
  };

  xcb_create_window (self->connection, XCB_COPY_FROM_PARENT, self->window,
                     self->screen->root, 0, 0, (uint16_t) extent.width,
                     (uint16_t) extent.height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                     self->screen->root_visual, XCB_CW_EVENT_MASK,
                     window_values);

  self->atom_wm_protocols = _get_atom (self, "WM_PROTOCOLS");
  self->atom_wm_delete_window = _get_atom (self, "WM_DELETE_WINDOW");

  xcb_change_property (self->connection, XCB_PROP_MODE_REPLACE, self->window,
                       self->atom_wm_protocols, XCB_ATOM_ATOM, 32, 1,
                       &self->atom_wm_delete_window);

  _update_window_title (self, title);

  xcb_map_window (self->connection, self->window);

  return TRUE;
}

static void
gulkan_window_xcb_class_init (GulkanWindowXcbClass *klass)
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
