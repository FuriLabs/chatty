project(
  'chatty', 'c', 'cpp',
  version: '0.8.0.rc0',
  meson_version: '>= 0.56.0',
)

i18n = import('i18n')
gnome = import('gnome')
cc = meson.get_compiler('c')

top_inc = include_directories('.')
src_inc = []

gtk_version = '4.8'
gtk_version_arr = gtk_version.split('.')
gtk_major = gtk_version_arr[0]
gtk_minor = gtk_version_arr[1]

purple_dep = dependency('purple', required: get_option('purple'))
libgdesktop_dep = dependency('gnome-desktop-4', required: false)

config_h = configuration_data()
config_h.set10('HAVE_EXPLICIT_BZERO', cc.has_function('explicit_bzero'))
config_h.set('PURPLE_ENABLED', purple_dep.found())
config_h.set('HAVE_GNOME_DESKTOP4', libgdesktop_dep.found())
config_h.set_quoted('GETTEXT_PACKAGE', 'purism-chatty')
config_h.set_quoted('LOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))
config_h.set_quoted('PACKAGE_NAME', meson.project_name())
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
config_h.set_quoted('BUILD_DIR', meson.current_build_dir())
config_h.set_quoted('SOURCE_DIR', meson.current_source_dir())

configure_file(
  output: 'config.h',
  configuration: config_h,
)
add_project_arguments([
  '-I' + meson.project_build_root(),
  '-DHAVE_CONFIG_H',
  '-DGLIB_DISABLE_DEPRECATION_WARNINGS',
  '-DG_LOG_USE_STRUCTURED',
  '-DGDK_VERSION_MIN_REQUIRED=GDK_VERSION_@0@_@1@'.format(gtk_major, gtk_minor),
  '-DGDK_VERSION_MAX_ALLOWED=GDK_VERSION_@0@_@1@'.format(gtk_major, gtk_minor),
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
libfeedback_dep = dependency('libfeedback-0.0')
libm_dep = cc.find_library('m')

libcmatrix = subproject('libcmatrix',
                        default_options: [
                          'build-examples=false',
                        ])
libcmatrix_dep = libcmatrix.get_variable('libcmatrix_dep')


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
        }, section: 'Toolchain')

summary({'Build type': get_option('buildtype'),
         'libpurple': purple_dep.found(),
        }, section: 'Configuration')

meson.add_install_script('build-aux/meson/postinstall.py')
