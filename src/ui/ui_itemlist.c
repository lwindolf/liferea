/**
 * @file ui_itemlist.c item list/view handling
 *  
 * Copyright (C) 2004-2007 Lars Lindner <lars.lindner@gmail.com>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <langinfo.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>

#include "support.h"
#include "callbacks.h"
#include "common.h"
#include "debug.h"
#include "feed.h"
#include "feedlist.h"
#include "item.h"
#include "itemlist.h"
#include "social.h"
#include "ui/ui_htmlview.h"
#include "ui/ui_itemlist.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_tray.h"

static GHashTable 	*item_to_iter = NULL;	/** hash table used for fast item->tree iter lookup */

static GtkWidget 	*itemlist_treeview = NULL;

/* helper functions for item <-> iter conversion */

static itemPtr ui_iter_to_item(GtkTreeIter *iter) {
	nodePtr		sourceNode = NULL, node = NULL;
	gulong		sourceNr;
	
	gtk_tree_model_get(GTK_TREE_MODEL(ui_itemlist_get_tree_store()), 
	                   iter, IS_PARENT, &node, IS_SOURCE, &sourceNode, IS_NR, &sourceNr, -1);

	if(node)
		return itemset_lookup_item(node->itemSet, sourceNode, sourceNr);
	else
		return NULL;
}

static gboolean ui_item_to_iter(itemPtr item, GtkTreeIter *iter) {
	GtkTreeIter *old_iter;

	old_iter = g_hash_table_lookup(item_to_iter, (gpointer)item);
	if(!old_iter) {
		return FALSE;
	} else {
		*iter = *old_iter;
		return TRUE;
	}
}

/* sort function for the item list date column */
static gint ui_itemlist_date_sort_func(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data) {
	gulong	timea, timeb;
	double	diff;

	gtk_tree_model_get(model, a, IS_TIME, &timea, -1);
	gtk_tree_model_get(model, b, IS_TIME, &timeb, -1);
	diff = difftime((time_t)timeb, (time_t)timea);
	
	if(diff < 0)
		return 1;
	else if(diff > 0)
		return -1;
	else
		return 0;
}

static gint ui_itemlist_favicon_sort_func(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data) {
	nodePtr	node1, node2;
	
	gtk_tree_model_get(model, a, IS_SOURCE, &node1, -1);
	gtk_tree_model_get(model, b, IS_SOURCE, &node2, -1);
	
	if(!node1->id || !node2->id)
		return 0;
		
	return strcmp(node1->id, node2->id);
}

extern gint disableSortingSaving;

void ui_itemlist_reset_tree_store(void) {
	GtkTreeModel    *model;

	/* Disable sorting for performance reasons */
	model = GTK_TREE_MODEL(ui_itemlist_get_tree_store());

	ui_itemlist_clear();

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(itemlist_treeview));
	gtk_tree_view_set_model(GTK_TREE_VIEW(itemlist_treeview), NULL);
	
	g_object_unref(model);
	gtk_tree_view_set_model(GTK_TREE_VIEW(itemlist_treeview), GTK_TREE_MODEL(ui_itemlist_get_tree_store()));

	ui_itemlist_prefocus();
}

GtkTreeStore * ui_itemlist_get_tree_store(void) {
	GtkTreeModel	*model;
	GtkTreeStore	*itemstore;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(itemlist_treeview));
	if(!model) {
		itemstore = gtk_tree_store_new(IS_LEN,
		                               G_TYPE_ULONG,	/* IS_TIME */
		                               G_TYPE_STRING, 	/* IS_TIME_STR */
		                               G_TYPE_STRING,	/* IS_LABEL */
		                               GDK_TYPE_PIXBUF,	/* IS_STATEICON */
		                               G_TYPE_ULONG,	/* IS_NR */
					       G_TYPE_POINTER,	/* IS_PARENT */
		                               GDK_TYPE_PIXBUF,	/* IS_FAVICON */
		                               GDK_TYPE_PIXBUF,	/* IS_ENCICON */
					       G_TYPE_BOOLEAN,	/* IS_ENCLOSURE */
					       G_TYPE_POINTER,	/* IS_SOURCE */
					       G_TYPE_UINT	/* IS_STATE */
					       );
		gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(itemstore), IS_TIME, ui_itemlist_date_sort_func, NULL, NULL);
		gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(itemstore), IS_SOURCE, ui_itemlist_favicon_sort_func, NULL, NULL);
		g_signal_connect(G_OBJECT(itemstore), "sort-column-changed", G_CALLBACK(itemlist_sort_column_changed_cb), NULL);
	} else {
		itemstore = GTK_TREE_STORE(model);
	}
	
	return itemstore;
}

void ui_itemlist_remove_item(itemPtr item) {
	GtkTreeIter	*iter;

	g_assert(NULL != item);
	if(NULL != (iter = g_hash_table_lookup(item_to_iter, (gpointer)item))) {
		gtk_tree_store_remove(ui_itemlist_get_tree_store(), iter);
		g_hash_table_remove(item_to_iter, (gpointer)item);
	} else {
		/* g_warning("item to be removed not found in tree iter lookup hash!"); */
	}
}

/* cleans up the item list, sets up the iter hash when called for the first time */
void ui_itemlist_clear(void) {
	GtkAdjustment		*adj;
	GtkTreeView		*treeview;
	GtkTreeStore		*itemstore = ui_itemlist_get_tree_store();

	treeview = GTK_TREE_VIEW(itemlist_treeview);

	/* unselecting all items is important to remove items
	   whose removal is deferred until unselecting */
	gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(treeview));
	
	adj = gtk_tree_view_get_vadjustment(treeview);
	gtk_adjustment_set_value(adj, 0.0);
	gtk_tree_view_set_vadjustment(treeview, adj);

	if(itemstore)
		gtk_tree_store_clear(itemstore);
	if(item_to_iter)
		g_hash_table_destroy(item_to_iter);
	
	item_to_iter = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
}

void ui_itemlist_update_item(itemPtr item) {
	GtkTreeIter	iter;
	gchar		*label, *time_str, *esc_title, *esc_time_str, *tmp;
	const gchar 	*direction_marker;
	GdkPixbuf	*icon = NULL;

	if(ui_item_to_iter(item, &iter)) {
		/* Time */
		if(0 != item->time) {
			esc_time_str = itemview_format_date((time_t)item->time);
			/* the time value is no markup, so we escape it... */
			tmp = g_markup_escape_text(esc_time_str,-1);
			g_free(esc_time_str);
			esc_time_str = tmp;
		} else {
			esc_time_str = g_strdup("");
		}

		/* Label and state icon */
		if(!item->title || !strlen(item->title)) {
			esc_title = g_strdup(_("<i>*** No title ***</i>"));
		} else {
			/* we escape here to use Pango markup (the parsing ensures that
			   titles never contain escaped HTML) */
			esc_title = g_markup_escape_text(item->title, -1);
			esc_title = g_strstrip(esc_title);
		}

		direction_marker = common_get_direction_mark(item->itemSet->node->title);

		if(FALSE == item->readStatus) {
			time_str = g_strdup_printf("<span weight=\"bold\">%s</span>", esc_time_str);
			label = g_strdup_printf("%s<span weight=\"bold\">%s</span>", direction_marker, esc_title);
			icon = icons[ICON_UNREAD];
		} else if(TRUE == item->updateStatus) {
			time_str = g_strdup_printf("<span weight=\"bold\" color=\"#333\">%s</span>", esc_time_str);
			label = g_strdup_printf("%s<span weight=\"bold\" color=\"#333\">%s</span>", direction_marker, esc_title);
			icon = icons[ICON_UPDATED];
		} else {
			time_str = g_strdup(esc_time_str);
			label = g_strdup_printf("%s%s", direction_marker, esc_title);
			icon = icons[ICON_READ];
		}
		g_free(esc_title);
		g_free(esc_time_str);

		if(item->flagStatus) 
			icon = icons[ICON_FLAG];

		gtk_tree_store_set(ui_itemlist_get_tree_store(), &iter,
		                   IS_LABEL, label,
				   IS_TIME_STR, time_str,
				   IS_STATEICON, icon,
				   -1);
	
		g_free(time_str);
		g_free(label);
	}
}

static gboolean ui_itemlist_key_press_cb(GtkWidget *widget, GdkEventKey *event, gpointer data) {

	if((event->type == GDK_KEY_PRESS) &&
	   (event->state == 0) &&
	   (event->keyval == GDK_Delete)) {
		on_remove_item_activate(NULL, NULL);
	}
	return FALSE;
}

GtkWidget* ui_itemlist_new() {
	GtkCellRenderer		*renderer;
	GtkTreeViewColumn 	*column;
	GtkTreeSelection	*select;
	GtkTreeStore		*itemstore;	
	GtkWidget 		*itemlist;
	GtkWidget 		*ilscrolledwindow;
	
	ilscrolledwindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(ilscrolledwindow);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (ilscrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW (ilscrolledwindow), GTK_SHADOW_IN);

	itemlist_treeview = itemlist = gtk_tree_view_new();
	gtk_container_add(GTK_CONTAINER(ilscrolledwindow), itemlist);
	gtk_widget_show(itemlist);
	gtk_widget_set_name(itemlist, "itemlist");
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(itemlist), TRUE);
	
	g_object_set_data(G_OBJECT(mainwindow), "itemlist", itemlist);

	item_to_iter = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);

	itemstore = ui_itemlist_get_tree_store();

	gtk_tree_view_set_model(GTK_TREE_VIEW(itemlist), GTK_TREE_MODEL(itemstore));

	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes("", renderer, "pixbuf", IS_STATEICON, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(itemlist), column);
	gtk_tree_view_column_set_sort_column_id(column, IS_STATE);
	
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes("", renderer, "pixbuf", IS_ENCICON, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(itemlist), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Date"), renderer, "markup", IS_TIME_STR, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(itemlist), column);
	gtk_tree_view_column_set_sort_column_id(column, IS_TIME);
	g_object_set(column, "resizable", TRUE, NULL);
	
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes("", renderer, "pixbuf", IS_FAVICON, NULL);
	gtk_tree_view_column_set_sort_column_id(column, IS_SOURCE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(itemlist), column);
	
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Headline"));

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Headline"), renderer, "markup", IS_LABEL, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(itemlist), column);
	gtk_tree_view_column_set_sort_column_id(column, IS_LABEL);
	g_object_set(column, "resizable", TRUE, NULL);
#if GTK_CHECK_VERSION(2,6,0)
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
#endif

	/* And connect signals */	
	g_signal_connect(G_OBJECT(GTK_TREE_VIEW(itemlist)), "key-press-event", G_CALLBACK(ui_itemlist_key_press_cb), NULL);
	
	/* Setup the selection handler */
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(itemlist));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(select), "changed",
	                 G_CALLBACK(on_itemlist_selection_changed), NULL);
	g_signal_connect((gpointer)itemlist, "button_press_event",
	                 G_CALLBACK(on_itemlist_button_press_event), NULL);
	g_signal_connect((gpointer)itemlist, "row_activated",
	                 G_CALLBACK(on_Itemlist_row_activated), NULL);
		  
	return ilscrolledwindow;
}

/* typically called when filling the item tree view */
void ui_itemlist_prefocus(void) {
	GtkWidget		*itemlist, *focus_widget;
	GtkTreeSelection	*itemselection;
	
	/* the following is important to prevent setting the unread
	   flag for the first item in the item list when the user does
	   the first click into the treeview, if we don't do a focus and
	   unselect, GTK would always (exception: clicking on first item)
	   generate two selection-change events (one for the clicked and
	   one for the selected item)!!! */

	itemlist = itemlist_treeview;
	
	/* we need to restore the focus after we temporarily select the itemlist */
	focus_widget = gtk_window_get_focus(GTK_WINDOW(mainwindow));

	/* prevent marking as unread before focussing, which leads to a selection */
	gtk_widget_grab_focus(itemlist);

	if(NULL != (itemselection = gtk_tree_view_get_selection(GTK_TREE_VIEW(itemlist))))
		gtk_tree_selection_unselect_all(itemselection);
	
	if(focus_widget)
		gtk_widget_grab_focus(focus_widget);		
}

void ui_itemlist_add_item(itemPtr item) {
	GtkTreeStore	*itemstore = ui_itemlist_get_tree_store();
	GtkTreeIter	old_iter;
	gboolean	exists;

	exists = ui_item_to_iter(item, &old_iter);
	
	if(exists && !item->newStatus) {
		/* nothing to do */
	} else {
		GtkTreeIter *iter = &old_iter;
		gint state = 0;
		
		if(!exists) {
			iter = g_new0(GtkTreeIter, 1);
			gtk_tree_store_prepend(itemstore, iter, NULL);
			g_hash_table_insert(item_to_iter, (gpointer)item, (gpointer)iter);
		}
		
		if(item->flagStatus)
			state += 2;
		if(!item->readStatus)
			state += 1;

		gtk_tree_store_set(itemstore, iter,
		                	      IS_TIME, item->time,
		                	      IS_NR, item->sourceNr,
					      IS_PARENT, item->itemSet->node,
		                              IS_FAVICON, item->sourceNode->icon,
		                              IS_ENCICON, item->hasEnclosure?icons[ICON_ENCLOSURE]:NULL,
					      IS_ENCLOSURE, item->hasEnclosure,
					      IS_SOURCE, item->sourceNode,
					      IS_STATE, state,
		                	      -1);
		ui_itemlist_update_item(item);
	}
}

void ui_itemlist_enable_favicon_column(gboolean enabled) {

	/* we depend on the fact that the second column is the favicon column!!! 
	   if we are in search mode or have a folder or vfolder we show the favicon 
	   column to give a hint where the item comes from ... */
	gtk_tree_view_column_set_visible(gtk_tree_view_get_column(GTK_TREE_VIEW(itemlist_treeview), 3), enabled);
}

void ui_itemlist_enable_encicon_column(gboolean enabled) {

	/* we depend on the fact that the third column is the enclosure icon column!!! */
	gtk_tree_view_column_set_visible(gtk_tree_view_get_column(GTK_TREE_VIEW(itemlist_treeview), 1), enabled);
}

void on_popup_launchitem_selected(void) {
	itemPtr		item;

	if(NULL != (item = itemlist_get_selected()))
		ui_htmlview_launch_URL(ui_tabs_get_active_htmlview(), 
		                       (gchar *)item_get_source(item), 
				       UI_HTMLVIEW_LAUNCH_DEFAULT);
	else
		ui_mainwindow_set_status_bar(_("No item has been selected"));
}

void on_popup_launchitem_in_tab_selected(void) {
	itemPtr		item;
	const gchar	*link;

	if(NULL != (item = itemlist_get_selected())) {
		if(NULL != (link = item_get_source(item)))
			ui_tabs_new(link, link, FALSE);
		else
			ui_show_error_box(_("This item has no link specified!"));
	} else {
		ui_mainwindow_set_status_bar(_("No item has been selected"));
	}
}

void on_Itemlist_row_activated(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {

	on_popup_launchitem_selected();
}

/* menu callbacks */					
void on_toggle_item_flag(GtkMenuItem *menuitem, gpointer user_data) {
	itemPtr		item;
	
	if(NULL != (item = itemlist_get_selected()))
		itemlist_toggle_flag(item);
}

void on_popup_toggle_flag(gpointer callback_data, guint callback_action, GtkWidget *widget) { on_toggle_item_flag(NULL, NULL); }

void on_toggle_unread_status(GtkMenuItem *menuitem, gpointer user_data) {
	itemPtr		item;

	if(NULL != (item = itemlist_get_selected())) 
		itemlist_toggle_read_status(item);
}

void on_popup_toggle_read(gpointer callback_data, guint callback_action, GtkWidget *widget) { on_toggle_unread_status(NULL, NULL); }

void on_remove_items_activate(GtkMenuItem *menuitem, gpointer user_data) {
	nodePtr		node;
	
	node = feedlist_get_selected();
	if(node && ((NODE_TYPE_FEED == node->type) || (NODE_TYPE_NEWSBIN == node->type)))
		itemlist_remove_all_items(node->itemSet);
	else
		ui_show_error_box(_("You must select a feed to delete its items!"));
}

void on_remove_item_activate(GtkMenuItem *menuitem, gpointer user_data) {
	itemPtr		item;
	
	if(NULL != (item = itemlist_get_selected())) {
		on_treeview_move("itemlist", 1);
		itemlist_remove_item(item);
	} else {
		ui_mainwindow_set_status_bar(_("No item has been selected"));
	}
}

void on_popup_remove_selected(gpointer callback_data, guint callback_action, GtkWidget *widget) { on_remove_item_activate(NULL, NULL); }

void ui_itemlist_select(itemPtr item) {

	if(item) {
		GtkTreeStore		*itemstore = ui_itemlist_get_tree_store();
		GtkWidget		*treeview;
		GtkTreeSelection	*selection;
		GtkTreeIter		iter;
		GtkTreePath		*path;
		
		g_return_if_fail(ui_item_to_iter(item, &iter));

		treeview = itemlist_treeview;
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));

		path = gtk_tree_model_get_path(GTK_TREE_MODEL(itemstore), &iter);
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview), path, NULL, FALSE, 0.0, 0.0);
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(treeview), path, NULL, FALSE);
		gtk_tree_path_free(path);
	} else {
		gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(GTK_TREE_VIEW(itemlist_treeview)));
	}
}

static itemPtr ui_itemlist_find_unread_item_from_iter(GtkTreeIter *iter) {
	itemPtr	item;
	
	item = ui_iter_to_item(iter);
	if(item) {
		if(!item->readStatus) {
			if(2 != itemlist_get_view_mode()) {
				ui_itemlist_select(item);
				itemlist_set_read_status(item, TRUE);	/* needed when no selection happens (e.g. when the item is already selected) */
			} else {
				itemlist_mark_all_read(item->itemSet);
			}
			return item;
		}
	}
	return NULL;
}

itemPtr ui_itemlist_find_unread_item(itemPtr start) {
	GtkTreeStore		*itemstore;
	GtkTreeIter		iter;
	gboolean		valid = TRUE;

	itemstore = ui_itemlist_get_tree_store();
	
	if(start)
		valid = ui_item_to_iter(start, &iter);
	else
		valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(itemstore), &iter);
	
	while(valid) {
		itemPtr	item;
		item = ui_itemlist_find_unread_item_from_iter(&iter);
		if(item)
			return item;
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(itemstore), &iter);
	}

	return NULL;
}

void on_next_unread_item_activate(GtkMenuItem *menuitem, gpointer user_data) {

	itemlist_select_next_unread();
}

void on_popup_next_unread_item_selected(gpointer callback_data, guint callback_action, GtkWidget *widget) { 

	itemlist_select_next_unread();
}

void on_nextbtn_clicked(GtkButton *button, gpointer user_data) {

	itemlist_select_next_unread();
}

gboolean on_itemlist_button_press_event(GtkWidget *treeview, GdkEventButton *event, gpointer user_data) {
	GtkTreeViewColumn	*column;
	GtkTreePath		*path;
	GtkTreeIter		iter;
	itemPtr			item = NULL;
	
	if(event->type != GDK_BUTTON_PRESS)
		return FALSE;

	/* avoid handling header clicks */
	if(event->window != gtk_tree_view_get_bin_window(GTK_TREE_VIEW(treeview)))
		return FALSE;

	if(!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview), (gint)event->x, (gint)event->y, &path, NULL, NULL, NULL))
		return FALSE;

	if(gtk_tree_model_get_iter(GTK_TREE_MODEL(ui_itemlist_get_tree_store()), &iter, path))
		item = ui_iter_to_item(&iter);	
		
	gtk_tree_path_free(path);
	
	if(item) {
		GdkEventButton *eb = (GdkEventButton*)event; 
		switch(eb->button) {
			case 1:
				column = gtk_tree_view_get_column(GTK_TREE_VIEW(treeview), 0);
				if(!column)
					return FALSE;
					
				/* Allow flag toggling when left clicking in the flagging column.
				   We depend on the fact that the state column is the first!!! */
				if(event->x <= column->width) {
					itemlist_toggle_flag(item);
					return TRUE;
				}
				return FALSE;
				break;
			case 2:
				/* depending on the column middle mouse click toggles flag or read status
				   We depent on the fact that the state column is the first!!! 
				   code inspired by the GTK+ 2.0 Tree View Tutorial at:
				   http://scentric.net/tutorial/sec-misc-get-renderer-from-click.html */
				if(NULL != (column = gtk_tree_view_get_column(GTK_TREE_VIEW(treeview), 0))) {
					g_assert(NULL != column);
					if(event->x <= column->width)
						itemlist_toggle_flag(item);
					else
						itemlist_toggle_read_status(item);
				}
				return TRUE;
				break;
			case 3:
				ui_itemlist_select(item);
				gtk_menu_popup(make_item_menu(item), NULL, NULL, NULL, NULL, eb->button, eb->time);
				return TRUE;
				break;
		}
	}
	return FALSE;
}

void on_popup_copy_URL_clipboard(void) {
	itemPtr         item;

	item = itemlist_get_selected();
	if(item) {
		gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY), item_get_source(item), -1);
		gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), item_get_source(item), -1);
	} else {
		ui_mainwindow_set_status_bar(_("No item has been selected"));
	}
}

void ui_itemlist_add_item_bookmark(itemPtr item) {

	gchar *url = social_get_url(item_get_source(item), item_get_title(item));
	ui_htmlview_launch_URL(ui_mainwindow_get_active_htmlview(), url, 1);
	g_free(url);
}

void on_popup_social_bm_item_selected(void) {
	itemPtr	item;
	
	item = itemlist_get_selected();
	if(item)
		ui_itemlist_add_item_bookmark(item);
	else
		ui_mainwindow_set_status_bar(_("No item has been selected"));
}

void on_popup_social_bm_link_selected(gpointer selectedUrl, guint callback_action, GtkWidget *widget) {
	
	if(selectedUrl) {
		gchar *url = social_get_url(selectedUrl, "");
		ui_htmlview_launch_URL(ui_mainwindow_get_active_htmlview(), url, 1);
		g_free(url);
	} else {
		ui_mainwindow_set_status_bar(_("No link selected!"));
	}
}

void on_itemlist_selection_changed(GtkTreeSelection *selection, gpointer data) {
	GtkTreeIter 	iter;
	GtkTreeModel	*model;
	itemPtr		item = NULL;

	if(gtk_tree_selection_get_selected(selection, &model, &iter))
		item = ui_iter_to_item(&iter);

	if(item)
		itemlist_selection_changed(item);
}

void itemlist_sort_column_changed_cb(GtkTreeSortable *treesortable, gpointer user_data) {
	gint		sortColumn;
	GtkSortType	sortType;
	gboolean	sorted;
	
	if(feedlist_get_selected() == NULL || disableSortingSaving != 0)
		return;
	
	sorted = gtk_tree_sortable_get_sort_column_id(treesortable, &sortColumn, &sortType);
	node_set_sort_column(feedlist_get_selected(), sortColumn, sortType == GTK_SORT_DESCENDING);
}

/* needed because switching does sometimes returns to the tree 
   view with a very disturbing horizontal scrolling state */
void ui_itemlist_scroll_left() {
	GtkTreeViewColumn 	*column;
	GtkTreePath		*path;

	if(2 != itemlist_get_view_mode()) {
		gtk_tree_view_get_cursor(GTK_TREE_VIEW(itemlist_treeview), &path, &column);
		if(path) {
			column = gtk_tree_view_get_column(GTK_TREE_VIEW(itemlist_treeview), 1);
			gtk_tree_view_set_cursor(GTK_TREE_VIEW(itemlist_treeview), path, column, FALSE);
			gtk_tree_path_free(path);
		}
	}
}
