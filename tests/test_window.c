/*
 * gulkan
 * Copyright 2022 Lubosz Sarnecki
 * Author: Lubosz Sarnecki <lubosz@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "gulkan.h"
#include <glib.h>

static void
_test_window_xcb ()
{
  VkExtent2D    extent = {.width = 500, .height = 500};
  GulkanWindow *window = gulkan_window_new (extent, "Window Test");
  g_assert (window);
}

int
main ()
{
  _test_window_xcb ();
  return 0;
}
