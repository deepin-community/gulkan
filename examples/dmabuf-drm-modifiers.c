/*
 * gxr
 * Copyright 2021 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "common/plane-example.h"

#include <glib.h>

#include <fcntl.h>

#include "gulkan.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <drm_fourcc.h>

#include <gbm.h>

struct _Example
{
  PlaneExample parent;

  GdkPixbuf     *pixbuf;
  GulkanContext *gc;
  GulkanTexture *texture;
  GLuint         gl_texture;
};
G_DEFINE_TYPE (Example, gulkan_example, PLANE_TYPE_EXAMPLE)

static uint64_t drm_format = DRM_FORMAT_ABGR8888;
static uint64_t vk_format = VK_FORMAT_R8G8B8A8_UNORM;
static uint64_t gl_internal_format = GL_RGBA8;
static uint64_t gl_format = GL_RGBA;

static void
gulkan_example_init (Example *self)
{
  (void) self;
}

static void
_finalize (GObject *gobject)
{
  Example *self = GULKAN_EXAMPLE (gobject);

  // self->texture and self->pixbuf are owned by parent plane example

  glDeleteTextures (1, &self->gl_texture);

  G_OBJECT_CLASS (gulkan_example_parent_class)->finalize (gobject);
}

/* On radeonsi drm formats are used for opengl textures but the necessary drm
 * modifier is not provided through the egl API, the modifier will be
 * DRM_FORMAT_MOD_INVALID.
 * You can verify that the code works otherwise by running with
 * AMD_DEBUG=notiling
 */
static gboolean
from_gl (Example *self, struct GulkanDmabufAttributes *attribs)
{
  guint width = (guint) gdk_pixbuf_get_width (self->pixbuf);
  guint height = (guint) gdk_pixbuf_get_height (self->pixbuf);

  guint   length = 0;
  guchar *rgba = gdk_pixbuf_get_pixels_with_length (self->pixbuf, &length);

  glGenTextures (1, &self->gl_texture);
  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_2D, self->gl_texture);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_TILING_EXT, GL_OPTIMAL_TILING_EXT);
  glTexImage2D (GL_TEXTURE_2D, 0, (GLint) gl_internal_format, (GLsizei) width,
                (GLsizei) height, 0, (GLenum) gl_format, GL_UNSIGNED_BYTE,
                (GLvoid *) rgba);
  glFlush ();

  EGLDisplay eglDisplay = eglGetCurrentDisplay ();
  EGLContext eglContext = eglGetCurrentContext ();

  EGLImage egl_image = eglCreateImage (eglDisplay, eglContext,
                                       EGL_GL_TEXTURE_2D,
                                       (EGLClientBuffer) (gulong)
                                         self->gl_texture,
                                       NULL);

  PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC _eglExportDMABUFImageQueryMESA
    = (PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC)
      eglGetProcAddress ("eglExportDMABUFImageQueryMESA");
  if (!_eglExportDMABUFImageQueryMESA)
    {
      g_printerr ("Unable to load function eglExportDMABUFImageQueryMESA.");
      return FALSE;
    }

  // Only one set of modifiers is returned for all planes
  if (!_eglExportDMABUFImageQueryMESA (eglDisplay, egl_image,
                                       (int *) &attribs->format,
                                       &attribs->n_planes, &attribs->modifier))
    {
      g_printerr ("Unable to get dmabuf attribs\n");
      return false;
    }

  if (attribs->modifier == DRM_FORMAT_MOD_INVALID)
    {
      g_printerr ("drm modifier: DRM_FORMAT_MOD_INVALID\n");
      // return FALSE;
    }

  g_debug ("dmabuf attribs: format %d, n_planes %d, modifier %lu",
           attribs->format, attribs->n_planes, attribs->modifier);

  PFNEGLEXPORTDRMIMAGEMESAPROC _eglExportDMABUFImageMESA
    = (PFNEGLEXPORTDMABUFIMAGEMESAPROC)
      eglGetProcAddress ("eglExportDMABUFImageMESA");
  if (!_eglExportDMABUFImageMESA)
    {
      g_printerr ("Unable to load function eglExportDMABUFImageMESA.");
      return FALSE;
    }

  if (!_eglExportDMABUFImageMESA (eglDisplay, egl_image,
                                  (EGLint *) &attribs->fd, 0, 0))
    {
      g_printerr ("Unable to export DMABUF\n");
      return FALSE;
    }

  g_print ("exported dmabuf fds");
  for (int i = 0; i < attribs->n_planes; i++)
    g_print ("%d", attribs->fd[i]);

  attribs->width = (int32_t) width;
  attribs->height = (int32_t) height;

  eglDestroyImage (eglDisplay, egl_image);

  return TRUE;
}

static gboolean
from_gbm (Example *self, struct GulkanDmabufAttributes *attribs)
{
  guint width = (guint) gdk_pixbuf_get_width (self->pixbuf);
  guint height = (guint) gdk_pixbuf_get_height (self->pixbuf);

  guint   length = 0;
  guchar *rgba = gdk_pixbuf_get_pixels_with_length (self->pixbuf, &length);
  guint   stride = (guint) gdk_pixbuf_get_rowstride (self->pixbuf);

  const gchar *dev = g_getenv ("GULKAN_GBM_DEVICE");
  if (!dev)
    dev = "/dev/dri/renderD128";

  int32_t fd = open (dev, O_RDWR);
  if (fd <= 0)
    {
      g_printerr ("Failed to open render node\n");
      return 0;
    }

  struct gbm_device *gbm = gbm_create_device (fd);

  // choose a drm modifier supported by this vulkan driver
  VkDrmFormatModifierPropertiesEXT supported_modifier_props = {0};
  {
    VkPhysicalDevice vk_physical_device
      = gulkan_context_get_physical_device_handle (self->gc);

    VkDrmFormatModifierPropertiesListEXT modifier_prop_list = {
      .sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
    };
    VkFormatProperties2 format_props = {
      .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
      .pNext = &modifier_prop_list,
    };
    vkGetPhysicalDeviceFormatProperties2 (vk_physical_device,
                                          (VkFormat) vk_format, &format_props);

    modifier_prop_list.pDrmFormatModifierProperties
      = g_malloc (sizeof (VkDrmFormatModifierPropertiesEXT)
                  * modifier_prop_list.drmFormatModifierCount);
    vkGetPhysicalDeviceFormatProperties2 (vk_physical_device,
                                          (VkFormat) vk_format, &format_props);

    g_debug ("%d Supported modifiers:",
             modifier_prop_list.drmFormatModifierCount);
    for (uint32_t i = 0; i < modifier_prop_list.drmFormatModifierCount; i++)
      {
        VkDrmFormatModifierPropertiesEXT *m = &modifier_prop_list
                                                 .pDrmFormatModifierProperties
                                                   [i];
        g_debug ("modifier %lu: planes %d tiling features %d,",
                 m->drmFormatModifier, m->drmFormatModifierPlaneCount,
                 m->drmFormatModifierTilingFeatures);

        // as long as we haven't chosen any modifier yet (0 planes)
        // choose a drm modifier with 2 planes
        if (supported_modifier_props.drmFormatModifierPlaneCount == 0
            && m->drmFormatModifierPlaneCount == 2)
          supported_modifier_props = modifier_prop_list
                                       .pDrmFormatModifierProperties[i];
      }
    g_free (modifier_prop_list.pDrmFormatModifierProperties);
    modifier_prop_list.pDrmFormatModifierProperties = NULL;
  }
  g_print ("Chose modifier %lu with %d planes and features %d\n",
           supported_modifier_props.drmFormatModifier,
           supported_modifier_props.drmFormatModifierPlaneCount,
           supported_modifier_props.drmFormatModifierTilingFeatures);

#if 1
  uint64_t       modifiers = supported_modifier_props.drmFormatModifier;
  struct gbm_bo *bo = gbm_bo_create_with_modifiers (gbm, width, height,
                                                    (uint32_t) drm_format,
                                                    &modifiers, 1);
#else
  struct gbm_bo *bo = gbm_bo_create (gbm, width, height, drm_format,
                                     GBM_BO_USE_RENDERING);
#endif

  if (!bo)
    {
      g_printerr ("failed to create bo: %s\n", strerror (errno));
      return 0;
    }

  attribs->format = gbm_bo_get_format (bo);
  attribs->modifier = gbm_bo_get_modifier (bo);
  if (attribs->modifier == DRM_FORMAT_MOD_INVALID)
    {
      g_printerr ("gbm: modifier is DRM_FORMAT_MOD_INVALID!\n");
      return FALSE;
    }

  attribs->n_planes = gbm_bo_get_plane_count (bo);
  g_print ("gbm: %d planes\n", attribs->n_planes);

  for (int i = 0; i < attribs->n_planes; i++)
    {
      attribs->fd[i] = gbm_bo_get_fd_for_plane (bo, i);
      attribs->stride[i] = gbm_bo_get_stride_for_plane (bo, i);
      attribs->offset[i] = gbm_bo_get_offset (bo, i);

      g_print ("gbm: plane %d fd %d stride %d offset %d \n", i, attribs->fd[i],
               attribs->stride[i], attribs->offset[i]);
    }

  attribs->width = (int32_t) width;
  attribs->height = (int32_t) height;

  void    *bo_handle = NULL;
  uint32_t bo_stride;
  guchar  *bo_map = gbm_bo_map (bo, 0, 0, width, height, GBM_BO_TRANSFER_WRITE,
                                &bo_stride, &bo_handle);
  if (!bo_map)
    {
      g_printerr ("Failed to map bo: %d: %s\n", errno, strerror (errno));
      return 0;
    }

  if (bo_stride == stride)
    {
      memcpy (bo_map, rgba, length);
    }
  else
    {
      g_print ("bo stride %d != img stride %d, copy lines\n", bo_stride,
               stride);
      for (uint32_t i = 0; i < height; i++)
        {
          guchar *rgba_row = rgba + i * stride;
          guchar *bo_row = bo_map + i * bo_stride;
          memcpy (bo_row, rgba_row, stride);
        }
    }

  g_debug ("copied %d bytes to bo", length);
  gbm_bo_unmap (bo, bo_handle);

  return TRUE;
}

static gboolean
_export_import_dmabuf (Example *self)
{
  struct GulkanDmabufAttributes attribs = {0};

  gboolean use_gbm = TRUE;

  gboolean ret = FALSE;
  if (use_gbm)
    ret = from_gbm (self, &attribs);
  else
    ret = from_gl (self, &attribs);

  if (!ret)
    return FALSE;

  self->texture = gulkan_texture_new_from_dmabuf_attribs (self->gc, &attribs);
  if (self->texture == NULL)
    {
      g_printerr ("Unable to initialize vulkan dmabuf texture.\n");
      return FALSE;
    }

  if (!gulkan_texture_transfer_layout (self->texture, VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
    {
      g_printerr ("Unable to transfer layout.\n");
    }

  return TRUE;
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

    VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
    VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
    VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
    VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
    VK_KHR_MAINTENANCE1_EXTENSION_NAME,
    VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
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

static GulkanTexture *
_init_texture (PlaneExample *example, GulkanContext *context, GdkPixbuf *pixbuf)
{
  Example *self = GULKAN_EXAMPLE (example);
  self->gc = context;

  self->pixbuf = pixbuf;

  if (!_export_import_dmabuf (self))
    {
      g_printerr ("Unable to export dmabuf\n");
      glfwTerminate ();
      return NULL;
    }

  return self->texture;
}
static void
gulkan_example_class_init (ExampleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = _finalize;

  PlaneExampleClass *parent_class = PLANE_EXAMPLE_CLASS (klass);
  parent_class->init_texture = _init_texture;
}

int
main ()
{
  if (!glfwInit ())
    {
      g_printerr ("Unable to initialize GLFW3\n");
      return 1;
    }

  {
    glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    /* eglExportDMABUFImageMESA requires EGL GL context */
    glfwWindowHint (GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);

    /* window doesn't do anything, just for OpenGL context */
    glfwWindowHint (GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow *window = glfwCreateWindow (640, 480, "EGL window", NULL, NULL);
    if (!window)
      {
        g_printerr ("Unable to create window with GLFW3\n");
        glfwTerminate ();
        return 1;
      }
    glfwMakeContextCurrent (window);

    glfwWindowHint (GLFW_VISIBLE, GLFW_TRUE);
  }

  glewInit ();

  const GLubyte *renderer = glGetString (GL_RENDERER);
  const GLubyte *version = glGetString (GL_VERSION);
  g_print ("OpengL Renderer: %s\n", renderer);
  g_print ("OpenGL version : %s\n", version);

  Example *self = (Example *) g_object_new (GULKAN_TYPE_EXAMPLE, 0);
  if (!_init (self))
    return EXIT_FAILURE;

  plane_example_run (PLANE_EXAMPLE (self));

  g_object_unref (self);

  return 0;
}
