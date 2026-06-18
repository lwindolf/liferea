plugins_sources = []

subdir('trayicon')

plugins_static = static_library('plugins', plugins_sources,
  dependencies: liferea_deps,
  gnu_symbol_visibility: 'hidden'
)
