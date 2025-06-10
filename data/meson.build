subdir('man')

desktop_file = i18n.merge_file(
  input: 'net.sourceforge.liferea.desktop.in',
  output: 'net.sourceforge.liferea.desktop',
  type: 'desktop',
  po_dir: '../po',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'applications'),
)

appdata_file = i18n.merge_file(
  input: 'net.sourceforge.liferea.appdata.xml.in',
  output: 'net.sourceforge.liferea.appdata.xml',
  po_dir: '../po',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'metainfo'),
)# Validating app data

appstreamcli = find_program('appstreamcli', required: false)
if appstreamcli.found()
  test(
    'validate-appdata', appstreamcli,
    args: [
      'validate', '--no-net', '--explain', appdata_file.full_path()
    ]
  )
endif

icondir = join_paths(datadir, 'icons', 'hicolor')
foreach size: ['scalable']
  install_subdir(join_paths('pixmaps', size), install_dir: icondir)
endforeach

pixmapsdir = join_paths(datadir, 'pixmaps')
foreach size: ['16x16', '32x32', '22x22', '24x24', '32x32', '48x48']
  install_subdir(join_paths('pixmaps', size), install_dir: pixmapsdir)
endforeach
foreach file: [
  'default.svg',
  'emblem-important.svg',
  'emblem-web.svg',
  'fl_opml.png',
  'folder-saved-search.png',
  'folder.png',
  'newsbin.png',
  'unread.png'
]
  # FIXME: does this duplicate pixmaps?
  install_data(join_paths('pixmaps', file), install_dir: pixmapsdir)
endforeach

install_data (
  'net.sf.liferea.gschema.xml',
  install_dir: join_paths(get_option('datadir'), 'glib-2.0', 'schemas')
)

