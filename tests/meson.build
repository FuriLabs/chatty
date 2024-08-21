if not get_option('tests') == true
  subdir_done()
endif

tests_inc = [
  top_inc,
  src_inc,
]

env = environment()
env.set('G_TEST_SRCDIR', meson.current_source_dir())
env.set('G_TEST_BUILDDIR', meson.current_build_dir())
env.set('GSETTINGS_BACKEND', 'memory')
env.set('GSETTINGS_SCHEMA_DIR', join_paths(meson.project_build_root(), 'data'))
env.set('MALLOC_CHECK_', '2')

test_items = []

if purple_dep.found()
  test_items += ['account']
endif

test_items = [
  'clock',
  'history',
  'settings',
  'mm-account',
  'sms-uri',
  'pgp',
]

foreach item: test_items
  t = executable(
    item,
    item + '.c',
    include_directories: tests_inc,
    link_with: libchatty.get_static_lib(),
    dependencies: chatty_deps,
  )
  test(item, t, env: env, timeout: 300)
endforeach

test_items_shared_lib = [
  'message-row',
  'utils',
]

foreach item: test_items_shared_lib
  t = executable(
    item,
    item + '.c',
    include_directories: tests_inc,
    link_with: libchatty,
    dependencies: chatty_deps,
  )
  test(item, t, env: env, timeout: 300)
endforeach
