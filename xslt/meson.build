xslt_files = [
  'item.xml',
  'feed.xml',
  'source.xml',
  'folder.xml',
  'newsbin.xml',
  'vfolder.xml'
]

foreach file : xslt_files
  i18n.merge_file (
    input: file + '.in',
    output: file,
    po_dir: '../po',
    data_dirs: '../po',
    install: true,
    install_dir: join_paths (pkgdatadir, 'xslt')
  )
endforeach

install_data ('i18n-filter.xslt',
  install_dir: join_paths (pkgdatadir, 'xslt')
)
