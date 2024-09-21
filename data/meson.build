config_data = configuration_data()
config_data.set('APP_ID', app_id)

desktop_file = i18n.merge_file(
  input: configure_file(
    input: 'sm.puri.Chatty.desktop.in.in',
    output: '@0@.desktop.in'.format(app_id),
    configuration: config_data,
  ),
  output: '@0@.desktop'.format(app_id),
  type: 'desktop',
  po_dir: '../po',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'applications')
)

desktop_file = i18n.merge_file(
  input: configure_file(
    input: 'sm.puri.Chatty-daemon.desktop.in.in',
    output: '@0@-daemon.desktop.in'.format(app_id),
    configuration: config_data,
  ),
  output: '@0@-daemon.desktop'.format(app_id),
  type: 'desktop',
  po_dir: '../po',
  install: true,
  install_dir: join_paths(get_option('sysconfdir'), 'xdg/autostart')
)

desktop_utils = find_program('desktop-file-validate', required: false)
if desktop_utils.found()
  test('Validate desktop file', desktop_utils,
    args: [desktop_file]
  )
endif

metainfo_file = i18n.merge_file(
  input: configure_file(
    input: 'sm.puri.Chatty.metainfo.xml.in.in',
    output: '@0@.metainfo.xml.in'.format(app_id),
    configuration: config_data,
  ),
  output: '@0@.metainfo.xml'.format(app_id),
  po_dir: '../po',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'metainfo')
)

appstreamcli = find_program('appstreamcli', required: false)
if appstreamcli.found()
  test('Validate metainfo file', appstreamcli,
    args: ['validate', '--no-net', '--explain', metainfo_file]
  )
endif

schema_src = 'sm.puri.Chatty.gschema.xml'
compiled = gnome.compile_schemas(build_by_default: true,
                                 depend_files: files(schema_src))

install_data(schema_src,
  install_dir: join_paths(get_option('datadir'), 'glib-2.0/schemas')
)

compile_schemas = find_program('glib-compile-schemas', required: false)
if compile_schemas.found()
  test('Validate schema file', compile_schemas,
    args: ['--strict', '--dry-run', meson.current_source_dir()]
  )
endif

subdir('icons')
