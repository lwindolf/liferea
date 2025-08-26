project('liferea', 'c',
  version: '1.99.0',
  meson_version: '>= 0.62.0'
)

gnome = import('gnome')
i18n = import('i18n')

gettext_package = 'liferea'
add_project_arguments('-DGETTEXT_PACKAGE=' + gettext_package, language: 'c')
subdir('po')

prefix = get_option('prefix')
datadir = join_paths(prefix, get_option('datadir'), 'liferea')
libdir = join_paths(prefix, get_option('libdir'), 'liferea')
girdir = join_paths(prefix, get_option('datadir'), 'liferea', 'gir-1.0')
typelibdir = join_paths(prefix, get_option('libdir'), 'liferea', 'girepository-1.0')
localedir = join_paths(prefix, get_option('localedir'))
webprocessextensionsdir = join_paths(prefix, get_option('libdir'), 'liferea', 'web-process-extensions')

conf = configuration_data()
conf.set_quoted('APPLICATION_ID', 'net.sf.liferea')
conf.set_quoted('BUILD_ROOT', meson.project_build_root())
conf.set_quoted('WEB_EXTENSIONS_DIR', webprocessextensionsdir)
conf.set_quoted('PACKAGE', 'liferea')
conf.set_quoted('PACKAGE_URL', 'https://github.com/lwindolf/liferea')
conf.set_quoted('PACKAGE_DATA_DIR', datadir)
conf.set_quoted('PACKAGE_LIB_DIR', libdir)
conf.set_quoted('PACKAGE_LOCALE_DIR', localedir)
conf.set_quoted('VERSION', meson.project_version())
conf.set('ENABLE_NLS', 1)

# Due to the mess in pygobject we need to know with which girepository we
# are run. Please change to 2.0 if building again pygobject > 3.52.3
# (See also https://github.com/lwindolf/liferea/issues/1425)
#
# At a later point the default here will switch to 2.0
conf.set_quoted('GI_REPOSITORY', 'girepository-1.0')

configure_file(
  input: 'config.h.in',
  output: 'config.h',
  configuration: conf
)

subdir('resources')
subdir('src')
subdir('data')

liferea_deps = [
  dependency('glib-2.0', version: '>= 2.74.0'),
  dependency('gsettings-desktop-schemas'),
  dependency('gobject-introspection-1.0'),
  dependency('gtk4', version: '>= 4.12.0'),
  dependency('json-glib-1.0', version: '>= 1.6'),
  dependency('libxml-2.0', version: '>= 2.6.12'),
  dependency('libxslt', version: '>= 1.1.19'),
  dependency('sqlite3', version: '>= 3.22'),
  dependency('webkitgtk-6.0', version: '>= 2.43.4'),
  dependency('webkitgtk-web-process-extension-6.0', version: '>= 2.43.4'),
  dependency('libpeas-2'),
  dependency('fribidi', version: '>= 0.19.7')
]

liferea = executable('liferea',
  liferea_sources + files('src/main.c'),
  liferea_resources,
  dependencies: liferea_deps,
  include_directories: include_directories('.', 'src')
)

foreach name: [
  'parse_date',
  'parse_html',
  'parse_rss',
  'parse_uri',
  'parse_xml',
  'social',
  'update'
]
  test(name, liferea, args: ['--test', name])
endforeach

install_subdir('plugins', install_dir: libdir)
install_subdir('css', install_dir: datadir)

gir = gnome.generate_gir(liferea,
  namespace: 'Liferea',
  sources: liferea_gir_sources,
  nsversion: '3.0',
  includes: ['Gtk-4.0', 'libxml2-2.0', 'Json-1.0'],
  install: true,
  install_dir_gir: girdir,
  install_dir_typelib: typelibdir,
  extra_args: '--accept-unprefixed',
)

gnome.post_install(
  gtk_update_icon_cache: true,
  glib_compile_schemas: true,
  update_desktop_database: true
)
