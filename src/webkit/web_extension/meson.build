lifereawebextension_sources = [
  'web_extension_main.c',
  'liferea_web_extension.c'
]

lifereawebextension = shared_module ('lifereawebextension',
  lifereawebextension_sources,
  dependencies : webextension_deps,
  install: true,
  install_dir: join_paths (pkglibdir, 'web-extension')
)
