/**
 * @file ui_itemlist.c Item list/view handling
 *  
 * Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include <langinfo.h>
#include <string.h>
#include "support.h"
#include "callbacks.h"
#include "common.h"
#include "debug.h"
#include "feed.h"
#include "item.h"
#include "conf.h"
#include "ui_htmlview.h"
#include "ui_itemlist.h"
#include "ui_mainwindow.h"
#include "ui_tray.h"

typedef struct ui_item_data {
	GtkTreeIter row;
} ui_item_data;

extern GdkPixbuf	*icons[];

static nodePtr		displayed_node = NULL;
static itemPtr		displayed_item = NULL;

static GHashTable 	*iterhash = NULL;	/* hash table used for fast item->tree iter lookup */

static gint		itemlist_loading;	/* freaky workaround for item list focussing problem */
static gint		disableSortingSaving;

#define	TIMESTRLEN	256

static gchar 		*date_format = NULL;	/* date formatting string */

/* mouse/keyboard interaction callbacks */
static void on_itemlist_selection_changed(GtkTreeSelection *selection, gpointer data);
static itemPtr ui_itemlist_get_selected();

/* sort function for the item list date column */
static gint timeCompFunc(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data) {
	time_t	timea, timeb;
	
	g_assert(model != NULL);
	g_assert(a != NULL);
	g_assert(b != NULL);
	gtk_tree_model_get(model, a, IS_TIME, &timea, -1);
	gtk_tree_model_get(model, b, IS_TIME, &timeb, -1);
	
	return timeb-timea;
}

void ui_itemlist_sort_column_changed_cb(GtkTreeSortable *treesortable, gpointer user_data) {
	gint		sortColumn;
	GtkSortType	sortType;
	gboolean	sorted;
	nodePtr		np;
	
	np = ui_feedlist_get_selected();	
	if(np == NULL || disableSortingSaving != 0)
		return;
	
	sorted = gtk_tree_sortable_get_sort_column_id(treesortable, &sortColumn, &sortType);
	if((FST_FEED == np->type) || (FST_VFOLDER == np->type))
		feed_set_sort_column((feedPtr)np, sortColumn, sortType == GTK_SORT_DESCENDING);
}

GtkTreeStore * ui_itemlist_get_tree_store(void) {
	GtkTreeModel	*model;
	GtkTreeStore	*itemstore;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(lookup_widget(mainwindow, "Itemlist")));
	if(NULL == model) {
		/* set up a store of these attributes: 
			- item title
			- item label
			- item state (read/unread)		
			- pointer to item data
			- date time_t value
			- the type of the feed the item belongs to
			- feed icon
		 */
		itemstore = gtk_tree_store_new(IS_LEN,
		                               G_TYPE_INT,
		                               G_TYPE_STRING,
		                               G_TYPE_STRING, 
		                               G_TYPE_STRING,
		                               GDK_TYPE_PIXBUF,
		                               G_TYPE_POINTER, 
		                               G_TYPE_INT,
		                               GDK_TYPE_PIXBUF);
		gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(itemstore), IS_TIME, timeCompFunc, NULL, NULL);
		g_signal_connect(G_OBJECT(itemstore), "sort-column-changed", G_CALLBACK(ui_itemlist_sort_column_changed_cb), NULL);
	} else {
		itemstore = GTK_TREE_STORE(model);
	}
	
	return itemstore;
}

gchar * ui_itemlist_format_date(time_t t) {
	gchar		*tmp;
	gchar		*timestr;

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
	
	if(NULL == date_format) {	
		switch(getNumericConfValue(TIME_FORMAT_MODE)) {
			case 1:
				date_format = g_strdup_printf("%s", nl_langinfo(T_FMT));
				break;
			case 3:
				tmp = getStringConfValue(TIME_FORMAT);
				date_format = g_locale_from_utf8(tmp, -1, NULL, NULL, NULL);
				g_free(tmp);
				break;
			case 2:
			default:
				date_format = g_strdup_printf("%s %s", nl_langinfo(D_FMT), nl_langinfo(T_FMT));
				break;
		}
	}
	
	tmp = g_new0(gchar, TIMESTRLEN+1);
	strftime(tmp, TIMESTRLEN, date_format, localtime(&t));
	timestr = g_locale_to_utf8(tmp, -1, NULL, NULL, NULL);
	g_free(tmp);
	
	return timestr;
}

void ui_itemlist_reset_date_format(void) {

	g_free(date_format);
	date_format = NULL;
}

static void ui_itemlist_remove_item(itemPtr ip) {
	GtkTreeStore *itemstore = ui_itemlist_get_tree_store();

	gtk_tree_store_remove(itemstore, &(((ui_item_data *)ip->ui_data)->row));
	g_free(ip->ui_data);
	ip->ui_data = NULL;
}

static gboolean ui_itemlist_remove_foreach(gpointer key, gpointer value, gpointer user_data) {
	
	ui_itemlist_remove_item((itemPtr)key);
	return TRUE;
}

/* cleans up the item list, sets up the iter hash when called for the first time */
void ui_itemlist_clear(void) {
	GtkTreeSelection	*itemselection;

	if(NULL == iterhash) {
		iterhash = g_hash_table_new(g_direct_hash, g_direct_equal);
	} else {
		/* unselecting all items is important for to remove items
		   from vfolders whose removal is deferred until unselecting */
		if(NULL != (itemselection = gtk_tree_view_get_selection(GTK_TREE_VIEW(lookup_widget(mainwindow, "Itemlist")))))
			gtk_tree_selection_unselect_all(itemselection);
			
		g_hash_table_foreach_remove(iterhash, ui_itemlist_remove_foreach, NULL);
	}		
}

static void ui_item_update_from_iter(GtkTreeIter *iter) {
	GtkTreeStore	*itemstore = ui_itemlist_get_tree_store();
	gpointer	ip;
	gchar		*title, *label, *time_str, *esc_title, *esc_time_str, *tmp;
	gint		time;
	GdkPixbuf	*icon = NULL, *favicon = NULL;

	gtk_tree_model_get(GTK_TREE_MODEL(itemstore), iter,
				    IS_PTR, &ip,
				    IS_TITLE, &title,
				    IS_TIME, &time,
				    -1);

	/* favicon for vfolders */
	if(NULL != ((itemPtr)ip)->sourceFeed)
		favicon = ((itemPtr)ip)->sourceFeed->icon;

	/* Time */
	if(0 != time) {
		esc_time_str = ui_itemlist_format_date((time_t)time);
		/* the time value is no markup, so we escape it... */
		tmp = g_markup_escape_text(esc_time_str,-1);
		g_free(esc_time_str);
		esc_time_str = tmp;
	} else {
		esc_time_str = g_strdup("");
	}
	
	/* Label and state icon */
	if(title == NULL) 
		title = g_strdup(_("[No title]"));
		
	/* Here we have the following problem: a title string might contain 
	   either escaped markup (which we should not escape again) or 
	   non-markup text (which might contain ampersands, which must be
	   escaped). We assume no mixed case! */
	/*if(is_escaped_markup(title))
	  esc_title = unescape_markup(title);	/ * FIXME: unescaped!!! * /
		else */
		esc_title = g_markup_escape_text(title, -1);

		esc_title = filter_title(esc_title);

	if(FALSE == item_get_read_status(ip)) {
		time_str = g_strdup_printf("<span weight=\"bold\">%s</span>", esc_time_str);
		label = g_strdup_printf("<span weight=\"bold\">%s</span>", esc_title);
		icon = icons[ICON_UNREAD];
	} else if(TRUE == item_get_update_status(ip)) {
		time_str = g_strdup_printf("<span weight=\"bold\" color=\"#333\">%s</span>", esc_time_str);
		label = g_strdup_printf("<span weight=\"bold\" color=\"#333\">%s</span>", esc_title);
		icon = icons[ICON_UPDATED];
	} else {
		time_str = g_strdup(esc_time_str);
		label = g_strdup(esc_title);
		icon = icons[ICON_READ];
	}
	g_free(esc_title);
	g_free(esc_time_str);
	
	if(TRUE == item_get_mark(ip)) 
		icon = icons[ICON_FLAG];

	/* Finish 'em... */
	gtk_tree_store_set(ui_itemlist_get_tree_store(), iter,
				    IS_LABEL, label,
				    IS_TIME_STR, time_str,
				    IS_ICON, icon,
				    IS_ICON2, favicon,
				    -1);
	g_free(time_str);
	g_free(title);
	g_free(label);
}

void ui_item_update(itemPtr ip) {
	g_assert(NULL != ip);
	if (ip->ui_data)
		ui_item_update_from_iter(&((ui_item_data*)ip->ui_data)->row);
}

void ui_itemlist_update() {
	GtkTreeStore	*itemstore = ui_itemlist_get_tree_store();
	GtkTreeIter	iter;
	
	if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(itemstore), &iter)) {
		do {
			ui_item_update_from_iter(&iter);
		} while(gtk_tree_model_iter_next(GTK_TREE_MODEL(itemstore), &iter));
	}
         
}

void ui_itemlist_update_vfolder(nodePtr vp) {

	if(displayed_node == vp)
		ui_itemlist_load(vp);
}

void ui_itemlist_init(GtkWidget *itemlist) {
	GtkCellRenderer		*renderer;
	GtkTreeViewColumn 	*column;
	GtkTreeSelection	*select;
	GtkTreeStore		*itemstore;	
	
	g_assert(mainwindow != NULL);
	g_assert(itemlist != NULL);

	itemstore = ui_itemlist_get_tree_store();

	gtk_tree_view_set_model(GTK_TREE_VIEW(itemlist), GTK_TREE_MODEL(itemstore));

	/* we only render the state, title and time */
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes("", renderer, "pixbuf", IS_ICON, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(itemlist), column);
	/*gtk_tree_view_column_set_sort_column_id(column, IS_STATE); ...leads to segfaults on tab-bing through 
	 Also might be a bad idea because when an item is clicked, it will immediatly change the sorting order */

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Date"), renderer, "markup", IS_TIME_STR, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(itemlist), column);
	gtk_tree_view_column_set_sort_column_id(column, IS_TIME);
	g_object_set(column, "resizable", TRUE, NULL);
	
	
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes("", renderer, "pixbuf", IS_ICON2, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(itemlist), column);
	
	renderer = gtk_cell_renderer_text_new();	
	column = gtk_tree_view_column_new_with_attributes(_("Headline"), renderer, "markup", IS_LABEL, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(itemlist), column);
	gtk_tree_view_column_set_sort_column_id(column, IS_TITLE);

	g_object_set(column, "resizable", TRUE, NULL);
	
	/* Setup the selection handler */
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(itemlist));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(select), "changed",
                  G_CALLBACK(on_itemlist_selection_changed),
                  NULL);
		  
	ui_itemlist_reset_date_format();
}

/* typically called when filling the item tree view */
void ui_itemlist_prefocus(void) {
	GtkWidget		*itemlist, *focus_widget;
	GtkTreeSelection	*itemselection;
	GtkAdjustment		*adj;
	
	/* the following is important to prevent setting the unread
	   flag for the first item in the item list when the user does
	   the first click into the treeview, if we don't do a focus and
	   unselect, GTK would always (exception: clicking on first item)
	   generate two selection-change events (one for the clicked and
	   one for the selected item)!!! */

	itemlist = lookup_widget(mainwindow, "Itemlist");
	
	/* we need to restore the focus after we temporarily select the itemlist */
	focus_widget = gtk_window_get_focus(GTK_WINDOW(mainwindow));

	/* prevent marking as unread before focussing, which leads to a selection */
	itemlist_loading = 1;
	gtk_widget_grab_focus(itemlist);

	if(NULL != (itemselection = gtk_tree_view_get_selection(GTK_TREE_VIEW(itemlist))))
		gtk_tree_selection_unselect_all(itemselection);

	gtk_widget_grab_focus(focus_widget);		
	itemlist_loading = 0;
	
	/* Finally reset scrolling to the first item. Note: this functionality
	   means that the current selection and the itemlist positioning is 
	   lost when a feed is updated which items the user is currently reading.
	   But one can say this is good because its a rare event and shows the
	   user the most recent items at the top, while keeping the originally
	   selected item in the html view. (Lars) */
	adj = gtk_tree_view_get_vadjustment(GTK_TREE_VIEW(itemlist));
	gtk_adjustment_set_value(adj, 0.0);
	gtk_tree_view_set_vadjustment(GTK_TREE_VIEW(itemlist), adj);
}

/* Function which is called when the contents of currently
   selected object (feed or item) is updated or if the 
   selection has changed and initial display is requested. 
   
   What the function prints to the HTML view depends on the
   focussed widget. If feedlist is selected feed info is printed.
   If itemlist is selected the selected items content is shown. 
   If anything other is focussed nothing is printed to avoid
   disturbing the user. */
void ui_itemlist_display(void) {
	GtkTreeStore	*itemstore = ui_itemlist_get_tree_store();
	GtkTreeIter	iter;
	nodePtr		np;
	itemPtr		ip;
	gchar		*buffer = NULL;
	gboolean	valid;
	gchar		*tmp = NULL;

	g_assert(NULL != mainwindow);
	
	if(TRUE == ui_itemlist_get_two_pane_mode()) {
		/* two pane mode */
		ui_htmlview_start_output(&buffer, FALSE);
		valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(itemstore), &iter);
		while(valid) {	
			gtk_tree_model_get(GTK_TREE_MODEL(itemstore), &iter, IS_PTR, &ip, -1);

			if(item_get_read_status(ip)) 
				addToHTMLBuffer(&buffer, UNSHADED_START);
			else
				addToHTMLBuffer(&buffer, SHADED_START);

			addToHTMLBuffer(&buffer, item_render(ip));

			if(item_get_read_status(ip))
				addToHTMLBuffer(&buffer, UNSHADED_END);
			else {
				addToHTMLBuffer(&buffer, SHADED_END);
			}

			valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(itemstore), &iter);
		}
		ui_htmlview_finish_output(&buffer);
	} else {
		/* we only update anything if the feedlist is focussed */
		if(lookup_widget(mainwindow, "feedlist") == gtk_window_get_focus(GTK_WINDOW(mainwindow))) {
			/* three pane mode */

			/* display feed info */
			if(np = ui_feedlist_get_selected()) {
				ui_htmlview_start_output(&buffer, TRUE);
				if((FST_FEED == np->type) || 
				   (FST_VFOLDER == np->type)) {
					tmp = feed_render((feedPtr)np);
					addToHTMLBufferFast(&buffer, tmp);
					g_free(tmp);
				}
				ui_htmlview_finish_output(&buffer);
			}

			/* we never overwrite the last selected items contents
			   item reselections trigger new content printing via
			   item selection changed callback */

			/* no scrolling reset, because this code should only be
			   triggered for redraw purposes! */
		}
	}
		
	if(buffer) {
		if(np != NULL &&
		   (FST_FEED == np->type) &&
		   ((feedPtr)np)->source != NULL &&
		   ((feedPtr)np)->source[0] != '|' &&
		   strstr(((feedPtr)np)->source, "://") != NULL)
			ui_htmlview_write(ui_mainwindow_get_active_htmlview(), buffer, ((feedPtr)np)->source);
		else
			ui_htmlview_write(ui_mainwindow_get_active_htmlview(), buffer, NULL);
		g_free(buffer);
	}

	ui_feedlist_update();
}

static gboolean ui_itemlist_check_for_stale_item(gpointer key, gpointer value, gpointer user_data) {
	itemPtr		ip = (itemPtr)key;
	GSList		*itemlist = (GSList *)user_data;
	
	if(NULL == g_slist_find(itemlist, ip)) {
		g_print("removing stale item %d\n", ip);
		ui_itemlist_remove_item(ip);
		return TRUE;
	}
	return FALSE;
}

static void ui_itemlist_load_feed(feedPtr fp, gpointer data) {
	gboolean	merge = *(gboolean *)data;
	GtkTreeStore	*itemstore = ui_itemlist_get_tree_store();
	GtkTreeIter	*iter;
	GSList		*item, *itemlist;
	itemPtr		ip;
	
	/* we depend on the fact that the third column is the favicon column!!! 
	   if we are in search mode (or have a vfolder) we show the favicon 
	   column to give a hint where the item comes from ... */
	gtk_tree_view_column_set_visible(gtk_tree_view_get_column(GTK_TREE_VIEW(lookup_widget(mainwindow, "Itemlist")), 2), (FST_VFOLDER == feed_get_type(fp)));
	
	/* step 1, reverse item list so we can prepend any new items in the correct order */
	feed_load(fp);
	itemlist = feed_get_item_list(fp);
	itemlist = g_slist_copy(itemlist);
	itemlist = g_slist_reverse(itemlist);
		
	/* step 2, remove all tree store entries that do not exist in the feed list anymore! */
	if(TRUE == merge) 
		g_hash_table_foreach_remove(iterhash, ui_itemlist_check_for_stale_item, (gpointer)itemlist);
	
	/* step 3, add all tree store entries that do not yet exist in the feed list */	
	item = itemlist;
	while(NULL != item) {
		ip = item->data;
		g_assert(NULL != ip);
		iter = NULL;

		if((TRUE == merge) && (NULL != (iter = (GtkTreeIter *)g_hash_table_lookup(iterhash, (gpointer)ip)))) {
			/* g_print("found iter for item %d\n", ip); */
		}

		if((NULL != iter) && (FALSE == item_get_new_status(ip))) {
			/* nothing to do */
			/* g_print("nothing to do for iter %d\n", ip); */
		} else {	
			if(NULL == iter) {
				if(ip->ui_data != NULL) {
					//g_warning("fatal: item added twice.\n"); this triggers!!!!!! FIXME!!!!!!
					g_free(iter);
					item = g_slist_next(item);
					continue;
				}

				iter = g_new0(GtkTreeIter, 1);
				gtk_tree_store_prepend(itemstore, iter, NULL);
				g_hash_table_insert(iterhash, (gpointer)ip, (gpointer)iter);
				g_assert(ip->ui_data == NULL);
				ip->ui_data = g_new0(ui_item_data, 1);
				((ui_item_data*)(ip->ui_data))->row = *iter;
			}	

			gtk_tree_store_set(itemstore, iter,
		                        	      IS_TITLE, item_get_title(ip),
		                        	      IS_PTR, ip,
		                        	      IS_TIME, item_get_time(ip),
		                        	      -1);
			ui_item_update_from_iter(iter);
		}
		item = g_slist_next(item);
	}
	g_slist_free(itemlist);	
}

/* Loads or merges the passed feeds items into the itemlist. 
   If the selected feed is equal to the passed one we do 
   merging. Otherwise we can just clear the list and load
   the new items. */
void ui_itemlist_load(nodePtr node) {
	GtkTreeModel	*model;
	gint		sortColumn;
	GtkSortType	sortType;
	gboolean	isFeed;
	gboolean	merge = FALSE;
	
	merge = (node == displayed_node);
	displayed_node = node;

	isFeed = ((node != NULL) && ((FST_FEED == node->type) || (FST_VFOLDER == node->type)));

	model = GTK_TREE_MODEL(ui_itemlist_get_tree_store());
	gtk_tree_sortable_get_sort_column_id(GTK_TREE_SORTABLE(model), &sortColumn, &sortType);
	
	if(FALSE == merge) {
		ui_itemlist_clear();
		/* explicitly no ui_htmlview_clear() !!! */
	
		if(isFeed) {
			disableSortingSaving++;
			gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model), ((feedPtr)node)->sortColumn, ((feedPtr)node)->sortReversed);
			disableSortingSaving--;
		}
		
		if(!getBooleanConfValue(KEEP_FEEDS_IN_MEMORY)) {
			debug0(DEBUG_CACHE, "unloading everything...");
			ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED | ACTION_FILTER_DIRECTORY, feed_unload);
		}
	}	

	/* Add the new items */
	if(TRUE == isFeed) {
		ui_itemlist_load_feed((feedPtr)node, (gpointer)&merge);
	} else if(FST_FOLDER == node->type) {
		ui_feedlist_do_for_all_data(node, ACTION_FILTER_FEED | ACTION_FILTER_DIRECTORY, ui_itemlist_load_feed, (gpointer)&merge);
	}

	ui_itemlist_display();
	
	if(FALSE == merge)
		ui_itemlist_prefocus();
}

static itemPtr ui_itemlist_get_selected() {
	GtkWidget		*itemlist;
	GtkTreeIter		iter;
	GtkTreeModel		*model;
	itemPtr			item;
	GtkTreeSelection	*selection;

	if(NULL == (itemlist = lookup_widget(mainwindow, "Itemlist"))) {
		g_warning("could not find item list widget!");
		return NULL;
	}
	
	if(NULL == (selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(itemlist)))) {
		g_warning("could not retrieve selection of item list!");
		return NULL;
	}
	
	if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
		gtk_tree_model_get(model, &iter, IS_PTR, &item, -1);
		return item;
	}

	return NULL;
}

/* mouse/keyboard interaction callbacks */
static void on_itemlist_selection_changed(GtkTreeSelection *selection, gpointer data) {
	GtkTreeIter 	iter;
	GtkTreeModel	*model;
	itemPtr 	ip;
	
	if(!itemlist_loading) {
		/* vfolder postprocessing to remove unselected items not
		   more matching the rules because they have changed state */
		if((displayed_item != NULL) && (displayed_item->fp->type == FST_VFOLDER)) {
			if(FALSE == vfolder_check_item(displayed_item)) {
				ui_itemlist_remove_item(displayed_item);
				vfolder_remove_item(displayed_item);
				g_hash_table_remove(iterhash, (gpointer)displayed_item);
			}
		}
	
		if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
			gtk_tree_model_get(model, &iter, IS_PTR, &ip, -1);			
			displayed_item = ip;
			item_display(ip);
			/* set read and unset update status */
			item_set_read(ip);
			item_set_update_status(ip, FALSE);
			ui_feedlist_update();
		} else {
			displayed_item = NULL;
		}
	}
}

void on_popup_toggle_read(gpointer callback_data, guint callback_action, GtkWidget *widget) {

	on_toggle_unread_status(NULL, NULL);
}

void on_popup_toggle_flag(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	itemPtr ip = (itemPtr)callback_data;
	
	item_set_mark(ip, !item_get_mark(ip));		
}

void on_Itemlist_row_activated(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {

	on_popup_launchitem_selected();
}

/* menu callbacks */					
void on_toggle_item_flag(GtkMenuItem *menuitem, gpointer user_data) {
	itemPtr		ip;
	
	if(NULL != (ip = ui_itemlist_get_selected()))
		item_set_mark(ip, !item_get_mark(ip));	
}

void on_popup_launchitem_selected(void) {
	itemPtr		ip;

	if(NULL != (ip = ui_itemlist_get_selected()))
		ui_htmlview_launch_URL((gchar *)item_get_source(ip), TRUE);
	else
		ui_mainwindow_set_status_bar(_("No item has been selected"));
}

void on_toggle_unread_status(GtkMenuItem *menuitem, gpointer user_data) {
	itemPtr		ip;

	if(NULL != (ip = ui_itemlist_get_selected())) {
		if(item_get_read_status(ip)) 
			item_set_unread(ip);
		else
			item_set_read(ip);
		ui_feedlist_update();
	}
}

void on_remove_items_activate(GtkMenuItem *menuitem, gpointer user_data) {
	nodePtr		np;
	
	np = ui_feedlist_get_selected();
	if((NULL != np) && (FST_FEED == np->type)) {
		ui_itemlist_clear();
		feed_remove_items((feedPtr)np);
		ui_feedlist_update();
	} else {
		ui_show_error_box(_("You must select a feed to delete its items!"));
	}
}

void on_remove_item_activate(GtkMenuItem *menuitem, gpointer user_data) {
	GtkTreeStore	*itemstore = ui_itemlist_get_tree_store();
	nodePtr		np;
	itemPtr		ip;
	
	np = ui_feedlist_get_selected();
	if((NULL != np) && (FST_FEED == np->type)) {
		if(NULL != (ip = ui_itemlist_get_selected())) {
			ui_itemlist_remove_item(ip);
			feed_remove_item((feedPtr)np, ip);
			ui_feedlist_update();
		} else {
			ui_mainwindow_set_status_bar(_("No item has been selected"));
		}
	} else {
		ui_show_error_box(_("You must select a feed to delete its items!"));
	}
}

void on_popup_remove_selected(gpointer callback_data, guint callback_action, GtkWidget *widget) { on_remove_item_activate(NULL, NULL); }

static gboolean ui_itemlist_find_unread_item(void) {
	GtkTreeStore		*itemstore = ui_itemlist_get_tree_store();
	GtkTreeSelection	*selection;
	GtkTreePath		*path;
	GtkWidget		*treeview;
	GtkTreeIter		iter;
	gboolean		valid;
	itemPtr			ip;
		
	g_assert(NULL != itemstore);
	valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(itemstore), &iter);
	while(valid) {
		gtk_tree_model_get(GTK_TREE_MODEL(itemstore), &iter, IS_PTR, &ip, -1);
		g_assert(ip != NULL);
		if(FALSE == item_get_read_status(ip)) {
			/* select found item and the item list... */
			treeview = lookup_widget(mainwindow, "Itemlist");
			g_assert(treeview != NULL);
			if(NULL != (selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)))) {
				gtk_tree_selection_select_iter(selection, &iter);
				path = gtk_tree_model_get_path(GTK_TREE_MODEL(itemstore), &iter);
				gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview), path, NULL, FALSE, 0.0, 0.0);
				gtk_tree_view_set_cursor(GTK_TREE_VIEW(treeview), path, NULL, FALSE);
				gtk_tree_path_free(path);
				item_set_read(ip);	/* needed when no selection happens (e.g. when the item is already selected) */
				ui_feedlist_update();
			} else
				g_warning(_("internal error! could not get feed tree view selection!\n"));
			
			return TRUE;
		}
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(itemstore), &iter);
	}

	return FALSE;
}

void on_next_unread_item_activate(GtkMenuItem *menuitem, gpointer user_data) {
	feedPtr			fp;
	
	/* before scanning the feed list, we test if there is a unread 
	   item in the currently selected feed! */
	if(TRUE == ui_itemlist_find_unread_item())
		return;
	
	/* scan feed list and find first feed with unread items */
	if(NULL != (fp = ui_feedlist_find_unread_feed(NULL))) {
		
		/* load found feed */
		ui_feedlist_select((nodePtr)fp);

		/* find first unread item */
		ui_itemlist_find_unread_item();
	} else {
		/* if we don't find a feed with unread items do nothing */
		ui_mainwindow_set_status_bar(_("There are no unread items "));
	}
}

void on_popup_next_unread_item_selected(gpointer callback_data, guint callback_action, GtkWidget *widget) { 

	on_next_unread_item_activate(NULL, NULL); 
}

void on_nextbtn_clicked(GtkButton *button, gpointer user_data) {

	on_next_unread_item_activate(NULL, NULL); 
}

static void ui_itemlist_select(itemPtr ip) {
	GtkTreeStore		*itemstore = ui_itemlist_get_tree_store();
	GtkTreeIter		iter;
	GtkWidget		*treeview;
	GtkTreeSelection	*selection;
	GtkTreePath		*path;

	g_assert(ip != NULL);
	iter = ((ui_item_data*)(ip->ui_data))->row;

	/* some comfort: select the created iter */
	if(NULL != (treeview = lookup_widget(mainwindow, "Itemlist"))) {
		if(NULL != (selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)))) {
			path = gtk_tree_model_get_path(GTK_TREE_MODEL(itemstore), &iter);
			gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview), path, NULL, FALSE, 0.0, 0.0);	
			gtk_tree_path_free(path);
			gtk_tree_selection_select_iter(selection, &iter);
		} else
			g_warning("internal error! could not get feed tree view selection!\n");
	} else {
		g_warning("internal error! could not select newly created treestore iter!");
	}
}

gboolean on_itemlist_button_press_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
	GtkTreeStore		*itemstore = ui_itemlist_get_tree_store();
	GdkEventButton		*eb;
	GtkWidget		*treeview;
	GtkTreeViewColumn	*column;
	GtkTreePath		*path;
	GtkTreeIter		iter;
	itemPtr			ip = NULL;
	
	if(event->type != GDK_BUTTON_PRESS)
		return FALSE;

	treeview = lookup_widget(mainwindow, "Itemlist");
	g_assert(treeview);

	if(!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview), event->x, event->y, &path, NULL, NULL, NULL))
		return FALSE;

	gtk_tree_model_get_iter(GTK_TREE_MODEL(ui_itemlist_get_tree_store()), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(itemstore), &iter,
					    IS_PTR, &ip,
					    -1);
	gtk_tree_path_free(path);
	g_assert(NULL != ip);
	eb = (GdkEventButton*)event; 
	switch(eb->button) {
		case 2:
			/* depending on the column middle mouse click toggles flag or read status
			   We depent on the fact that the state column is the first!!! 
			   code inspired by the GTK+ 2.0 Tree View Tutorial at:
			   http://scentric.net/tutorial/sec-misc-get-renderer-from-click.html */
			if(NULL != (column = gtk_tree_view_get_column(GTK_TREE_VIEW(treeview), 0))) {
				g_assert(NULL != column);
				if(event->x <= column->width)
					item_set_mark(ip, !item_get_mark(ip));				
				else
					if(TRUE == item_get_read_status(ip))
						item_set_unread(ip);
					else
						item_set_read(ip);
				ui_feedlist_update();
			}
			return TRUE;
			break;
		case 3:
			ui_itemlist_select(ip);
			gtk_menu_popup(make_item_menu(ip), NULL, NULL, NULL, NULL, eb->button, eb->time);
			return TRUE;
			break;
	}	
	return FALSE;
}

/*------------------------------------------------------------------------------*/
/* two/three pane mode callback							*/
/*------------------------------------------------------------------------------*/

gboolean ui_itemlist_get_two_pane_mode(void) {

	return gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(lookup_widget(mainwindow, "toggle_condensed_view")));
}

void ui_itemlist_set_two_pane_mode(gboolean new_mode) {
	gboolean	old_mode;
	nodePtr		np;

	old_mode = ui_itemlist_get_two_pane_mode();
	
	if((NULL != (np = ui_feedlist_get_selected())) &&
	   ((FST_FEED == np->type) || (FST_VFOLDER == np->type))) 
		feed_set_two_pane_mode((feedPtr)np, new_mode);

	ui_mainwindow_set_three_pane_mode(!new_mode);

	if(old_mode != new_mode)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(mainwindow, "toggle_condensed_view")), new_mode);
}

void on_toggle_condensed_view_activate(GtkMenuItem *menuitem, gpointer user_data) { 
	nodePtr		np;
	
	ui_itemlist_set_two_pane_mode(GTK_CHECK_MENU_ITEM(menuitem)->active);

	if(NULL != (np = ui_feedlist_get_selected())) {
		/* grab necessary to force HTML widget update (display must
		   change from feed description to list of items and vica 
		   versa */
		gtk_widget_grab_focus(lookup_widget(mainwindow, "feedlist"));
		ui_itemlist_load(np);
	}
}
