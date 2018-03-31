html_test_obj = liferea_exe.extract_objects ('html.c', 'common.c', 'debug.c', 'xml.c')

html_auto = executable ('html_auto', 'html.c',
  objects: html_test_obj,
  dependencies : liferea_deps,
  include_directories : src_inc,
  install : false
)

test ('html_test', html_auto)

date_test_obj = liferea_exe.extract_objects ('date.c', 'common.c', 'debug.c')

parse_date = executable ('parse_date', 'parse_date.c',
  objects: date_test_obj,
  dependencies : liferea_deps,
  include_directories : src_inc,
  install : false
)

test ('parse_date_test', parse_date)
