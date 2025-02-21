libsrc = []
chatty_sources = []
chatty_deps = []

version_h = vcs_tag(input: 'version.h.in',
		    output: 'version.h')

src_inc += include_directories('.')
src_inc += include_directories('users')

purple_plugdir = ''

subdir('matrix')
subdir('mm')
subdir('purple')

if purple_dep.found()
  libsrc += files([
    'dialogs/chatty-pp-chat-info.c',
  ])
endif

libsrc += [
  'contrib/demotaggedentry.c',
  'dialogs/chatty-ma-account-details.c',
  'dialogs/chatty-pp-account-details.c',
  'dialogs/chatty-chat-info.c',
  'dialogs/chatty-ma-chat-info.c',
  'dialogs/chatty-mm-chat-info.c',
  'dialogs/chatty-wrong-pw-dialog.c',
  'users/chatty-item.c',
  'users/chatty-contact.c',
  'users/chatty-account.c',
  'chatty-selectable-row.c',
  'chatty-fp-row.c',
  'chatty-file.c',
  'chatty-attachment.c',
  'chatty-attachments-bar.c',
  'chatty-message-bar.c',
  'chatty-list-row.c',
  'chatty-chat-page.c',
  'chatty-entry.c',
  'chatty-message-row.c',
  'chatty-progress-button.c',
  'chatty-file-item.c',
  'chatty-log.c',
  'chatty-avatar.c',
  'chatty-chat.c',
  'chatty-clock.c',
  'chatty-media.c',
  'chatty-contact-provider.c',
  'chatty-message.c',
  'chatty-settings.c',
  'chatty-history.c',
  'chatty-notification.c',
  'chatty-utils.c',
  'chatty-phone-utils.cpp',
  'chatty-pgp.c',
]

ui_files = files (
  'ui/chatty-chat-page.ui',
  'ui/chatty-contact-row.ui',
  # FIXME: Testing this fails in CI,
  # but works fine locally
  'ui/chatty-info-dialog.ui',
  'ui/chatty-dialog-join-muc.ui',
  'ui/chatty-dialog-wrong-pw.ui',
  # Can't test as this requires chatty-contact-list,
  # 'ui/chatty-dialog-new-chat.ui',
  'ui/chatty-fp-row.ui',
  'ui/chatty-list-row.ui',
  'ui/chatty-message-row.ui',
  # FIXME
  'ui/chatty-pp-account-details.ui',
  'ui/chatty-ma-chat-info.ui',
  'ui/chatty-pp-chat-info.ui',
  'ui/chatty-mm-chat-info.ui',
  'ui/chatty-settings-dialog.ui',
  # Can't test as this requires chatty-chat-list,
  # which is not in libsrc
  # 'ui/chatty-window.ui',
)

chatty_sources += [
  'main.c',
  'chatty-manager.c',
  'chatty-application.c',
  'chatty-chat-list.c',
  'chatty-contact-list.c',
  'chatty-main-view.c',
  'chatty-side-bar.c',
  'chatty-invite-page.c',
  'chatty-verification-page.c',
  'chatty-window.c',
  'dialogs/chatty-info-dialog.c',
  'dialogs/chatty-settings-dialog.c',
  'dialogs/chatty-new-chat-dialog.c',
  'dialogs/chatty-new-muc-dialog.c',
  version_h,
]

libphonenumber_dep = cc.find_library('phonenumber', required: true)
libadwaita_dep = dependency('libadwaita-1', version: '>= @0@'.format(adw_version))
libgtk_dep = dependency('gtk4', version: '>= @0@'.format(gtk_version))

chatty_deps += [
  dependency('gio-2.0', version: '>= 2.78'),
  dependency('gnome-desktop-4', version: '>= 43'),
  dependency('sqlite3', version: '>=3.0.0'),
  dependency('camel-1.2'),
  dependency('libebook-contacts-1.2'),
  dependency('libebook-1.2'),
  dependency('gsettings-desktop-schemas'),
  dependency('gstreamer-1.0'),
  dependency('gtksourceview-5', version: '>= 5.4.0'),
  libspell_dep,
  libadwaita_dep,
  libgtk_dep,
  libebook_dep,
  libm_dep,
  libphonenumber_dep,
  libcmatrix_dep,
]

gtk_next_version = gtk_minor.to_int() + 1
adw_next_version = adw_minor.to_int() + 1

summary({
    'GTK matches machine version': libgtk_dep.version().version_compare('< @0@.@1@'.format(gtk_major, gtk_next_version.to_string())),
    'Libadwaita matches machine version': libadwaita_dep.version().version_compare('< @0@.@1@'.format(adw_major, adw_next_version.to_string())),
  },
  bool_yn: true,
  section: 'Versions',
)

gnome = import('gnome')

resources = gnome.compile_resources('chatty-resources',
  'chatty.gresource.xml',
  c_name: 'chatty'
)

libchatty = both_libraries(
  'chatty', [libsrc, 'library.c'],
  resources,
  include_directories: src_inc,
  dependencies: chatty_deps,
)

# xxx: Requires GTK 4.10, seems to fail with older GTK.  To be enabled in the future
# gtk_builder_tool = find_program('gtk4-builder-tool', required: false)
# if gtk_builder_tool.found()
#   preload_env = 'LD_PRELOAD=libadwaita-1.so:@0@'.format(libchatty_shared.full_path())
#   foreach file: ui_files
#     test('Validate @0@'.format(file), gtk_builder_tool,
#          env: [preload_env],
#          args: ['validate', file])
#   endforeach
# endif

executable('chatty', chatty_sources, resources,
  include_directories: src_inc,
  dependencies: chatty_deps,
  link_with: libchatty.get_static_lib(),
  install: true,
  install_rpath: purple_plugdir,
)
