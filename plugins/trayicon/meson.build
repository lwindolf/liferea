plugins_sources += files([
        'trayicon.c'
])

plugin_trayicon_resources = gnome.compile_resources(
  'trayicon-resources',
  'trayicon.gresource.xml',
  c_name: 'lp_trayicon',
)

plugins_sources += plugin_trayicon_resources
