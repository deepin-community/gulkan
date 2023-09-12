/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <gulkan.h>

#include "../examples/common/common.h"

static void
_test_without_client ()
{
  GulkanRenderer *renderer =
    (GulkanRenderer*) g_object_new (GULKAN_TYPE_RENDERER, 0);
  g_object_unref (renderer);

  GulkanSwapchainRenderer *swapchain_renderer =
    (GulkanSwapchainRenderer*) g_object_new (GULKAN_TYPE_SWAPCHAIN_RENDERER, 0);
  g_object_unref (swapchain_renderer);
}

static void
_test_with_client ()
{
  GulkanClient *client = gulkan_client_new ();
  GulkanRenderer *renderer =
    (GulkanRenderer*) g_object_new (GULKAN_TYPE_RENDERER, 0);
  gulkan_renderer_set_client (renderer, client);
  g_object_unref (renderer);

  client = gulkan_client_new ();
  GulkanSwapchainRenderer *swapchain_renderer =
    (GulkanSwapchainRenderer*) g_object_new (GULKAN_TYPE_SWAPCHAIN_RENDERER, 0);
  gulkan_renderer_set_client (GULKAN_RENDERER (swapchain_renderer), client);
  g_object_unref (swapchain_renderer);
}

static void
_test_with_init ()
{
  glfwInit ();

  glfwWindowHint (GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint (GLFW_RESIZABLE, GLFW_FALSE);
  glfwWindowHint (GLFW_VISIBLE, GLFW_FALSE);

  GLFWwindow *window = glfwCreateWindow (100, 100, "Test", NULL, NULL);
  g_assert_nonnull (window);

  GulkanClient *client = gulkan_client_new_glfw ();
  VkInstance instance = gulkan_client_get_instance_handle (client);
  VkSurfaceKHR surface;
  VkResult res = glfwCreateWindowSurface (instance, window, NULL, &surface);
  g_assert (res == VK_SUCCESS);

  GulkanSwapchainRenderer *swapchain_renderer =
    (GulkanSwapchainRenderer*) g_object_new (GULKAN_TYPE_SWAPCHAIN_RENDERER, 0);
  gulkan_renderer_set_client (GULKAN_RENDERER (swapchain_renderer), client);

  VkClearColorValue black = {
    .float32 = { 0.0f, 0.0f, 0.0f, 1.0f },
  };

  gulkan_swapchain_renderer_initialize (swapchain_renderer,
                                        surface, black, NULL);

  g_object_unref (swapchain_renderer);

  glfwDestroyWindow (window);
  glfwTerminate ();
}

int
main ()
{
  _test_without_client ();
  _test_with_client ();
  _test_with_init ();
  return 0;
}


