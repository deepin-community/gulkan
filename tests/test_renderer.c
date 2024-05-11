/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <gulkan.h>

#include "../examples/common/common.h"

static void
_test_without_context ()
{
  GulkanRenderer *renderer = (GulkanRenderer *)
    g_object_new (GULKAN_TYPE_RENDERER, 0);
  g_object_unref (renderer);

  GulkanSwapchainRenderer *swapchain_renderer = (GulkanSwapchainRenderer *)
    g_object_new (GULKAN_TYPE_SWAPCHAIN_RENDERER, 0);
  g_object_unref (swapchain_renderer);
}

static void
_test_with_context ()
{
  GulkanContext  *context = gulkan_context_new ();
  GulkanRenderer *renderer = (GulkanRenderer *)
    g_object_new (GULKAN_TYPE_RENDERER, 0);
  gulkan_renderer_set_context (renderer, context);
  g_object_unref (renderer);

  context = gulkan_context_new ();
  GulkanSwapchainRenderer *swapchain_renderer = (GulkanSwapchainRenderer *)
    g_object_new (GULKAN_TYPE_SWAPCHAIN_RENDERER, 0);
  gulkan_renderer_set_context (GULKAN_RENDERER (swapchain_renderer), context);
  g_object_unref (swapchain_renderer);
  g_object_unref (context);
}

static void
_test_with_init ()
{
  VkExtent2D    extent = {100, 100};
  GulkanWindow *window = gulkan_window_new (extent, "Test");
  g_assert_nonnull (window);

  GSList *instance_ext_list = gulkan_window_required_extensions (window);

  const gchar *device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };

  GSList *device_ext_list = NULL;
  for (uint64_t i = 0; i < G_N_ELEMENTS (device_extensions); i++)
    {
      char *device_ext = g_strdup (device_extensions[i]);
      device_ext_list = g_slist_append (device_ext_list, device_ext);
    }

  GulkanContext *context
    = gulkan_context_new_from_extensions (instance_ext_list, device_ext_list,
                                          VK_NULL_HANDLE);

  g_assert (gulkan_window_has_support (window, context));

  g_slist_free (instance_ext_list);
  g_slist_free (device_ext_list);

  GulkanSwapchainRenderer *swapchain_renderer = (GulkanSwapchainRenderer *)
    g_object_new (GULKAN_TYPE_SWAPCHAIN_RENDERER, 0);
  gulkan_renderer_set_context (GULKAN_RENDERER (swapchain_renderer), context);

  g_object_unref (swapchain_renderer);
  g_object_unref (context);
  g_object_unref (window);
}

int
main ()
{
  _test_without_context ();
  _test_with_context ();
  _test_with_init ();
  return 0;
}
