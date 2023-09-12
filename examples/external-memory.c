/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 *
 * based on code from
 * https://github.com/lostgoat/ogl-samples/blob/master/tests/gl-450-culling.cpp
 * https://gitlab.com/beVR_nz/VulkanIPC_Demo/
 */

#include <GL/glew.h>

#include "common/plane-example.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define ENUM_TO_STR(r) case r: return #r

static const gchar*
gl_error_string (GLenum code)
{
  switch (code)
    {
      ENUM_TO_STR(GL_NO_ERROR);
      ENUM_TO_STR(GL_INVALID_ENUM);
      ENUM_TO_STR(GL_INVALID_VALUE);
      ENUM_TO_STR(GL_INVALID_OPERATION);
      ENUM_TO_STR(GL_INVALID_FRAMEBUFFER_OPERATION);
      ENUM_TO_STR(GL_OUT_OF_MEMORY);
      ENUM_TO_STR(GL_STACK_UNDERFLOW);
      ENUM_TO_STR(GL_STACK_OVERFLOW);
      default:
        return "UNKNOWN GL Error";
    }
}

struct _Example
{
  PlaneExample parent;

  GLuint gl_texture;
  GLuint gl_mem_object;

  GLFWwindow *opengl_window;
};

G_DEFINE_TYPE (Example, gulkan_example, PLANE_TYPE_EXAMPLE)

static void
_finalize (GObject *gobject)
{
  Example *self = GULKAN_EXAMPLE (gobject);
  glDeleteTextures (1, &self->gl_texture);
  if (self->gl_mem_object != 0)
    glDeleteMemoryObjectsEXT (1, &self->gl_mem_object);
  G_OBJECT_CLASS (gulkan_example_parent_class)->finalize (gobject);
}

static void
_init_gl (Example *self)
{
  glfwWindowHint (GLFW_CLIENT_API, GLFW_OPENGL_API);
  glfwWindowHint (GLFW_RESIZABLE, GLFW_FALSE);

  glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint (GLFW_VISIBLE, GLFW_FALSE);

  self->opengl_window = glfwCreateWindow (10, 10, "GLFW OpenGL", NULL, NULL);
  glfwMakeContextCurrent (self->opengl_window);
}

static void
gulkan_example_init (Example *self)
{
  self->gl_mem_object = 0;
}

static void
gl_check_error (char* prefix)
{
  GLenum err = 0 ;
  while((err = glGetError ()) != GL_NO_ERROR)
    g_printerr ("GL ERROR: %s - %s\n", prefix, gl_error_string (err));
}

static GulkanTexture*
_init_texture (PlaneExample *example,
               GulkanClient *client,
               GdkPixbuf    *pixbuf)
{
  Example *self = GULKAN_EXAMPLE (example);

  VkExtent2D extent = {
    .width = (uint32_t) gdk_pixbuf_get_width (pixbuf),
    .height = (uint32_t) gdk_pixbuf_get_height (pixbuf)
  };

  VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  int fd;
  gsize size;
  GulkanTexture *texture = gulkan_texture_new_export_fd (
    client, extent, VK_FORMAT_R8G8B8A8_SRGB, layout, &size, &fd);

  if (!texture)
    {
      g_printerr ("Could not initialize texture.\n");
      return NULL;
    }

  _init_gl (self);

  GLenum err = glewInit ();
  if (GLEW_OK != err)
    {
      g_printerr ("glewInit Error: %s\n", glewGetErrorString (err));
      return NULL;
    }

  if (!glewIsSupported ("GL_EXT_memory_object"))
  {
    g_printerr ("GL_EXT_memory_object is not supported.\n");
    return NULL;
  }

  GLint gl_dedicated_mem = GL_TRUE;

  glCreateMemoryObjectsEXT (1, &self->gl_mem_object);
  gl_check_error ("glCreateMemoryObjectsEXT");

  g_print ("created memory object id: %d\n", self->gl_mem_object);

  glMemoryObjectParameterivEXT (self->gl_mem_object,
                                GL_DEDICATED_MEMORY_OBJECT_EXT,
                                &gl_dedicated_mem);
  gl_check_error ("glCreateMemoryObjectsEXT");

  /* Note: ImportMemoryFd takes ownership of the fd from the Vulkan driver.
   * The Vulkan driver is now free to reuse the fd number */
  glImportMemoryFdEXT (self->gl_mem_object, size,
                       GL_HANDLE_TYPE_OPAQUE_FD_EXT, fd);
  gl_check_error ("glImportMemoryFdEXT");

  g_print ("Imported texture from fd %d\n", fd);


  glGenTextures (1, &self->gl_texture);
  glBindTexture (GL_TEXTURE_2D, self->gl_texture);
  gl_check_error ("glBindTexture");

  glTexParameteri (GL_TEXTURE_2D,GL_TEXTURE_TILING_EXT, GL_OPTIMAL_TILING_EXT);
  gl_check_error ("glTexParameteri GL_OPTIMAL_TILING_EXT");
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl_check_error ("glTexParameteri GL_TEXTURE_MIN_FILTER");
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl_check_error ("glTexParameteri GL_TEXTURE_MAG_FILTER");

  glTexStorageMem2DEXT (GL_TEXTURE_2D, 1, GL_SRGB8_ALPHA8, extent.width,
                        extent.height, self->gl_mem_object, 0);
  gl_check_error ("glTexStorageMem2DEXT");

  guchar *rgb = gdk_pixbuf_get_pixels (pixbuf);

  /* we can NOT use glTexImage2D() because the memory is already allocated */
  glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, (GLint) extent.width,
                   (GLint) extent.height, GL_RGBA,
                   GL_UNSIGNED_BYTE, (GLvoid*)rgb);
  gl_check_error ("glTexSubImage2D");

  glFinish();

  if (!gulkan_texture_transfer_layout (texture,
                                       VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
    {
      g_printerr ("Could not transfer image layout!");
      /* not fatal, will just fail validation now */
    }
  return texture;
}

static gboolean
_init (Example *self)
{
  GSList *instance_ext_list =
    gulkan_client_get_external_memory_instance_extensions ();

  GSList *device_ext_list =
    gulkan_client_get_external_memory_device_extensions ();

  if (!plane_example_initialize (PLANE_EXAMPLE (self),
                                 "/res/cat_srgb.jpg",
                                 instance_ext_list,
                                 device_ext_list))
    return FALSE;

  return TRUE;
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
main () {
  Example *self = (Example *) g_object_new (GULKAN_TYPE_EXAMPLE, 0);
  if (!_init (self))
    {
      g_object_unref (self);
      return EXIT_FAILURE;
    }

  plane_example_run (PLANE_EXAMPLE (self));

  g_object_unref (self);

  return EXIT_SUCCESS;
}
