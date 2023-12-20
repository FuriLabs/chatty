/*
 * Copyright (C) 2023 Chris Talbot
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define CHATTY_TYPE_WRONG_PW_DIALOG (chatty_wrong_pw_dialog_get_type())
G_DECLARE_FINAL_TYPE (ChattyWrongPwDialog, chatty_wrong_pw_dialog, CHATTY, WRONG_PW_DIALOG, GtkDialog)


GtkWidget *chatty_wrong_pw_dialog_new (GtkWindow *parent_window);

void       chatty_wrong_pw_dialog_set_description (ChattyWrongPwDialog *self,
                                                   const char          *description);
void       chatty_wrong_pw_dialog_set_ma_account  (ChattyWrongPwDialog *self,
                                                   ChattyMaAccount     *account);
void       chatty_wrong_pw_dialog_set_cm_client   (ChattyWrongPwDialog *self,
                                                   CmClient            *cm_client);

G_END_DECLS
