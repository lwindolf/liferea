# The .. is to include config.h
src_inc = include_directories ('.', '..')

subdir ('parsers')
subdir ('ui')
subdir ('fl_sources')
subdir ('webkit')

liferea_sources = [
  'auth.c',
  'auth_activatable.c',
  'browser.c',
  'browser_history.c',
  'comments.c',
  'common.c',
  'conf.c',
  'date.c',
  'db.c',
  'dbus.c',
  'debug.c',
  'enclosure.c',
  'export.c',
  'favicon.c',
  'feed.c',
  'feed_parser.c',
  'feedlist.c',
  'folder.c',
  'html.c',
  'htmlview.c',
  'item.c',
  'item_history.c',
  'item_loader.c',
  'item_state.c',
  'itemset.c',
  'itemlist.c',
  'json.c',
  'liferea_application.c',
  'metadata.c',
  'migrate.c',
  'net.c',
  'net_monitor.c',
  'newsbin.c',
  'node.c',
  'node_type.c',
  'plugins_engine.c',
  'render.c',
  'rule.c',
  'social.c',
  'subscription.c',
  'update.c',
  'main.c',
  'vfolder.c',
  'vfolder_loader.c',
  'xml.c'
]

liferea_exe = executable ('liferea', liferea_sources,
  dependencies : liferea_deps,
  include_directories : src_inc,
  link_with : [liparsers, liui, liflsources, liwebkit],
  install: true
)

scriptconf = configuration_data ()
scriptconf.set ('prefix', prefix)

configure_file (input: 'liferea-add-feed.in',
  output: 'liferea-add-feed',
  configuration : scriptconf,
  install: true,
  install_dir: bindir
)

gir_files = [
  'auth.c', 'auth.h',
  'auth_activatable.c', 'auth_activatable.h',
  'enclosure.h',
  'feedlist.c', 'feedlist.h',
  'item.h',
  'itemlist.c', 'itemlist.h',
  'itemset.c', 'itemset.h',
  'node.h', 'node.c',
  'node_view.h',
  'social.c', 'social.h',
  'subscription_type.h',
  'ui/browser_tabs.c', 'ui/browser_tabs.h',
  'ui/icons.c', 'ui/icons.h',
  'ui/itemview.c', 'ui/itemview.h',
  'ui/item_list_view.c', 'ui/item_list_view.h',
  'ui/liferea_htmlview.c', 'ui/liferea_htmlview.h',
  'ui/liferea_shell.c', 'ui/liferea_shell.h',
  'ui/liferea_shell_activatable.c', 'ui/liferea_shell_activatable.h',
  'ui/media_player.c', 'ui/media_player.h',
  'ui/media_player_activatable.c', 'ui/media_player_activatable.h',
  'fl_sources/node_source.c', 'fl_sources/node_source.h',
  'fl_sources/node_source_activatable.c', 'fl_sources/node_source_activatable.h'
]

gnome.generate_gir ( liferea_exe,
  sources: gir_files,
  namespace: 'Liferea',
  nsversion: '3.0',
  includes: ['Gtk-3.0', 'libxml2-2.0'],
  identifier_prefix: 'Liferea',
  extra_args: ['--warn-all', '--accept-unprefixed',  '--verbose'],
  install: true,
  install_dir_gir: join_paths (pkgdatadir, 'gir-1.0'),
  install_dir_typelib: join_paths (pkglibdir, 'girepository-1.0')
)

subdir ('tests')
