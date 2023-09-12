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
  g_assert_nonnull (instance);

  g_assert (gulkan_instance_create (instance, NULL));

  GulkanDevice *device = gulkan_device_new ();
  g_assert_nonnull (device);

  g_assert (gulkan_device_create (device, instance,
                                         VK_NULL_HANDLE, NULL));

  g_object_unref (device);
  g_object_unref (instance);
}

static void
_test_extensions ()
{
  GulkanInstance *instance = gulkan_instance_new ();
  g_assert_nonnull (instance);

  g_assert (gulkan_instance_create (instance, NULL));

  GulkanDevice *device = gulkan_device_new ();
  g_assert_nonnull (device);

  GSList *extensions = NULL;
  extensions = g_slist_append (extensions, "VK_KHR_external_memory_fd");

  g_assert (gulkan_device_create (device, instance,
                                  VK_NULL_HANDLE, extensions));

  g_slist_free (extensions);

  g_object_unref (device);
  g_object_unref (instance);
}

int
main ()
{
  _test_minimal ();
  _test_extensions ();

  return 0;
}


