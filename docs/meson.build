if get_option('gtk_doc')

toml_data = configuration_data()
toml_data.set('VERSION', meson.project_version())

chatty_toml = configure_file(
  input: 'chatty.toml.in',
  output: 'chatty.toml',
  configuration: toml_data
)

dependency('gi-docgen', version: '>= 2024.1',
           fallback: ['gi-docgen', 'dummy_dep'],
           native: true,
           required: get_option('gtk_doc'))

gidocgen = find_program('gi-docgen')

prefix = get_option ('prefix')
docs_dir = join_paths(prefix, get_option('datadir'), 'doc')

custom_target('chatty-doc',
  input: [ chatty_toml, chatty_gir[0] ],
  output: 'chatty-0',
  command: [
    gidocgen,
    'generate',
    '--fatal-warnings',
    '--add-include-path=@0@'.format(meson.current_build_dir() / '../src'),
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--no-namespace-dir',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT1@',
  ],
#  depend_files: [ expand_content_md_files ],
  build_by_default: true,
  install: true,
  install_dir: docs_dir,
)

endif
