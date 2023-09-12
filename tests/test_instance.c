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
  GulkanInstance *instance = gulkan_instance_new ();

  g_assert (gulkan_instance_create (instance, NULL));
  g_assert_nonnull (instance);

  g_object_unref (instance);
}

static void
_test_validation ()
{
  GulkanInstance *instance = gulkan_instance_new ();

  g_assert (gulkan_instance_create (instance, NULL));
  g_assert_nonnull (instance);

  g_object_unref (instance);
}

static void
_test_extensions ()
{
  GulkanInstance *instance = gulkan_instance_new ();

  GSList *instance_extensions = NULL;
  instance_extensions = g_slist_append (instance_extensions, "VK_KHR_surface");
  g_assert (gulkan_instance_create (instance, instance_extensions));
  g_assert_nonnull (instance);

  g_slist_free (instance_extensions);
  g_object_unref (instance);
}

int
main ()
{
  _test_minimal ();
  _test_validation ();
  _test_extensions ();

  return 0;
}


