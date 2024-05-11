project('gulkan', 'c',
        version: '0.16.0',
        meson_version: '>= 0.52.0',
        default_options : [
           meson.version().version_compare('<0.56') ?
            'c_std=c18' :
            'c_std=c2x',
          'warning_level=3',
        ],
)

gnome = import('gnome')

config_h = configuration_data()
configure_file(
  output: 'gulkan-config.h',
  configuration: config_h,
)

project_args = ['-I' + meson.build_root(),
  '-Wno-overlength-strings'
]

compiler = meson.get_compiler('c')
compiler_id = compiler.get_id()

if compiler_id == 'clang'
  project_args += [
    '-Weverything',
    '-Wno-reserved-id-macro',
    '-Wno-documentation',
    '-Wno-documentation-unknown-command',
    '-Wno-padded',
    '-Wno-disabled-macro-expansion',
    '-Wno-atomic-implicit-seq-cst',
    '-Wno-switch-enum',
    '-Wno-used-but-marked-unused',
    '-Wno-cast-qual', # G_DEFINE_TYPE produces this
    '-Wno-reserved-identifier', # GObject
    '-Wno-declaration-after-statement',
    '-Wno-unused-macros', # GIO
    '-Wno-gnu-binary-literal'
  ]
endif

add_project_arguments([project_args], language: ['c'])

# Paths
gulkan_prefix = get_option('prefix')
gulkan_libdir = join_paths(gulkan_prefix, get_option('libdir'))
gulkan_includedir = join_paths(gulkan_prefix, get_option('includedir'))
gulkan_datadir = join_paths(gulkan_prefix, get_option('datadir'))
src_inc = include_directories('src')

### Dependencies

## Required
gio_dep = dependency('gio-2.0', version: '>= 2.50')
gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0', version: '>= 2.36')
vulkan_dep = dependency('vulkan')

cairo_dep = dependency('cairo')
libdrm_dep = dependency('libdrm')
if meson.version().version_compare('>=0.52')
  graphene_dep = dependency('graphene-1.0', include_type: 'system')
else
  graphene_dep = dependency('graphene-1.0')
endif

## Optional
pango_dep = dependency('pangocairo', required : false)
libdrm_amdgpu_dep = dependency('libdrm_amdgpu', required : false)
glfw_dep = dependency('glfw3', required : false)
glew_dep = dependency('glew', version : '>=2.1.0', required : false)
egl_dep = dependency('egl', required : false)
# gbm_bo_get_fd_for_plane was added in mesa 21.1
gbm_dep = dependency('gbm', version : '>= 21.1', required : false)

shaderc_dep = dependency('shaderc', required : false)
json_glib_dep = dependency('json-glib-1.0', required : false)

# Window
xkbcommon_dep = dependency('xkbcommon')

xcb_dep = dependency('xcb', required : false)
xcb_key_dep = dependency('xcb-keysyms', required : false)

wayland_client_dep = dependency('wayland-client', required : false)
wayland_protocols_dep = dependency('wayland-protocols', required : false)
wayland_scanner_dep = dependency('wayland-scanner', required : false)

subdir('src')
subdir('shaders')
subdir('res')

if get_option('examples')
  subdir('examples')
endif

if get_option('tests')
  subdir('tests')
endif

if get_option('api_doc')
  subdir('doc')
endif
