/* chatty-application.c
 *
 * Copyright 2019 Purism SPC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-application"

#ifdef HAVE_CONFIG_H
# include "config.h"
# include "version.h"
#endif

#define CMATRIX_USE_EXPERIMENTAL_API
#include <cmatrix.h>
#include <glib/gi18n.h>
#include <adwaita.h>

#include "chatty-window.h"
#include "chatty-manager.h"
#include "chatty-purple.h"
#include "chatty-application.h"
#include "chatty-settings.h"
#include "chatty-history.h"
#include "chatty-utils.h"
#include "chatty-clock.h"
#include "chatty-log.h"

/**
 * SECTION: chatty-application
 * @title: ChattyApplication
 * @short_description: Base Application class
 * @include: "chatty-application.h"
 */

struct _ChattyApplication
{
  AdwApplication  parent_instance;

  GtkWidget      *main_window;
  ChattySettings *settings;
  ChattyManager  *manager;

  char *uri;
  guint open_uri_id;
};

G_DEFINE_TYPE (ChattyApplication, chatty_application, ADW_TYPE_APPLICATION)

static gboolean    cmd_verbose_cb   (const char *option_name,
                                     const char *value,
                                     gpointer    data,
                                     GError     **error);

static GOptionEntry cmd_options[] = {
  { "version", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, N_("Show release version"), NULL },
  { "nologin", 'n', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, N_("Disable all accounts"), NULL },
#ifdef PURPLE_ENABLED
  { "debug", 'd', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, N_("Enable libpurple debug messages"), NULL },
#endif
  { "verbose", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, cmd_verbose_cb,
    N_("Enable verbose debug messages (repeat option for more verbosity)"), NULL },
  { NULL }
};

static gboolean
cmd_verbose_cb (const char  *option_name,
                const char  *value,
                gpointer     data,
                GError     **error)
{
  chatty_log_increase_verbosity ();

  return TRUE;
}

static void
application_open_chat (ChattyApplication *self,
                       ChattyChat        *chat)
{
  g_assert (CHATTY_IS_APPLICATION (self));
  g_assert (CHATTY_IS_CHAT (chat));

  g_application_activate (G_APPLICATION (self));
  chatty_window_open_chat (CHATTY_WINDOW (self->main_window), chat);
}

static gboolean
application_open_uri (ChattyApplication *self)
{
  g_clear_handle_id (&self->open_uri_id, g_source_remove);

  CHATTY_INFO (self->uri, "Opening uri:");

  if (self->main_window && self->uri)
    chatty_window_set_uri (CHATTY_WINDOW (self->main_window), self->uri, NULL);

  g_clear_pointer (&self->uri, g_free);

  return G_SOURCE_REMOVE;
}

static void
chatty_application_show_about (GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       user_data)
{
  ChattyApplication *self = user_data;

  static const gchar *authors[] = {
    "Adrien Plazas <kekun.plazas@laposte.net>",
    "Andrea Schäfer <mosibasu@me.com>",
    "Benedikt Wildenhain <benedikt.wildenhain@hs-bochum.de>",
    "Chris Talbot (kop316) <chris@talbothome.com>",
    "Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>",
    "Guido Günther <agx@sigxcpu.org>",
    "Julian Sparber <jsparber@gnome.org>",
    "Leland Carlye <leland.carlye@protonmail.com>",
    "Mohammed Sadiq https://www.sadiqpk.org/",
    "Richard Bayerle (OMEMO Plugin) https://github.com/gkdr/lurch",
    "Ruslan Marchenko <me@ruff.mobi>",
    "and more...",
    NULL
  };

  static const gchar *artists[] = {
    "Tobias Bernard <tbernard@gnome.org>",
    NULL
  };

  static const gchar *documenters[] = {
    "Heather Ellsworth <heather.ellsworth@puri.sm>",
    NULL
  };

  if (!self->main_window)
    return;

  adw_show_about_dialog (GTK_WIDGET (gtk_application_get_active_window (user_data)),
                         "application-name", _("Chats"),
                         "application-icon", CHATTY_APP_ID,
                         "version", GIT_VERSION,
                         "comments", _("An SMS and XMPP messaging client"),
                         "website", "https://gitlab.gnome.org/World/Chatty",
                         "copyright", "© 2018–2023 Purism SPC",
                         "issue-url", "https://gitlab.gnome.org/World/Chatty/-/issues/new",
                         "license-type", GTK_LICENSE_GPL_3_0,
                         "developers", authors,
                         "designers", artists,
                         "documenters", documenters,
                         "translator-credits", _("translator-credits"),
                         NULL);
}

static void
show_help_cb (GObject      *object,
              GAsyncResult *result,
              gpointer      user_data)
{
  ChattyApplication *self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (CHATTY_IS_APPLICATION (self));

  gtk_uri_launcher_launch_finish (GTK_URI_LAUNCHER (object), result, &error);

  g_clear_object (&object);

  if (error) {
    AdwDialog *dialog;
    GtkWindow *window;

    window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
    dialog = adw_alert_dialog_new (("Error"), NULL);
    adw_alert_dialog_format_body (ADW_ALERT_DIALOG (dialog),
                                    _("There was an error displaying help:\n%s"),
                                     error->message);
    adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "close", _("Close"));

    adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "close");
    adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (dialog), "close");

    adw_dialog_present (dialog, GTK_WIDGET (window));
  }
}

static void
chatty_application_show_help (GSimpleAction *action,
			      GVariant      *parameter,
			      gpointer       user_data)
{
  ChattyApplication *self = CHATTY_APPLICATION (user_data);
  GtkUriLauncher *uri_launcher = NULL;
  GtkWindow *window;

  const char *url = "help:chatty";

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
  uri_launcher = gtk_uri_launcher_new (url);
  gtk_uri_launcher_launch (uri_launcher, window, NULL, show_help_cb, self);
}


static void
chatty_application_show_window (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data)
{
  ChattyApplication *self = user_data;

  g_assert (CHATTY_IS_APPLICATION (self));

  g_application_activate (G_APPLICATION (self));
}

static void
chatty_application_open_chat (GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       user_data)
{
  ChattyApplication *self = user_data;
  const char *room_id, *account_id;
  ChattyChat *chat;
  ChattyProtocol protocol;

  g_assert (CHATTY_IS_APPLICATION (self));

  g_variant_get (parameter, "(ssi)", &room_id, &account_id, &protocol);
  g_return_if_fail (room_id && account_id);

  chat = chatty_manager_find_chat_with_name (self->manager, protocol, account_id, room_id);
  g_return_if_fail (chat);

  if (chatty_log_get_verbosity () > 1) {
    g_autoptr(GString) str = NULL;

    str = g_string_new (NULL);
    g_string_append_printf (str, "Opening chat (%p):", chat);
    chatty_log_anonymize_value (str, room_id);
    g_string_append (str, ", account:");
    chatty_log_anonymize_value (str, account_id);
    g_info ("%s", str->str);
  }

  g_application_activate (G_APPLICATION (self));
  chatty_window_open_chat (CHATTY_WINDOW (self->main_window), chat);
}

static void
main_window_focus_changed_cb (ChattyApplication *self)
{
  ChattyChat *chat = NULL;
  GtkWindow *window;
  gboolean has_focus;

  g_assert (CHATTY_IS_APPLICATION (self));

  window = (GtkWindow *)self->main_window;
  has_focus = window && chatty_utils_window_has_toplevel_focus (window);

  if (has_focus)
    chatty_clock_start (chatty_clock_get_default ());
  else
    chatty_clock_stop (chatty_clock_get_default ());

  if (!self->main_window)
    return;

  window = GTK_WINDOW (self->main_window);

  if (gtk_application_get_active_window (GTK_APPLICATION (self)) != window)
    return;

  if (has_focus)
    chat = chatty_application_get_active_chat (self);

  if (chat) {
    chatty_chat_set_unread_count (chat, 0);
    chatty_chat_withdraw_notification (chat);
  }
}

static void
app_window_removed_cb (ChattyApplication *self,
                       GtkWidget         *window)
{
  g_assert (CHATTY_IS_APPLICATION (self));

  if (window == self->main_window)
    chatty_clock_stop (chatty_clock_get_default ());
}

static void
chatty_application_finalize (GObject *object)
{
  ChattyApplication *self = (ChattyApplication *)object;

  g_clear_handle_id (&self->open_uri_id, g_source_remove);
  g_clear_object (&self->manager);

  G_OBJECT_CLASS (chatty_application_parent_class)->finalize (object);
}

static gint
chatty_application_handle_local_options (GApplication *application,
                                         GVariantDict *options)
{
  if (g_variant_dict_contains (options, "version")) {
    g_print ("%s %s\n", PACKAGE_NAME, GIT_VERSION);
    return 0;
  }

  return -1;
}

static gint
chatty_application_command_line (GApplication            *application,
                                 GApplicationCommandLine *command_line)
{
  ChattyApplication *self = (ChattyApplication *)application;
  GVariantDict  *options;
  g_auto(GStrv) arguments = NULL;
  gint argc;

  options = g_application_command_line_get_options_dict (command_line);

  if (g_variant_dict_contains (options, "nologin"))
    chatty_manager_disable_auto_login (chatty_manager_get_default (), TRUE);

#ifdef PURPLE_ENABLED
  if (g_variant_dict_contains (options, "debug"))
    chatty_purple_enable_debug ();
#endif

  arguments = g_application_command_line_get_arguments (command_line, &argc);

  /* Keep only the last URI, if there are many */
  for (guint i = 0; i < argc; i++)
    if (g_str_has_prefix (arguments[i], "sms:")) {
      g_free (self->uri);
      self->uri = g_strdup (arguments[i]);
    }

  g_application_activate (application);

  return 0;
}

static const GActionEntry app_entries[] = {
  { "about", chatty_application_show_about },
  { "help", chatty_application_show_help, },
  { "open-chat", chatty_application_open_chat, "(ssi)" },
  { "show-window", chatty_application_show_window }
};

static void
chatty_application_startup (GApplication *application)
{
  ChattyApplication *self = (ChattyApplication *)application;
  g_autofree char *db_path = NULL;
  g_autofree char *purple_dir = NULL;
  const char *help_accels[] = { "F1", NULL };

  self->manager = chatty_manager_get_default ();

  G_APPLICATION_CLASS (chatty_application_parent_class)->startup (application);

  g_info ("%s %s, git version: %s", PACKAGE_NAME, PACKAGE_VERSION, GIT_VERSION);

  cm_init (TRUE);

  g_set_application_name (_("Chats"));

  if (!gtk_window_get_default_icon_name ())
    gtk_window_set_default_icon_name (CHATTY_APP_ID);

  purple_dir = chatty_utils_get_purple_dir ();
  db_path =  g_build_filename (purple_dir, "chatty", "db", NULL);
  chatty_history_open (chatty_manager_get_history (self->manager),
                       db_path, "chatty-history.db");

  self->settings = chatty_settings_get_default ();
  g_signal_connect_object (self->manager, "open-chat",
                           G_CALLBACK (application_open_chat),
                           self, G_CONNECT_SWAPPED);

  g_signal_connect_object (self, "window-removed",
                           G_CALLBACK (app_window_removed_cb),
                           self, G_CONNECT_AFTER);

  g_action_map_add_action_entries (G_ACTION_MAP (self), app_entries,
                                   G_N_ELEMENTS (app_entries), self);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.help", help_accels);

  if (g_application_get_flags (application) & G_APPLICATION_IS_SERVICE) {
    g_application_hold (application);

    chatty_manager_load (self->manager);
    g_debug ("Enable daemon mode");
  }
}


static void
chatty_application_activate (GApplication *application)
{
  GtkApplication *app = (GtkApplication *)application;
  ChattyApplication *self = (ChattyApplication *)application;

  g_assert (GTK_IS_APPLICATION (app));

  chatty_manager_load (self->manager);

  if (!self->main_window) {
    g_set_weak_pointer (&self->main_window, chatty_window_new (app));
    g_info ("New main window created");

    g_signal_connect_object (self->main_window, "notify::has-toplevel-focus",
                             G_CALLBACK (main_window_focus_changed_cb),
                             self, G_CONNECT_SWAPPED);
  }

  gtk_window_present (GTK_WINDOW (self->main_window));

  /* Open with some delay so that the modem is ready when not in daemon mode */
  if (self->uri)
    self->open_uri_id = g_timeout_add (100,
                                       G_SOURCE_FUNC (application_open_uri),
                                       self);
}

static void
chatty_application_shutdown (GApplication *application)
{
  ChattyApplication *self = (ChattyApplication *)application;

  g_object_unref (chatty_settings_get_default ());
  chatty_history_close (chatty_manager_get_history (self->manager));

  G_APPLICATION_CLASS (chatty_application_parent_class)->shutdown (application);
}

static void
chatty_application_class_init (ChattyApplicationClass *klass)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = chatty_application_finalize;

  application_class->handle_local_options = chatty_application_handle_local_options;
  application_class->command_line = chatty_application_command_line;
  application_class->startup = chatty_application_startup;
  application_class->activate = chatty_application_activate;
  application_class->shutdown = chatty_application_shutdown;
}


static void
chatty_application_init (ChattyApplication *self)
{
  g_application_add_main_option_entries (G_APPLICATION (self), cmd_options);
}


ChattyApplication *
chatty_application_new (void)
{
  return g_object_new (CHATTY_TYPE_APPLICATION,
                       "application-id", CHATTY_APP_ID,
                       "flags", G_APPLICATION_HANDLES_COMMAND_LINE,
                       "register-session", TRUE,
                       NULL);
}

/**
 * chatty_application_get_active_chat:
 * @self: A #ChattyApplication
 *
 * Get the currently shown chat
 *
 * Returns: (transfer none): A #ChattyChat if a chat
 * is shown. %NULL if no chat is shown.  If window is
 * hidden, %NULL is returned regardless of wether a
 * chat is shown or not.
 */
ChattyChat *
chatty_application_get_active_chat (ChattyApplication *self)
{
  g_return_val_if_fail (CHATTY_IS_APPLICATION (self), NULL);

  if (self->main_window)
    return chatty_window_get_active_chat (CHATTY_WINDOW (self->main_window));

  return NULL;
}
