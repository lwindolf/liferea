/**
 * @file ui_feedlist.c GUI feed list handling
 *
 * Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>
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

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "guitreemodelfilter.h"
#include "support.h"
#include "interface.h"
#include "callbacks.h"
#include "feed.h"
#include "folder.h"
#include "conf.h"
#include "ui_feedlist.h"
#include "ui_mainwindow.h"
#include "ui_tray.h"
#include "ui_feed.h"
#include "update.h"
#include "htmlview.h"
#include "favicon.h"
#include "debug.h"

extern GtkWidget	*mainwindow;
extern GHashTable	*feedHandler;

GtkTreeModel		*filter;
GtkTreeStore		*feedstore = NULL;

/* flag to enable/disable the GtkTreeModel filter */
gboolean filter_feeds_without_unread_headlines = FALSE;

folderPtr ui_feedlist_get_parent(nodePtr ptr) {
	GtkTreeIter *iter = &((ui_data*)(ptr->ui_data))->row;
	GtkTreeIter parent;
	folderPtr parentPtr;
	
	if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(feedstore), &parent, iter)) {
		gtk_tree_model_get(GTK_TREE_MODEL(feedstore), &parent,
					    FS_PTR, &parentPtr,
					    -1);
		return parentPtr;
	}
	
	return NULL;
}

nodePtr ui_feedlist_get_selected() {

	GtkWidget		*treeview;
	GtkTreeSelection	*select;
	GtkTreeModel		*model = GTK_TREE_MODEL(feedstore);
	GtkTreeIter	iter;
	nodePtr		ptr;

	g_assert(mainwindow);
	treeview = lookup_widget(mainwindow, "feedlist");
	g_assert(treeview);
	
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	g_assert(select);
	
	if(gtk_tree_selection_get_selected(select, &model, &iter)) {
		gtk_tree_model_get(model, &iter, 
					    FS_PTR, &ptr, 
					    -1);
		return ptr;
	} else
		return NULL;
}

folderPtr ui_feedlist_get_target_folder(int *pos) {
	nodePtr ptr = ui_feedlist_get_selected();
	GtkTreeIter *iter;
	
	if (ptr == NULL) {
		*pos = -1;
		return NULL;
	}

	iter = &((ui_data*)(ptr->ui_data))->row;

	if(IS_FOLDER(ptr->type)) {
		*pos = -1;
		return (folderPtr)ptr;
	} else {
		GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(feedstore), iter);
		gint *indices = gtk_tree_path_get_indices(path);
		*pos = indices[gtk_tree_path_get_depth(path)-1] + 1;
		gtk_tree_path_free(path);
		return ui_feedlist_get_parent(ptr);
	}
}


static GdkPixbuf* ui_feed_select_icon(feedPtr fp) {
	gpointer	favicon;
	g_assert(!IS_FOLDER(fp->type));
	
	if(!feed_get_available(fp)) {
		return icons[ICON_UNAVAILABLE];
	}

	if(NULL != (favicon = feed_get_favicon(fp))) {
		return favicon;
	}
	
	if (fp->fhp != NULL && fp->fhp->icon < MAX_ICONS) {
		return icons[fp->fhp->icon];
	}

	debug0(DEBUG_GUI, "internal error! unknown entry type! cannot display appropriate icon!\n");
	/* And default to the available icon.... */
	return icons[ICON_AVAILABLE];
}

static void ui_feedlist_update_(GtkTreeIter *iter) {
	GtkTreeModel *tree_model = GTK_TREE_MODEL(feedstore);
	GtkTreeIter childiter;
	gboolean valid;
	nodePtr ptr = NULL;
	
	if(iter != NULL) {
		gtk_tree_model_get(tree_model, iter,
					    FS_PTR, &ptr,
					    -1);
		
		valid = gtk_tree_model_iter_children(tree_model, &childiter, iter);
	} else {
		valid = gtk_tree_model_get_iter_first(tree_model, &childiter);
	}

	if (ptr != NULL)
		((ui_data*)(ptr->ui_data))->row = *iter;

	while(valid) {
		ui_feedlist_update_(&childiter);
		valid = gtk_tree_model_iter_next(tree_model, &childiter);
	}

	if (ptr != NULL) {
		if (IS_FOLDER(ptr->type))
			ui_folder_update((folderPtr)ptr);
		else
			ui_feed_update((feedPtr)ptr);
	}
}

void ui_feedlist_update_iter(GtkTreeIter *iter) {

	ui_feedlist_update_(iter);

	if(filter_feeds_without_unread_headlines)
		gui_tree_model_filter_refilter(GUI_TREE_MODEL_FILTER(filter));
		
	ui_redraw_widget("feedlist");
}

void ui_feed_update(feedPtr fp) {
	GtkTreeModel      *model;
	GtkTreeIter       *iter;
	gchar     *label, *tmp;
	int		count;
	
	if (fp->ui_data == NULL)
		return;
	
	iter = &((ui_data*)fp->ui_data)->row;
	model =  GTK_TREE_MODEL(feedstore);
	
	g_assert(!IS_FOLDER(fp->type));
	
	count = feed_get_unread_counter(fp);
	label = unhtmlize(g_strdup(feed_get_title(fp)));
	/* FIXME: Unescape text here! */
	tmp = g_markup_escape_text(label,-1);
	g_free(label);
	if(count > 0) {
		label = g_strdup_printf("<span weight=\"bold\">%s (%d)</span>", tmp, count);
	} else {
		label = g_strdup_printf("%s", tmp);
	}
	g_free(tmp);
	
	if(NULL != fp->parseErrors) {
		tmp = g_strdup_printf("<span foreground=\"red\">%s</span>", label);
		g_free(label);
		label = tmp;
	}
	
	gtk_tree_store_set(feedstore, iter,
				    FS_LABEL, label,
				    FS_UNREAD, count,
				    FS_ICON, ui_feed_select_icon(fp),
				    -1);
	
	g_free(label);
}

static void ui_feedlist_selection_changed_cb(GtkTreeSelection *selection, gpointer data) {
	GtkTreeIter		iter;
	GtkTreeModel		*model;
	feedPtr			fp;
	GdkGeometry		geometry;
	gint				type = FST_INVALID;

	ui_tray_zero_new();
	
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		gtk_tree_model_get(model, &iter, 
					    FS_PTR, &fp,
					    -1);
		if (fp != NULL) 
			type = fp->type;
		
		/* make sure thats no grouping iterator */
		if(fp && (IS_FEED(fp->type) || FST_VFOLDER == fp->type)) {
			
			/* FIXME: another workaround to prevent strange window
			   size increasings after feed selection changing */
			geometry.min_height=480;
			geometry.min_width=640;
			g_assert(mainwindow != NULL);
			gtk_window_set_geometry_hints(GTK_WINDOW(mainwindow), mainwindow, &geometry, GDK_HINT_MIN_SIZE);
			
			/* workaround to ensure the feedlist is focussed when we click it
			   (Mozilla might prevent this, ui_itemlist_display() depends on this */
			gtk_widget_grab_focus(lookup_widget(mainwindow, "feedlist"));
			
			/* Set up the item list */
			ui_itemlist_load((nodePtr)fp, NULL);
		} else { /* Selecting a folder */
			ui_itemlist_clear();
		}
	} else {
		/* If we cannot get the new selection we keep the old one
		   this happens when we're doing drag&drop for example. */
	}
	ui_mainwindow_update_feed_menu(type);
}

static void ui_feedlist_row_activated_cb(GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data) {
	GtkTreeIter iter;
	nodePtr ptr;
	
	gtk_tree_model_get_iter(gtk_tree_view_get_model(tv), &iter, path);

	gtk_tree_model_get(gtk_tree_view_get_model(tv), &iter,
				    FS_PTR, &ptr, -1);
	if (IS_FOLDER(ptr->type)) {
		if (gtk_tree_view_row_expanded(tv, path))
			gtk_tree_view_collapse_row(tv, path);
		else
			gtk_tree_view_expand_row(tv,path,FALSE);
	}

}

static gboolean ui_feedlist_key_press_cb(GtkWidget *widget, GdkEventKey *event, gpointer data) {
	if (event->type == GDK_KEY_PRESS &&
	    event->state == 0 &&
	    event->keyval == GDK_Delete) {
		nodePtr ptr = ui_feedlist_get_selected();
		
		if (ptr != NULL) {
			ui_feedlist_delete(ptr);
			return TRUE;
		}
	}
	return FALSE;
}

static gboolean filter_visible_function(GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
	gint		count;

	if(!filter_feeds_without_unread_headlines)
		return TRUE;
		
	gtk_tree_model_get(model, iter, FS_UNREAD, &count, -1);

	if(0 != count) 
		return TRUE;
	else 
		return FALSE;
}

/* Sets either the unread feeds filter model or the standard
   GTK tree model. This is necessary because only the standard
   model supports drag and drop. */
void ui_feedlist_set_model(GtkTreeView *feedview, GtkTreeStore *feedstore, gboolean filtered) {

	if(filtered) {
		filter = gui_tree_model_filter_new(GTK_TREE_MODEL(feedstore), NULL);

		gui_tree_model_filter_set_visible_func(GUI_TREE_MODEL_FILTER(filter),
	        	                               filter_visible_function,
	                	                       NULL,
	                        	               NULL);

		gtk_tree_view_set_model(GTK_TREE_VIEW(feedview), GTK_TREE_MODEL(filter));
	} else {
		gtk_tree_view_set_model(GTK_TREE_VIEW(feedview), GTK_TREE_MODEL(feedstore));
	}
}

/* sets up the entry list store and connects it to the entry list
   view in the main window */
void ui_feedlist_init(GtkWidget *feedview) {
	GtkCellRenderer		*textRenderer;
	GtkCellRenderer		*iconRenderer;	
	GtkTreeViewColumn 	*column;
	GtkTreeSelection	*select;	
	
	g_assert(mainwindow != NULL);
	g_assert(feedview != NULL);
	
	/* Set up store */
	feedstore = gtk_tree_store_new(FS_LEN,
	                               G_TYPE_STRING,
	                               GDK_TYPE_PIXBUF,
	                               G_TYPE_POINTER,
	                               G_TYPE_INT);

	ui_feedlist_set_model(GTK_TREE_VIEW(feedview), feedstore, FALSE);

	/* we only render the state and title */
	iconRenderer = gtk_cell_renderer_pixbuf_new();
	textRenderer = gtk_cell_renderer_text_new();

	column = gtk_tree_view_column_new();
	
	gtk_tree_view_column_pack_start(column, iconRenderer, FALSE);
	gtk_tree_view_column_pack_start(column, textRenderer, TRUE);
	
	gtk_tree_view_column_add_attribute(column, iconRenderer, "pixbuf", FS_ICON);
	gtk_tree_view_column_add_attribute(column, textRenderer, "markup", FS_LABEL);
	
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(feedview), column);

	/* And connect signals */
	g_signal_connect(G_OBJECT(feedview), "row-activated", G_CALLBACK(ui_feedlist_row_activated_cb), NULL);
	g_signal_connect(G_OBJECT(feedview), "key-press-event", G_CALLBACK(ui_feedlist_key_press_cb), NULL);

	/* Setup the selection handler for the main view */
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(feedview));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	
	g_signal_connect(G_OBJECT(select), "changed",
                 	 G_CALLBACK(ui_feedlist_selection_changed_cb),
                	 lookup_widget(mainwindow, "feedlist"));
	
	ui_dnd_init();			
	ui_mainwindow_update_feed_menu(FST_INVALID);
}

void ui_feedlist_select(nodePtr np) {
	GtkTreeIter iter = ((ui_data*)(np->ui_data))->row;
	GtkWidget		*treeview;
	GtkWidget		*focused;
	GtkTreeSelection	*selection;
	GtkTreePath		*path;

	/* some comfort: select the created iter */
	treeview = lookup_widget(mainwindow, "feedlist");
	g_assert(treeview != NULL);
	/* To work around a GTK+ bug. If the treeview is not
	   focused, setting the selected item will always select the
	   first item! */
	focused = gtk_window_get_focus(GTK_WINDOW(mainwindow));
	gtk_window_set_focus(GTK_WINDOW(mainwindow), treeview);
	
	if(NULL != (selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)))) {
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(feedstore), &iter);
		gtk_tree_view_expand_to_path(GTK_TREE_VIEW(treeview), path);
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview), path, NULL, FALSE, 0.0, 0.0);	
		gtk_tree_selection_select_path(selection, path);
		gtk_tree_path_free(path);
	} else
		g_warning("internal error! could not get feed tree view selection!\n");
	gtk_window_set_focus(GTK_WINDOW(mainwindow), focused);
}

static void on_popup_refresh_selected_cb(nodePtr ptr) {
	feed_schedule_update((feedPtr)ptr, 0);
}

void on_popup_refresh_selected(gpointer callback_data,
						 guint callback_action,
						 GtkWidget *widget) {
	nodePtr ptr = (nodePtr)callback_data;

	if (ptr == NULL) {
		ui_show_error_box(_("You have to select a feed entry"));
		return;
	}
	
	if(download_is_online()) {
		if (IS_FEED(ptr->type))
			feed_schedule_update((feedPtr)ptr, FEED_REQ_PRIORITY_HIGH);
		else
			ui_feedlist_do_for_all(ptr, ACTION_FILTER_FEED, on_popup_refresh_selected_cb);
	} else
		ui_mainwindow_set_status_bar(_("Liferea is in offline mode. No update possible."));
}

void on_popup_mark_as_read(gpointer callback_data,
					  guint callback_action,
					  GtkWidget *widget) {
     /*nodePtr ptr = (nodePtr)callback_data;*/
	
	on_popup_allunread_selected();
}

/*------------------------------------------------------------------------------*/
/* feedlist filter [de]activation callback					*/
/*------------------------------------------------------------------------------*/

void on_filter_feeds_without_unread_headlines_activate(GtkMenuItem *menuitem, gpointer user_data) {
	GtkWidget	*feedview;

	filter_feeds_without_unread_headlines = GTK_CHECK_MENU_ITEM(menuitem)->active;
	feedview = lookup_widget(mainwindow, "feedlist");
	g_assert(feedview != NULL);
	ui_feedlist_set_model(GTK_TREE_VIEW(feedview), feedstore, filter_feeds_without_unread_headlines);
	
	if(filter_feeds_without_unread_headlines) {
		ui_mainwindow_set_status_bar(_("Note: Using the subscriptions filter disables drag & drop"));
		gui_tree_model_filter_refilter(GUI_TREE_MODEL_FILTER(filter));
	}
}

/*------------------------------------------------------------------------------*/
/* delete entry callbacks 							*/
/*------------------------------------------------------------------------------*/

static void ui_feedlist_delete_(nodePtr ptr) {

	if(ptr->type == FST_HELPFEED) {
		ui_show_error_box(_("You cannot delete the help! Disable loading the help by changing the "
						"preferences. You will be able to delete the help folder after "
						"restarting Liferea."));
		return;
	}
	
	if (IS_FEED(ptr->type)) {
		feed_free((feedPtr)ptr);
	} else {
		ui_feedlist_do_for_all(ptr, ACTION_FILTER_CHILDREN | ACTION_FILTER_ANY, ui_feedlist_delete_);
		if(ui_folder_is_empty((folderPtr)ptr))
			folder_free((folderPtr)ptr);
	}
}

static void ui_feedlist_delete_response_cb(GtkDialog *dialog, gint response_id, gpointer user_data) {
	nodePtr ptr = (nodePtr)user_data;
	
	switch(response_id) {
	case GTK_RESPONSE_YES:
		ui_feedlist_delete_(ptr);
	}
	gtk_widget_destroy(GTK_WIDGET(dialog));
}


void ui_feedlist_delete(nodePtr ptr) {
	GtkWidget *dialog;
	gchar *text;
	
	g_assert(ptr != NULL);
	g_assert(ptr->ui_data != NULL);
	g_assert(ptr == ui_feedlist_get_selected());

	if(ptr->type == FST_HELPFEED) {
		ui_show_error_box(_("You cannot delete the help! Disable loading the help by changing the "
						"preferences. You will be able to delete the help folder after "
						"restarting Liferea."));
		return;
	}
	
	if (IS_FOLDER(ptr->type)) {
		ui_mainwindow_set_status_bar(_("Deleting \"%s\""),"Deleting entry", folder_get_title((folderPtr)ptr));
		text = g_strdup_printf(_("Are you sure that you want to delete %s and its contents?"), folder_get_title((folderPtr)ptr));
	} else {
		ui_mainwindow_set_status_bar("%s \"%s\"",_("Deleting entry"), feed_get_title((feedPtr)ptr));
		text = g_strdup_printf(_("Are you sure that you want to delete %s?"), feed_get_title((feedPtr)ptr));
	}

	dialog = gtk_message_dialog_new(GTK_WINDOW(mainwindow),
							  GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
							  GTK_MESSAGE_QUESTION,
							  GTK_BUTTONS_YES_NO,
							  "%s", text);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Deletion Confirmation"));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(mainwindow));

	g_free(text);
	
	gtk_widget_show_all(dialog);

	g_signal_connect (G_OBJECT (dialog), "response",
				   G_CALLBACK (ui_feedlist_delete_response_cb), ptr);
}

void on_popup_delete(gpointer callback_data,
                                             guint callback_action,
                                             GtkWidget *widget) {
	nodePtr ptr = (nodePtr)callback_data;
	
	ui_feedlist_delete(ptr);
	
}

/*------------------------------------------------------------------------------*/
/* property dialog callbacks 							*/
/*------------------------------------------------------------------------------*/

void on_popup_prop_selected(gpointer callback_data,
                                             guint callback_action,
                                             GtkWidget *widget) {
	feedPtr		fp = (feedPtr)callback_data;
	
	if(!fp || !IS_FEED(feed_get_type(fp))) {
		g_message(_("You must select a feed entry"));
		ui_show_error_box(_("You must select a feed entry."));
		return;
	}
	
	if(fp->type == FST_HELPFEED) {
		ui_show_error_box(_("You cannot modify help feeds."));
		return;
	}
	
	/* prop dialog may not yet exist */
	ui_feed_propdialog_new(GTK_WINDOW(mainwindow),fp);
	return;
}

/*------------------------------------------------------------------------------*/
/* new entry dialog callbacks 							*/
/*------------------------------------------------------------------------------*/

void ui_feedlist_new_subscription(const gchar *source, const gchar *filter, gint flags) {
	feedPtr			fp;
	gchar			*tmp;
	int			pos;
	folderPtr		parent;
	
	debug_enter("ui_feedlist_new_subscription");	
	
	fp = feed_new();
	fp->needsCacheSave = TRUE;

	tmp = conf_new_id();
	feed_set_id(fp, tmp);
	g_free(tmp);

	feed_set_source(fp, source);
	feed_set_title(fp, _("New subscription"));
	feed_set_filter(fp, filter);
	parent = ui_feedlist_get_target_folder(&pos);
	feed_set_available(fp, TRUE); /* To prevent the big red X from being next to the new feed */
	ui_folder_add_feed(parent, fp, pos);
	ui_feedlist_update();
	ui_feedlist_select((nodePtr)fp);
	
	feed_schedule_update(fp, flags | FEED_REQ_PRIORITY_HIGH | FEED_REQ_DOWNLOAD_FAVICON);
	
	/*ui_show_error_box(_("The newly created feed's type could not be detected! Please check if the source really points to a resource provided in one of the supported syndication formats"));*/
	
	debug_exit("ui_feedlist_new_subscription");
}

void on_newbtn_clicked(GtkButton *button, gpointer user_data) {	
	GtkWidget	*newdialog;
	
	newdialog = ui_feed_newdialog_new(GTK_WINDOW(mainwindow));
	
	gtk_widget_show(newdialog);
}

/* recursivly calls func for every feed in the feed list */
void ui_feedlist_do_for_all_full(nodePtr ptr, gint filter, gpointer func, gint params, gpointer user_data) {
	GtkTreeIter	childiter;
	gboolean	valid, apply, descend;
	nodePtr		child;
	
	if(NULL == ptr)
		valid = gtk_tree_model_get_iter_root(GTK_TREE_MODEL(feedstore), &childiter);
	else {
		g_assert(ptr->ui_data);
		valid = gtk_tree_model_iter_children(GTK_TREE_MODEL(feedstore), &childiter, &((ui_data*)ptr->ui_data)->row);
	}
	
	while(valid) {
		gtk_tree_model_get(GTK_TREE_MODEL(feedstore), &childiter,
					    FS_PTR, &child, -1);
		/* Must update counter here because the current node may be deleted! */
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(feedstore), &childiter);
		/* If child == NULL, this is an empty node. */
		if (child != NULL) {
			gboolean directory = IS_FEED(child->type) && (((feedPtr)child)->fhp != NULL) && ((feedPtr)child)->fhp->directory;
			apply = (filter & ACTION_FILTER_CHILDREN) ||
				((filter & ACTION_FILTER_FEED) && IS_FEED(child->type) && !directory) ||
				((filter & ACTION_FILTER_DIRECTORY) && IS_FEED(child->type) && directory) ||
				((filter & ACTION_FILTER_FOLDER) && IS_FOLDER(child->type));
			descend = !(filter & ACTION_FILTER_CHILDREN);
			
			if(TRUE == apply) {
				if (params==0)
					((nodeActionFunc)func)(child);
				else 
					((nodeActionDataFunc)func)(child, user_data);
			}
			
			/* if the iter has children and we are descending, iterate over the children. */
			if(descend)
				ui_feedlist_do_for_all_data(child, filter, func, user_data);
		}
	}
}

static void ui_feedlist_check_update_counter(feedPtr fp) {
	GTimeVal	now;
	gint interval;

	g_get_current_time(&now);
	interval = feed_get_update_interval(fp);
	
	if (interval > 0)
		if (fp->lastPoll.tv_sec + interval*60 <= now.tv_sec)
			feed_schedule_update(fp, 0);

	/* And check for favicon updating */
	if (fp->lastFaviconPoll.tv_sec + 30*24*60*60 <= now.tv_sec)
		favicon_download(fp);
}

gboolean ui_feedlist_auto_update(void *data) {
	debug_enter("ui_feedlist_auto_update");

	if(download_is_online()) {
		ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED, (gpointer)ui_feedlist_check_update_counter);
	} else {
		debug0(DEBUG_UPDATE, "no update processing because we are offline!");
	}
	debug_exit("ui_feedlist_auto_update");

	return TRUE;
}
