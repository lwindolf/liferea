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
#include "itemlist.h"
#include "conf.h"
#include "ui_htmlview.h"
#include "ui_itemlist.h"
#include "ui_mainwindow.h"
#include "ui_tray.h"

extern GdkPixbuf	*icons[];

static GHashTable 	*iterhash = NULL;	/* hash table used for fast item->tree iter lookup */

gint			itemlist_loading;	/* freaky workaround for item list focussing problem */

#define	TIMESTRLEN	256

static gchar 		*date_format = NULL;	/* date formatting string */

/* mouse/keyboard interaction callbacks */
static itemPtr ui_itemlist_get_selected();

/* sort function for the item list date column */
static gint timeCompFunc(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data) {
	time_t	timea, timeb;
	
	g_assert(model != NULL);
	g_assert(a != NULL);
	g_assert(b != NULL);
	gtk_tree_model_get(model, a, IS_TIME, &timea, -1);
	gtk_tree_model_get(model, b, IS_TIME, &timeb, -1);
	
	return (-1)*(timeb-timea);
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
		g_signal_connect(G_OBJECT(itemstore), "sort-column-changed", G_CALLBACK(itemlist_sort_column_changed_cb), NULL);
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

/* don't forget to remove the iterhash entry after calling this method! */
void ui_itemlist_remove_item(itemPtr ip) {
	GtkTreeStore	*itemstore = ui_itemlist_get_tree_store();
	GtkTreeIter	*iter;

	g_assert(NULL != ip);
	iter = g_hash_table_lookup(iterhash, (gpointer)ip);
	g_assert(NULL != iter);
	gtk_tree_store_remove(itemstore, iter);
}

/* cleans up the item list, sets up the iter hash when called for the first time */
void ui_itemlist_clear(void) {
	GtkTreeSelection	*itemselection;
	GtkTreeStore		*itemstore = ui_itemlist_get_tree_store();

	/* unselecting all items is important for to remove items
	   from vfolders whose removal is deferred until unselecting */
	if(NULL != (itemselection = gtk_tree_view_get_selection(GTK_TREE_VIEW(lookup_widget(mainwindow, "Itemlist")))))
		gtk_tree_selection_unselect_all(itemselection);

	if(NULL != iterhash) {		
		g_hash_table_destroy(iterhash);
		gtk_tree_store_clear(itemstore);
	}		
	iterhash = g_hash_table_new(g_direct_hash, g_direct_equal);
}

void ui_itemlist_update_item(itemPtr ip) {
	GtkTreeStore	*itemstore = ui_itemlist_get_tree_store();
	GtkTreeIter	*iter;
	gchar		*title, *label, *time_str, *esc_title, *esc_time_str, *tmp;
	GdkPixbuf	*icon = NULL, *favicon = NULL;

	/* favicon for vfolders */
	if(NULL != ip->sourceFeed)
		favicon = ip->sourceFeed->icon;

	/* Time */
	if(0 != ip->time) {
		esc_time_str = ui_itemlist_format_date((time_t)ip->time);
		/* the time value is no markup, so we escape it... */
		tmp = g_markup_escape_text(esc_time_str,-1);
		g_free(esc_time_str);
		esc_time_str = tmp;
	} else {
		esc_time_str = g_strdup("");
	}
	
	/* Label and state icon */
	title = g_strdup(ip->title);
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
	
	if(TRUE == item_get_flag(ip)) 
		icon = icons[ICON_FLAG];

	/* Finish 'em... */
	iter = g_hash_table_lookup(iterhash, (gpointer)ip);
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

void ui_itemlist_add_item(itemPtr ip, gboolean merge) {
	GtkTreeStore	*itemstore = ui_itemlist_get_tree_store();
	GtkTreeIter	*iter = NULL;

	if((TRUE == merge) && (NULL != (iter = (GtkTreeIter *)g_hash_table_lookup(iterhash, (gpointer)ip)))) {
		/* g_print("found iter for item %d\n", ip); */
	}
	
	if((NULL != iter) && (FALSE == item_get_new_status(ip))) {
		/* nothing to do */
		/* g_print("nothing to do for iter %d\n", ip); */
	} else {
		if(NULL == iter) {
			iter = g_new0(GtkTreeIter, 1);
			gtk_tree_store_prepend(itemstore, iter, NULL);
			g_hash_table_insert(iterhash, (gpointer)ip, (gpointer)iter);
		}	

		gtk_tree_store_set(itemstore, iter,
		                	      IS_TITLE, item_get_title(ip),
		                	      IS_PTR, ip,
		                	      IS_TIME, item_get_time(ip),
		                	      -1);
		ui_itemlist_update_item(ip);
	}
}

void ui_itemlist_enable_favicon_column(gboolean enabled) {

	/* we depend on the fact that the third column is the favicon column!!! 
	   if we are in search mode (or have a vfolder) we show the favicon 
	   column to give a hint where the item comes from ... */
	gtk_tree_view_column_set_visible(gtk_tree_view_get_column(GTK_TREE_VIEW(lookup_widget(mainwindow, "Itemlist")), 2), enabled);
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

void on_popup_launchitem_selected(void) {
	itemPtr		ip;

	if(NULL != (ip = ui_itemlist_get_selected()))
		ui_htmlview_launch_URL(ui_tabs_get_active_htmlview(), (gchar *)item_get_source(ip), TRUE);
	else
		ui_mainwindow_set_status_bar(_("No item has been selected"));
}

void on_popup_launchitem_in_tab_selected(void) {
	itemPtr		ip;

	if(NULL != (ip = ui_itemlist_get_selected()))
		ui_tabs_new(item_get_source(ip), item_get_source(ip));
	else
		ui_mainwindow_set_status_bar(_("No item has been selected"));
}

void on_Itemlist_row_activated(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {

	on_popup_launchitem_selected();
}

/* menu callbacks */					
void on_toggle_item_flag(GtkMenuItem *menuitem, gpointer user_data) {
	itemPtr		ip;
	
	if(NULL != (ip = ui_itemlist_get_selected()))
		itemlist_toggle_flag(ip);
}

void on_popup_toggle_flag(gpointer callback_data, guint callback_action, GtkWidget *widget) { on_toggle_item_flag(NULL, NULL); }

void on_toggle_unread_status(GtkMenuItem *menuitem, gpointer user_data) {
	itemPtr		ip;

	if(NULL != (ip = ui_itemlist_get_selected())) 
		itemlist_toggle_read_status(ip);
}

void on_popup_toggle_read(gpointer callback_data, guint callback_action, GtkWidget *widget) { on_toggle_unread_status(NULL, NULL); }

void on_remove_items_activate(GtkMenuItem *menuitem, gpointer user_data) {
	nodePtr		np;
	
	np = ui_feedlist_get_selected();
	if((NULL != np) && (FST_FEED == np->type))
		itemlist_remove_items((feedPtr)np);
	else
		ui_show_error_box(_("You must select a feed to delete its items!"));
}

void on_remove_item_activate(GtkMenuItem *menuitem, gpointer user_data) {
	GtkTreeStore		*itemstore = ui_itemlist_get_tree_store();
	GtkTreeSelection	*selection;
	nodePtr			np;
	itemPtr			ip;
	
	np = ui_feedlist_get_selected();
	if((NULL != np) && (FST_FEED == np->type)) {
		if(NULL != (ip = ui_itemlist_get_selected())) {
			/* must unselect the item to avoid deferred removal */
			selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(lookup_widget(mainwindow, "Itemlist")));
			gtk_tree_selection_unselect_all(selection);
			itemlist_remove_item(ip);
			ui_htmlview_clear(ui_mainwindow_get_active_htmlview());
		} else {
			ui_mainwindow_set_status_bar(_("No item has been selected"));
		}
	} else {
		ui_show_error_box(_("You must select a feed to delete its items!"));
	}
}

void on_popup_remove_selected(gpointer callback_data, guint callback_action, GtkWidget *widget) { on_remove_item_activate(NULL, NULL); }

static void ui_itemlist_select(GtkTreeIter iter) {
	GtkTreeStore		*itemstore = ui_itemlist_get_tree_store();
	GtkWidget		*treeview;
	GtkTreeSelection	*selection;
	GtkTreePath		*path;

	/* some comfort: select the created iter */
	treeview = lookup_widget(mainwindow, "Itemlist");
	if(NULL != (selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)))) {
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(itemstore), &iter);
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview), path, NULL, FALSE, 0.0, 0.0);	
		gtk_tree_path_free(path);
		gtk_tree_selection_select_iter(selection, &iter);
	} else {
		g_warning("internal error! could not get feed tree view selection!\n");
	}
}

static gboolean ui_itemlist_find_unread_item(void) {
	GtkTreeStore		*itemstore = ui_itemlist_get_tree_store();
	GtkTreeSelection	*selection;
	GtkTreePath		*path;
	GtkWidget		*treeview;
	GtkTreeIter		iter;
	gboolean		valid;
	itemPtr			ip;
		
	valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(itemstore), &iter);
	while(valid) {
		gtk_tree_model_get(GTK_TREE_MODEL(itemstore), &iter, IS_PTR, &ip, -1);
		g_assert(ip != NULL);
		if(FALSE == item_get_read_status(ip)) {
			if(!ui_itemlist_get_two_pane_mode()) {
				ui_itemlist_select(iter);
				itemlist_set_read_status(ip, TRUE);	/* needed when no selection happens (e.g. when the item is already selected) */
			} else {
				itemlist_mark_all_read(ip->fp);
			}
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
					itemlist_toggle_flag(ip);
				else
					itemlist_toggle_read_status(ip);
			}
			return TRUE;
			break;
		case 3:
			ui_itemlist_select(iter);
			gtk_menu_popup(make_item_menu(ip), NULL, NULL, NULL, NULL, eb->button, eb->time);
			return TRUE;
			break;
	}	
	return FALSE;
}

/* two/three pane mode setting */
gboolean ui_itemlist_get_two_pane_mode(void) {

	return gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(lookup_widget(mainwindow, "toggle_condensed_view")));
}

void ui_itemlist_set_two_pane_mode(gboolean new_mode) {
	gboolean	old_mode;
	
	old_mode = ui_itemlist_get_two_pane_mode();
	ui_mainwindow_set_three_pane_mode(!new_mode);

	/* needed because switching does sometimes returns to the tree 
	   view with a very disturbing horizontal scrolling state */
	gtk_tree_view_scroll_to_point(GTK_TREE_VIEW(lookup_widget(mainwindow, "Itemlist")), 0, 0);

	if(new_mode != old_mode)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(mainwindow, "toggle_condensed_view")), new_mode);
}

void on_toggle_condensed_view_activate(GtkMenuItem *menuitem, gpointer user_data) { 
	nodePtr		np;
	
	itemlist_set_two_pane_mode(GTK_CHECK_MENU_ITEM(menuitem)->active);

	if(NULL != (np = ui_feedlist_get_selected())) {
		/* grab necessary to force HTML widget update (display must
		   change from feed description to list of items and vica 
		   versa */
		gtk_widget_grab_focus(lookup_widget(mainwindow, "feedlist"));
		itemlist_load(np);
	}
}
