/* Item list/view handling
   
   Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>
   		      Nathan J. Conrad <t98502@users.sourceforge.net>

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
#include "conf.h"
#include "ui_itemlist.h"
#include "ui_mainwindow.h" /* FIXME: Should switchpanemode be moved into here? */
#include "ui_tray.h"

typedef struct ui_item_data {
	GtkTreeIter row;
} ui_item_data;

extern GdkPixbuf	*icons[];

static GtkTreeStore	*itemstore = NULL;

extern gboolean 	itemlist_mode;

static gint		itemlist_loading;	/* freaky workaround for item list focussing problem */

/* displayed_fp should almost always be the same as selected_fp in ui_feedlist */
feedPtr	displayed_fp = NULL;

/* Resets the horizontal and vertical scrolling of the items HTML view. */
static void resetItemViewScrolling(GtkScrolledWindow *itemview);

/* mouse/keyboard interaction callbacks */
static void on_itemlist_selection_changed(GtkTreeSelection *selection, gpointer data);
static itemPtr ui_itemlist_get_selected();

GtkTreeStore * getItemStore(void) {

	if(NULL == itemstore) {
		/* set up a store of these attributes: 
			- item title
			- item label
			- item state (read/unread)		
			- pointer to item data
			- date time_t value
			- the type of the feed the item belongs to
		 */
		itemstore = gtk_tree_store_new(IS_LEN,
								 G_TYPE_STRING, 
								 G_TYPE_STRING,
								 GDK_TYPE_PIXBUF, 
								 G_TYPE_POINTER, 
								 G_TYPE_INT,
								 G_TYPE_STRING,
								 G_TYPE_INT);
	}
	
	return itemstore;
}

void ui_free_item_ui_data(itemPtr ip) {

	g_assert(ip->ui_data);
	g_free(ip->ui_data);
	ip->ui_data = NULL;
}

static gboolean ui_free_item_ui_data_foreach(GtkTreeModel *model,
					  GtkTreePath *path,
					  GtkTreeIter *iter,
					  gpointer data) {
	itemPtr ip;
	gtk_tree_model_get(GTK_TREE_MODEL(itemstore), iter,
				    IS_PTR, &ip, -1);
	ui_free_item_ui_data(ip);
	return FALSE;
}

void clearItemList(void) {
	displayed_fp = NULL;
	gtk_tree_model_foreach(GTK_TREE_MODEL(itemstore), &ui_free_item_ui_data_foreach, NULL);
	gtk_tree_store_clear(GTK_TREE_STORE(itemstore));
	clearHTMLView();
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

static void ui_update_item_from_iter(GtkTreeIter *iter) {
	GtkTreeStore *itemstore = getItemStore();
	gpointer	ip;
	gchar *title, *label, *time_str, *esc_title, *tmp;
	gint	time;
	GdkPixbuf *pixbuf;

	gtk_tree_model_get(GTK_TREE_MODEL(itemstore), iter,
				    IS_PTR, &ip,
				    IS_TITLE, &title,
				    IS_TIME, &time,
				    -1);

	/* Icon */
	if(FALSE == getItemMark(ip)) {
		if(FALSE == getItemReadStatus(ip)) {
			pixbuf = icons[ICON_UNREAD];
		} else {
			pixbuf = icons[ICON_READ];
		}
	} else {
		pixbuf = icons[ICON_FLAG];
	}

	/* Label */
	if ( title != NULL) {
		/* Here we have the following problem: a title string might contain 
		   either escaped markup (which we should not escape again) or 
		   non-markup text (which might contain ampersands, which must be
		   escaped). We assume no mixed case! */
		//if(is_escaped_markup(title))
		//	esc_title = unescape_markup(title);	// FIXME: unescaped!!!
		//else
			esc_title = g_markup_escape_text(title, -1);
			
		if(FALSE == getItemReadStatus(ip)) {
			label = g_strdup_printf("<span weight=\"bold\">%s</span>", esc_title);
		} else {
			label = g_strdup_printf("%s", esc_title);
		}
		g_free(esc_title);
	} else {
		label = g_strdup("");
	}

	/* Time */
	if(0 != time) {
		if(FALSE == getItemReadStatus(ip)) {
			/* the time value is no markup, so we escape it... */
			time_str = formatDate((time_t)time);
			tmp = g_markup_escape_text(time_str,-1);
			g_free(time_str);
			time_str = g_strdup_printf("<span weight=\"bold\">%s</span>", tmp);
			g_free(tmp);
		} else {
			time_str = formatDate((time_t)time);
		}
	} else {
		time_str = g_strdup("");
	}

	/* Finish 'em... */
	gtk_tree_store_set(getItemStore(), iter,
				    IS_LABEL, label,
				    IS_TIME_STR, time_str,
				    IS_ICON, pixbuf,
				    -1);
	g_free(time_str);
	g_free(title);
	g_free(label);
}

void ui_update_item(itemPtr ip) {
	g_assert(NULL != ip);
	if (ip->ui_data)
		ui_update_item_from_iter(&((ui_item_data*)ip->ui_data)->row);
}

void initItemList(GtkWidget *itemlist) {
	GtkCellRenderer		*renderer;
	GtkTreeViewColumn 	*column;
	GtkTreeSelection	*select;
	GtkTreeStore		*itemstore;	
	
	g_assert(mainwindow != NULL);

	switchPaneMode(!getBooleanConfValue(LAST_ITEMLIST_MODE));
	
	itemstore = getItemStore();

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
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(itemstore), IS_TIME, timeCompFunc, NULL, NULL);
	g_object_set(column, "resizable", TRUE, NULL);

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

void displayItemList(void) {
	GtkTreeIter		iter;
	gchar			*buffer = NULL;
	gboolean		valid;
	itemPtr			ip;
	gchar               *tmp = NULL;

	g_assert(NULL != mainwindow);
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
		itemPtr ip = ui_itemlist_get_selected();
		if(!ip) {
			/* display feed info */
			if(displayed_fp) {
				if(!getFeedAvailable(displayed_fp) || 
				   (NULL != displayed_fp->parseErrors)) {
					tmp = getFeedErrorDescription(displayed_fp);
					addToHTMLBuffer(&buffer, tmp);
					g_free(tmp);
				}
  				addToHTMLBuffer(&buffer, getFeedDescription(displayed_fp));
			}
		} else {
			/* display item content */
			markItemAsRead(ip);
			addToHTMLBuffer(&buffer, getItemDescription(ip));
		}

		/* no scrolling reset, because this code should only be
		   triggered for redraw purposes! */
	}
	finishHTML(&buffer);
	writeHTML(buffer);
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
	displayed_fp = fp;
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
			g_assert(ip->ui_data == NULL);
			ip->ui_data = g_malloc(sizeof(ui_item_data));
			((ui_item_data*)(ip->ui_data))->row = iter;

			ui_update_item_from_iter(&iter);
		}

		itemlist = g_slist_next(itemlist);
	}
	displayItemList();
	preFocusItemlist();
}

/* Resets the horizontal and vertical scrolling of the items HTML view. */
static void resetItemViewScrolling(GtkScrolledWindow *itemview) {
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

static itemPtr ui_itemlist_get_selected() {
	GtkWidget		*itemlist;
	GtkTreeIter		iter;
	GtkTreeModel		*model;
	itemPtr			item;
	GtkTreeSelection	*selection;

	if(NULL == (itemlist = lookup_widget(mainwindow, "Itemlist"))) {
		print_status(g_strdup(_("could not find item list widget!")));
		return NULL;
	}
	
	if(NULL == (selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(itemlist)))) {
		print_status(g_strdup(_("could not retrieve selection of item list!")));
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
	GtkTreeIter iter;
	GtkTreeModel		*model;
	itemPtr ip;
	if(!itemlist_loading) {
		if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
			gtk_tree_model_get(model, &iter, IS_PTR, &ip, -1);

			displayItem(ip);

			/* reset HTML widget scrolling */
			resetItemViewScrolling(GTK_SCROLLED_WINDOW(lookup_widget(mainwindow, "itemview")));			
		}
	}
}

void on_popup_toggle_read(gpointer callback_data,
						 guint callback_action,
						 GtkWidget *widget) {
	itemPtr ip = (itemPtr)callback_data;
	if(getItemReadStatus(ip)) 
		markItemAsUnread(ip);
	else
		markItemAsRead(ip);
}

void on_popup_toggle_flag(gpointer callback_data,
						 guint callback_action,
						 GtkWidget *widget) {
	itemPtr ip = (itemPtr)callback_data;
	setItemMark(ip, !getItemMark(ip));		
}

void on_popup_allunread_selected(void) {
	itemPtr		ip;
	gboolean    valid;
	GtkTreeIter iter;
	g_assert(NULL != itemstore);

	valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(itemstore), &iter);
	while(valid) {
		gtk_tree_model_get(GTK_TREE_MODEL(itemstore), &iter,
					    IS_PTR, &ip, -1);
		g_assert(ip != NULL);
		markItemAsRead(ip);
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(itemstore), &iter);
	}
}

void on_Itemlist_row_activated(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {

	on_popup_launchitem_selected();
}

/* menu callbacks */					
void on_toggle_item_flag(void) {
	itemPtr ip = ui_itemlist_get_selected();
	if(ip)
		setItemMark(ip, !getItemMark(ip));	
}


void on_popup_launchitem_selected(void) {
	itemPtr		ip;
	
	ip = ui_itemlist_get_selected();

	if(ip)
		launchURL(getItemSource(ip));
	else
		print_status(g_strdup(_("No item has been selected!")));
}

void on_toggle_unread_status(void) {

	itemPtr ip = ui_itemlist_get_selected();
	if(ip) {
		if(getItemReadStatus(ip)) 
			markItemAsUnread(ip);
		else
			markItemAsRead(ip);
	}
}

void on_remove_items_activate(GtkMenuItem *menuitem, gpointer user_data) {
	feedPtr fp = displayed_fp;
	if(fp) {
		clearItemList();		/* clear tree view */
		clearFeedItemList(fp);	/* delete items */
	} else {
		showErrorBox(_("You have to select a feed to delete its items!"));
	}
}

static gboolean findUnreadItem(void) {
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

void on_next_unread_item_activate(GtkMenuItem *menuitem, gpointer user_data) {
	feedPtr			fp;
	
	/* before scanning the feed list, we test if there is a unread 
	   item in the currently selected feed! */
	if(TRUE == findUnreadItem())
		return;
	
	/* find first feed with unread items */
	fp = ui_feed_find_unread(NULL);
	
	if (fp) {
		/* ui_select_feed(fp); FIXME: This would require an easy lookup from fp to a row, but would be a cleaner feedlist interface. */
	
		if(NULL == fp) {
			return;	/* if we don't find a feed with unread items do nothing */
		}
		
		/* load found feed */
		loadItemList(fp, NULL);
		
		/* find first unread item */
		findUnreadItem();
	} else {
		print_status(g_strdup(_("There are no unread items!")));
	}
}

static void ui_select_item(itemPtr ip) {
	GtkTreeIter		iter;
	GtkWidget		*treeview;
	GtkTreeSelection	*selection;
	GtkTreePath		*path;

	g_assert(ip->ui_data);
	iter = ((ui_item_data*)(ip->ui_data))->row;

	/* some comfort: select the created iter */
	if(NULL != (treeview = lookup_widget(mainwindow, "Itemlist"))) {
		if(NULL != (selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)))) {
			path = gtk_tree_model_get_path(GTK_TREE_MODEL(itemstore), &iter);
			gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview), path, NULL, FALSE, 0.0, 0.0);	
			gtk_tree_path_free(path);
			gtk_tree_selection_select_iter(selection, &iter);
		} else
			g_warning(_("internal error! could not get feed tree view selection!\n"));
	} else {
		print_status(g_strdup(_("internal error! could not select newly created treestore iter!")));
	}
}

gboolean on_itemlist_button_press_event(GtkWidget *widget,
								GdkEventButton *event,
								gpointer user_data)
{
	GdkEventButton      *eb;
	GtkWidget      *treeview;
	GtkTreePath    *path;
	GtkTreeIter    iter;
	itemPtr		ip=NULL;
	gboolean       selected = TRUE;
	
	treeview = lookup_widget(mainwindow, "Itemlist");
	g_assert(treeview);

	if (event->type != GDK_BUTTON_PRESS)
		return FALSE;

	eb = (GdkEventButton*) event;

	if (eb->button != 3)
		return FALSE;

	if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview), event->x, event->y, &path, NULL, NULL, NULL))
		selected=FALSE;
	else {
		gtk_tree_model_get_iter(GTK_TREE_MODEL(getItemStore()), &iter, path);
		gtk_tree_model_get(GTK_TREE_MODEL(itemstore), &iter,
					    IS_PTR, &ip,
					    -1);
		ui_select_item(ip);
	}
	
	gtk_menu_popup(make_item_menu(ip), NULL, NULL, NULL, NULL, eb->button, eb->time);
	
	return TRUE;
}
