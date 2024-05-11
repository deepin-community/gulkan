so_version = 0
api_version = '0.16'
api_path = 'gulkan-' + api_version

gulkan_sources = [
  'gulkan-instance.c',
  'gulkan-device.c',
  'gulkan-texture.c',
  'gulkan-context.c',
  'gulkan-uniform-buffer.c',
  'gulkan-vertex-buffer.c',
  'gulkan-frame-buffer.c',
  'gulkan-geometry.c',
  'gulkan-renderer.c',
  'gulkan-render-pass.c',
  'gulkan-swapchain.c',
  'gulkan-descriptor-pool.c',
  'gulkan-swapchain-renderer.c',
  'gulkan-buffer.c',
  'gulkan-cmd-buffer.c',
  'gulkan-queue.c',
  'gulkan-descriptor-set.c',
  'gulkan-pipeline.c',
  'gulkan-window.c',
]

gulkan_headers = [
  'gulkan.h',
  'gulkan-instance.h',
  'gulkan-device.h',
  'gulkan-texture.h',
  'gulkan-context.h',
  'gulkan-uniform-buffer.h',
  'gulkan-vertex-buffer.h',
  'gulkan-frame-buffer.h',
  'gulkan-geometry.h',
  'gulkan-renderer.h',
  'gulkan-render-pass.h',
  'gulkan-swapchain.h',
  'gulkan-descriptor-pool.h',
  'gulkan-swapchain-renderer.h',
  'gulkan-buffer.h',
  'gulkan-cmd-buffer.h',
  'gulkan-queue.h',
  'gulkan-descriptor-set.h',
  'gulkan-pipeline.h',
  'gulkan-window.h',
]

version_split = meson.project_version().split('.')
MAJOR_VERSION = version_split[0]
MINOR_VERSION = version_split[1]
MICRO_VERSION = version_split[2]

version_conf = configuration_data()
version_conf.set('VERSION', meson.project_version())
version_conf.set('MAJOR_VERSION', MAJOR_VERSION)
version_conf.set('MINOR_VERSION', MINOR_VERSION)
version_conf.set('MICRO_VERSION', MICRO_VERSION)

configure_file(
  input: 'gulkan-version.h.in',
  output: 'gulkan-version.h',
  configuration: version_conf,
  install_dir: join_paths(get_option('includedir'), api_path)
)

gulkan_deps = [
  cairo_dep,
  gio_dep,
  gdk_pixbuf_dep,
  vulkan_dep,
  graphene_dep,
  libdrm_dep,
]

gulkan_args = ['-DGULKAN_COMPILATION']

# XCB
if xcb_dep.found() and xcb_key_dep.found()
  gulkan_args += ['-DVK_USE_PLATFORM_XCB_KHR', '-DGULKAN_HAVE_XCB']
  gulkan_sources += ['gulkan-window-xcb.c']
  gulkan_deps += [xcb_dep, xcb_key_dep]
endif

# Wayland
if wayland_client_dep.found() and wayland_protocols_dep.found() and wayland_scanner_dep.found()

  wayland_scanner_bin = find_program(
    wayland_scanner_dep.get_pkgconfig_variable('wayland_scanner'),
    native: true,
  )

  wl_protos_src = []
  wl_protos_headers = []
  wl_protocol_dir = wayland_protocols_dep.get_pkgconfig_variable('pkgdatadir')

  protocols = [
    [wl_protocol_dir, 'stable/xdg-shell/xdg-shell.xml'],
  ]

  foreach p : protocols
    xml = join_paths(p)
    wl_protos_src += custom_target(
      xml.underscorify() + '_c',
      input: xml,
      output: '@BASENAME@-protocol.c',
      command: [wayland_scanner_bin, 'private-code', '@INPUT@', '@OUTPUT@'],
    )
    wl_protos_headers += custom_target(
      xml.underscorify() + '_client_h',
      input: xml,
      output: '@BASENAME@-client-protocol.h',
      command: [wayland_scanner_bin, 'client-header', '@INPUT@', '@OUTPUT@'],
    )
  endforeach

  lib_wl_protos = static_library(
    'wl_protos',
    wl_protos_src + wl_protos_headers,
    dependencies: wayland_client_dep.partial_dependency(compile_args: true),
  )

  wl_protos = declare_dependency(
    link_with: lib_wl_protos,
    sources: wl_protos_headers,
  )

  gulkan_args += ['-DVK_USE_PLATFORM_WAYLAND_KHR', '-DGULKAN_HAVE_WAYLAND']
  gulkan_sources += ['gulkan-window-wayland.c']
  gulkan_deps += [wayland_client_dep, wl_protos, xkbcommon_dep]
endif

gulkan_lib = shared_library(api_path,
  gulkan_sources,
  version: meson.project_version(),
  soversion: so_version,
  dependencies: gulkan_deps,
  install: true,
  c_args : gulkan_args
)

gulkan_inc = include_directories('.')

girdir = '${datadir}/gir-1.0'
vars = { 'girdir': girdir }

gulkan_dep = declare_dependency(
  sources: [],
  link_with: gulkan_lib,
  include_directories: [ gulkan_inc ],
  dependencies: gulkan_deps,
  variables: vars
)

install_headers(gulkan_headers, subdir: api_path)

pkg = import('pkgconfig')

pkgconfig_variables = [
  'datarootdir=${prefix}/share',
  'datadir=${datarootdir}',
  'girdir=' + girdir,
]

pkg.generate(
  description: 'GLib wrapper for Vulkan',
    libraries: gulkan_lib,
         name: 'gulkan',
     filebase: api_path,
      version: meson.project_version(),
      subdirs: api_path,
     requires: ['glib-2.0', 'vulkan', 'cairo',
                'graphene-1.0', 'gdk-pixbuf-2.0', 'gio-2.0'],
  install_dir: join_paths(get_option('libdir'), 'pkgconfig'),
  variables: pkgconfig_variables,
)

if get_option('introspection')
  gulkan_gir = gnome.generate_gir(
    gulkan_lib,
    sources: gulkan_sources + gulkan_headers,
    namespace: 'Gulkan',
    nsversion: api_version,
    identifier_prefix: 'Gulkan',
    symbol_prefix: 'gulkan',
    export_packages: api_path,
    includes: [ 'cairo-1.0', 'GdkPixbuf-2.0', 'Gdk-3.0', 'Graphene-1.0', 'Vulkan-1.0' ],
    header: 'gulkan.h',
    install: true,
  )
endif
