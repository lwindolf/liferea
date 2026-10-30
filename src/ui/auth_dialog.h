/**
 * @file auth_dialog.h  authentication support dialogs
 *
 * Copyright (C) 2007-2026 Lars Windolf <lars.windolf@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _UI_AUTH_H
#define _UI_AUTH_H

#include <gtk/gtk.h>

#include "subscription.h"

G_BEGIN_DECLS

#define AUTH_DIALOG_TYPE (auth_dialog_get_type ())
G_DECLARE_FINAL_TYPE (AuthDialog, auth_dialog, AUTH, DIALOG, GObject)

/**
 * auth_dialog_new:
 * @subscription:	the subscription whose authentication info is needed
 * @flags:		the flags for the update request after authenticating
 * 
 * Create a new authentication dialog if there is not already one for
 * the given subscription. Ensures there is only one auth dialog at any time.
 * If there is already one open the function does nothing. We rely on 
 * subscription auto-updating to repeat the opening (using the subscription->authPending
 * flag).
 */
void auth_dialog_new (subscriptionPtr subscription, gint flags);

/**
 * AuthTokenCallback:
 * @token:      the non-empty token entered by the user
 * @user_data:  user data passed to auth_token_dialog_new()
 *
 * Called only when the user submitted a token. Not called on cancel.
 */
typedef void (*AuthTokenCallback) (const gchar *token, gpointer user_data);

/**
 * auth_token_dialog_new:
 * @url:        microblog service URL to be shown to user
 * @tokenId:    lookup id
 * @cb:         success callback
 * @user_data:  user data for the callback
 *
 * Asks for an app token. Closes previous dialog if needed.
 */
void auth_token_dialog_new (const gchar *url, const gchar *tokenId, AuthTokenCallback cb, gpointer user_data);

G_END_DECLS

#endif /* _UI_AUTH_H */
