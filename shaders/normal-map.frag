/*
 * gulkan
 * Copyright 2020 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * Author: Manas Chaudhary <manaschaudhary2000@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#version 460 core

layout (binding = 1) uniform sampler2D diffuse_texture;
layout (binding = 2) uniform sampler2D normal_texture;

layout (location = 0) in vec3 positon_view;
layout (location = 1) in vec3 normal_view;
layout (location = 2) in vec2 uv;
layout (location = 3) in vec3 tangent;

layout (location = 0) out vec4 out_color;

const vec3 position_light = vec3 (1.0, 1.0, 1.0);
const vec4 specular_color = vec4 (1.0, 1.0, 1.0, 1.0);

const float shininess = 16.0;

void
main ()
{
  vec3 direction_light = normalize (position_light - positon_view);
  vec3 normal = normalize (normal_view);

  vec3 normal_map_sample = texture (normal_texture, uv).rgb;
  vec3 uncompressed_normal = 2.0f * normal_map_sample - 1.0f;
  vec3 N = normal;
  vec3 T = normalize (tangent - dot (tangent, N) * N);
  vec3 B = cross (N, T);

  mat3 TBN = mat3 (T, B, N);

  normal = normalize (TBN * uncompressed_normal);

  float lambertian = max (dot (direction_light, normal), 0.0);
  float specular = 0.0;

  if (lambertian > 0.0)
    {
      vec3 direction_reflection = reflect (-direction_light, normal);
      vec3 direction_view = normalize (-positon_view);

      float specular_angle = max (dot (direction_reflection, direction_view),
                                  0.0);
      specular = pow (specular_angle, shininess);
    }

  out_color = lambertian * texture (diffuse_texture, uv)
              + specular * specular_color;
}
