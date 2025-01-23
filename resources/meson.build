liferea_resources = gnome.compile_resources(
  'resources', 'gresource.xml',
  source_dir : '.',
  c_name : 'liferea_resources',
  dependencies : [
        custom_target(
                input: 'item.xml.in',
                output: 'item.xml',
                command: [
                        find_program('intltool-merge'),
                        '-x',
                        '-u',
                        '-c',
                        join_paths(meson.project_source_root(), 'po/.intltool-merge-cache'),
                        join_paths(meson.project_source_root(), 'po'),
                        '@INPUT@',
                        '@OUTPUT@',
                ]
        ),
        custom_target(
                input: 'node.xml.in',
                output: 'node.xml',
                command: [
                        find_program('intltool-merge'),
                        '-x',
                        '-u',
                        '-c',
                        join_paths(meson.project_source_root(), 'po/.intltool-merge-cache'),
                        join_paths(meson.project_source_root(), 'po'),
                        '@INPUT@',
                        '@OUTPUT@',
                ]
        )
  ]
)