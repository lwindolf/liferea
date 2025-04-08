/**
 * @file ui_dialog.h UI dialog handling
 *
 * Copyright (C) 2007-2016  Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _UI_DIALOG_H
#define _UI_DIALOG_H

#include <gtk/gtk.h>
#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define LIFEREA_DIALOG_TYPE		(liferea_dialog_get_type ())
#define LIFEREA_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LIFEREA_DIALOG_TYPE, LifereaDialog))
#define LIFEREA_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), LIFEREA_DIALOG_TYPE, LifereaDialogClass))
#define IS_LIFEREA_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIFEREA_DIALOG_TYPE))
#define IS_LIFEREA_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), LIFEREA_DIALOG_TYPE))

typedef struct LifereaDialog		LifereaDialog;
typedef struct LifereaDialogClass	LifereaDialogClass;
typedef struct LifereaDialogPrivate	LifereaDialogPrivate;

struct LifereaDialog
{
	GObject		parent;

	/*< private >*/
	LifereaDialogPrivate	*priv;
};

struct LifereaDialogClass
{
	GObjectClass parent_class;
};

GType liferea_dialog_get_type	(void);

/**
 * liferea_dialog_new:
 * @name:		the dialog name
 *
 * Convenience wrapper to create a new dialog and set up its GUI
 * 
 * Returns: the dialog widget
 */
GtkWidget * liferea_dialog_new (const gchar *name);

/**
 * liferea_dialog_lookup:
 * @widget:	the dialog widget
 * @name:	widget name
 *
 * Helper function to look up child widgets of a dialog window.
 * 
 * Returns: found widget (or NULL)
 */
GtkWidget * liferea_dialog_lookup (GtkWidget *widget, const gchar *name);

/**
 * liferea_dialog_entry_get:
 * @widget:	the dialog widget
 * @name:	the GtkEntry name
 * 
 * Returns: the text of the GtkEntry widget with the given name
 */
const gchar * liferea_dialog_entry_get (GtkWidget *widget, const gchar *name);

/**
 * liferea_dialog_entry_set:
 * @widget:	the dialog widget
 * @name:	the GtkEntry name
 * @text:	the text to set
 */
void liferea_dialog_entry_set (GtkWidget *widget, const gchar *name, const gchar *text);

G_END_DECLS

#endif
