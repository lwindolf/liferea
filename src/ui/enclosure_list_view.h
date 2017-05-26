/**
 * @file enclosure-list-view.h enclosures list view
 *
 * Copyright (C) 2005-2008 Lars Windolf <lars.windolf@gmx.de>
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
	GObjectClass parent_class;
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

/**
 * enclosure_list_view_select:
 *
 * Select the nth enclosure in the enclosure list.
 *
 * @elv:	the enclosure list view
 * @position:	the position to select
 */
void enclosure_list_view_select (EnclosureListView *elv, guint position);

/**
 * Hides the enclosure list view.
 *
 * @param elv	the enclosure list view
 */
void enclosure_list_view_hide (EnclosureListView *elv);

/* related menu creation and callbacks */

void on_popup_open_enclosure(gpointer callback_data);
void on_popup_save_enclosure(gpointer callback_data);
void on_popup_copy_enclosure(gpointer callback_data);

// FIXME: this does not belong here!
void ui_enclosure_change_type (encTypePtr type);

G_END_DECLS

#endif /* _UI_ENCLOSURE_H */
