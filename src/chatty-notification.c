/* chatty-message.c
 *
 * Copyright 2021 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-notification"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define LIBFEEDBACK_USE_UNSTABLE_API
#include <libfeedback.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "chatty-notification.h"
#include "chatty-settings.h"
#include "chatty-utils.h"


#define NOTIFICATION_TIMEOUT 300 /* milliseconds */

struct _ChattyNotification
{
  GObject         parent_instance;

  ChattyChat     *chat;
  char           *chat_name;

  GNotification  *notification;
  int             timeout_id;
};

G_DEFINE_TYPE (ChattyNotification, chatty_notification, G_TYPE_OBJECT)

static void
show_notification (gpointer user_data)
{
  ChattyNotification *self = user_data;
  GApplication *app;

  g_assert (CHATTY_IS_NOTIFICATION (self));

  self->timeout_id = 0;
  app = g_application_get_default ();

  if (app)
    g_application_send_notification (app, self->chat_name,
                                     self->notification);
}

void
chatty_notification_withdraw_notification (ChattyNotification *self)
{
  GApplication *app;

  g_assert (CHATTY_IS_NOTIFICATION (self));

  if (chatty_chat_get_unread_count (self->chat) > 0)
    return;

  g_clear_handle_id (&self->timeout_id, g_source_remove);
  app = g_application_get_default ();

  if (app)
    g_application_withdraw_notification (app, self->chat_name);
}

static void
notification_chat_changed_cb (ChattyNotification *self)
{
  g_assert (CHATTY_IS_NOTIFICATION (self));

  chatty_notification_withdraw_notification (self);
}

static void
feedback_triggered_cb (GObject      *object,
		       GAsyncResult *result,
                       gpointer      user_data)
{
  g_autoptr (ChattyNotification) self = user_data;
  LfbEvent *event = (LfbEvent *)object;
  g_autoptr (GError) error = NULL;

  g_assert (LFB_IS_EVENT (event));
  g_assert (CHATTY_IS_NOTIFICATION (self));

  if (!lfb_event_trigger_feedback_finish (event, result, &error)) {
    g_warning ("Failed to trigger feedback for %s",
	       lfb_event_get_event (event));
  }
}

static void
chatty_notification_finalize (GObject *object)
{
  ChattyNotification *self = (ChattyNotification *)object;

  g_clear_handle_id (&self->timeout_id, g_source_remove);
  g_clear_object (&self->notification);

  g_free (self->chat_name);

  G_OBJECT_CLASS (chatty_notification_parent_class)->finalize (object);
}

static void
chatty_notification_class_init (ChattyNotificationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = chatty_notification_finalize;
}

static void
chatty_notification_init (ChattyNotification *self)
{
  self->notification = g_notification_new ("chatty");

#if GLIB_CHECK_VERSION(2,70,0)
  g_notification_set_category (self->notification, "im.received");
#endif
}

ChattyNotification *
chatty_notification_new (ChattyChat *chat)
{
  ChattyNotification *self;

  g_return_val_if_fail (CHATTY_IS_CHAT (chat), NULL);

  self = g_object_new (CHATTY_TYPE_NOTIFICATION, NULL);
  self->chat = chat;
  self->chat_name = g_strdup (chatty_chat_get_chat_name (chat));
  g_set_weak_pointer (&self->chat, chat);

  g_signal_connect_object (self->chat, "changed",
                           G_CALLBACK (notification_chat_changed_cb),
                           self, G_CONNECT_SWAPPED);

  g_notification_set_default_action_and_target (self->notification, "app.open-chat",
                                                "(ssi)", self->chat_name,
                                                chatty_item_get_username (CHATTY_ITEM (chat)),
                                                chatty_item_get_protocols (CHATTY_ITEM (chat)));

  return self;
}

void
chatty_notification_show_message (ChattyNotification *self,
                                  ChattyMessage      *message,
                                  const char         *name,
                                  unsigned int        unread_count,
                                  gboolean            is_sms)
{
  g_autofree char *title = NULL;
  g_autoptr(LfbEvent) event = NULL;
  ChattyMsgType type;

  g_return_if_fail (CHATTY_IS_NOTIFICATION (self));
  g_return_if_fail (CHATTY_IS_MESSAGE (message));

  if (name)
    title = g_strdup_printf (_("New message from %s"), name);
  else
    title = g_strdup (_("Message Received"));

  type = chatty_message_get_msg_type (message);

  switch (type) {
    case CHATTY_MESSAGE_UNKNOWN:
    case CHATTY_MESSAGE_TEXT:
    case CHATTY_MESSAGE_HTML:
    case CHATTY_MESSAGE_HTML_ESCAPED:
    case CHATTY_MESSAGE_MATRIX_HTML:
    case CHATTY_MESSAGE_LOCATION:
      ChattySettings *settings;
      settings = chatty_settings_get_default ();

      if (chatty_settings_get_strip_url_tracking_ids (settings)) {
        g_autofree char *utm_stripped_message = NULL;

        utm_stripped_message = chatty_utils_strip_utm_from_message (chatty_message_get_text (message));
        g_notification_set_body (self->notification, utm_stripped_message);
      } else
        g_notification_set_body (self->notification, chatty_message_get_text (message));
      break;

    case CHATTY_MESSAGE_FILE:
      g_notification_set_body (self->notification, _("File"));
      break;

    case CHATTY_MESSAGE_IMAGE:
      g_notification_set_body (self->notification, _("Picture"));
      break;

    case CHATTY_MESSAGE_VIDEO:
      g_notification_set_body (self->notification, _("Video"));
      break;

    case CHATTY_MESSAGE_AUDIO:
      g_notification_set_body (self->notification, _("Audio File"));
      break;

    case CHATTY_MESSAGE_MMS:
      if (chatty_message_get_text (message) && *chatty_message_get_text (message))
        g_notification_set_body (self->notification, chatty_message_get_text (message));
      else
        g_notification_set_body (self->notification, _("MMS"));
      break;

    default:
      g_notification_set_body (self->notification, chatty_message_get_text (message));
      break;
    }

  if (unread_count >= 2) {
    g_autofree char *message_unread = NULL;

    message_unread = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE,
                                                   "%i Message",
                                                   "%i Messages",
                                                   unread_count),
                                      unread_count);

    g_notification_set_body (self->notification, message_unread);
  }

  g_notification_set_title (self->notification, title);
  g_notification_set_priority (self->notification, G_NOTIFICATION_PRIORITY_HIGH);

  /* Delay the notification a bit so that we can squash multiple notifications
   * if we get them too fast */
  g_clear_handle_id (&self->timeout_id, g_source_remove);
  self->timeout_id = g_timeout_add_once (NOTIFICATION_TIMEOUT,
                                         show_notification, self);
  if (is_sms)
    event = lfb_event_new ("message-new-sms");
  else
    event = lfb_event_new ("message-new-instant");

  lfb_event_trigger_feedback_async (event, NULL,
                                    feedback_triggered_cb,
                                    g_object_ref (self));
}
