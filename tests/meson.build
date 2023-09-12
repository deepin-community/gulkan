test_instance = executable(
  'test_instance', ['test_instance.c'],
  dependencies: gulkan_deps,
  link_with: gulkan_lib,
  include_directories: gulkan_inc,
  install: false)
test('test_instance', test_instance)

test_device = executable(
  'test_device', ['test_device.c'],
  dependencies: gulkan_deps,
  link_with: gulkan_lib,
  include_directories: gulkan_inc,
  install: false)
test('test_device', test_device)

test_client = executable(
  'test_client', ['test_client.c'],
  dependencies: gulkan_deps,
  link_with: gulkan_lib,
  include_directories: gulkan_inc,
  install: false)
test('test_client', test_client)

test_texture = executable(
  'test_texture', ['test_texture.c', test_resources],
  dependencies: gulkan_deps,
  link_with: gulkan_lib,
  include_directories: gulkan_inc,
  install: false)
test('test_texture', test_texture)

if glfw_dep.found()
  if glew_dep.found()
    test_texture_external = executable(
      'test_texture_external', ['test_texture_external.c', test_resources],
      dependencies: [gulkan_deps, glfw_dep, glew_dep],
      link_with: gulkan_lib,
      include_directories: gulkan_inc,
      install: false)
    test('test_texture_external', test_texture_external)
  endif

  test_renderer = executable(
    'test_renderer', ['test_renderer.c', test_resources, shader_resources],
    dependencies: [gulkan_deps, glfw_dep],
    link_with: gulkan_lib,
    link_whole: common_lib,
    include_directories: gulkan_inc,
    install: false)
test('test_renderer', test_renderer)
endif
