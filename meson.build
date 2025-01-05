project('liferea', 'c',
  version: '2.0',
  meson_version: '>= 0.59.0'
)

gnome = import('gnome')
i18n = import('i18n')

subdir('po')

prefix = get_option('prefix')
datadir = join_paths(prefix, get_option('datadir'))
libdir = join_paths(prefix, get_option('libdir'))
localedir = join_paths(prefix, get_option('localedir'))
pkglibdir = join_paths(libdir, 'liferea')
webprocessextensionsdir = join_paths(pkglibdir, 'web-process-extensions')

conf = configuration_data()
conf.set_quoted('APPLICATION_ID', 'net.sf.liferea')
conf.set_quoted('BUILD_ROOT', meson.project_build_root())
conf.set_quoted('WEB_EXTENSIONS_DIR', webprocessextensionsdir)
conf.set_quoted('PACKAGE', 'liferea')
conf.set_quoted('PACKAGE_URL', 'https://github.com/liferea')
conf.set_quoted('PACKAGE_DATA_DIR', datadir)
conf.set_quoted('PACKAGE_LIB_DIR', libdir)
conf.set_quoted('PACKAGE_LOCALE_DIR', localedir)
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('GETTEXT_PACKAGE', 'liferea')
conf.set('ENABLE_NLS', 1)

configure_file(
  input: 'config.h.in',
  output: 'config.h',
  configuration: conf
)

subdir('src')

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

resources = gnome.compile_resources(
  'resources', 'resources/gresource.xml',
  source_dir : 'resources',
  c_name : 'resources'
)

executable('liferea',
  liferea_sources,
  resources,
  dependencies: liferea_deps,
  include_directories: include_directories('.', 'src')
)


# Introspection
#if get_option('introspection')
#  gir = gnome.generate_gir('Liferea-3.0',
#    sources: liferea_sources,
#    nsversion: '3.0',
#    includes: ['Gtk-3.0', 'libxml2-2.0'],
#    install: true
#  )
#  gnome.compile_resources('Liferea-3.0', 'resources/gresource.xml')
#endif

# Install scripts
#install_data('liferea-add-feed', install_dir: get_option('bindir'))

gnome.post_install(
  gtk_update_icon_cache: true,
  glib_compile_schemas: true,
  update_desktop_database: true
)
