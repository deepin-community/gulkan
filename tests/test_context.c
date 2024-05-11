/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "gulkan.h"

static void
_test_minimal ()
{
  GulkanContext *context = gulkan_context_new ();
  g_assert_nonnull (context);
  g_object_unref (context);
}

int
main ()
{
  _test_minimal ();

  return 0;
}
