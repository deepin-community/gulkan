/*
 * gulkan
 *
 * Copyright 2020 Collabora Ltd
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <gmodule.h>
#include <json-glib/json-glib.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <gulkan.h>

#include <shaderc/shaderc.h>

#include "common/common.h"

#define GULKAN_TYPE_EXAMPLE gulkan_example_get_type ()
G_DECLARE_FINAL_TYPE (Example, gulkan_example, GULKAN,
                      EXAMPLE, GulkanSwapchainRenderer)

static const gchar *API_URL = "https://www.shadertoy.com/api/v1";
static const gchar *API_KEY = "ft8KMr";
static const gchar *URL_PREFIX = "https://www.shadertoy.com/view/";

static gboolean dump = FALSE;

typedef struct
{
  float iResolution[3];           // viewport resolution (in pixels)
  float iTime;                    // shader playback time (in seconds)
  float iMouse[4];                // mouse pixel coords. xy: current (if MLB down), zw: click
  float iChannelResolution[4][3]; // channel resolution (in pixels)
  float iTimeDelta;               // render time (in seconds)
  int   iFrame;                   // shader playback frame
  float iChannelTime[4];          // channel playback time (in seconds)
  float iDate[4];                 // (year, month, day, time in seconds)
  float iSampleRate;              // sound sample rate (i.e., 44100)
} UB;

typedef struct {
  float position[2];
  float uv[2];
} Vertex;

static const uint16_t indices[6] = {0, 1, 2, 2, 3, 0};

static const VkClearColorValue background_color = {
  .float32 = { 0.05f, 0.05f, 0.05f, 1.0f },
};

typedef struct TextureInput
{
  gint channel;
  const gchar *src;

  const gchar *filter;
  const gchar *wrap;
  const gchar *internal;

  gboolean vflip;
  gboolean srgb;

  GdkPixbuf *pixbuf;

  VkSampler sampler;
  GulkanTexture *texture;

  VkDescriptorImageInfo info;
} TextureInput;

struct _Example
{
  GulkanSwapchainRenderer parent;

  GulkanVertexBuffer *vb;

  GulkanUniformBuffer *ub;
  GulkanDescriptorPool *descriptor_pool;

  VkPipeline pipeline;
  VkDescriptorSet descriptor_set;

  GMainLoop *loop;
  GLFWwindow *window;
  gboolean should_quit;

  int button_state;

  GSList *inputs;

  UB ub_data;

  VkExtent2D last_window_size;
  VkOffset2D last_window_position;
};

G_DEFINE_TYPE (Example, gulkan_example, GULKAN_TYPE_SWAPCHAIN_RENDERER)

static void
_finalize (GObject *gobject)
{
  Example *self = GULKAN_EXAMPLE (gobject);
  g_main_loop_unref (self->loop);

  GulkanClient *client = gulkan_renderer_get_client (GULKAN_RENDERER (self));
  if (client)
    {
      g_object_unref (self->vb);
      VkDevice device = gulkan_client_get_device_handle (client);
      gulkan_device_wait_idle (gulkan_client_get_device (client));

      g_object_unref (self->descriptor_pool);
      vkDestroyPipeline (device, self->pipeline, NULL);
      g_object_unref (self->ub);

      for (guint i = 0; i < g_slist_length (self->inputs); i++)
      {
        GSList* entry = g_slist_nth (self->inputs, i);
        TextureInput *input = entry->data;
        g_object_unref (input->texture);
        vkDestroySampler (device, input->sampler, NULL);
      }
    }

  for (guint i = 0; i < g_slist_length (self->inputs); i++)
    {
      GSList* entry = g_slist_nth (self->inputs, i);
      TextureInput *input = entry->data;
      g_object_unref (input->pixbuf);
    }

  if (self->inputs)
    g_slist_free_full (self->inputs, g_free);

  G_OBJECT_CLASS (gulkan_example_parent_class)->finalize (gobject);

  /*
   * GLFW needs to be destroyed after the surface,
   * which belongs to GulkanSwapchain.
   */
  glfwDestroyWindow (self->window);
  glfwTerminate ();
}

static void
gulkan_example_init (Example *self)
{
  gulkan_renderer_set_extent (GULKAN_RENDERER (self),
                              (VkExtent2D) { .width = 1280, .height = 720 });
  self->should_quit = FALSE;
  self->loop = g_main_loop_new (NULL, FALSE);
  self->inputs = NULL;
  self->descriptor_set = VK_NULL_HANDLE;
  self->descriptor_pool = NULL;
}

static void
_update_uniform_buffer (Example *self)
{
  int64_t t = gulkan_renderer_get_msec_since_start (GULKAN_RENDERER (self));
  self->ub_data.iTime = t / 1000.0f;
  gulkan_uniform_buffer_update (self->ub, (gpointer) &self->ub_data);
}

static gboolean
_load_resource (const gchar* path, GBytes **res)
{
  GError *error = NULL;
  *res = g_resources_lookup_data (path, G_RESOURCE_LOOKUP_FLAGS_NONE, &error);

  if (error != NULL)
    {
      g_printerr ("Unable to read file: %s\n", error->message);
      g_error_free (error);
      return FALSE;
    }

  return TRUE;
}

static gboolean
_build_shader (Example        *self,
               VkShaderModule *module,
               const char     *source)
{
  GBytes *template_bytes;
  if (!_load_resource ("/shaders/toy.frag.template", &template_bytes))
    return FALSE;

  gsize template_size = 0;
  const gchar *template_string = g_bytes_get_data (template_bytes,
                                                   &template_size);

  GString *source_str = g_string_new_len (template_string,
                                          (gssize) template_size);

  for (guint i = 0; i < g_slist_length (self->inputs); i++)
    {
      GSList* entry = g_slist_nth (self->inputs, i);
      TextureInput *input = entry->data;
      g_string_append_printf (source_str,
                              "layout (binding = %d) uniform sampler2D iChannel%d;\n",
                              input->channel + 1, input->channel);
    }

  g_string_append (source_str, source);

  g_debug ("%s", source_str->str);

  shaderc_compiler_t compiler = shaderc_compiler_initialize();
  shaderc_compilation_result_t result =
    shaderc_compile_into_spv(compiler,
                             source_str->str, source_str->len,
                             shaderc_glsl_fragment_shader,
                             "main.frag", "main", NULL);

  shaderc_compilation_status status =
    shaderc_result_get_compilation_status(result);

  if (status != shaderc_compilation_status_success) {
    g_print ("Result code %d\n", (int) status);
    g_print ("shaderc error:\n%s\n", shaderc_result_get_error_message(result));
    shaderc_result_release(result);
    shaderc_compiler_release(compiler);
    return FALSE;
  }

  const char* bytes = shaderc_result_get_bytes (result);
  size_t len = shaderc_result_get_length(result);

  /* Make clang happy and copy the data to fix alignment */
  uint32_t* code = g_malloc (len);
  memcpy (code, bytes, len);

  VkShaderModuleCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = len,
    .pCode = code,
  };

  GulkanClient *client = gulkan_renderer_get_client (GULKAN_RENDERER (self));
  VkDevice device = gulkan_client_get_device_handle (client);

  VkResult res = vkCreateShaderModule (device, &info, NULL, module);
  vk_check_error ("vkCreateShaderModule", res, FALSE);

  g_free (code);

  shaderc_result_release(result);
  shaderc_compiler_release(compiler);

  g_string_free (source_str, TRUE);

  return TRUE;
}

static gboolean
_init_pipeline (GulkanSwapchainRenderer *renderer,
                gconstpointer            data)
{
  Example *self = GULKAN_EXAMPLE (renderer);

  const gchar *shader_src = (const gchar*) data;

  GulkanClient *client = gulkan_renderer_get_client (GULKAN_RENDERER (self));
  VkDevice device = gulkan_client_get_device_handle (client);

  VkPipelineVertexInputStateCreateInfo vi_create_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = 1,
    .pVertexBindingDescriptions = &(VkVertexInputBindingDescription) {
      .binding = 0,
      .stride = sizeof(Vertex),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    },
    .vertexAttributeDescriptionCount = 2,
    .pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[]) {
      {
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof (Vertex, position)
      },
      {
        .location = 1,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof (Vertex, uv)
      }
    },
  };

  VkShaderModule vs_module;
  if (!gulkan_renderer_create_shader_module (
        GULKAN_RENDERER (self), "/shaders/toy.vert.spv", &vs_module))
    return FALSE;

  VkShaderModule fs_module;
  if (!_build_shader (self, &fs_module, shader_src))
    return FALSE;

  VkPipelineLayout layout =
    gulkan_descriptor_pool_get_pipeline_layout (self->descriptor_pool);

  GulkanSwapchainRenderer *scr = GULKAN_SWAPCHAIN_RENDERER (self);
  VkRenderPass pass = gulkan_swapchain_renderer_get_render_pass_handle (scr);
  VkExtent2D extent = gulkan_renderer_get_extent (GULKAN_RENDERER (self));

  self->ub_data.iResolution[0] = extent.width;
  self->ub_data.iResolution[1] = extent.height;
  self->ub_data.iResolution[2] = 1;

  VkGraphicsPipelineCreateInfo pipeline_info = {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .stageCount = 2,
    .pStages =
      (VkPipelineShaderStageCreateInfo[]){
        {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = vs_module,
          .pName = "main",
        },
        {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = fs_module,
          .pName = "main",
        },
      },
    .pVertexInputState = &vi_create_info,
    .pInputAssemblyState =
      &(VkPipelineInputAssemblyStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      },
    .pViewportState = &(VkPipelineViewportStateCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1,
    },
    .pDynamicState =
      &(VkPipelineDynamicStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates =
          (VkDynamicState[]){
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
         },
     },
    .pRasterizationState =
      &(VkPipelineRasterizationStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_FRONT_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0f,
      },
    .pMultisampleState =
      &(VkPipelineMultisampleStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      },
    .pColorBlendState =
      &(VkPipelineColorBlendStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .blendConstants = { 0.f, 0.f, 0.f, 0.f },
        .pAttachments =
          (VkPipelineColorBlendAttachmentState[]){
            { .colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT },
          } },
    .layout = layout,
    .renderPass = pass,
  };

  VkResult res = vkCreateGraphicsPipelines (
    device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &self->pipeline);

  vkDestroyShaderModule (device, vs_module, NULL);
  vkDestroyShaderModule (device, fs_module, NULL);

  vk_check_error ("vkCreateGraphicsPipelines", res, FALSE);

  return TRUE;
}

static void
_update_descriptor_sets (Example *self)
{
  GulkanClient *client = gulkan_renderer_get_client (GULKAN_RENDERER (self));
  VkDevice device = gulkan_client_get_device_handle (client);

  uint32_t size = g_slist_length (self->inputs) + 1;

  VkWriteDescriptorSet *sets = g_malloc (sizeof (VkWriteDescriptorSet) * size);

  sets[0] = (VkWriteDescriptorSet) {
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .dstSet = self->descriptor_set,
    .dstBinding = 0,
    .dstArrayElement = 0,
    .descriptorCount = 1,
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .pTexelBufferView = NULL,
    .pBufferInfo = &(VkDescriptorBufferInfo){
      .buffer = gulkan_uniform_buffer_get_handle (self->ub),
      .offset = 0,
      .range = VK_WHOLE_SIZE,
    },
  };

  for (guint i = 0; i < g_slist_length (self->inputs); i++)
    {
      GSList* entry = g_slist_nth (self->inputs, i);
      TextureInput *input = entry->data;
      guint j = i + 1;
      sets[j] = (VkWriteDescriptorSet) {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = self->descriptor_set,
        .dstBinding = (uint32_t) input->channel + 1,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pTexelBufferView = NULL,
        .pImageInfo = &input->info,
      };
    }

  vkUpdateDescriptorSets (device, size, sets, 0, NULL);

  g_free (sets);
}

static gboolean
_init_texture_sampler (VkDevice device, VkFilter filter, VkSampler *sampler)
{
  VkSamplerMipmapMode mipmap_mode =
    (filter == VK_FILTER_LINEAR) ? VK_SAMPLER_MIPMAP_MODE_LINEAR :
                                   VK_SAMPLER_MIPMAP_MODE_NEAREST;

  VkSamplerCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .magFilter = filter,
    .minFilter = filter,
    .mipmapMode = mipmap_mode,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .anisotropyEnable = VK_FALSE,
    .maxAnisotropy = 16,
    .compareEnable = VK_FALSE,
    .compareOp = VK_COMPARE_OP_ALWAYS,
    .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    .unnormalizedCoordinates = VK_FALSE
  };

  VkResult res = vkCreateSampler (device, &info, NULL, sampler);
  vk_check_error ("vkCreateSampler", res, FALSE);

  return TRUE;
}

static void
_init_draw_cmd (GulkanSwapchainRenderer *renderer,
                VkCommandBuffer          cmd_buffer)
{
  Example *self = GULKAN_EXAMPLE (renderer);

  vkCmdBindPipeline (cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                     self->pipeline);

  VkPipelineLayout layout =
    gulkan_descriptor_pool_get_pipeline_layout (self->descriptor_pool);

  vkCmdBindDescriptorSets (cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           layout, 0, 1, &self->descriptor_set, 0, NULL);

  gulkan_vertex_buffer_draw_indexed (self->vb, cmd_buffer);
}

static void
_key_cb (GLFWwindow *window, int key, int scancode, int action, int mods)
{
  (void) scancode;
  (void) mods;

  Example *self = (Example *) glfwGetWindowUserPointer (window);
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
      self->should_quit = TRUE;
    }
  if (key == GLFW_KEY_F && action == GLFW_PRESS)
    {
      glfw_toggle_fullscreen (window,
                             &self->last_window_position,
                             &self->last_window_size);
    }
}

static void
_mouse_pos_cb (GLFWwindow* window, double x, double y)
{
  Example *self = (Example *) glfwGetWindowUserPointer (window);
  if (self->button_state == GLFW_PRESS)
    {
      VkExtent2D extent = gulkan_renderer_get_extent (GULKAN_RENDERER (self));
      self->ub_data.iMouse[0] = (float) x;
      self->ub_data.iMouse[1] = (float) extent.height - (float) y;
    }
}

static void
_mouse_button_cb (GLFWwindow* window, int button, int action, int mods)
{
  (void) mods;

  if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
      Example *self = (Example *) glfwGetWindowUserPointer (window);
      if (self->button_state != action)
        {
          self->button_state = action;
          if (action == GLFW_PRESS)
            {
              double x, y;
              glfwGetCursorPos(window, &x, &y);
              VkExtent2D extent =
                gulkan_renderer_get_extent (GULKAN_RENDERER (self));
              self->ub_data.iMouse[2] = (float) x;
              self->ub_data.iMouse[3] = (float) extent.height - (float) y;

              self->ub_data.iMouse[0] = self->ub_data.iMouse[2];
              self->ub_data.iMouse[1] = self->ub_data.iMouse[3];
            }
          else
            {
              self->ub_data.iMouse[2] = 0;
              self->ub_data.iMouse[3] = 0;
            }
        }
    }
}

static void
_framebuffer_size_cb (GLFWwindow* window, int w, int h)
{
  Example *self = (Example *) glfwGetWindowUserPointer (window);
  GulkanClient *client = gulkan_renderer_get_client (GULKAN_RENDERER (self));

  VkSurfaceKHR surface;

  VkInstance instance = gulkan_client_get_instance_handle (client);
  VkResult res = glfwCreateWindowSurface (instance, self->window,
                                          NULL, &surface);
  if (res != VK_SUCCESS)
    {
      g_printerr ("Creating surface failed.");
      return;
    }

  if (!gulkan_swapchain_renderer_resize (GULKAN_SWAPCHAIN_RENDERER (self),
                                         surface))
    g_warning ("Resize failed.");

  self->ub_data.iResolution[0] = w;
  self->ub_data.iResolution[1] = h;
}


static gboolean
_init_glfw (Example *self)
{
  glfwInit ();

  glfwWindowHint (GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint (GLFW_RESIZABLE, TRUE);
  glfwWindowHint (GLFW_AUTO_ICONIFY, GLFW_FALSE);

  VkExtent2D extent = gulkan_renderer_get_extent (GULKAN_RENDERER (self));
  self->window =
    glfwCreateWindow ((int) extent.width, (int) extent.height,
                      "Gulkan Toy", NULL, NULL);

  if (!self->window)
    return FALSE;

  glfwSetKeyCallback (self->window, _key_cb);

  glfwSetWindowUserPointer (self->window, self);

  glfwSetCursorPosCallback (self->window, _mouse_pos_cb);
  glfwSetMouseButtonCallback (self->window, _mouse_button_cb);

  return TRUE;
}

static gboolean
_iterate (gpointer _self)
{
  Example *self = (Example *) _self;

  glfwPollEvents ();
  if (glfwWindowShouldClose (self->window) || self->should_quit)
    {
      g_main_loop_quit (self->loop);
      return FALSE;
    }

  _update_uniform_buffer (self);

  return gulkan_renderer_draw (GULKAN_RENDERER (self));
}

static gboolean
_init_vertex_buffer (Example *self)
{
  GulkanClient *client = gulkan_renderer_get_client (GULKAN_RENDERER (self));
  GulkanDevice *device = gulkan_client_get_device (client);

  Vertex vertices[4] = {
    {{-1.f, -1.f}, {1.0, 1.0}},
    {{ 1.f, -1.f}, {0.f, 1.0}},
    {{ 1.f,  1.f}, {0.f, 0.f}},
    {{-1.f,  1.f}, {1.0, 0.f}}
  };

  self->vb = gulkan_vertex_buffer_new ();
  if (!gulkan_vertex_buffer_alloc_data (self->vb, device, vertices,
                                        sizeof (vertices)))
    return FALSE;

  if (!gulkan_vertex_buffer_alloc_index_data (self->vb, device, indices,
                                              sizeof (uint16_t),
                                              G_N_ELEMENTS (indices)))
    return FALSE;

  return TRUE;
}

static gboolean
_init_descriptors (Example *self)
{
  GulkanClient *client = gulkan_renderer_get_client (GULKAN_RENDERER (self));
  GulkanDevice *gulkan_device = gulkan_client_get_device (client);

  uint32_t size = g_slist_length (self->inputs) + 1;

  VkDescriptorSetLayoutBinding *bindings =
    g_malloc (sizeof (VkDescriptorSetLayoutBinding) * size);
  uint32_t set_count = 1;
  VkDescriptorPoolSize *pool_sizes =
    g_malloc (sizeof (VkDescriptorPoolSize) * size);

  bindings[0] = (VkDescriptorSetLayoutBinding) {
    .binding = 0,
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
  };
  pool_sizes[0] = (VkDescriptorPoolSize) {
    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = set_count,
  };

  for (guint i = 0; i < g_slist_length (self->inputs); i++)
    {
      guint j = i + 1;
      GSList* entry = g_slist_nth (self->inputs, i);
      TextureInput *input = entry->data;
      bindings[j] = (VkDescriptorSetLayoutBinding) {
        .binding = (uint32_t) input->channel + 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      };
      pool_sizes[j] = (VkDescriptorPoolSize) {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = set_count,
      };
    }

  VkDevice device = gulkan_device_get_handle (gulkan_device);
  self->descriptor_pool = gulkan_descriptor_pool_new (device, bindings, size,
                                                      pool_sizes, size,
                                                      set_count);
  if (!self->descriptor_pool)
    return FALSE;

  if (!gulkan_descriptor_pool_allocate_sets (self->descriptor_pool, 1,
                                            &self->descriptor_set))
    return FALSE;

  g_free (bindings);
  g_free (pool_sizes);

  return TRUE;
}

static gboolean
_init_textures (Example *self)
{
  GulkanClient *client = gulkan_renderer_get_client (GULKAN_RENDERER (self));
  GulkanDevice *gulkan_device = gulkan_client_get_device (client);
  VkDevice device = gulkan_device_get_handle (gulkan_device);

  for (guint i = 0; i < g_slist_length (self->inputs); i++)
    {
      GSList* entry = g_slist_nth (self->inputs, i);
      TextureInput *input = entry->data;
      g_debug ("Creating textue for %s", input->src);

      int w = gdk_pixbuf_get_width (input->pixbuf);
      int h = gdk_pixbuf_get_height (input->pixbuf);
      g_debug ("Got pixbuf with %dx%d", w, h);

      self->ub_data.iChannelResolution[input->channel][0] = w;
      self->ub_data.iChannelResolution[input->channel][1] = h;
      self->ub_data.iChannelResolution[input->channel][2] = 1;

      gboolean mipmapping = FALSE;
      VkFilter filter = VK_FILTER_LINEAR;
      if (g_strcmp0 (input->filter, "mipmap") == 0)
          mipmapping = TRUE;
      else if (g_strcmp0 (input->filter, "nearest") == 0)
          filter = VK_FILTER_NEAREST;

      input->texture =
        gulkan_texture_new_from_pixbuf (client, input->pixbuf,
                                        VK_FORMAT_R8G8B8A8_SRGB,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        mipmapping);

      if (!input->texture)
        {
          g_printerr ("Could not load texture.\n");
          return FALSE;
        }

      if (!_init_texture_sampler (device, filter, &input->sampler))
        return FALSE;

      input->info = (VkDescriptorImageInfo) {
        .sampler = input->sampler,
        .imageView = gulkan_texture_get_image_view (input->texture),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
      };
    }

  return TRUE;
}

static gboolean
_init (Example     *self,
       const gchar *shader_src)
{
  if (dump)
    g_print ("====\n%s\n====\n", shader_src);

  if (!_init_glfw (self))
    {
      g_printerr ("failed to initialize glfw\n");
      return FALSE;
    }

  GulkanClient *client = gulkan_client_new_glfw ();
  if (!client)
    {
      g_printerr ("Could not init gulkan.\n");
      return FALSE;
    }

  gulkan_renderer_set_client (GULKAN_RENDERER (self), client);

  GulkanInstance *gulkan_instance = gulkan_client_get_instance (client);
  GulkanDevice *gulkan_device = gulkan_client_get_device (client);

  VkInstance instance = gulkan_instance_get_handle (gulkan_instance);

  VkSurfaceKHR surface;
  VkResult res = glfwCreateWindowSurface (instance, self->window,
                                          NULL, &surface);
  vk_check_error ("glfwCreateWindowSurface", res, FALSE);

  if (!_init_vertex_buffer (self))
    return FALSE;

  self->ub = gulkan_uniform_buffer_new (gulkan_device, sizeof (UB));
  if (!self->ub)
    return FALSE;

  if (!_init_textures (self))
    return FALSE;

  if (!_init_descriptors (self))
    return FALSE;

  _update_descriptor_sets (self);

  if (!gulkan_swapchain_renderer_initialize (GULKAN_SWAPCHAIN_RENDERER (self),
                                             surface, background_color,
                                             (gconstpointer) shader_src))
    return FALSE;

  glfwSetFramebufferSizeCallback(self->window, _framebuffer_size_cb);

  return TRUE;
}

typedef struct Unsupported
{
  gboolean has_sound;
  gboolean has_multipass;
} Unsupported;

static void
_has_buffer_pass (JsonArray *array,
                  guint index,
                  JsonNode *node,
                  gpointer _unsupported)
{
  (void) array;
  (void) index;
  JsonObject *obj = json_node_get_object (node);
  const gchar *type = json_object_get_string_member (obj, "type");
  Unsupported *unsupported = _unsupported;
  if (g_strcmp0 (type, "sound") == 0)
    unsupported->has_sound = TRUE;
  else if (g_strcmp0 (type, "buffer") == 0)
    unsupported->has_multipass = TRUE;
}

static GString*
_get_or_create_cache ()
{
  GString *cache_path = g_string_new ("");
  g_string_printf (cache_path, "%s/gulkan-toy/", g_get_user_cache_dir ());
  if (g_mkdir_with_parents (cache_path->str, 0700) == -1)
    {
      g_printerr ("Unable to create cache directory %s\n", cache_path->str);
      g_string_free (cache_path, TRUE);
      return NULL;
    }
  return cache_path;
}

static gboolean
_cache_remote_file (const gchar *src, GFile *cached_file)
{
  GString *url = g_string_new ("https://shadertoy.com");
  g_string_append (url, src);

  g_print ("Fetching %s\n", url->str);

  GFile *remote_file = g_file_new_for_uri (url->str);
  GError *error = NULL;
  GFileInputStream *in_stream = g_file_read (remote_file, NULL, &error);
  if (error != NULL)
    {
      g_printerr ("Unable to download texture: %s\n", error->message);
      g_error_free (error);
      g_object_unref (remote_file);
      return FALSE;
    }

  error = NULL;
  GFileOutputStream *out_stream = g_file_create (cached_file,
                                                 G_FILE_CREATE_NONE,
                                                 NULL, &error);
  if (error != NULL)
    {
      g_printerr ("Unable to write file cache file: %s\n", error->message);
      g_error_free (error);
      g_object_unref (remote_file);
      return FALSE;
    }

  gssize n_bytes_written =
    g_output_stream_splice (G_OUTPUT_STREAM (out_stream),
                            G_INPUT_STREAM (in_stream),
                            (GOutputStreamSpliceFlags)
                              (G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET |
                               G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE),
                            NULL, &error);

  if (error != NULL)
    {
      g_printerr ("Unable to write to output stream: %s\n", error->message);
      g_error_free (error);
      return FALSE;
    }

  if (n_bytes_written == 0)
    {
      g_printerr ("No bytes written\n");
      return FALSE;
    }

   g_print ("Done.\n");

  g_object_unref (in_stream);
  g_object_unref (out_stream);
  g_object_unref (remote_file);
  g_string_free (url, TRUE);
  return TRUE;
}

static gboolean
_check_file_cache (const gchar *src, GString* cache_path)
{
  GFile *cached_file = g_file_new_for_path (cache_path->str);
  if (!g_file_query_exists (cached_file, NULL))
    if (!_cache_remote_file (src, cached_file))
      {
        g_object_unref (cached_file);
        return FALSE;
      }

  g_object_unref (cached_file);
  return TRUE;
}

static gboolean
_get_pixbuf_from_remote_path (TextureInput* input)
{
  GString* cache_path = _get_or_create_cache ();
  g_string_append (cache_path, g_path_get_basename (input->src));
  if (!_check_file_cache (input->src, cache_path))
    return FALSE;

  GError *error = NULL;
  GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file (cache_path->str, &error);

  if (!gdk_pixbuf_get_has_alpha (pixbuf))
    {
      input->pixbuf = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);
      g_object_unref (pixbuf);
    }
  else
    {
      input->pixbuf = pixbuf;
    }

  if (error != NULL)
    {
      g_printerr ("Unable to download texture: %s\n", error->message);
      g_error_free (error);
      return FALSE;
    }

  g_string_free (cache_path, TRUE);

  return TRUE;
}

static void
_parse_inputs (JsonArray *array,
               guint index,
               JsonNode *node,
               gpointer _self)
{
  (void) array;
  (void) index;
  JsonObject *obj = json_node_get_object (node);

  const gchar *ctype;
  if (json_object_has_member (obj, "ctype"))
    ctype = json_object_get_string_member (obj, "ctype");
  else if (json_object_has_member (obj, "type"))
    ctype = json_object_get_string_member (obj, "type");
  else
    {
      g_warning ("Input does not have a type!");
      return;
    }

  if (g_strcmp0 (ctype, "texture") != 0)
    {
      g_warning ("Non texture inputs are not supported yet.");
      return;
    }

  TextureInput *input = g_malloc (sizeof (TextureInput));

  if (json_object_has_member (obj, "src"))
    input->src = json_object_get_string_member (obj, "src");
  else if (json_object_has_member (obj, "filepath"))
    input->src = json_object_get_string_member (obj, "filepath");
  else
    {
      g_warning ("Input does not have a src!");
      g_free (input);
      return;
    }

  input->channel = (gint) json_object_get_int_member (obj, "channel");

  JsonObject *sampler_obj = json_object_get_object_member (obj, "sampler");
  input->filter = json_object_get_string_member (sampler_obj, "filter");
  input->wrap = json_object_get_string_member (sampler_obj, "wrap");
  input->internal = json_object_get_string_member (sampler_obj, "internal");
  input->vflip = json_object_get_boolean_member (sampler_obj, "vflip");
  input->srgb = json_object_get_boolean_member (sampler_obj, "srgb");
  input->sampler = VK_NULL_HANDLE;

  Example *self = _self;
  self->inputs = g_slist_append (self->inputs, input);
}

static void
gulkan_example_class_init (ExampleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = _finalize;

  GulkanSwapchainRendererClass *parent_class =
    GULKAN_SWAPCHAIN_RENDERER_CLASS (klass);
  parent_class->init_draw_cmd = _init_draw_cmd;
  parent_class->init_pipeline = _init_pipeline;
}

static gboolean
_init_from_json_node (Example  *self,
                      JsonNode *root)
{
  JsonObject *root_object = json_node_get_object (root);

  JsonObject *info_obj = json_object_get_object_member (root_object, "info");
  const gchar *name = json_object_get_string_member (info_obj, "name");
  const gchar *username = json_object_get_string_member (info_obj, "username");

  g_print ("Loading '%s' by %s.\n", name, username);

  if (!json_object_has_member (root_object, "renderpass"))
    {
      g_printerr ("JSON object has no render passes.\n");
      return FALSE;
    }

  JsonNode *renderpasses_node = json_object_get_member (root_object, "renderpass");
  JsonArray *renderpasses_array = json_node_get_array (renderpasses_node);

  if (json_array_get_length (renderpasses_array) > 1)
    {
      Unsupported unsupported = { FALSE, FALSE };
      json_array_foreach_element (renderpasses_array,
                                  (JsonArrayForeach) _has_buffer_pass,
                                  &unsupported);

      if (unsupported.has_multipass)
        {
          g_warning ("Multiple render passes are not supported yet.");
          return FALSE;
        }
      else if (unsupported.has_sound)
        {
          g_print ("Sound is not supported.\n");
        }
    }

  JsonNode *renderpass_node = json_array_get_element (renderpasses_array, 0);
  JsonObject *renderpass_object = json_node_get_object (renderpass_node);

  JsonNode *inputs_node = json_object_get_member (renderpass_object, "inputs");
  JsonArray *inputs_array = json_node_get_array (inputs_node);
  if (json_array_get_length (inputs_array) > 0)
    {
      json_array_foreach_element (inputs_array,
                                  (JsonArrayForeach) _parse_inputs, self);

      for (guint i = 0; i < g_slist_length (self->inputs); i++)
        {
          GSList* entry = g_slist_nth (self->inputs, i);
          TextureInput *input = entry->data;
          g_debug ("INPUT:\n channel: %d\n src: %s\n filter: %s\n wrap: %s\n"
                   " internal: %s\n vflip: %d\n srgb: %d",
                   input->channel, input->src, input->filter, input->wrap,
                   input->internal, input->vflip, input->srgb);

          if (!_get_pixbuf_from_remote_path (input))
            {
              g_print ("Could not get pixbuf from url.\n");
              return FALSE;
            }
        }
    }

  const gchar *src = json_object_get_string_member (renderpass_object, "code");

  if (!_init (self, src))
    {
      g_printerr ("Error: Could not init json.\n");
      return FALSE;
    }

  return TRUE;
}

static gboolean
_init_from_json (Example    *self,
                 const char *path)
{
  JsonParser *parser = json_parser_new ();

  g_print ("json: loading %s\n", path);

  GError *error = NULL;
  json_parser_load_from_file (parser, path, &error);
  if (error)
    {
      g_print ("Unable to parse `%s': %s\n", path, error->message);
      g_error_free (error);
      g_object_unref (parser);
      return FALSE;
    }

  JsonNode *root = json_parser_get_root (parser);

  gboolean ret = _init_from_json_node (self, root);

  g_object_unref (parser);

  return ret;
}

static gboolean
_init_from_glsl (Example *self,
                 const char *path)
{
  GFile *source_file = g_file_new_for_path (path);

  GError *error = NULL;
  GBytes *source_bytes = g_file_load_bytes (source_file, NULL, NULL, &error);
  if (error != NULL)
    {
      g_printerr ("Unable to read file: %s\n", error->message);
      g_error_free (error);
      return FALSE;
    }

  gsize source_size = 0;
  const gchar *src = g_bytes_get_data (source_bytes, &source_size);

  gboolean ret = _init (self, src);

  g_bytes_unref (source_bytes);
  g_object_unref (source_file);

  return ret;
}

static gboolean
_init_from_id (Example    *self,
               const char *id)
{
  if (strlen (id) != 6)
    g_warning ("ID '%s' has not 6 characters long.\n", id);

  GString *url = g_string_new (API_URL);
  g_string_append_printf (url, "/shaders/%s?key=%s", id, API_KEY);
  g_print ("Fetching %s.\n", id);
  g_debug ("Fetching %s", url->str);

  GFile *json_file = g_file_new_for_uri (url->str);

  GError *error = NULL;
  GBytes *json_bytes = g_file_load_bytes (json_file, NULL, NULL, &error);
  if (error != NULL)
    {
      g_printerr ("Unable to access API: %s\n", error->message);
      g_error_free (error);
      g_object_unref (json_file);
      return FALSE;
    }

  gsize json_size = 0;
  const gchar *json_string = g_bytes_get_data (json_bytes,
                                              &json_size);

  g_debug ("Got response %s", json_string);

  JsonParser *parser = json_parser_new ();

  error = NULL;
  json_parser_load_from_data (parser, json_string, (gssize) json_size, &error);
  if (error)
    {
      g_printerr ("Unable to parse `%s': %s\n", url->str, error->message);
      g_error_free (error);
      g_object_unref (parser);
      g_bytes_unref (json_bytes);
      g_object_unref (json_file);
      return FALSE;
    }

  JsonNode *root = json_parser_get_root (parser);
  JsonObject *root_object = json_node_get_object (root);

  if (json_object_has_member (root_object, "Error"))
    {
      const gchar *error_str =
        json_object_get_string_member (root_object, "Error");
      g_printerr ("API Error: %s.\n", error_str);
      g_object_unref (parser);
      g_bytes_unref (json_bytes);
      g_object_unref (json_file);
      return FALSE;
    }

  gboolean ret;
  if (json_object_has_member (root_object, "Shader"))
    {
      JsonNode *shader_node = json_object_get_member (root_object, "Shader");
      ret = _init_from_json_node (self, shader_node);
    }
  else
    {
      g_printerr ("JSON object has no 'Shader' member.\n");
      ret = FALSE;
    }

  g_object_unref (parser);
  g_bytes_unref (json_bytes);
  g_object_unref (json_file);

  return ret;
}

static gboolean
_init_from_url (Example    *self,
                const char *url)
{
  GString *id_string = g_string_new (url);
  g_string_erase (id_string, 0, (gssize) strlen (URL_PREFIX));

  if (id_string->len != 6)
    g_warning ("Parsed ID '%s' is not 6 characters long\n", id_string->str);

  gboolean ret = _init_from_id (self, id_string->str);

  g_string_free (id_string, TRUE);

  return ret;
}

static const gchar *summary =
  "Examples:\n\n"
  "Download shader by ID\n"
  "gulkan-toy 3lsSzf\n\n"
  "Download shader by URL\n"
  "gulkan-toy https://www.shadertoy.com/view/4tjGRh\n\n"
  "Load JSON file (from browser plugin):\n"
  "gulkan-toy 4sfGDB.json\n\n"
  "Load GLSL file:\n"
  "gulkan-toy XtlSD7.frag\n\n"
  "More recommended toys:\n"
  "ld3Gz2 MdX3zr XslGRr ldl3zN MsfGRr MdfGRr ltlSWf 4dSGW1\n"
  "4ds3WS tsBXW3 XsSSRW lsXGzH MdBGzG 4sS3zG llj3Rz lslyRn";

static GOptionEntry entries[] =
{
  { "dump", 'd', 0, G_OPTION_ARG_NONE, &dump, "Dump shader to stdout.", NULL },
  { NULL, 0, 0, 0, NULL, NULL, NULL },
};

int
main (int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *context;

  context = g_option_context_new ("[FILE/URL/ID] - render Shadertoy shaders in Gulkan");
  g_option_context_add_main_entries (context, entries, NULL);

  g_option_context_set_summary (context, summary);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("Error: %s\n", error->message);
      g_printerr ("%s", g_option_context_get_help (context, TRUE, NULL));
      return EXIT_FAILURE;
    }

  if (argc != 2)
    {
      g_printerr ("%s", g_option_context_get_help (context, TRUE, NULL));
      return EXIT_FAILURE;
    }

  gboolean (*init) (Example *s, const char *p) = NULL;

    if (g_str_has_prefix (argv[1], URL_PREFIX))
      init = _init_from_url;
    else if (g_str_has_suffix (argv[1], ".json"))
      init = _init_from_json;
    else if (strlen (argv[1]) == 6)
      init = _init_from_id;
    else if (g_str_has_suffix (argv[1], ".frag") ||
             g_str_has_suffix (argv[1], ".glsl"))
      init = _init_from_glsl;

  if (!init)
    {
      g_printerr ("%s", g_option_context_get_help (context, TRUE, NULL));
      return EXIT_FAILURE;
    }

  Example *self = (Example *) g_object_new (GULKAN_TYPE_EXAMPLE, 0);
  /* Silence clang warning */
  g_assert (GULKAN_IS_EXAMPLE (self));
  if (!init (self, argv[1]))
    {
      g_object_unref (self);
      return EXIT_FAILURE;
    }

  g_timeout_add (1, _iterate, self);
  g_main_loop_run (self->loop);

  g_object_unref (self);

  return EXIT_SUCCESS;
}
