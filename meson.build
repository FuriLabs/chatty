project(
  'chatty', 'c', 'cpp',
  version: '0.8.6~rc.0',
  meson_version: '>= 0.56.0',
)

i18n = import('i18n')
gnome = import('gnome')
cc = meson.get_compiler('c')

top_inc = include_directories('.')
src_inc = []

gtk_version = '4.12'
gtk_version_arr = gtk_version.split('.')
gtk_major = gtk_version_arr[0]
gtk_minor = gtk_version_arr[1]

adw_version = '1.5'
adw_version_arr = adw_version.split('.')
adw_major = adw_version_arr[0]
adw_minor = adw_version_arr[1]

purple_dep = dependency('purple', required: get_option('purple'))

libspell_dep = dependency('libspelling-1', required: false)

app_id = 'sm.puri.Chatty'
if get_option('profile') == 'devel'
  app_id = app_id + '.Devel'
endif

config_h = configuration_data()
config_h.set10('HAVE_EXPLICIT_BZERO', cc.has_function('explicit_bzero'))
config_h.set('PURPLE_ENABLED', purple_dep.found())
config_h.set('LIBSPELL_ENABLED', libspell_dep.found())
config_h.set_quoted('GETTEXT_PACKAGE', 'purism-chatty')
config_h.set_quoted('LOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))
config_h.set_quoted('PACKAGE_NAME', meson.project_name())
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
config_h.set_quoted('BUILD_DIR', meson.current_build_dir())
config_h.set_quoted('SOURCE_DIR', meson.current_source_dir())
config_h.set_quoted('CHATTY_APP_ID', app_id)

configure_file(
  output: 'config.h',
  configuration: config_h,
)
add_project_arguments([
  '-I' + meson.project_build_root(),
  '-DHAVE_CONFIG_H',
  '-DG_LOG_USE_STRUCTURED',
  '-DGDK_VERSION_MIN_REQUIRED=GDK_VERSION_@0@_@1@'.format(gtk_major, gtk_minor),
  '-DGDK_VERSION_MAX_ALLOWED=GDK_VERSION_@0@_@1@'.format(gtk_major, gtk_minor),
  '-DADW_VERSION_MIN_REQUIRED=ADW_VERSION_@0@_@1@'.format(adw_major, adw_minor),
  '-DADW_VERSION_MAX_ALLOWED=ADW_VERSION_@0@_@1@'.format(adw_major, adw_minor),

], language: 'c')

global_c_args = []
test_c_args = [
  '-Wcast-align',
  '-Wdate-time',
  '-Wdeclaration-after-statement',
  ['-Werror=format-security', '-Werror=-format-nonliteral'],
  '-Wendif-labels',
  '-Werror=incompatible-pointer-types',
  '-Werror=missing-declarations',
  '-Werror=overflow',
  '-Werror=return-type',
  '-Werror=shift-count-overflow',
  '-Werror=shift-overflow=1',
  '-Werror=implicit-fallthrough=3',
  '-Wfloat-equal',
  '-Wformat-nonliteral',
  '-Wformat-security',
  '-Winit-self',
  '-Wmaybe-uninitialized',
  '-Wmissing-field-initializers',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wnested-externs',
  '-Wno-missing-field-initializers',
  '-Wno-sign-compare',
  '-Wno-strict-aliasing',
  '-Wno-unused-parameter',
  '-Wold-style-definition',
  '-Wpointer-arith',
  '-Wredundant-decls',
  '-Wshadow',
  '-Wstrict-prototypes',
  '-Wswitch-default',
  '-Wswitch-enum',
  '-Wtype-limits',
  '-Wundef',
  '-Wunused-function',
]
if get_option('buildtype') != 'plain'
  test_c_args += '-fstack-protector-strong'
endif

foreach arg: test_c_args
  if cc.has_multi_arguments(arg)
    global_c_args += arg
  endif
endforeach
add_project_arguments(
  global_c_args,
  language: 'c'
)

run_data = configuration_data()
run_data.set('ABS_BUILDDIR', meson.current_build_dir())
run_data.set('ABS_SRCDIR', meson.current_source_dir())
configure_file(
  input: 'run.in',
  output: 'run',
  configuration: run_data)

libebook_dep = dependency('libebook-contacts-1.2')
libm_dep = cc.find_library('m')

libcmatrix_dep = dependency('libcmatrix',
  version: '>= 0.0.2',
  fallback: ['libcmatrix', 'libcmatrix_dep'],
  default_options: [
    'build-examples=false',
    'gtk_doc=false',
  ])

subdir('completion')
subdir('data')
subdir('help')
subdir('src')
subdir('tests')
subdir('po')

system = target_machine.system()
if system == 'linux'
  system = 'GNU/Linux'
endif

summary({'Target': system,
         'Target arch': target_machine.cpu(),
         'Compiler': cc.get_id(),
         'Version': cc.version(),
         'Linker': cc.get_linker_id(),
        },
        bool_yn: true,
        section: 'Toolchain')

summary({'Build type': get_option('buildtype'),
         'libpurple': purple_dep.found(),
         'libspelling': libspell_dep.found(),
        },
        bool_yn: true,
        section: 'Configuration')

# gnome.post_install() is available since meson 0.59.0
# Distributions use their own tooling (e.g. postinst, triggers, etc)
# so it is okay if the post_install() is not run on distro builds
if meson.version().version_compare('>=0.59.0')
  gnome.post_install(
    glib_compile_schemas: true,
    gtk_update_icon_cache: true,
  )
endif
