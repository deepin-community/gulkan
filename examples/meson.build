# gulkan samples

subdir('common')

if libdrm_amdgpu_dep.found()
  executable(
    'dmabuf', ['dmabuf.c', test_resources, shader_resources],
    dependencies: [gulkan_deps, libdrm_dep, libdrm_amdgpu_dep, glfw_dep],
    link_with: gulkan_lib,
    link_whole: common_lib,
    include_directories: gulkan_inc,
    install: false)
endif

executable(
  'pixbuf', ['pixbuf.c', test_resources, shader_resources],
  dependencies: [gulkan_deps, glfw_dep],
  link_with: gulkan_lib,
  link_whole: common_lib,
  include_directories: gulkan_inc,
  install: false)

executable(
  'pixbuf-linear', ['pixbuf-linear.c', test_resources, shader_resources],
  dependencies: [gulkan_deps, glfw_dep],
  link_with: gulkan_lib,
  link_whole: common_lib,
  include_directories: gulkan_inc,
  install: false)

executable(
  'cairo', ['cairo.c', test_resources, shader_resources],
  dependencies: [gulkan_deps, glfw_dep],
  link_with: gulkan_lib,
  link_whole: common_lib,
  include_directories: gulkan_inc,
  install: false)

executable(
  'threading', ['threading.c', test_resources, shader_resources],
  dependencies: [gulkan_deps, glfw_dep],
  link_with: gulkan_lib,
  link_whole: common_lib,
  include_directories: gulkan_inc,
  install: false)

if glew_dep.found()
  executable(
    'external-memory', ['external-memory.c', test_resources, shader_resources],
    dependencies: [gulkan_deps, glfw_dep, glew_dep],
    link_with: gulkan_lib,
    link_whole: common_lib,
    include_directories: gulkan_inc,
    install: false)
endif

executable(
  'gulkan-cube', ['cube.c', shader_resources],
  dependencies : [gulkan_deps, glfw_dep, graphene_dep],
  link_with: gulkan_lib,
  link_whole: common_lib,
  include_directories: gulkan_inc,
  install: true)

if shaderc_dep.found() and json_glib_dep.found()
  executable(
    'gulkan-toy', ['toy.c', shader_resources],
    dependencies : [gulkan_deps, glfw_dep, graphene_dep,
                    shaderc_dep, json_glib_dep],
    link_with: gulkan_lib,
    link_whole: common_lib,
    include_directories: gulkan_inc,
    install: true)
endif
