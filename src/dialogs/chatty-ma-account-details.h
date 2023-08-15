/* chatty-ma-account-details.h
 *
 * Copyright 2021 Purism SPC
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
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

#include "chatty-account.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_MA_ACCOUNT_DETAILS (chatty_ma_account_details_get_type ())

G_DECLARE_FINAL_TYPE (ChattyMaAccountDetails, chatty_ma_account_details, CHATTY, MA_ACCOUNT_DETAILS, AdwPreferencesPage)

GtkWidget       *chatty_ma_account_details_new      (void);
void             chatty_ma_account_details_save_async  (ChattyMaAccountDetails *self,
                                                        GAsyncReadyCallback     callback,
                                                        gpointer                user_data);
gboolean         chatty_ma_account_details_save_finish (ChattyMaAccountDetails *self,
                                                        GAsyncResult           *result,
                                                        GError                **error);
ChattyAccount   *chatty_ma_account_details_get_item (ChattyMaAccountDetails *self);
void             chatty_ma_account_details_set_item (ChattyMaAccountDetails *self,
                                                     ChattyAccount          *account);

G_END_DECLS
