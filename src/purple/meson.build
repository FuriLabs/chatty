src_inc += include_directories('.')

purple = dependency('purple')
purple_plugdir = purple.get_pkgconfig_variable('plugindir')
jabber = meson.get_compiler('c').find_library('jabber', dirs: purple_plugdir)
jabber_incdir = include_directories('xeps/prpl/jabber')
src_inc += jabber_incdir

chatty_deps += [
  purple,
  jabber,
]

libsrc += files([
  'chatty-pp-buddy.c',
  'chatty-pp-account.c',
  'chatty-pp-chat.c',
])

chatty_sources += files([
  'chatty-purple.c',
  'xeps/xeps.c',
  'xeps/chatty-xep-0184.c',
  'xeps/chatty-xep-0313.c',
  'xeps/chatty-xep-0352.c',
  'chatty-purple-request.c',
  'chatty-purple-notify.c',
])
