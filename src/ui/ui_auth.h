/**
 * @file ui_auth.h authentication dialog
 *
 * Copyright (C) 2007 Lars Lindner <lars.lindner@gmail.com>
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

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define AUTH_DIALOG_TYPE		(auth_dialog_get_type ())
#define AUTH_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), AUTH_DIALOG_TYPE, AuthDialog))
#define AUTH_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), AUTH_DIALOG_TYPE, AuthDialogClass))
#define IS_AUTH_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), AUTH_DIALOG_TYPE))
#define IS_AUTH_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), AUTH_DIALOG_TYPE))

typedef struct AuthDialog		AuthDialog;
typedef struct AuthDialogClass		AuthDialogClass;
typedef struct AuthDialogPrivate	AuthDialogPrivate;

struct AuthDialog
{
	GObject		parent;
	
	/*< private >*/
	AuthDialogPrivate	*priv;
};

struct AuthDialogClass 
{
	GtkObjectClass parent_class;
};

GType auth_dialog_get_type	(void);

/**
 * Create a new authentication dialog.
 *
 * @param subscription	the subscription whose authentication info is needed
 * @param flags		the flags for the update request after authenticating (FIXME!)
 *
 * @returns new dialog
 */
AuthDialog * ui_auth_dialog_new	(subscriptionPtr subscription, gint flags);

G_END_DECLS

#endif /* _UI_AUTH_H */
