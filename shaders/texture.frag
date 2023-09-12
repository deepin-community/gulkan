/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#version 460 core

layout (binding = 0) uniform sampler2D image;
layout (location = 0) in vec2 uv;
layout (location = 0) out vec4 out_color;

void main ()
{
  out_color = texture (image, uv);
}

