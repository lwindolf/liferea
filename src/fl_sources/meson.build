liflsources_sources = [
  'node_source.c',
  'node_source_activatable.c',
  'default_source.c',
  'dummy_source.c',
  'google_reader_api_edit.c',
  'google_source.c',
  'inoreader_source.c',
  'inoreader_source_feed.c',
  'inoreader_source_feed_list.c',
  'json_api_mapper.c',
  'opml_source.c',
  'reedah_source.c',
  'reedah_source_feed.c',
  'reedah_source_feed_list.c',
  'theoldreader_source.c',
  'theoldreader_source_feed.c',
  'theoldreader_source_feed_list.c',
  'ttrss_source.c',
  'ttrss_source_feed.c',
  'ttrss_source_feed_list.c'
]

liflsources = static_library ('liflsources',
  liflsources_sources,
  dependencies : liferea_deps,
  include_directories : src_inc,
  install : false
)
