/*
 * gulkan
 * Copyright 2020 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef COMMON_H_
#define COMMON_H_

#include <gulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

GulkanClient*
gulkan_client_new_glfw (void);

GLFWmonitor*
glfw_window_get_current_monitor (GLFWwindow *window);

void
glfw_toggle_fullscreen (GLFWwindow *window,
                        VkOffset2D *position,
                        VkExtent2D *size);

#endif /* COMMON_H_ */
