/**
 * @file itemview.c    item display interface abstraction
 * 
 * Copyright (C) 2006-2007 Lars Lindner <lars.lindner@gmail.com>
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
#include <string.h>

#include "common.h"
#include "conf.h"
#include "debug.h"
#include "folder.h"
#include "htmlview.h"
#include "itemlist.h"
#include "itemview.h"
#include "node.h"
#include "vfolder.h"
#include "ui/ui_itemlist.h"
#include "ui/ui_mainwindow.h"

gint disableSortingSaving;		/* set in ui_itemlist.c to disable sort-changed callback */

static struct itemView_priv {
	gboolean	htmlOnly;		/**< TRUE if HTML only mode */
	guint		mode;			/**< current item view mode */
	nodePtr		node;			/**< the node whose item are displayed */
	gchar 		*userDefinedDateFmt;	/**< user defined date formatting string */
	gboolean	needsHTMLViewUpdate;	/**< flag to be set when HTML rendering is to be 
						     updated, used to delay HTML updates */
} itemView_priv;

void itemview_init(void) {

	/* Determine date format code */
	
	itemView_priv.userDefinedDateFmt = getStringConfValue(DATE_FORMAT);
	
	/* We now have an empty string or a format string... */
	if(itemView_priv.userDefinedDateFmt && !strlen(itemView_priv.userDefinedDateFmt)) {
	   	/* It's empty and useless... */
		g_free(itemView_priv.userDefinedDateFmt);
		itemView_priv.userDefinedDateFmt = NULL;
	}
	
	/* NOTE: This code is partially broken. In the case of a user
	   supplied format string, such a string is in UTF-8. The
	   strftime function expects the user locale as its input, BUT
	   the user's locale may have an alternate representation of '%'
	   (For example UCS16 has 2 byte characters, although this may be
	   handled by glibc correctly) or may not be able to represent a
	   character used in the string. We shall hope that the user's
	   locale has neither of these problems and convert the format
	   string to the user's locale before calling strftime. The
	   result must be converted back to UTF-8 so that it can be
	   displayed by the itemlist correctly. */

	if(itemView_priv.userDefinedDateFmt) {
		debug1(DEBUG_GUI, "new user defined date format: >>>%s<<<", itemView_priv.userDefinedDateFmt);
		gchar *tmp = itemView_priv.userDefinedDateFmt;
		itemView_priv.userDefinedDateFmt = g_locale_from_utf8(tmp, -1, NULL, NULL, NULL);
		g_free(tmp);
	}
	
	/* Setup HTML widget */

	htmlview_init();
}

void itemview_clear(void) {

	ui_itemlist_clear();
	htmlview_clear();
	
	itemView_priv.needsHTMLViewUpdate = TRUE;
}

void itemview_set_mode(guint mode) {

	if(itemView_priv.mode != mode) {
		itemView_priv.mode = mode;
		htmlview_clear();	/* drop HTML rendering cache */
	}
}

void
itemview_set_displayed_node (nodePtr node)
{
	GtkTreeModel	*model;

	if (node != itemView_priv.node) {
		itemView_priv.node = node;

		/* 1. Perform UI item list preparations ... */
		
		/* Free the old itemstore and create a new one; this is the only way to disable sorting */
		ui_itemlist_reset_tree_store ();	 /* this also clears the itemlist. */
		model = GTK_TREE_MODEL (ui_itemlist_get_tree_store ());

		/* Disable attachment icon column (will be enabled when loading first item with an enclosure) */
		ui_itemlist_enable_encicon_column (FALSE);

		if(node) {
			ui_itemlist_enable_favicon_column (IS_FOLDER (node) || IS_VFOLDER (node));

			/* c)  disable sorting for performance reasons, set sort column
			       and enable sorting again */
			disableSortingSaving++;
			gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE(model), 
	                                                      node->sortColumn, 
	                                                      node->sortReversed?GTK_SORT_DESCENDING:GTK_SORT_ASCENDING);
			disableSortingSaving--;
		}

		/* 2. Reset view state */
		itemview_clear ();
		
		/* 3. And repare HTML view */
		htmlview_set_displayed_node (node);
	}
}

void itemview_add_item(itemPtr item) {

	if(ITEMVIEW_ALL_ITEMS != itemView_priv.mode)
		/* add item in 3 pane mode */
		ui_itemlist_add_item(item);
	else
		/* force HTML update in 2 pane mode */
		itemView_priv.needsHTMLViewUpdate = TRUE;
		
	htmlview_add_item(item);
}

void itemview_remove_item(itemPtr item) {

	if(!ui_itemlist_contains_item(item->id))
		return;

	if(ITEMVIEW_ALL_ITEMS != itemView_priv.mode)
		/* remove item in 3 pane mode */
		ui_itemlist_remove_item(item);
	else
		/* force HTML update in 2 pane mode */
		itemView_priv.needsHTMLViewUpdate = TRUE;

	htmlview_remove_item(item);
}

void itemview_select_item(itemPtr item) {

	if(!itemView_priv.node)
		return;
		
	itemView_priv.needsHTMLViewUpdate = TRUE;

	ui_itemlist_select(item);
	htmlview_select_item(item);
}

void itemview_update_item(itemPtr item) {

	if(!itemView_priv.node)
		return;
		
	/* Always update the GtkTreeView (bail-out done in ui_itemlist_update_item() */
	if(ITEMVIEW_ALL_ITEMS != itemView_priv.mode)
		ui_itemlist_update_item(item);

	/* Bail out if no HTML update necessary */
	switch(itemView_priv.mode) {
		case ITEMVIEW_ALL_ITEMS:
			/* No HTML update needed if 2 pane mode and item not in item set */
			if(!ui_itemlist_contains_item(item->id))
				return;
			break;
		case ITEMVIEW_SINGLE_ITEM:		
			/* No HTML update needed if 3 pane mode and item not displayed */
			if((item != itemlist_get_selected()) && 
			   !ui_itemlist_contains_item(item->id))
				return;
			break;
		default:
			/* Return in all other display modes */
			return;
			break;
	}
	
	itemView_priv.needsHTMLViewUpdate = TRUE;
	htmlview_update_item(item);
}

void itemview_update_all_items(void) {

	if(!itemView_priv.node)
		return;
		
	/* Always update the GtkTreeView (bail-out done in ui_itemlist_update_item() */
	if(ITEMVIEW_ALL_ITEMS != itemView_priv.mode)
		ui_itemlist_update_all_items();
		
	itemView_priv.needsHTMLViewUpdate = TRUE;
	htmlview_update_all_items();
}

void itemview_update_node_info(nodePtr node) {

	if(!itemView_priv.node)
		return;
	
	if(itemView_priv.node != node)
		return;

	if(ITEMVIEW_NODE_INFO != itemView_priv.mode)
		return;

	itemView_priv.needsHTMLViewUpdate = TRUE;
	/* Just setting the update flag, because node info is not cached */
}

void itemview_update(void) {

	if(itemView_priv.needsHTMLViewUpdate) {
		itemView_priv.needsHTMLViewUpdate = FALSE;
		htmlview_update(ui_mainwindow_get_active_htmlview(), itemView_priv.mode);
	}
}

/* date format handling (not sure if this is the right place) */

gchar * itemview_format_date(time_t date) {
	gchar		*timestr;

	if(itemView_priv.userDefinedDateFmt)
		timestr = common_format_date(date, itemView_priv.userDefinedDateFmt);
	else
		timestr = common_format_nice_date(date);
	
	return timestr;
}
