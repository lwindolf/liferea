/**
 * @file ui_subscription.h subscription dialogs
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

#ifndef _UI_SUBSCRIPTION_H
#define _UI_SUBSCRIPTION_H

#include <glib-object.h>
#include <glib.h>

#include "subscription.h"

G_BEGIN_DECLS

typedef struct SubscriptionDialogPrivate	SubscriptionDialogPrivate;

#define SUBSCRIPTION_PROP_DIALOG_TYPE			(subscription_prop_dialog_get_type ())
#define SUBSCRIPTION_PROP_DIALOG(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), SUBSCRIPTION_PROP_DIALOG_TYPE, SubscriptionPropDialog))
#define SUBSCRIPTION_PROP_DIALOG_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), SUBSCRIPTION_PROP_DIALOG_TYPE, SubscriptionPropDialogClass))
#define IS_SUBSCRIPTION_PROP_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), SUBSCRIPTION_PROP_DIALOG_TYPE))
#define IS_SUBSCRIPTION_PROP_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), SUBSCRIPTION_PROP_DIALOG_TYPE))

typedef struct SubscriptionPropDialog		SubscriptionPropDialog;
typedef struct SubscriptionPropDialogClass	SubscriptionPropDialogClass;

struct SubscriptionPropDialog
{
	GObject parent;
	
	/*< private >*/
	SubscriptionDialogPrivate	*priv;
};

struct SubscriptionPropDialogClass 
{
	GtkObjectClass parent_class;
};

GType subscription_prop_dialog_get_type	(void);

/**
 * Creates a feed properties dialog (FIXME: handle 
 * generic subscriptions too)
 *
 * @param subscription	the subscription to load into the dialog
 *
 * @returns a properties dialog
 */
SubscriptionPropDialog *ui_subscription_prop_dialog_new	(subscriptionPtr subscription);


#define NEW_SUBSCRIPTION_DIALOG_TYPE		(new_subscription_dialog_get_type ())
#define NEW_SUBSCRIPTION_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), NEW_SUBSCRIPTION_DIALOG_TYPE, NewSubscriptionDialog))
#define NEW_SUBSCRIPTION_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), NEW_SUBSCRIPTION_DIALOG_TYPE, NewSubscriptionDialogClass))
#define IS_NEW_SUBSCRIPTION_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEW_SUBSCRIPTION_DIALOG_TYPE))
#define IS_NEW_SUBSCRIPTION_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), NEW_SUBSCRIPTION_DIALOG_TYPE))

typedef struct NewSubscriptionDialog		NewSubscriptionDialog;
typedef struct NewSubscriptionDialogClass	NewSubscriptionDialogClass;

struct NewSubscriptionDialog
{
	GObject parent;
	
	/*< private >*/
	SubscriptionDialogPrivate	*priv;
};

struct NewSubscriptionDialogClass 
{
	GtkObjectClass parent_class;
};

GType new_subscription_dialog_get_type	(void);

/**
 * Create a new complex subscription dialog. Used only
 * when requested by user from simple subscription dialog.
 *
 * @param parentNode	the parent node for the new subscription
 *
 * @returns dialog instance
 */
NewSubscriptionDialog *ui_complex_subscription_dialog_new	(nodePtr parentNode);


#define SIMPLE_SUBSCRIPTION_DIALOG_TYPE			(simple_subscription_dialog_get_type ())
#define SIMPLE_SUBSCRIPTION_DIALOG(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), SIMPLE_SUBSCRIPTION_DIALOG_TYPE, SimpleSubscriptionDialog))
#define SIMPLE_SUBSCRIPTION_DIALOG_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), SIMPLE_SUBSCRIPTION_DIALOG_TYPE, SimpleSubscriptionDialogClass))
#define IS_SIMPLE_SUBSCRIPTION_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), SIMPLE_SUBSCRIPTION_DIALOG_TYPE))
#define IS_SIMPLE_SUBSCRIPTION_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), SIMPLE_SUBSCRIPTION_DIALOG_TYPE))

typedef struct SimpleSubscriptionDialog		SimpleSubscriptionDialog;
typedef struct SimpleSubscriptionDialogClass	SimpleSubscriptionDialogClass;

struct SimpleSubscriptionDialog
{
	GObject parent;
	
	/*< private >*/
	SubscriptionDialogPrivate	*priv;
};

struct SimpleSubscriptionDialogClass 
{
	GtkObjectClass parent_class;
};

GType simple_subscription_dialog_get_type	(void);

/**
 * Create a simple subscription dialog.
 *
 * @param parentNode	the parent node for the new subscription
 *
 * @returns dialog instance
 */
SimpleSubscriptionDialog *ui_subscription_dialog_new	(nodePtr parentNode);


G_END_DECLS

#endif /* _UI_SUBSCRIPTION_H */
