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

#include <gulkan.h>

#include <shaderc/shaderc.h>

#define GULKAN_TYPE_EXAMPLE gulkan_example_get_type ()
G_DECLARE_FINAL_TYPE (Example,
                      gulkan_example,
                      GULKAN,
                      EXAMPLE,
                      GulkanSwapchainRenderer)

static const gchar *API_URL = "https://www.shadertoy.com/api/v1";
static const gchar *API_KEY = "ft8KMr";
static const gchar *URL_PREFIX = "https://www.shadertoy.com/view/";

static gboolean dump = FALSE;

// clang-format off
typedef struct
{
  float iResolution[3];           // viewport resolution (in pixels)
  float iTime;                    // shader playback time (in seconds)
  float iMouse[4];                // pointer pixel coords. xy: current (if MLB down), zw: click
  float iChannelResolution[4][3]; // channel resolution (in pixels)
  float iTimeDelta;               // render time (in seconds)
  int   iFrame;                   // shader playback frame
  float iChannelTime[4];          // channel playback time (in seconds)
  float iDate[4];                 // (year, month, day, time in seconds)
  float iSampleRate;              // sound sample rate (i.e., 44100)
} UB;
// clang-format on

typedef struct
{
  float position[2];
  float uv[2];
} Vertex;

static const uint16_t indices[6] = {0, 1, 2, 2, 3, 0};

static const VkClearColorValue background_color = {
  .float32 = {0.05f, 0.05f, 0.05f, 1.0f},
};

typedef struct TextureInput
{
  gint         channel;
  const gchar *src;

  const gchar *filter;
  const gchar *wrap;
  const gchar *internal;

  gboolean vflip;
  gboolean srgb;

  GdkPixbuf *pixbuf;

  GulkanTexture *texture;

  VkDescriptorImageInfo info;
} TextureInput;

struct _Example
{
  GulkanSwapchainRenderer parent;

  GulkanVertexBuffer *vb;

  GulkanUniformBuffer  *ub;
  GulkanDescriptorPool *descriptor_pool;

  GulkanPipeline      *pipeline;
  GulkanDescriptorSet *descriptor_set;

  GMainLoop    *loop;
  GulkanWindow *window;
  gboolean      should_quit;

  gboolean   is_left_button_pressed;
  VkOffset2D last_cursor_position;

  GSList *inputs;

  gchar *shader_src;

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

  GulkanContext *context = gulkan_renderer_get_context (GULKAN_RENDERER (self));
  if (context)
    {
      g_object_unref (self->vb);
      gulkan_device_wait_idle (gulkan_context_get_device (context));

      g_object_unref (self->descriptor_pool);
      g_object_unref (self->pipeline);
      g_object_unref (self->ub);

      for (guint i = 0; i < g_slist_length (self->inputs); i++)
        {
          GSList       *entry = g_slist_nth (self->inputs, i);
          TextureInput *input = entry->data;
          g_object_unref (input->texture);
        }
    }

  for (guint i = 0; i < g_slist_length (self->inputs); i++)
    {
      GSList       *entry = g_slist_nth (self->inputs, i);
      TextureInput *input = entry->data;
      g_object_unref (input->pixbuf);
    }

  if (self->inputs)
    g_slist_free_full (self->inputs, g_free);

  g_free (self->shader_src);

  G_OBJECT_CLASS (gulkan_example_parent_class)->finalize (gobject);

  // Wayland surface needs to be deinited after vk swapchain
  g_object_unref (self->window);
}

static void
gulkan_example_init (Example *self)
{
  gulkan_renderer_set_extent (GULKAN_RENDERER (self),
                              (VkExtent2D){.width = 1280, .height = 720});
  self->should_quit = FALSE;
  self->loop = g_main_loop_new (NULL, FALSE);
  self->inputs = NULL;
  self->descriptor_set = VK_NULL_HANDLE;
  self->descriptor_pool = NULL;
  self->shader_src = NULL;
  gulkan_swapchain_renderer_initialize (GULKAN_SWAPCHAIN_RENDERER (self),
                                        background_color, NULL);
}

static void
_update_uniform_buffer (Example *self)
{
  int64_t t = gulkan_renderer_get_msec_since_start (GULKAN_RENDERER (self));
  self->ub_data.iTime = (float) t / 1000.0f;
  gulkan_uniform_buffer_update (self->ub, (gpointer) &self->ub_data);
}

static gboolean
_load_resource (const gchar *path, GBytes **res)
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
_build_shader (Example *self, VkShaderModule *module)
{
  if (dump)
    g_print ("====\n%s\n====\n", self->shader_src);

  GBytes *template_bytes;
  if (!_load_resource ("/shaders/toy.frag.template", &template_bytes))
    return FALSE;

  gsize        template_size = 0;
  const gchar *template_string = g_bytes_get_data (template_bytes,
                                                   &template_size);

  GString *source_str = g_string_new_len (template_string,
                                          (gssize) template_size);

  for (guint i = 0; i < g_slist_length (self->inputs); i++)
    {
      GSList       *entry = g_slist_nth (self->inputs, i);
      TextureInput *input = entry->data;
      g_string_append_printf (source_str,
                              "layout (binding = %d) uniform sampler2D "
                              "iChannel%d;\n",
                              input->channel + 1, input->channel);
    }

  g_string_append (source_str, self->shader_src);

  g_debug ("%s", source_str->str);

  shaderc_compiler_t           compiler = shaderc_compiler_initialize ();
  shaderc_compilation_result_t result
    = shaderc_compile_into_spv (compiler, source_str->str, source_str->len,
                                shaderc_glsl_fragment_shader, "main.frag",
                                "main", NULL);

  shaderc_compilation_status status
    = shaderc_result_get_compilation_status (result);

  if (status != shaderc_compilation_status_success)
    {
      g_print ("Result code %d\n", (int) status);
      g_print ("shaderc error:\n%s\n",
               shaderc_result_get_error_message (result));
      shaderc_result_release (result);
      shaderc_compiler_release (compiler);
      return FALSE;
    }

  const char *bytes = shaderc_result_get_bytes (result);
  size_t      len = shaderc_result_get_length (result);

  /* Make clang happy and copy the data to fix alignment */
  uint32_t *code = g_malloc (len);
  memcpy (code, bytes, len);

  VkShaderModuleCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = len,
    .pCode = code,
  };

  GulkanContext *context = gulkan_renderer_get_context (GULKAN_RENDERER (self));
  VkDevice       device = gulkan_context_get_device_handle (context);

  VkResult res = vkCreateShaderModule (device, &info, NULL, module);
  vk_check_error ("vkCreateShaderModule", res, FALSE);

  g_free (code);

  shaderc_result_release (result);
  shaderc_compiler_release (compiler);

  g_string_free (source_str, TRUE);

  return TRUE;
}

static gboolean
_init_pipeline (GulkanSwapchainRenderer *renderer, gconstpointer data)
{
  (void) data;

  Example *self = GULKAN_EXAMPLE (renderer);

  GulkanContext *context = gulkan_renderer_get_context (GULKAN_RENDERER (self));
  GulkanDevice  *device = gulkan_context_get_device (context);

  VkShaderModule vs_module;
  if (!gulkan_device_create_shader_module (device, "/shaders/toy.vert.spv",
                                           &vs_module))
    return FALSE;

  VkShaderModule fs_module;
  if (!_build_shader (self, &fs_module))
    return FALSE;

  VkExtent2D extent = gulkan_renderer_get_extent (GULKAN_RENDERER (self));

  self->ub_data.iResolution[0] = (float) extent.width;
  self->ub_data.iResolution[1] = (float) extent.height;
  self->ub_data.iResolution[2] = 1;

  GulkanPipelineConfig config = {
    .sample_count = VK_SAMPLE_COUNT_1_BIT,
    .vertex_shader_uri = "/shaders/toy.vert.spv",
    .fragment_shader = fs_module,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .attribs = (VkVertexInputAttributeDescription[]) {
      {
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof (Vertex, position),
      },
      {
        .location = 1,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof (Vertex, uv),
      },
    },
    .attrib_count = 2,
    .bindings = &(VkVertexInputBindingDescription) {
      .binding = 0,
      .stride = sizeof(Vertex),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    },
    .binding_count = 1,
    .blend_attachments = (VkPipelineColorBlendAttachmentState[]){
      {
        .colorWriteMask =
          VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        },
      },
    .rasterization_state = &(VkPipelineRasterizationStateCreateInfo){
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_FRONT_BIT,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
      .lineWidth = 1.0f,
    },
    .dynamic_viewport = TRUE,
  };

  GulkanRenderPass *pass = gulkan_swapchain_renderer_get_render_pass (renderer);
  self->pipeline = gulkan_pipeline_new (context, self->descriptor_pool, pass,
                                        &config);

  return TRUE;
}

static void
_update_descriptor_sets (Example *self)
{
  gulkan_descriptor_set_update_buffer (self->descriptor_set, 0, self->ub);

  for (guint i = 0; i < g_slist_length (self->inputs); i++)
    {
      GSList       *entry = g_slist_nth (self->inputs, i);
      TextureInput *input = entry->data;
      gulkan_descriptor_set_update_texture_at (self->descriptor_set, i + 1,
                                               (uint32_t) input->channel + 1,
                                               input->texture);
    }
}

static void
_init_draw_cmd (GulkanSwapchainRenderer *renderer, VkCommandBuffer cmd_buffer)
{
  Example *self = GULKAN_EXAMPLE (renderer);

  gulkan_pipeline_bind (self->pipeline, cmd_buffer);

  VkPipelineLayout layout
    = gulkan_descriptor_pool_get_pipeline_layout (self->descriptor_pool);

  gulkan_descriptor_set_bind (self->descriptor_set, layout, cmd_buffer);

  gulkan_vertex_buffer_draw_indexed (self->vb, cmd_buffer);
}

static void
_key_cb (GulkanWindow *window, GulkanKeyEvent *event, Example *self)
{
  (void) self;

  if (!event->is_pressed)
    {
      return;
    }

  if (event->key == XKB_KEY_Escape)
    {
      self->should_quit = TRUE;
    }
  else if (event->key == XKB_KEY_f)
    {
      gulkan_window_toggle_fullscreen (window);
    }
}

static void
_pointer_position_cb (GulkanWindow        *window,
                      GulkanPositionEvent *event,
                      Example             *self)
{
  (void) window;

  self->last_cursor_position = event->offset;

  if (self->is_left_button_pressed)
    {
      VkExtent2D extent = gulkan_renderer_get_extent (GULKAN_RENDERER (self));
      self->ub_data.iMouse[0] = (float) event->offset.x;
      self->ub_data.iMouse[1] = (float) extent.height - (float) event->offset.y;
    }
}

static void
_pointer_button_cb (GulkanWindow      *window,
                    GulkanButtonEvent *event,
                    Example           *self)
{
  (void) window;

  // Use left pointer button
  if (event->button != BTN_LEFT)
    return;

  // Return on no state change
  if (self->is_left_button_pressed == event->is_pressed)
    return;

  self->is_left_button_pressed = event->is_pressed;

  if (event->is_pressed)
    {
      VkExtent2D extent = gulkan_renderer_get_extent (GULKAN_RENDERER (self));
      self->ub_data.iMouse[2] = (float) self->last_cursor_position.x;
      self->ub_data.iMouse[3] = (float) extent.height
                                - (float) self->last_cursor_position.y;

      self->ub_data.iMouse[0] = self->ub_data.iMouse[2];
      self->ub_data.iMouse[1] = self->ub_data.iMouse[3];
    }
  else
    {
      self->ub_data.iMouse[2] = 0;
      self->ub_data.iMouse[3] = 0;
    }
}

static void
_configure_cb (GulkanWindow *window, GulkanConfigureEvent *event, Example *self)
{
  GulkanContext *context = gulkan_renderer_get_context (GULKAN_RENDERER (self));
  VkInstance     instance = gulkan_context_get_instance_handle (context);

  VkSurfaceKHR surface;
  if (gulkan_window_create_surface (window, instance, &surface) != VK_SUCCESS)
    {
      g_printerr ("Creating surface failed.");
      return;
    }

  if (!gulkan_swapchain_renderer_resize (GULKAN_SWAPCHAIN_RENDERER (self),
                                         surface, event->extent))
    g_warning ("Resize failed.");

  self->ub_data.iResolution[0] = (float) event->extent.width;
  self->ub_data.iResolution[1] = (float) event->extent.height;
}

static void
_close_cb (GulkanWindow *window, Example *self)
{
  (void) window;
  g_main_loop_quit (self->loop);
}

static gboolean
_iterate (gpointer _self)
{
  Example *self = (Example *) _self;

  gulkan_window_poll_events (self->window);

  if (self->should_quit)
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
  GulkanContext *context = gulkan_renderer_get_context (GULKAN_RENDERER (self));
  GulkanDevice  *device = gulkan_context_get_device (context);

  // clang-format off
  Vertex vertices[4] = {
    {{-1.f, -1.f}, {1.0, 1.0}},
    {{ 1.f, -1.f}, {0.f, 1.0}},
    {{ 1.f,  1.f}, {0.f, 0.f}},
    {{-1.f,  1.f}, {1.0, 0.f}}
  };
  // clang-format on

  self->vb = gulkan_vertex_buffer_new (device,
                                       VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  if (!gulkan_vertex_buffer_alloc_data (self->vb, vertices, sizeof (vertices)))
    return FALSE;

  if (!gulkan_vertex_buffer_alloc_index_data (self->vb, indices,
                                              VK_INDEX_TYPE_UINT16,
                                              G_N_ELEMENTS (indices)))
    return FALSE;

  return TRUE;
}

static gboolean
_init_descriptors (Example *self)
{
  GulkanContext *context = gulkan_renderer_get_context (GULKAN_RENDERER (self));

  uint32_t size = g_slist_length (self->inputs) + 1;

  VkDescriptorSetLayoutBinding *bindings
    = g_malloc (sizeof (VkDescriptorSetLayoutBinding) * size);

  bindings[0] = (VkDescriptorSetLayoutBinding){
    .binding = 0,
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
  };

  for (guint i = 0; i < g_slist_length (self->inputs); i++)
    {
      guint         j = i + 1;
      GSList       *entry = g_slist_nth (self->inputs, i);
      TextureInput *input = entry->data;
      bindings[j] = (VkDescriptorSetLayoutBinding){
        .binding = (uint32_t) input->channel + 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      };
    }

  self->descriptor_pool = gulkan_descriptor_pool_new (context, bindings, size,
                                                      1);
  if (!self->descriptor_pool)
    return FALSE;

  self->descriptor_set
    = gulkan_descriptor_pool_create_set (self->descriptor_pool);

  g_free (bindings);

  return TRUE;
}

static gboolean
_init_textures (Example *self)
{
  GulkanContext *context = gulkan_renderer_get_context (GULKAN_RENDERER (self));
  for (guint i = 0; i < g_slist_length (self->inputs); i++)
    {
      GSList       *entry = g_slist_nth (self->inputs, i);
      TextureInput *input = entry->data;
      g_debug ("Creating textue for %s", input->src);

      int w = gdk_pixbuf_get_width (input->pixbuf);
      int h = gdk_pixbuf_get_height (input->pixbuf);
      g_debug ("Got pixbuf with %dx%d", w, h);

      self->ub_data.iChannelResolution[input->channel][0] = (float) w;
      self->ub_data.iChannelResolution[input->channel][1] = (float) h;
      self->ub_data.iChannelResolution[input->channel][2] = 1;

      gboolean mipmapping = FALSE;
      VkFilter filter = VK_FILTER_LINEAR;
      if (g_strcmp0 (input->filter, "mipmap") == 0)
        mipmapping = TRUE;
      else if (g_strcmp0 (input->filter, "nearest") == 0)
        filter = VK_FILTER_NEAREST;

      input->texture = gulkan_texture_new_from_pixbuf (
        context, input->pixbuf, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipmapping);

      if (!input->texture)
        {
          g_printerr ("Could not load texture.\n");
          return FALSE;
        }

      if (!gulkan_texture_init_sampler (input->texture, filter,
                                        VK_SAMPLER_ADDRESS_MODE_REPEAT))
        return FALSE;

      input->info = (VkDescriptorImageInfo){
        .sampler = gulkan_texture_get_sampler (input->texture),
        .imageView = gulkan_texture_get_image_view (input->texture),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    }

  return TRUE;
}

static gboolean
_init (Example *self)
{
  VkExtent2D extent = gulkan_renderer_get_extent (GULKAN_RENDERER (self));
  self->window = gulkan_window_new (extent, "Gulkan Cube");
  if (!self->window)
    {
      g_printerr ("Could not initialize window.\n");
      return FALSE;
    }

  GSList *instance_ext_list = gulkan_window_required_extensions (self->window);

  const gchar *device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };

  GSList *device_ext_list = NULL;
  for (uint64_t i = 0; i < G_N_ELEMENTS (device_extensions); i++)
    {
      char *device_ext = g_strdup (device_extensions[i]);
      device_ext_list = g_slist_append (device_ext_list, device_ext);
    }

  GulkanContext *context
    = gulkan_context_new_from_extensions (instance_ext_list, device_ext_list,
                                          VK_NULL_HANDLE);

  if (!gulkan_window_has_support (self->window, context))
    {
      g_printerr ("Window surface extension support check failed.\n");
      return FALSE;
    }

  g_slist_free (instance_ext_list);
  g_slist_free (device_ext_list);

  gulkan_renderer_set_context (GULKAN_RENDERER (self), context);

  if (!_init_vertex_buffer (self))
    return FALSE;

  GulkanDevice *gulkan_device = gulkan_context_get_device (context);
  self->ub = gulkan_uniform_buffer_new (gulkan_device, sizeof (UB));
  if (!self->ub)
    return FALSE;

  if (!_init_textures (self))
    return FALSE;

  if (!_init_descriptors (self))
    return FALSE;

  _update_descriptor_sets (self);

  g_signal_connect (self->window, "configure", (GCallback) _configure_cb, self);
  g_signal_connect (self->window, "pointer-position",
                    (GCallback) _pointer_position_cb, self);
  g_signal_connect (self->window, "pointer-button",
                    (GCallback) _pointer_button_cb, self);
  g_signal_connect (self->window, "close", (GCallback) _close_cb, self);
  g_signal_connect (self->window, "key", (GCallback) _key_cb, self);

  return TRUE;
}

typedef struct Unsupported
{
  gboolean has_sound;
  gboolean has_multipass;
} Unsupported;

static void
_has_buffer_pass (JsonArray *array,
                  guint      index,
                  JsonNode  *node,
                  gpointer   _unsupported)
{
  (void) array;
  (void) index;
  JsonObject  *obj = json_node_get_object (node);
  const gchar *type = json_object_get_string_member (obj, "type");
  Unsupported *unsupported = _unsupported;
  if (g_strcmp0 (type, "sound") == 0)
    unsupported->has_sound = TRUE;
  else if (g_strcmp0 (type, "buffer") == 0)
    unsupported->has_multipass = TRUE;
}

static GString *
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

  GFile            *remote_file = g_file_new_for_uri (url->str);
  GError           *error = NULL;
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
                                                 G_FILE_CREATE_NONE, NULL,
                                                 &error);
  if (error != NULL)
    {
      g_printerr ("Unable to write file cache file: %s\n", error->message);
      g_error_free (error);
      g_object_unref (remote_file);
      return FALSE;
    }

  gssize n_bytes_written = g_output_stream_splice (
    G_OUTPUT_STREAM (out_stream), G_INPUT_STREAM (in_stream),
    (GOutputStreamSpliceFlags) (G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET
                                | G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE),
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
_check_file_cache (const gchar *src, GString *cache_path)
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
_get_pixbuf_from_remote_path (TextureInput *input)
{
  GString *cache_path = _get_or_create_cache ();
  g_string_append (cache_path, g_path_get_basename (input->src));
  if (!_check_file_cache (input->src, cache_path))
    return FALSE;

  GError    *error = NULL;
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
_parse_inputs (JsonArray *array, guint index, JsonNode *node, gpointer _self)
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

  Example *self = _self;
  self->inputs = g_slist_append (self->inputs, input);
}

static void
gulkan_example_class_init (ExampleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = _finalize;

  GulkanSwapchainRendererClass *parent_class
    = GULKAN_SWAPCHAIN_RENDERER_CLASS (klass);
  parent_class->init_draw_cmd = _init_draw_cmd;
  parent_class->init_pipeline = _init_pipeline;
}

static gboolean
_init_from_json_node (Example *self, JsonNode *root)
{
  JsonObject *root_object = json_node_get_object (root);

  JsonObject  *info_obj = json_object_get_object_member (root_object, "info");
  const gchar *name = json_object_get_string_member (info_obj, "name");
  const gchar *username = json_object_get_string_member (info_obj, "username");

  g_print ("Loading '%s' by %s.\n", name, username);

  if (!json_object_has_member (root_object, "renderpass"))
    {
      g_printerr ("JSON object has no render passes.\n");
      return FALSE;
    }

  JsonNode  *renderpasses_node = json_object_get_member (root_object,
                                                         "renderpass");
  JsonArray *renderpasses_array = json_node_get_array (renderpasses_node);

  if (json_array_get_length (renderpasses_array) > 1)
    {
      Unsupported unsupported = {FALSE, FALSE};
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

  JsonNode   *renderpass_node = json_array_get_element (renderpasses_array, 0);
  JsonObject *renderpass_object = json_node_get_object (renderpass_node);

  JsonNode  *inputs_node = json_object_get_member (renderpass_object, "inputs");
  JsonArray *inputs_array = json_node_get_array (inputs_node);
  if (json_array_get_length (inputs_array) > 0)
    {
      json_array_foreach_element (inputs_array,
                                  (JsonArrayForeach) _parse_inputs, self);

      for (guint i = 0; i < g_slist_length (self->inputs); i++)
        {
          GSList       *entry = g_slist_nth (self->inputs, i);
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
  self->shader_src = g_strdup (src);

  if (!_init (self))
    {
      g_printerr ("Error: Could not init json.\n");
      return FALSE;
    }

  return TRUE;
}

static gboolean
_init_from_json (Example *self, const char *path)
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
_init_from_glsl (Example *self, const char *path)
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

  gsize        source_size = 0;
  const gchar *src = g_bytes_get_data (source_bytes, &source_size);
  self->shader_src = g_strdup (src);

  gboolean ret = _init (self);

  g_bytes_unref (source_bytes);
  g_object_unref (source_file);

  return ret;
}

static gboolean
_init_from_id (Example *self, const char *id)
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

  gsize        json_size = 0;
  const gchar *json_string = g_bytes_get_data (json_bytes, &json_size);

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

  JsonNode   *root = json_parser_get_root (parser);
  JsonObject *root_object = json_node_get_object (root);

  if (json_object_has_member (root_object, "Error"))
    {
      const gchar *error_str = json_object_get_string_member (root_object,
                                                              "Error");
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
_init_from_url (Example *self, const char *url)
{
  GString *id_string = g_string_new (url);
  g_string_erase (id_string, 0, (gssize) strlen (URL_PREFIX));

  if (id_string->len != 6)
    g_warning ("Parsed ID '%s' is not 6 characters long\n", id_string->str);

  gboolean ret = _init_from_id (self, id_string->str);

  g_string_free (id_string, TRUE);

  return ret;
}

static const gchar *summary
  = "Examples:\n\n"
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

static GOptionEntry entries[] = {
  {"dump", 'd', 0, G_OPTION_ARG_NONE, &dump, "Dump shader to stdout.", NULL},
  {NULL, 0, 0, 0, NULL, NULL, NULL},
};

int
main (int argc, char *argv[])
{
  GError         *error = NULL;
  GOptionContext *context;

  context = g_option_context_new ("[FILE/URL/ID] - render Shadertoy shaders in "
                                  "Gulkan");
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

  gboolean (*init) (Example * s, const char *p) = NULL;

  if (g_str_has_prefix (argv[1], URL_PREFIX))
    init = _init_from_url;
  else if (g_str_has_suffix (argv[1], ".json"))
    init = _init_from_json;
  else if (strlen (argv[1]) == 6)
    init = _init_from_id;
  else if (g_str_has_suffix (argv[1], ".frag")
           || g_str_has_suffix (argv[1], ".glsl"))
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
