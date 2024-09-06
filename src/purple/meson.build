src_inc += include_directories('.')

if (not purple_dep.found())
  subdir_done()
endif

purple_plugdir = purple_dep.get_variable(pkgconfig: 'plugindir')
jabber = meson.get_compiler('c').find_library('jabber', dirs: purple_plugdir)
jabber_incdir = include_directories('xeps/prpl/jabber')
src_inc += jabber_incdir

chatty_deps += [
  purple_dep,
  jabber,
]

libsrc += files([
  'chatty-pp-buddy.c', 'chatty-pp-buddy.h',
  'chatty-pp-account.c', 'chatty-pp-account.h',
  'chatty-pp-chat.c', 'chatty-pp-chat.h',
  'chatty-pp-utils.c', 'chatty-pp-utils.h',
  'chatty-purple.c', 'chatty-purple.h',
  'xeps/xeps.c', 'xeps/xeps.h',
  'xeps/chatty-xep-0184.c', 'xeps/chatty-xep-0184.h',
  'xeps/chatty-xep-0313.c', 'xeps/chatty-xep-0313.h',
  'xeps/chatty-xep-0352.c', 'xeps/chatty-xep-0352.h',
  'chatty-purple-request.c', 'chatty-purple-request.h',
  'chatty-purple-notify.c', 'chatty-purple-notify.h',
])

