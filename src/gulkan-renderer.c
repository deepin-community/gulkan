/*
 * gulkan
 * Copyright 2020 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "gulkan-renderer.h"

#include <sys/time.h>
#include <vulkan/vulkan.h>

typedef struct _GulkanRendererPrivate
{
  GObject parent;

  GulkanContext *context;

  struct timeval start;

  VkExtent2D extent;

} GulkanRendererPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GulkanRenderer, gulkan_renderer, G_TYPE_OBJECT)

static void
gulkan_renderer_finalize (GObject *gobject);

static void
gulkan_renderer_class_init (GulkanRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gulkan_renderer_finalize;
}

static void
gulkan_renderer_init (GulkanRenderer *self)
{
  GulkanRendererPrivate *priv = gulkan_renderer_get_instance_private (self);
  gettimeofday (&priv->start, NULL);
}

static void
gulkan_renderer_finalize (GObject *gobject)
{
  GulkanRenderer        *self = GULKAN_RENDERER (gobject);
  GulkanRendererPrivate *priv = gulkan_renderer_get_instance_private (self);
  if (priv->context)
    g_object_unref (priv->context);
  G_OBJECT_CLASS (gulkan_renderer_parent_class)->finalize (gobject);
}

/**
 * gulkan_renderer_get_context:
 * @self: a #GulkanRenderer
 *
 * Returns: (transfer none): a #GulkanContext
 */
GulkanContext *
gulkan_renderer_get_context (GulkanRenderer *self)
{
  GulkanRendererPrivate *priv = gulkan_renderer_get_instance_private (self);
  return priv->context;
}

void
gulkan_renderer_set_context (GulkanRenderer *self, GulkanContext *context)
{
  GulkanRendererPrivate *priv = gulkan_renderer_get_instance_private (self);
  if (priv->context != context)
    priv->context = g_object_ref (context);
}

/**
 * gulkan_renderer_get_extent:
 * @self: a #GulkanRenderer
 *
 * Returns: (transfer none): a #VkExtent2D
 */
VkExtent2D
gulkan_renderer_get_extent (GulkanRenderer *self)
{
  GulkanRendererPrivate *priv = gulkan_renderer_get_instance_private (self);
  return priv->extent;
}

void
gulkan_renderer_set_extent (GulkanRenderer *self, VkExtent2D extent)
{
  GulkanRendererPrivate *priv = gulkan_renderer_get_instance_private (self);
  priv->extent = extent;
}

float
gulkan_renderer_get_aspect (GulkanRenderer *self)
{
  VkExtent2D extent = gulkan_renderer_get_extent (self);
  return (float) extent.width / (float) extent.height;
}

static int64_t
_timeval_to_msec (struct timeval *tv)
{
  return tv->tv_sec * 1000 + tv->tv_usec / 1000;
}

int64_t
gulkan_renderer_get_msec_since_start (GulkanRenderer *self)
{
  GulkanRendererPrivate *priv = gulkan_renderer_get_instance_private (self);

  struct timeval now;
  gettimeofday (&now, NULL);
  return _timeval_to_msec (&now) - _timeval_to_msec (&priv->start);
}

gboolean
gulkan_renderer_draw (GulkanRenderer *self)
{
  GulkanRendererClass *klass = GULKAN_RENDERER_GET_CLASS (self);
  if (klass->draw == NULL)
    return FALSE;
  return klass->draw (self);
}
