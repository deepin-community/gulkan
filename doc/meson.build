subdir('xml')

ignore_headers = [
  'gulkan.h',
  'gulkan-config.h',
  'gulkan-version.h',
  'gulkan-cmd-buffer-private.h',
  'gulkan-texture-private.h',
  'xdg-shell-client-protocol.h',
  'gulkan-window-wayland.h',
  'gulkan-window-xcb.h',
]

glib_prefix = dependency('glib-2.0').get_pkgconfig_variable('prefix')
glib_docpath = join_paths(glib_prefix, 'share', 'gtk-doc', 'html')
docpath = join_paths(gulkan_datadir, 'gtk-doc', 'html')

gnome.gtkdoc('gulkan',
  main_xml: 'gulkan-docs.xml',
  src_dir: [
    src_inc,
  ],
  dependencies: gulkan_dep,
  gobject_typesfile: 'gulkan.types',
  scan_args: [
    '--rebuild-types',
    '--rebuild-sections',
    '--ignore-headers=' + ' '.join(ignore_headers),
  ],
  mkdb_args: [
    '--default-include=gulkan.h',
    '--ignore-files=' + ' '.join(ignore_headers),
  ],
  fixxref_args: [
    '--html-dir=@0@'.format(docpath),
    '--extra-dir=@0@'.format(join_paths(glib_docpath, 'glib')),
    '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gobject')),
    '--extra-dir=@0@'.format(join_paths(glib_docpath, 'graphene')),
  ],
  c_args: '-w',
  # html_assets: html_images,
  install: true,
  check: false,
)
