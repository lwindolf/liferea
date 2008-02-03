/**
 * @file enclosure-list-view.h enclosures list view
 *
 * Copyright (C) 2005-2008 Lars Lindner <lars.lindner@gmail.com>
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

#ifndef _ENCLOSURE_LIST_VIEW_H
#define _ENCLOSURE_LIST_VIEW_H

#include <gtk/gtk.h>
#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#include "item.h"
#include "enclosure.h"		// FIXME: should not be necessary

#define ENCLOSURE_LIST_VIEW_TYPE		(enclosure_list_view_get_type ())
#define ENCLOSURE_LIST_VIEW(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), ENCLOSURE_LIST_VIEW_TYPE, EnclosureListView))
#define ENCLOSURE_LIST_VIEW_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), ENCLOSURE_LIST_VIEW_TYPE, EnclosureListViewClass))
#define IS_ENCLOSURE_LIST_VIEW(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), ENCLOSURE_LIST_VIEW_TYPE))
#define IS_ENCLOSURE_LIST_VIEW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), ENCLOSURE_LIST_VIEW_TYPE))

typedef struct EnclosureListView		EnclosureListView;
typedef struct EnclosureListViewClass	EnclosureListViewClass;
typedef struct EnclosureListViewPrivate	EnclosureListViewPrivate;

struct EnclosureListView
{
	GObject		parent;
	
	/*< private >*/
	EnclosureListViewPrivate	*priv;
};

struct EnclosureListViewClass 
{
	GtkObjectClass parent_class;
};

GType enclosure_list_view_get_type	(void);

/**
 * Sets up a new enclosure list view.
 *
 * @returns a new enclosure list view
 */
EnclosureListView * enclosure_list_view_new (void);

/**
 * Returns the rendering widget for a HTML view. Only
 * to be used by ui_mainwindow.c for widget reparenting.
 *
 * @param elv	the enclosure list view
 *
 * @returns the rendering widget
 */
GtkWidget * enclosure_list_view_get_widget (EnclosureListView *elv);

/**
 * Loads the enclosure list of the given item into the
 * given enclosure list view widget.
 *
 * @param elv	the enclosure list view
 * @param item	the item
 */
void enclosure_list_view_load (EnclosureListView *elv, itemPtr item);

/* related menu creation and callbacks */

/** 
 * Opens a popup menu for the given link 
 *
 * @param url		valid HTTP URL
 */
void ui_enclosure_new_popup(const gchar *url);

void on_popup_open_enclosure(gpointer callback_data, guint callback_action, GtkWidget *widget);
void on_popup_save_enclosure(gpointer callback_data, guint callback_action, GtkWidget *widget);

// FIXME: these do not belong here!
void ui_enclosure_change_type (encTypePtr type);
void ui_enclosure_remove_type (encTypePtr type);

#endif /* _UI_ENCLOSURE_H */
