/*
   item list/view handling
   
   Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <string.h>
#include "support.h"
#include "callbacks.h"
#include "common.h"
#include "feed.h"
#include "item.h"
#include "htmlview.h"
#include "ui_itemlist.h"

extern GtkWidget	*mainwindow;
extern GdkPixbuf	*icons[];

static GtkTreeStore	*itemstore = NULL;

extern feedPtr		selected_fp;
extern gint		selected_type;

extern gboolean 	itemlist_mode;

static gint		itemlist_loading = 0;	/* freaky workaround for item list focussing problem */

/* like selected_fp, to remember the last selected item */
itemPtr	selected_ip = NULL;

GtkTreeStore * getItemStore(void) {

	if(NULL == itemstore) {
		/* set up a store of these attributes: 
			- item title
			- item state (read/unread)		
			- pointer to item data
			- date time_t value
			- the type of the feed the item belongs to

		 */
		itemstore = gtk_tree_store_new(5, G_TYPE_STRING, 
						  GDK_TYPE_PIXBUF, 
						  G_TYPE_POINTER, 
						  G_TYPE_INT,
						  G_TYPE_INT);
	}
	g_assert(NULL != itemstore);
	return itemstore;
}

static void renderItemTitle(GtkTreeViewColumn *tree_column,
	             GtkCellRenderer   *cell,
	             GtkTreeModel      *model,
        	     GtkTreeIter       *iter,
	             gpointer           data)
{
	gpointer	ip;

	gtk_tree_model_get(model, iter, IS_PTR, &ip, -1);

	if(FALSE == getItemReadStatus(ip)) {
		g_object_set(GTK_CELL_RENDERER(cell), "font", "bold", NULL);
	} else {
		g_object_set(GTK_CELL_RENDERER(cell), "font", "normal", NULL);
	}
}

static void renderItemStatus(GtkTreeViewColumn *tree_column,
	             GtkCellRenderer   *cell,
	             GtkTreeModel      *model,
        	     GtkTreeIter       *iter,
	             gpointer           data)
{
	gpointer	ip;

	gtk_tree_model_get(model, iter, IS_PTR, &ip, -1);

	if(FALSE == getItemMark(ip)) {
		if(FALSE == getItemReadStatus(ip)) {
			g_object_set(GTK_CELL_RENDERER(cell), "pixbuf", icons[ICON_UNREAD], NULL);
		} else {
			g_object_set(GTK_CELL_RENDERER(cell), "pixbuf", icons[ICON_READ], NULL);
		}
	} else {
		g_object_set(GTK_CELL_RENDERER(cell), "pixbuf", icons[ICON_FLAG], NULL);
	}
}

static void renderItemDate(GtkTreeViewColumn *tree_column,
	             GtkCellRenderer   *cell,
	             GtkTreeModel      *model,
        	     GtkTreeIter       *iter,
	             gpointer           data)
{
	gint		time;
	gchar		*tmp;

	gtk_tree_model_get(model, iter, IS_TIME, &time, -1);
	if(0 != time) {
		tmp = formatDate((time_t)time);	// FIXME: sloooowwwwww...
		g_object_set(GTK_CELL_RENDERER(cell), "text", tmp, NULL);
		g_free(tmp);
	} else {
		g_object_set(GTK_CELL_RENDERER(cell), "text", "", NULL);
	}
}

/* sort function for the item list date column */
static gint timeCompFunc(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data) {
	time_t	timea, timeb;
	
	g_assert(model != NULL);
	g_assert(a != NULL);
	g_assert(b != NULL);
	gtk_tree_model_get(model, a, IS_TIME, &timea, -1);
	gtk_tree_model_get(model, b, IS_TIME, &timeb, -1);
	
	return timea-timeb;
}

void setupItemList(GtkWidget *itemlist) {
	GtkCellRenderer		*renderer;
	GtkTreeViewColumn 	*column;
	GtkTreeSelection	*select;
	GtkTreeStore		*itemstore;	
	
	g_assert(mainwindow != NULL);
	
	itemstore = getItemStore();

	gtk_tree_view_set_model(GTK_TREE_VIEW(itemlist), GTK_TREE_MODEL(itemstore));

	/* we only render the state, title and time */
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes("", renderer, "pixbuf", IS_STATE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(itemlist), column);
	/*gtk_tree_view_column_set_sort_column_id(column, IS_STATE); ...leads to segfaults on tab-bing through */
	gtk_tree_view_column_set_cell_data_func(column, renderer, renderItemStatus, NULL, NULL);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Date"), renderer, "text", IS_TIME, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(itemlist), column);
	gtk_tree_view_column_set_sort_column_id(column, IS_TIME);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(itemstore), IS_TIME, timeCompFunc, NULL, NULL);
	gtk_tree_view_column_set_cell_data_func(column, renderer, renderItemDate, NULL, NULL);
	g_object_set(column, "resizable", TRUE, NULL);

	renderer = gtk_cell_renderer_text_new();						   	
	column = gtk_tree_view_column_new_with_attributes(_("Headline"), renderer, "text", IS_TITLE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(itemlist), column);
	gtk_tree_view_column_set_sort_column_id(column, IS_TITLE);
	gtk_tree_view_column_set_cell_data_func(column, renderer, renderItemTitle, NULL, NULL);
	g_object_set(column, "resizable", TRUE, NULL);
	
	/* Setup the selection handler */
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(itemlist));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(select), "changed",
                  G_CALLBACK(on_itemlist_selection_changed),
                  NULL);
}

/* typically called when filling the item tree view */
void preFocusItemlist(void) {
	GtkWidget		*itemlist;
	GtkTreeSelection	*itemselection;
	GtkAdjustment		*adj;
	
	/* the following is important to prevent setting the unread
	   flag for the first item in the item list when the user does
	   the first click into the treeview, if we don't do a focus and
	   unselect, GTK would always (exception: clicking on first item)
	   generate two selection-change events (one for the clicked and
	   one for the selected item)!!! */

	if(NULL == (itemlist = lookup_widget(mainwindow, "Itemlist"))) {
		g_warning(_("item list widget lookup failed!\n"));
		return;
	}

	/* prevent marking as unread before focussing, which leads 
	   to a selection */
	itemlist_loading = 1;
	gtk_widget_grab_focus(itemlist);

	if(NULL == (itemselection = gtk_tree_view_get_selection(GTK_TREE_VIEW(itemlist)))) {
		g_warning(_("could not retrieve selection of item list!\n"));
		return;
	}
	gtk_tree_selection_unselect_all(itemselection);

	gtk_widget_grab_focus(lookup_widget(mainwindow, "feedlist"));
	itemlist_loading = 0;
	
	/* finally reset scrolling to the first item */
	adj = gtk_tree_view_get_vadjustment(GTK_TREE_VIEW(itemlist));
	gtk_adjustment_set_value(adj, 0.0);
	gtk_tree_view_set_vadjustment(GTK_TREE_VIEW(itemlist), adj);
}

void clearItemList(void) {

	selected_ip = NULL;
	gtk_tree_store_clear(GTK_TREE_STORE(itemstore));
	clearHTMLView();
}

void displayItemList(void) {
	GtkTreeIter		iter;
	gchar			*buffer = NULL;
	gboolean		valid;
	itemPtr			ip;
	gchar               *tmp = NULL;
	g_assert(NULL != mainwindow);
	
	/* HTML widget can be used only from GTK thread */	
	if(gnome_vfs_is_primary_thread()) {
		startHTML(&buffer, itemlist_mode);
		if(!itemlist_mode) {
			/* two pane mode */
			valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(itemstore), &iter);
			while(valid) {	
				gtk_tree_model_get(GTK_TREE_MODEL(itemstore), &iter, IS_PTR, &ip, -1);
				
				if(getItemReadStatus(ip)) 
					addToHTMLBuffer(&buffer, UNSHADED_START);
				else
					addToHTMLBuffer(&buffer, SHADED_START);
				
				addToHTMLBuffer(&buffer, getItemDescription(ip));
				
				if(getItemReadStatus(ip))
					addToHTMLBuffer(&buffer, UNSHADED_END);
				else {
					addToHTMLBuffer(&buffer, SHADED_END);
					markItemAsRead(ip);
				}
					
				valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(itemstore), &iter);
			}
			
			/* reset HTML widget scrolling */
			resetItemViewScrolling(GTK_SCROLLED_WINDOW(lookup_widget(mainwindow, "itemlistview")));
		} else {	
			/* three pane mode */
			if(NULL == selected_ip) {
				/* display feed info */
				if(NULL != selected_fp) {
					if(!getFeedAvailable(selected_fp) || 
					   (NULL != selected_fp->parseErrors)) {
						tmp = getFeedErrorDescription(selected_fp);
						addToHTMLBuffer(&buffer, tmp);
						g_free(tmp);
					}
  					addToHTMLBuffer(&buffer, getFeedDescription(selected_fp));
				}
			} else {
				/* display item content */
				markItemAsRead(ip);
				addToHTMLBuffer(&buffer, getItemDescription(selected_ip));
			}
			
			/* no scrolling reset, because this code should only be
			   triggered for redraw purposes! */
		}
		finishHTML(&buffer);
		writeHTML(buffer);
		g_free(buffer);
	}
}

void loadItemList(feedPtr fp, gchar *searchstring) {
	GtkTreeIter	iter;
	GSList		*itemlist;
	itemPtr		ip;
	gchar		*title, *description;
	gboolean	add;

	if(NULL == fp) {
		g_warning(_("internal error! item list display for NULL pointer requested!\n"));
		return;
	}

	clearItemList();	
	itemlist = getFeedItemList(fp);
	while(NULL != itemlist) {
		ip = itemlist->data;
		title = getItemTitle(ip);
		description = getItemDescription(ip);
		
		add = TRUE;
		if(NULL != searchstring) {
			add = FALSE;
				
			if((NULL != title) && (NULL != strstr(title, searchstring)))
				add = TRUE;

			if((NULL != description) && (NULL != strstr(description, searchstring)))
				add = TRUE;
		}

		if(add) {
			gtk_tree_store_append(itemstore, &iter, NULL);
			gtk_tree_store_set(itemstore, &iter,
	     		   		IS_TITLE, title,
					IS_PTR, ip,
					IS_TIME, getItemTime(ip),
					IS_TYPE, getFeedType(fp),	/* not the item type, this would fail for VFolders! */
					-1);
		}

		itemlist = g_slist_next(itemlist);
	}
	displayItemList();
	preFocusItemlist();
}

/* Resets the horizontal and vertical scrolling of the items HTML view. */
void resetItemViewScrolling(GtkScrolledWindow *itemview) {
	GtkAdjustment	*adj;

	if(NULL != itemview) {
		adj = gtk_scrolled_window_get_vadjustment(itemview);
		gtk_adjustment_set_value(adj, 0.0);
		gtk_scrolled_window_set_vadjustment(itemview, adj);
		gtk_adjustment_value_changed(adj);

		adj = gtk_scrolled_window_get_hadjustment(itemview);
		gtk_adjustment_set_value(adj, 0.0);
		gtk_scrolled_window_set_hadjustment(itemview, adj);
		gtk_adjustment_value_changed(adj);
	} else {
		g_warning(_("internal error! could not reset HTML widget scrolling!"));
	}
}

/* Function scrolls down the item views scrolled window.
   This function returns FALSE if the scrolled window
   vertical scroll position is at the maximum and TRUE
   if the vertical adjustment was increased. */
gboolean scrollItemView(GtkWidget *itemView) {
	GtkAdjustment	*vertical_adjustment;
	gdouble		old_value;
	gdouble		new_value;
	gdouble		limit;

	vertical_adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(itemView));
	old_value = gtk_adjustment_get_value(vertical_adjustment);
	new_value = old_value + vertical_adjustment->page_increment;
	limit = vertical_adjustment->upper - vertical_adjustment->page_size;
	if(new_value > limit)
		new_value = limit;
	gtk_adjustment_set_value(vertical_adjustment, new_value);
	gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(itemView), vertical_adjustment);
	return (new_value > old_value);
}

/* mouse/keyboard interaction callbacks */
void itemlist_selection_changed(void) {
	GtkTreeSelection	*selection;
	GtkWidget		*itemlist;
	GtkTreeIter		iter;
        GtkTreeModel		*model;

	gint		type;

	undoTrayIcon();
	
	/* do nothing upon initial focussing */
	if(!itemlist_loading) {
		g_assert(mainwindow != NULL);
		if(NULL == (itemlist = lookup_widget(mainwindow, "Itemlist"))) {
			print_status(_("could not find item list widget!"));
			return;
		}

		if(NULL == (selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(itemlist)))) {
			print_status(_("could not retrieve selection of item list!"));
			return;
		}

       		if(gtk_tree_selection_get_selected(selection, &model, &iter)) {

               		gtk_tree_model_get(model, &iter, IS_PTR, &selected_ip,
							 IS_TYPE, &type, -1);

			g_assert(selected_ip != NULL);
			if(!itemlist_loading) {
				if(NULL != (itemlist = lookup_widget(mainwindow, "Itemlist"))) {
					displayItem(selected_ip);

					/* reset HTML widget scrolling */
					resetItemViewScrolling(GTK_SCROLLED_WINDOW(lookup_widget(mainwindow, "itemview")));

					/* redraw feed list to update unread items numbers */
					redrawFeedList();
				}
			}
       		}
	}
}

void on_itemlist_selection_changed(GtkTreeSelection *selection, gpointer data) {

	itemlist_selection_changed();
}

void on_toggle_condensed_view_activate(GtkMenuItem *menuitem, gpointer user_data) { 

	if(!itemlist_mode != GTK_CHECK_MENU_ITEM(menuitem)->active)
		toggle_condensed_view(); 
}

void on_popup_toggle_condensed_view(gpointer cb_data, guint cb_action, GtkWidget *item) {

	if(!itemlist_mode != GTK_CHECK_MENU_ITEM(item)->active)
		toggle_condensed_view(); 
}

gboolean on_Itemlist_move_cursor(GtkTreeView *treeview, GtkMovementStep  step, gint count, gpointer user_data) {

	itemlist_selection_changed();
	return FALSE;
}

void on_popup_allunread_selected(void) {
	itemPtr		ip;
	gboolean    valid;
	GtkWidget		*treeview;
	GtkTreeSelection	*select;
	GtkTreeModel		*model;
	GtkTreeIter iter, child;
	gchar			*ckey;
	feedPtr     fp;
	GSList		*itemlist;

	if (selected_type == FST_NODE) {
		/* A folder is selected, mark as read the content of all its feeds */

		g_assert(NULL != mainwindow);
		treeview = lookup_widget(mainwindow, "feedlist");
		g_assert(NULL != treeview);

		select = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
		g_assert(NULL != select);

		if(!gtk_tree_selection_get_selected(select, &model, &iter)) {
			print_status(_("could not retrieve selected of entry list!"));
			return;
		}

		valid = gtk_tree_model_iter_children(model, &child, &iter);
		while (valid) {

			/* get the feed pointer */
			gtk_tree_model_get(model, &child, FS_KEY, &ckey, -1);
			g_assert(NULL != ckey);
			fp = getFeed(ckey);
			g_free(ckey);

			/* Mark all as read */
			itemlist = getFeedItemList(fp);
			while(NULL != itemlist) {
				ip = itemlist->data;
				markItemAsRead(ip);
				itemlist = g_slist_next(itemlist);
			}

			/* next feed in selected folder */
			valid = gtk_tree_model_iter_next(model, &child);
		}

	} else {

		g_assert(NULL != itemstore);

		valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(itemstore), &iter);
		while(valid) {
			gtk_tree_model_get(GTK_TREE_MODEL(itemstore), &iter, IS_PTR, &ip, -1);
			g_assert(ip != NULL);
			markItemAsRead(ip);

			valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(itemstore), &iter);
		}

	}

	/* redraw feed list to update unread items count */
	redrawFeedList();

	/* necessary to rerender the formerly bold text labels */
	redrawItemList();	
}

void on_Itemlist_row_activated(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {

	on_popup_launchitem_selected();
}

/* menu callbacks */					
void on_toggle_item_flag(void) {
	
	if(NULL == selected_ip)
		return;
		
	setItemMark(selected_ip, !getItemMark(selected_ip));	
	redrawItemList();
}


void on_popup_launchitem_selected(void) {
	GtkWidget		*itemlist;
	GtkTreeSelection	*selection;
	GtkTreeModel 		*model;
	GtkTreeIter		iter;
	gpointer		tmp_ip;
	gint			tmp_type;

	if(NULL == (itemlist = lookup_widget(mainwindow, "Itemlist")))
		return;

	if(NULL == (selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(itemlist)))) {
		print_status(_("could not retrieve selection of item list!"));
		return;
	}

	if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
	       	gtk_tree_model_get(model, &iter, IS_PTR, &tmp_ip,
						 IS_TYPE, &tmp_type, -1);
		g_assert(tmp_ip != NULL);
		launchURL(getItemSource(tmp_ip));
	}
}

void on_toggle_unread_status(void) {

	if(NULL == selected_ip)
		return;	
		
	if(getItemReadStatus(selected_ip)) 
		markItemAsUnread(selected_ip);
	else
		markItemAsRead(selected_ip);
	redrawItemList();
}

void on_remove_items_activate(GtkMenuItem *menuitem, gpointer user_data) {

	if(NULL != selected_fp) {
		clearItemList();		/* clear tree view */
		clearFeedItemList(selected_fp);	/* delete items */
	} else {
		showErrorBox(_("You have to select a feed to delete its items!"));
	}
}

gboolean findUnreadItem(void) {
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
		if(FALSE == getItemReadStatus(ip)) {
			/* select found item and the item list... */
			if(NULL != (treeview = lookup_widget(mainwindow, "Itemlist"))) {
				if(NULL != (selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)))) {
					gtk_tree_selection_select_iter(selection, &iter);
					path = gtk_tree_model_get_path(GTK_TREE_MODEL(itemstore), &iter);
 					gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview), path, NULL, FALSE, 0.0, 0.0);
 					gtk_tree_view_set_cursor(GTK_TREE_VIEW(treeview), path, NULL, FALSE);
					gtk_tree_path_free(path);
					gtk_widget_grab_focus(treeview);
				} else
					g_warning(_("internal error! could not get feed tree view selection!\n"));
			} else {
				g_warning(_("internal error! could not find feed tree view widget!\n"));
			}			
			return TRUE;
		}
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(itemstore), &iter);
	}
	
	return FALSE;
}
