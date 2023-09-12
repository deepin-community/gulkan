#include "common.h"

GulkanClient*
gulkan_client_new_glfw (void)
{
  uint32_t num_glfw_extensions = 0;
  const char **glfw_extensions;
  glfw_extensions = glfwGetRequiredInstanceExtensions (&num_glfw_extensions);

  GSList *instance_ext_list = NULL;
  for (uint32_t i = 0; i < num_glfw_extensions; i++)
    {
      char *instance_ext = g_strdup (glfw_extensions[i]);
      instance_ext_list = g_slist_append (instance_ext_list, instance_ext);
    }

  const gchar *device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };

  GSList *device_ext_list = NULL;
  for (uint64_t i = 0; i < G_N_ELEMENTS (device_extensions); i++)
    {
      char *device_ext = g_strdup (device_extensions[i]);
      device_ext_list = g_slist_append (device_ext_list, device_ext);
    }

  GulkanClient *client =
    gulkan_client_new_from_extensions (instance_ext_list, device_ext_list);

  g_slist_free (instance_ext_list);
  g_slist_free (device_ext_list);

  return client;
}

/* Based on https://stackoverflow.com/a/31526753/1946875 */
GLFWmonitor*
glfw_window_get_current_monitor (GLFWwindow *window)
{
  int wx, wy;
  glfwGetWindowPos (window, &wx, &wy);

  int ww, wh;
  glfwGetWindowSize (window, &ww, &wh);

  GLFWmonitor *monitor = NULL;
  int best_overlap = 0;

  int size;
  GLFWmonitor **monitors = glfwGetMonitors (&size);
  for (int i = 0; i < size; i++)
    {
      const GLFWvidmode *mode = glfwGetVideoMode (monitors[i]);
      int mx, my;
      glfwGetMonitorPos (monitors[i], &mx, &my);
      int mw = mode->width;
      int mh = mode->height;

      int overlap = MAX (0, MIN (wx + ww, mx + mw) - MAX (wx, mx)) *
                    MAX (0, MIN (wy + wh, my + mh) - MAX (wy, my));

      if (best_overlap < overlap) {
        best_overlap = overlap;
        monitor = monitors[i];
      }
    }

  return monitor;
}

void
glfw_toggle_fullscreen (GLFWwindow *window,
                        VkOffset2D *position,
                        VkExtent2D *size)
{
  GLFWmonitor* monitor = glfwGetWindowMonitor (window);
  if (!monitor)
    {
      glfwGetWindowPos (window, &position->x, &position->y);
      int w, h;
      glfwGetWindowSize (window, &w, &h);
      size->width = (uint32_t) w;
      size->height = (uint32_t) h;

      monitor = glfw_window_get_current_monitor (window);
      const GLFWvidmode* mode = glfwGetVideoMode (monitor);
      glfwSetWindowMonitor (window, monitor, 0, 0,
                            mode->width, mode->height,
                            mode->refreshRate);
    }
  else
    {
      glfwSetWindowMonitor (window, NULL,
                            position->x, position->y,
                            (int) size->width, (int) size->height, 0);
    }
}
