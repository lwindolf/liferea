/**
 * @file ui_dialog.h UI dialog handling
 *
 * Copyright (C) 2007 Lars Lindner <lars.lindner@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
 
#ifndef _UI_DIALOG_H
#define _UI_DIALOG_H
 
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
	GtkObjectClass parent_class;	
};

GType liferea_dialog_get_type	(void);

/**
 * Convenience wrapper to create a new dialog and set up its GUI
 *
 * @param filename	path of glade widget file (or NULL for default)
 * @param name		the dialog name
 *
 * @returns the dialog widget
 */
GtkWidget * liferea_dialog_new (const gchar *filename, const gchar *name);

/**
 * Helper function to look up child widgets of a dialog window.
 *
 * @param ld		the dialog widget
 * @param name		widget name
 *
 * @returns found widget (or NULL)
 */
GtkWidget * liferea_dialog_lookup (GtkWidget *widget, const gchar *name);

G_END_DECLS
 
#endif
