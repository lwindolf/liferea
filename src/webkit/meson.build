liwebkit_sources = [
  'webkit.c',
  'liferea_web_view.c'
]

liwebkit = static_library (
  'liwebkit',
  liwebkit_sources,
  dependencies : liferea_deps,
  include_directories : src_inc,
  install : false
)

subdir ('web_extension')
