/*
 * gulkan
 * Copyright 2019 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "gulkan.h"

#include <GL/glew.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define ENUM_TO_STR(r)                                                         \
  case r:                                                                      \
    return #r

static void GLAPIENTRY
_gl_debug_cb (GLenum        source,
              GLenum        type,
              GLuint        id,
              GLenum        severity,
              GLsizei       length,
              const GLchar *message,
              const void   *user_param)
{
  (void) source;
  (void) id;
  (void) length;
  (void) user_param;
  g_print ("GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
           (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type,
           severity, message);
}

typedef struct Example
{
  GLFWwindow    *opengl_window;
  GulkanContext *context;
  gboolean       ati_meminfo;
} Example;

static GdkPixbuf *
load_gdk_pixbuf (int x, int y)
{
  GError    *error = NULL;
  GdkPixbuf *pixbuf_rgb = gdk_pixbuf_new_from_resource ("/res/cat_srgb.jpg",
                                                        &error);

  g_assert_null (error);

  GdkPixbuf *pixbuf = gdk_pixbuf_add_alpha (pixbuf_rgb, FALSE, 0, 0, 0);
  g_object_unref (pixbuf_rgb);

  GdkPixbuf *ret = gdk_pixbuf_scale_simple (pixbuf, x, y, GDK_INTERP_NEAREST);
  g_object_unref (pixbuf);

  return ret;
}

static const gchar *
gl_error_string (GLenum code)
{
  switch (code)
    {
      ENUM_TO_STR (GL_NO_ERROR);
      ENUM_TO_STR (GL_INVALID_ENUM);
      ENUM_TO_STR (GL_INVALID_VALUE);
      ENUM_TO_STR (GL_INVALID_OPERATION);
      ENUM_TO_STR (GL_INVALID_FRAMEBUFFER_OPERATION);
      ENUM_TO_STR (GL_OUT_OF_MEMORY);
      ENUM_TO_STR (GL_STACK_UNDERFLOW);
      ENUM_TO_STR (GL_STACK_OVERFLOW);
      default:
        return "UNKNOWN GL Error";
    }
}

static void
gl_check_error (char *prefix)
{
  GLenum err = 0;
  while ((err = glGetError ()) != GL_NO_ERROR)
    g_printerr ("GL ERROR: %s - %s\n", prefix, gl_error_string (err));
}

static void
_create_gl_texture_from_fd (int        fd,
                            gsize      size,
                            VkExtent2D extent,
                            GLvoid    *rgb,
                            GLuint    *handle,
                            GLuint    *mem_object)
{
  glCreateMemoryObjectsEXT (1, mem_object);
  gl_check_error ("glCreateMemoryObjectsEXT");
  g_debug ("created memory object id: %d", *mem_object);

  GLint gl_dedicated_mem = GL_TRUE;
  glMemoryObjectParameterivEXT (*mem_object, GL_DEDICATED_MEMORY_OBJECT_EXT,
                                &gl_dedicated_mem);
  gl_check_error ("glCreateMemoryObjectsEXT");

  glImportMemoryFdEXT (*mem_object, size, GL_HANDLE_TYPE_OPAQUE_FD_EXT, fd);
  gl_check_error ("glImportMemoryFdEXT");
  g_debug ("Imported texture from fd %d", fd);

  glGenTextures (1, handle);
  glBindTexture (GL_TEXTURE_2D, *handle);
  gl_check_error ("glBindTexture");

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_TILING_EXT, GL_OPTIMAL_TILING_EXT);
  gl_check_error ("glTexParameteri GL_OPTIMAL_TILING_EXT");
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl_check_error ("glTexParameteri GL_TEXTURE_MIN_FILTER");
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl_check_error ("glTexParameteri GL_TEXTURE_MAG_FILTER");

  glTexStorageMem2DEXT (GL_TEXTURE_2D, 1, GL_RGBA8, extent.width, extent.height,
                        *mem_object, 0);
  gl_check_error ("glTexStorageMem2DEXT");

  glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, (int) extent.width,
                   (int) extent.height, GL_RGBA, GL_UNSIGNED_BYTE, rgb);
  gl_check_error ("glTexSubImage2D");

  glFinish ();
}

static void
_create_and_delete_external_texture (GulkanContext *context, GdkPixbuf *pixbuf)
{
  int   fd;
  gsize size;

  VkExtent2D extent = {
    .width = (uint32_t) gdk_pixbuf_get_width (pixbuf),
    .height = (uint32_t) gdk_pixbuf_get_height (pixbuf),
  };

  VkImageLayout  layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  GulkanDevice  *device = gulkan_context_get_device (context);
  GulkanTexture *gk = gulkan_texture_new_export_fd (context, extent,
                                                    VK_FORMAT_R8G8B8A8_UNORM,
                                                    layout, &size, &fd);

  GLuint gl_texture = 0;
  GLuint gl_mem_object = 0;

  GLvoid *rgba = gdk_pixbuf_get_pixels (pixbuf);
  _create_gl_texture_from_fd (fd, size, extent, rgba, &gl_texture,
                              &gl_mem_object);

  if (!gulkan_texture_transfer_layout (gk, VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
    {
      g_printerr ("Could not transfer image layout!");
      /* not fatal, will just fail validation now */
    }

  glDeleteTextures (1, &gl_texture);
  glDeleteMemoryObjectsEXT (1, &gl_mem_object);
  glFinish ();

  g_object_unref (gk);
  gulkan_device_wait_idle (device);
}

static void
_create_and_delete_gulkan_texture (GulkanContext *context, GdkPixbuf *pixbuf)
{
  GulkanTexture *gk
    = gulkan_texture_new_from_pixbuf (context, pixbuf, VK_FORMAT_R8G8B8A8_UNORM,
                                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                      FALSE);
  g_object_unref (gk);
}

static void
_create_and_delete_opengl_texture (GulkanContext *context, GdkPixbuf *pixbuf)
{
  (void) context;

  GLuint handle;
  glGenTextures (1, &handle);

  glBindTexture (GL_TEXTURE_2D, handle);

  GLsizei w = gdk_pixbuf_get_width (pixbuf);
  GLsizei h = gdk_pixbuf_get_height (pixbuf);
  GLvoid *rgba = gdk_pixbuf_get_pixels (pixbuf);

  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                rgba);

  glDeleteTextures (1, &handle);
  glFinish ();
}

static void
_test_leak (Example *self,
            void (*create_and_delete) (GulkanContext *context,
                                       GdkPixbuf     *pixbuf))
{
  GdkPixbuf *pixbuf = load_gdk_pixbuf (1024, 1024);

  GulkanDevice *device = gulkan_context_get_device (self->context);
  gulkan_device_print_memory_budget (device);

  VkDeviceSize budget_before = gulkan_device_get_heap_budget (device, 0);

  GLint free_before[4];
  if (self->ati_meminfo)
    {
      glGetIntegerv (GL_TEXTURE_FREE_MEMORY_ATI, free_before);
      g_print ("Free texture VRAM before: %d\n", free_before[0]);
    }

  for (uint32_t i = 0; i < 500; i++)
    create_and_delete (self->context, pixbuf);

  glFinish ();
  gulkan_device_wait_idle (device);

  struct timespec ts = {.tv_sec = 1};
  while (nanosleep (&ts, &ts))
    ;

  glFinish ();
  gulkan_device_wait_idle (device);

  if (self->ati_meminfo)
    {
      GLint free_after[4];
      glGetIntegerv (GL_TEXTURE_FREE_MEMORY_ATI, free_after);
      g_print ("Free texture VRAM after: %d\n", free_after[0]);

      g_print ("OpenGL: Leaked: ~ %.2f MB\n",
               (free_before[0] - free_after[0]) / (1024.0));
    }

  VkDeviceSize budget_after = gulkan_device_get_heap_budget (device, 0);

  g_print ("Vulkan: Leaked ~ %.2f MB\n",
           ((double) budget_before / 1024.0 / 1024.0
            - (double) budget_after / 1024.0 / 1024.0));

  gulkan_device_print_memory_budget (device);

  g_object_unref (pixbuf);
}

static void
init_glfw (Example *self, int width, int height)
{
  glfwInit ();

  glfwWindowHint (GLFW_CLIENT_API, GLFW_OPENGL_API);
  glfwWindowHint (GLFW_RESIZABLE, GLFW_FALSE);
  glfwWindowHint (GLFW_VISIBLE, GLFW_FALSE);

  glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  self->opengl_window = glfwCreateWindow (width, height, "GLFW OpenGL", NULL,
                                          NULL);
  glfwMakeContextCurrent (self->opengl_window);
  glfwHideWindow (self->opengl_window);
}

int
main ()
{
  Example example = {0};

  init_glfw (&example, 100, 100);

  GLenum err = glewInit ();
  if (GLEW_OK != err)
    {
      g_printerr ("glewInit Error: %s\n", glewGetErrorString (err));
      return 1;
    }

  if (!glewIsSupported ("GL_EXT_memory_object"))
    {
      g_printerr ("GL_EXT_memory_object is not supported.\n");
      return -1;
    }

  if (glewIsSupported ("GL_ATI_meminfo"))
    example.ati_meminfo = TRUE;
  else
    example.ati_meminfo = FALSE;

  glEnable (GL_DEBUG_OUTPUT);
  glDebugMessageCallback (_gl_debug_cb, 0);

  GSList *instance_ext_list
    = gulkan_context_get_external_memory_instance_extensions ();

  GSList *device_ext_list
    = gulkan_context_get_external_memory_device_extensions ();

  example.context = gulkan_context_new_from_extensions (instance_ext_list,
                                                        device_ext_list,
                                                        VK_NULL_HANDLE);

  g_slist_free_full (instance_ext_list, g_free);
  g_slist_free_full (device_ext_list, g_free);

  GulkanDevice *device = gulkan_context_get_device (example.context);
  gulkan_device_print_memory_properties (device);

  g_print ("\n= Testing Vulkan textures =\n");
  _test_leak (&example, _create_and_delete_gulkan_texture);

  g_print ("\n= Testing OpenGL textures =\n");
  _test_leak (&example, _create_and_delete_opengl_texture);

  g_print ("\n= Testing external textures =\n");
  _test_leak (&example, _create_and_delete_external_texture);

  g_object_unref (example.context);

  return 0;
}
