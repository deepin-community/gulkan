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
  GulkanClient *client = gulkan_client_new ();
  g_assert_nonnull (client);
  g_object_unref (client);
}

int
main ()
{
  _test_minimal ();

  return 0;
}


