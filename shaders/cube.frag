/*
 * gulkan
 * Copyright 2020 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#version 460 core

layout (location = 0) in vec3 positon_view;
layout (location = 1) in vec3 normal_view;
layout (location = 2) in vec4 diffuse_color;

layout (location = 0) out vec4 out_color;

const vec3 position_light = vec3 (1.0, 1.0, 1.0);
const vec3 specular_color = vec3 (1.0, 1.0, 1.0);

const float shininess = 16.0;

void
main ()
{
  vec3 direction_light = normalize (position_light - positon_view);
  vec3 normal = normalize (normal_view);

  float lambertian = max (dot (direction_light, normal), 0.0);
  float specular = 0.0;

  if (lambertian > 0.0)
    {
      vec3 direction_reflection = reflect (-direction_light, normal);
      vec3 direction_view = normalize (-positon_view);

      float specular_angle =
        max (dot (direction_reflection, direction_view), 0.0);
      specular = pow (specular_angle, shininess);
    }

  out_color = vec4 (lambertian * diffuse_color.xyz +
                      lambertian * specular * specular_color,
                    1.0);
}
