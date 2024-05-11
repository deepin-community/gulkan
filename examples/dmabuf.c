/*
 * gulkan
 * Copyright 2014 Rob Clark <robdclark@gmail.com>
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <amdgpu.h>
#include <amdgpu_drm.h>
#include <fcntl.h>

#include "common/plane-example.h"

struct _Example
{
  PlaneExample parent;

  amdgpu_bo_handle     amd_bo;
  amdgpu_device_handle amd_dev;
};
G_DEFINE_TYPE (Example, gulkan_example, PLANE_TYPE_EXAMPLE)

static void
gulkan_example_init (Example *self)
{
  (void) self;
}

static void *
_allocate_dmabuf_amd (Example *self, gsize size, int *fd)
{
  /* use render node to avoid needing to authenticate: */
  int dev_fd = open ("/dev/dri/renderD128", 02, 0);

  uint32_t major_version;
  uint32_t minor_version;
  int ret = amdgpu_device_initialize (dev_fd, &major_version, &minor_version,
                                      &self->amd_dev);
  if (ret < 0)
    {
      g_printerr ("Could not create amdgpu device: %s\n", strerror (-ret));
      return NULL;
    }

  g_print ("Initialized amdgpu drm device with fd %d. Version %d.%d\n", dev_fd,
           major_version, minor_version);

  struct amdgpu_bo_alloc_request alloc_buffer = {
    .alloc_size = (uint64_t) size,
    .preferred_heap = AMDGPU_GEM_DOMAIN_GTT,
  };

  ret = amdgpu_bo_alloc (self->amd_dev, &alloc_buffer, &self->amd_bo);
  if (ret < 0)
    {
      g_printerr ("amdgpu_bo_alloc failed: %s\n", strerror (-ret));
      return NULL;
    }

  uint32_t shared_handle;
  ret = amdgpu_bo_export (self->amd_bo, amdgpu_bo_handle_type_dma_buf_fd,
                          &shared_handle);

  if (ret < 0)
    {
      g_printerr ("amdgpu_bo_export failed: %s\n", strerror (-ret));
      return NULL;
    }

  *fd = (int) shared_handle;
  void *cpu_buffer;
  ret = amdgpu_bo_cpu_map (self->amd_bo, &cpu_buffer);

  if (ret < 0)
    {
      g_printerr ("amdgpu_bo_cpu_map failed: %s\n", strerror (-ret));
      return NULL;
    }

  return cpu_buffer;
}

static void
dma_buf_fill (char *pixels, uint32_t width, uint32_t height, uint32_t stride)
{
  uint32_t i, j;
  /* paint the buffer with a colored gradient */
  for (j = 0; j < height; j++)
    {
      /* pixel data is BGRA, each channel in a char. */
      char *fb_ptr = (char *) (pixels + j * stride);
      for (i = 0; i < width; i++)
        {
          fb_ptr[i * 4] = 0;
          fb_ptr[i * 4 + 1] = (char) (i * 255 / width);
          fb_ptr[i * 4 + 2] = (char) (j * 255 / height);
          fb_ptr[i * 4 + 3] = (char) 255;
          // printf ("b %d\n", fb_ptr[i+3]);
        }
    }
}

#define ALIGN(_v, _d) (((_v) + ((_d) -1)) & ~((_d) -1))

static GulkanTexture *
_init_texture (PlaneExample *example, GulkanContext *context, GdkPixbuf *pixbuf)
{
  (void) pixbuf;

  Example *self = GULKAN_EXAMPLE (example);

  /* create dmabuf */
  VkExtent2D extent = {.width = 1280, .height = 720};

  int   fd = -1;
  guint stride = (guint) ALIGN ((int) extent.width, 32) * 4;
  gsize size = stride * extent.height;
  char *map = (char *) _allocate_dmabuf_amd (self, size, &fd);
  if (!map)
    return NULL;

  dma_buf_fill (map, extent.width, extent.height, stride);

  GulkanTexture *texture
    = gulkan_texture_new_from_dmabuf (context, fd, extent,
                                      VK_FORMAT_B8G8R8A8_SRGB);

  if (texture == NULL)
    {
      g_printerr ("Unable to initialize vulkan dmabuf texture.\n");
      return NULL;
    }

  gulkan_texture_transfer_layout (texture, VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  return texture;
}

static void
_finalize (GObject *gobject)
{
  Example *self = GULKAN_EXAMPLE (gobject);
  int      ret = amdgpu_bo_free (self->amd_bo);
  if (ret < 0)
    g_printerr ("Could not free amdgpu buffer: %s\n", strerror (-ret));

  ret = amdgpu_device_deinitialize (self->amd_dev);
  if (ret < 0)
    g_printerr ("Could not free amdgpu device: %s\n", strerror (-ret));

  G_OBJECT_CLASS (gulkan_example_parent_class)->finalize (gobject);
}

static void
gulkan_example_class_init (ExampleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = _finalize;

  PlaneExampleClass *parent_class = PLANE_EXAMPLE_CLASS (klass);
  parent_class->init_texture = _init_texture;
}

static gboolean
_init (Example *self)
{
  GSList      *instance_ext_list = NULL;
  const gchar *instance_extensions[] = {
    VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
  };
  for (uint32_t i = 0; i < G_N_ELEMENTS (instance_extensions); i++)
    {
      char *instance_ext = g_strdup (instance_extensions[i]);
      instance_ext_list = g_slist_append (instance_ext_list, instance_ext);
    }

  const gchar *device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
    VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
    VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
  };

  GSList *device_ext_list = NULL;
  for (uint32_t i = 0; i < G_N_ELEMENTS (device_extensions); i++)
    {
      char *device_ext = g_strdup (device_extensions[i]);
      device_ext_list = g_slist_append (device_ext_list, device_ext);
    }

  if (!plane_example_initialize (PLANE_EXAMPLE (self), "/res/cat_srgb.jpg",
                                 instance_ext_list, device_ext_list))
    return FALSE;

  return TRUE;
}

int
main ()
{
  Example *self = (Example *) g_object_new (GULKAN_TYPE_EXAMPLE, 0);
  if (!_init (self))
    return EXIT_FAILURE;

  plane_example_run (PLANE_EXAMPLE (self));

  g_object_unref (self);

  return EXIT_SUCCESS;
}
