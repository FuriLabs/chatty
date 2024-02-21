/*
 * Copyright 2023, Purism SPC
 *           2023, Chris Talbot
 *
 * Author(s):
 *   Chris Talbot
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <gtk/gtk.h>
#include <gtk/gtkwidget.h>

#define MM_DEFAULT                  "mm-default"
#define ERROR_MM_SMS_SEND_RECEIVE      "error-sms"
#define ERROR_MM_MMS_TEMP_SEND_RECEIVE "error-mms-temp-send-recieve"
#define ERROR_MM_MMS_SEND_RECEIVE      "error-mms-send-recieve"
#define ERROR_MM_MMS_DELIVERY          "error-mms-delivery"
#define ERROR_MM_MMS_CONFIGURATION     "error-mms-configuration"

void        chatty_mm_notify_message (const char          *title,
                                      const char          *id,
                                      const char          *body);

void        chatty_mm_notify_withdraw_notification (const char *id);
