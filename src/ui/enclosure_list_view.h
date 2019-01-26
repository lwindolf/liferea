/**
 * @file enclosure-list-view.h enclosures list view
 *
 * Copyright (C) 2005-2018 Lars Windolf <lars.windolf@gmx.de>
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

#define ENCLOSURE_LIST_VIEW_TYPE (enclosure_list_view_get_type ())
G_DECLARE_FINAL_TYPE (EnclosureListView, enclosure_list_view, ENCLOSURE_LIST, VIEW, GObject)

/**
 * enclosure_list_view_new:
 * Sets up a new enclosure list view.
 *
 * Returns: (transfer none): a new enclosure list view
 */
EnclosureListView * enclosure_list_view_new (void);

/**
 * enclosure_list_view_get_widget: (skip)
 *
 * Returns the rendering widget for a HTML view. Only
 * to be used by ui_mainwindow.c for widget reparenting.
 */
GtkWidget * enclosure_list_view_get_widget (EnclosureListView *elv);

/**
 * enclosure_list_view_load:
 * Loads the enclosure list of the given item into the
 * given enclosure list view widget.
 *
 * @elv:	the enclosure list view
 * @item:	the item
 */
void enclosure_list_view_load (EnclosureListView *elv, itemPtr item);

/**
 * enclosure_list_view_select:
 * @elv:		the enclosure list view
 * @position:	the position to select
 *
 * Select the nth enclosure in the enclosure list.
 */
void enclosure_list_view_select (EnclosureListView *elv, guint position);

/**
 * enclosure_list_view_select_next:
 * @elv:	the enclosure list view
 *
 * Select the next enclosure in the list, or the first if none was
 * selected or the end of the list was reached.
 */
void enclosure_list_view_select_next (EnclosureListView *elv);


/**
 * enclosure_list_view_open_next:
 * @elv: the enclosure list view
 *
 * Select the next enclosure in the list and open it.
 */
void enclosure_list_view_open_next (EnclosureListView *elv);

/**
 * enclosure_list_view_hide:
 * Hides the enclosure list view.
 *
 * @elv:	the enclosure list view
 */
void enclosure_list_view_hide (EnclosureListView *elv);

/* related menu creation and callbacks */

void on_popup_open_enclosure(gpointer callback_data);
void on_popup_save_enclosure(gpointer callback_data);
void on_popup_copy_enclosure(gpointer callback_data);

// FIXME: this does not belong here!
void ui_enclosure_change_type (encTypePtr type);

G_END_DECLS

#endif
