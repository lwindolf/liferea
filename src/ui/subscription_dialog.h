/**
 * @file subscription_dialog.h  property dialog for feed subscriptions
 *
 * Copyright (C) 2007-2025 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _SUBSCRIPTION_DIALOG_H
#define _SUBSCRIPTION_DIALOG_H

#include <gtk/gtk.h>

#include "subscription.h"

G_BEGIN_DECLS

#define SUBSCRIPTION_PROP_DIALOG_TYPE (subscription_prop_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SubscriptionPropDialog, subscription_prop_dialog, SUBSCRIPTION_PROP, DIALOG, GObject)

/**
 * subscription_prop_dialog_new:
 * Creates a feed properties dialog (FIXME: handle
 * generic subscriptions too)
 *
 * @subscription:	the subscription to load into the dialog
 *
 * Returns: (transfer none): a properties dialog
 */
SubscriptionPropDialog *subscription_prop_dialog_new	(subscriptionPtr subscription);

#define SIMPLE_SUBSCRIPTION_DIALOG_TYPE (simple_subscription_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SimpleSubscriptionDialog, simple_subscription_dialog, SIMPLE_SUBSCRIPTION, DIALOG, GObject)

/**
 * subscription_dialog_new:
 * Create a simple subscription dialog.
 *
 * Returns: (transfer none): dialog instance
 */
SimpleSubscriptionDialog *subscription_dialog_new (void);

G_END_DECLS

#endif /* _SUBSCRIPTION_DIALOG_H */
