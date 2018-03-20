project ('liferea', 'c',
  version : '1.12.2',
  license : 'GPL2',
  meson_version : '>=0.42.0'
)

i18n = import ('i18n')
gnome = import ('gnome')

liferea_deps = [
  dependency ('gtk+-3.0', version : '>= 3.14.0'),
  dependency ('glib-2.0', version : '>= 2.42.0'),
  dependency ('gio-2.0'),
  dependency ('pango'),
  dependency ('libxml-2.0'),
  dependency ('libxslt'),
  dependency ('sqlite3'),
  dependency ('gmodule-2.0'),
  dependency ('gthread-2.0'),
  dependency ('libsoup-2.4', version : '>= 2.42'),
  dependency ('webkit2gtk-4.0'),
  dependency ('json-glib-1.0'),
  dependency ('gobject-introspection-1.0'),
  dependency ('gsettings-desktop-schemas'),
  dependency ('libpeas-1.0'),
  dependency ('libpeas-gtk-1.0')
]

webextension_deps = dependency ('webkit2gtk-web-extension-4.0')

prefix = get_option ('prefix')
datadir = join_paths (prefix, get_option ('datadir'))
pkgdatadir = join_paths (datadir, meson.project_name ())
bindir = join_paths (prefix, get_option ('bindir'))
pkglibdir = join_paths (prefix, get_option ('libdir'), meson.project_name ())
webextensiondir = join_paths (pkglibdir, 'web-extension')
localedir = join_paths (prefix, get_option ('localedir'))

uname_run = run_command ('uname')
osname = uname_run.stdout().strip()

conf = configuration_data ()
conf.set_quoted('PACKAGE', meson.project_name ())
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name ())
conf.set_quoted('VERSION', meson.project_version ())
conf.set_quoted('PACKAGE_LIB_DIR', pkglibdir)
conf.set_quoted('PACKAGE_DATA_DIR', datadir)
conf.set_quoted('PACKAGE_LOCALE_DIR', localedir)
conf.set_quoted('BIN_DIR', bindir)
conf.set_quoted('WEB_EXTENSIONS_DIR', webextensiondir)
conf.set_quoted('OSNAME', osname)

configure_file (output : 'config.h',
  configuration : conf
)

dbusconf = configuration_data ()
dbusconf.set ('bindir', bindir)

configure_file (input: 'net.sourceforge.liferea.service.in',
  output: 'net.sourceforge.liferea.service',
  configuration : dbusconf,
  install: true,
  install_dir: join_paths (datadir, 'dbus-1', 'services')
)

i18n.merge_file(
  input: 'net.sourceforge.liferea.desktop.in',
  output: 'net.sourceforge.liferea.desktop',
  type: 'desktop',
  po_dir: 'po',
  install: true,
  install_dir: join_paths(datadir, 'applications')
)

i18n.merge_file(
  input: 'liferea.appdata.xml.in',
  output: 'liferea.appdata.xml',
  po_dir: 'po',
  install: true,
  install_dir: join_paths(datadir, 'appdata')
)

install_data ('net.sf.liferea.gschema.xml',
  install_dir: join_paths(datadir, 'glib-2.0', 'schemas')
)

install_data ('liferea.convert',
  install_dir: join_paths (datadir, 'GConf', 'gsettings')
)

install_data ( ['css/liferea.css', 'css/user.css', 'css/adblock.css'],
  install_dir: join_paths (pkgdatadir, 'css')
)

install_data ('dtd/html.ent',
  install_dir: join_paths (pkgdatadir, 'dtd')
)

install_data (
  [
    'plugins/bold-unread.py',
    'plugins/bold-unread.plugin',
    'plugins/gnome-keyring.py',
    'plugins/gnome-keyring.plugin',
    'plugins/libnotify.py',
    'plugins/libnotify.plugin',
    'plugins/media-player.py',
    'plugins/media-player.plugin',
    'plugins/plugin-installer.py',
    'plugins/plugin-installer.plugin',
    'plugins/trayicon.py',
    'plugins/trayicon.plugin'
  ],
  install_dir: join_paths (pkglibdir, 'plugins')
)

subdir ('po')
subdir ('doc/html')
subdir ('man')
subdir ('opml')
subdir ('pixmaps')
subdir ('src')
subdir ('xslt')
subdir ('glade')

meson.add_install_script ('meson_post_install.py')
