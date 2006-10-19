/**
 * @file itemview.c    item display interface abstraction
 * 
 * Copyright (C) 2006 Lars Lindner <lars.lindner@gmx.net>
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
 
#include "htmlview.h"
#include "itemview.h"
#include "ui/ui_itemlist.h"
#include "ui/ui_mainwindow.h"

static struct itemViewPriv {
	gboolean	htmlOnly;	/**< TRUE if HTML only mode */
	guint		mode;		/**< current item view mode */
} itemView_priv;

void itemview_init(void) {

	htmlview_init();
}

void itemview_clear(void) {

	ui_itemlist_clear();
	htmlview_clear();
}

void itemview_set_mode(guint mode) {

	if(itemView_priv.mode != mode) {
		itemView_priv.mode = mode;
		htmlview_clear();	/* drop HTML rendering cache */
	}
}

void itemview_set_itemset(itemSetPtr itemSet) {

	itemview_clear();
	htmlview_set_itemset(itemSet);
}

void itemview_add_item(itemPtr item) {

	if(ITEMVIEW_ALL_ITEMS != itemView_priv.mode)
		ui_itemlist_add_item(item);
		
	htmlview_add_item(item);
}

void itemview_remove_item(itemPtr item) {

	if(ITEMVIEW_ALL_ITEMS != itemView_priv.mode)
		ui_itemlist_remove_item(item);

	htmlview_remove_item(item);
}

void itemview_select_item(itemPtr item) {

	ui_itemlist_select(item);
}

void itemview_update_item(itemPtr item) {

	if(ITEMVIEW_ALL_ITEMS != itemView_priv.mode)
		ui_itemlist_update_item(item);
		
	htmlview_update_item(item);
}

void itemview_update(void) {

	ui_itemlist_update();
	
	htmlview_update(ui_mainwindow_get_active_htmlview(), itemView_priv.mode);
}
