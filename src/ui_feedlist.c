/*
   GUI feed list handling
   
   Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/

#include <gtk/gtk.h>
#include "support.h"
#include "interface.h"
#include "callbacks.h"
#include "feed.h"
#include "folder.h"
#include "ui_feedlist.h"
#include "ui_tray.h"
#include "htmlview.h"
/* possible selected new dialog feed types */
static gint selectableTypes[] = {	FST_AUTODETECT,
					FST_RSS,
					FST_CDF,
					FST_PIE,
					FST_OCS,
					FST_OPML
				};
				
#define MAX_TYPE_SELECT	6

extern GHashTable	*folders; // FIXME!
extern GtkWidget	*mainwindow;
extern GdkPixbuf	*icons[];

static GtkTreeStore	*feedstore = NULL;

GtkWidget		*filedialog = NULL;
static GtkWidget	*newdialog = NULL;
static GtkWidget	*propdialog = NULL;

/* feedPtr which points to the last selected (and in most cases actually selected) feed */
feedPtr	selected_fp = NULL;

/* contains the type of the actually selected feedlist entry */
gint selected_type = FST_INVALID;

/* pointer to the selected directory key prefix, needed when creating new feeds */
gchar	*selected_keyprefix = NULL;

GtkTreeStore * getFeedStore(void) {

	if(NULL == feedstore) {
		/* set up a store of these attributes: 
			- feed title
			- feed state icon (not available/available)
			- feed key in gconf
			- feed list view type (node/rss/cdf/pie/ocs...)
		 */
		feedstore = gtk_tree_store_new(4, G_TYPE_STRING, 
						  GDK_TYPE_PIXBUF, 
						  G_TYPE_STRING,
						  G_TYPE_INT);
	}

	g_assert(NULL != feedstore);
	return feedstore;
}

/* sets the selected_* variables which indicate the selected feed list
   entry which can either be a feed or a directory 
 
   to set the selection info for a feed: fp must be specified and
                                         keyprefix can be NULL
					 
   to set the selection info for a folder: fp has to be NULL and
                                           keyprefix has to be given				 
 */
static void setSelectedFeed(feedPtr fp, gchar *keyprefix) {

	if(NULL != (selected_fp = fp)) {
		/* we select a feed */
		selected_type = getFeedType(fp);;
		g_free(selected_keyprefix);
		if(NULL == keyprefix)
			selected_keyprefix = g_strdup(getFeedKeyPrefix(fp));
		else
			selected_keyprefix = g_strdup(keyprefix);
	} else {
		/* we select a folder */
		if(0 == strcmp(keyprefix, "empty"))
			selected_type = FST_EMPTY;
		else
			selected_type = FST_NODE;

		g_free(selected_keyprefix);
		selected_keyprefix = g_strdup(keyprefix);
	}
}

/* Calls setSelectedFeed to fill the global select_* variables
   with the selection information. The function returns TRUE
   if the selected feed/folder could be determined. */
gboolean getFeedListIter(GtkTreeIter *iter) {
	GtkWidget		*treeview;
	GtkTreeSelection	*select;
        GtkTreeModel		*model;
	gchar			*tmp_key;
	gint			tmp_type;
	
	if(NULL == mainwindow)
		return FALSE;
	
	if(NULL == (treeview = lookup_widget(mainwindow, "feedlist"))) {
		g_warning(_("entry list widget lookup failed!\n"));
		return FALSE;
	}
		
	if(NULL == (select = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)))) {
		print_status(g_strdup(_("could not retrieve selection of entry list!")));
		return FALSE;
	}

        if(gtk_tree_selection_get_selected(select, &model, iter)) {
		gtk_tree_model_get(model, iter, 
				   FS_KEY, &tmp_key, 
 				   FS_TYPE, &tmp_type,
				   -1);

		if(IS_NODE(tmp_type)) {
			/* its a folder */
			setSelectedFeed(NULL, tmp_key);
		} else {
			/* its a feed */
			if(NULL == tmp_key) {
				g_warning(_("fatal! selected feed entry has no key!!!"));
				return FALSE;
			}
			setSelectedFeed(getFeed(tmp_key), NULL);
		}
	
		return TRUE;
	}
	
	return FALSE;	
}

static void renderFeedTitle(GtkTreeViewColumn *tree_column,
	             GtkCellRenderer   *cell,
	             GtkTreeModel      *model,
        	     GtkTreeIter       *iter,
	             gpointer           data)
{
	gint		type, ctype;
	gchar		*key, *ckey, *title, *tmp;
	feedPtr		fp;
	int		count;
	GtkTreeIter	child;
	gboolean	rc;

	gtk_tree_model_get(model, iter, FS_TYPE, &type,
					FS_TITLE, &title,
	                                FS_KEY, &key, -1);	
	
	if(!IS_NODE(type) && (type != FST_EMPTY)) {
		g_assert(NULL != key);
		fp = getFeed(key);
		count = getFeedUnreadCount(fp);

		if(NULL != fp->parseErrors)
			g_object_set(GTK_CELL_RENDERER(cell), "foreground", "red", NULL);
		else
			g_object_set(GTK_CELL_RENDERER(cell), "foreground", "black", NULL);
		
		if(count > 0) {
			/* g_markup_escape_text() does prevent resolving of entities like &apos;
			   tmp = g_strdup_printf("%s (%d)", g_markup_escape_text(getFeedTitle(fp), -1), count); */
			   tmp = g_strdup_printf("%s (%d)", getFeedTitle(fp), count);
			g_object_set(GTK_CELL_RENDERER(cell), "font", "bold", NULL);
			g_object_set(GTK_CELL_RENDERER(cell), "text", tmp, NULL);
			g_free(tmp);
		} else {
			g_object_set(GTK_CELL_RENDERER(cell), "font", "normal", NULL);
			/* g_object_set(GTK_CELL_RENDERER(cell), "text", g_markup_escape_text(getFeedTitle(fp), -1), NULL); */
			g_object_set(GTK_CELL_RENDERER(cell), "text", getFeedTitle(fp), NULL);
		}
	} else if(IS_NODE(type)) {
		/* its a folder so count unread items of all feeds inside */
		count = 0;		
		rc = gtk_tree_model_iter_children(model, &child, iter);
		while (rc) {
			gtk_tree_model_get(model, &child, FS_KEY, &ckey, FS_TYPE, &ctype, -1);
			if(FST_EMPTY != ctype) {
				g_assert(NULL != ckey);
				fp = getFeed(ckey);
				g_free(ckey);
				count += getFeedUnreadCount(fp);
			}
			rc = gtk_tree_model_iter_next(model, &child);
		}

		g_object_set(GTK_CELL_RENDERER(cell), "foreground", "black", NULL);
		if (count>0) {
			/* tmp = g_strdup_printf("%s (%d)", g_markup_escape_text(title, -1), count); */
			tmp = g_strdup_printf("%s (%d)", title, count);
			g_object_set(GTK_CELL_RENDERER(cell), "font", "bold", NULL);
			g_object_set(GTK_CELL_RENDERER(cell), "text", tmp, NULL);
			g_free(tmp);
		} else {
			g_object_set(GTK_CELL_RENDERER(cell), "font", "normal", NULL);
			/* g_object_set(GTK_CELL_RENDERER(cell), "text", g_markup_escape_text(title, -1), NULL); */
			g_object_set(GTK_CELL_RENDERER(cell), "text", title, NULL);
		}
	}

	g_free(title);
	g_free(key);
}

static void renderFeedStatus(GtkTreeViewColumn *tree_column,
	             GtkCellRenderer   *cell,
	             GtkTreeModel      *model,
        	     GtkTreeIter       *iter,
	             gpointer           data)
{
	gchar		*tmp_key;
	gint		type, icon;
	feedPtr		fp;
	gpointer	favicon;
	
	gtk_tree_model_get(model, iter,	FS_KEY, &tmp_key, 
					FS_TYPE, &type, -1);

	if(IS_FEED(type)) {
		g_assert(NULL != tmp_key);
		fp = getFeed(tmp_key);
	}
	g_free(tmp_key);
	
	switch(type) {
		case FST_HELPNODE:
			icon = ICON_HELP;
			break;
		case FST_NODE:
			icon = ICON_FOLDER;
			break;
		case FST_EMPTY:
			icon = ICON_EMPTY;
			break;
		case FST_VFOLDER:
			icon = ICON_VFOLDER;
			break;
		case FST_OPML:
		case FST_OCS:
			if(getFeedAvailable(fp))
				icon = ICON_OCS;
			else
				icon = ICON_UNAVAILABLE;
			break;
		case FST_HELPFEED:
		case FST_PIE:
		case FST_RSS:			
		case FST_CDF:
			if(getFeedAvailable(fp)) {
				if(NULL != (favicon = getFeedIcon(fp))) {
					g_object_set(GTK_CELL_RENDERER(cell), "pixbuf", favicon, NULL);
					return;
				} else
					icon = ICON_AVAILABLE;
			} else
				icon = ICON_UNAVAILABLE;
			break;
		default:
			g_print(_("internal error! unknown entry type! cannot display appropriate icon!\n"));
			return;
			break;
	}	
	g_object_set(GTK_CELL_RENDERER(cell), "pixbuf", icons[icon], NULL);
}

/* set up the entry list store and connects it to the entry list
   view in the main window */
void setupFeedList(GtkWidget *mainview) {
	GtkCellRenderer		*textRenderer;
	GtkCellRenderer		*iconRenderer;	
	GtkTreeViewColumn 	*column;
	GtkTreeSelection	*select;	
	GtkTreeStore		*feedstore;
	
	g_assert(mainwindow != NULL);
		
	feedstore = getFeedStore();

	gtk_tree_view_set_model(GTK_TREE_VIEW(mainview), GTK_TREE_MODEL(feedstore));
	
	/* we only render the state and title */
	iconRenderer = gtk_cell_renderer_pixbuf_new();
	textRenderer = gtk_cell_renderer_text_new();

	column = gtk_tree_view_column_new();
	
	gtk_tree_view_column_pack_start(column, iconRenderer, FALSE);
	gtk_tree_view_column_pack_start(column, textRenderer, TRUE);
	
	gtk_tree_view_column_set_attributes(column, iconRenderer, "pixbuf", FS_STATE, NULL);
	gtk_tree_view_column_set_attributes(column, textRenderer, "text", FS_TITLE, NULL);
	
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mainview), column);

	gtk_tree_view_column_set_cell_data_func(column, iconRenderer, 
					   renderFeedStatus, NULL, NULL);
					   
	gtk_tree_view_column_set_cell_data_func(column, textRenderer,
                                           renderFeedTitle, NULL, NULL);			   
		
	/* Setup the selection handler for the main view */
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(mainview));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(select), "changed",
                 	 G_CALLBACK(feedlist_selection_changed_cb),
                	 lookup_widget(mainwindow, "feedlist"));
			
}

void feedlist_selection_changed_cb(GtkTreeSelection *selection, gpointer data) {
	GtkTreeIter		iter;
        GtkTreeModel		*model;
	gchar			*tmp_key;
	gint			tmp_type;
	feedPtr			fp;
	GdkGeometry		geometry;

	undoTrayIcon();
	
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		gtk_tree_model_get(model, &iter, 
					    FS_KEY, &tmp_key,
					    FS_TYPE, &tmp_type,
					    -1);
		
		g_assert(NULL != tmp_key);
		/* make sure thats no grouping iterator */
		if(!IS_NODE(tmp_type) && (FST_EMPTY != tmp_type)) {
			fp = getFeed(tmp_key);				
			
			/* FIXME: another workaround to prevent strange window
			   size increasings after feed selection changing */
			geometry.min_height=480;
			geometry.min_width=640;
			g_assert(mainwindow != NULL);
			gtk_window_set_geometry_hints(GTK_WINDOW(mainwindow), mainwindow, &geometry, GDK_HINT_MIN_SIZE);
	
			/* save new selection infos */
			setSelectedFeed(fp, NULL);		
			loadItemList(fp, NULL);
		} else {
			/* save new selection infos */
			setSelectedFeed(NULL, tmp_key);
		}
		g_free(tmp_key);
	}
}

void addToFeedList(feedPtr fp, gboolean startup) {
	GtkTreeSelection	*selection;
	GtkWidget		*treeview;
	GtkTreePath		*path;
	GtkTreeIter		selected_iter;
	GtkTreeIter		iter;
	GtkTreeIter		*topiter;
	
	g_assert(NULL != fp);
	g_assert(NULL != getFeedKey(fp));
	g_assert(NULL != getFeedKeyPrefix(fp));
	g_assert(NULL != feedstore);
	g_assert(NULL != folders);
	
	// FIXME: maybe this should not happen here?
	topiter = (GtkTreeIter *)g_hash_table_lookup(folders, (gpointer)(getFeedKeyPrefix(fp)));
	if(!startup) {
		/* used when creating feed entries manually */
		if(getFeedListIter(&selected_iter) && IS_FEED(selected_type))
			/* if a feed entry is marked after which we shall insert */
			gtk_tree_store_insert_after(feedstore, &iter, topiter, &selected_iter);
		else
			/* if no feed entry is marked (e.g. on empty folders) */		
			gtk_tree_store_prepend(feedstore, &iter, topiter);

	} else {
		/* typically on startup when adding feeds from configuration */
		gtk_tree_store_append(feedstore, &iter, topiter);
	}

	gtk_tree_store_set(feedstore, &iter,
			   FS_TITLE, getFeedTitle(fp),
			   FS_KEY, getFeedKey(fp),
			   FS_TYPE, getFeedType(fp),
			   -1);

	if(!startup) {			
		/* some comfort: select the created iter */
		if(NULL != (treeview = lookup_widget(mainwindow, "feedlist"))) {
			if(NULL != (selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview))))
				gtk_tree_selection_select_iter(selection, &iter);
			path = gtk_tree_model_get_path(GTK_TREE_MODEL(feedstore), &iter);
			gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview), path, NULL, FALSE, 0.0, 0.0);	
			gtk_tree_path_free(path);
		} else {
			print_status(g_strdup(_("internal error! could not select newly created treestore iter!")));
		}
	}
}

void on_popup_refresh_selected(void) { 

	if(!FEED_MENU(selected_type)) {
		showErrorBox(_("You have to select a feed entry!"));
		return;
	}

	updateFeed(selected_fp);
}

/*------------------------------------------------------------------------------*/
/* delete entry callbacks 							*/
/*------------------------------------------------------------------------------*/

void on_popup_delete_selected(void) {
	GtkTreeIter	iter;

	/* block deleting of empty entries */
	if(!FEED_MENU(selected_type)) {
		showErrorBox(_("You have to select a feed entry!"));
		return;
	}

	/* block deleting of help feeds */
	if(0 == strncmp(getFeedKey(selected_fp), "help", 4)) {
		showErrorBox(_("You can't delete help feeds!"));
		return;
	}
	
	print_status(g_strdup_printf("%s \"%s\"",_("Deleting entry"), getFeedTitle(selected_fp)));
	if(getFeedListIter(&iter)) {
		gtk_tree_store_remove(feedstore, &iter);
		removeFeed(selected_fp);
		setSelectedFeed(NULL, "");

		clearItemList();
		clearHTMLView();
		checkForEmptyFolders();
	} else {
		showErrorBox(_("It seems like there is no selected feed entry!"));
	}
}

/*------------------------------------------------------------------------------*/
/* property dialog callbacks 							*/
/*------------------------------------------------------------------------------*/

static void build_prop_dialog(void) {

	if(NULL == propdialog || !G_IS_OBJECT(propdialog))
		propdialog = create_propdialog();

	if(NULL == propdialog)
		return;	
}

void on_popup_prop_selected(void) {
	feedPtr		fp = selected_fp;
	GtkWidget 	*feednameentry, *feedurlentry, *updateIntervalBtn;
	GtkAdjustment	*updateInterval;
	gint		defaultInterval;
	gchar		*defaultIntervalStr;

	if(!FEED_MENU(selected_type)) {
		showErrorBox(_("You have to select a feed entry!"));
		return;
	}
	
	/* block changing of help feeds */
	if(0 == strncmp(getFeedKey(fp), "help", strlen("help"))) {
		showErrorBox("You can't modify help feeds!");
		return;
	}

	/* prop dialog may not yet exist */
	build_prop_dialog();
		
	feednameentry = lookup_widget(propdialog, "feednameentry");
	feedurlentry = lookup_widget(propdialog, "feedurlentry");
	updateIntervalBtn = lookup_widget(propdialog, "feedrefreshcount");
	updateInterval = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(updateIntervalBtn));
	
	gtk_entry_set_text(GTK_ENTRY(feednameentry), getFeedTitle(fp));
	gtk_entry_set_text(GTK_ENTRY(feedurlentry), getFeedSource(fp));

	if(IS_DIRECTORY(getFeedType(fp))) {	
		/* disable the update interval selector for directories (should this be the case for OPML?) */
		gtk_widget_set_sensitive(lookup_widget(propdialog, "feedrefreshcount"), FALSE);
	} else {
		/* enable and adjust values otherwise */
		gtk_widget_set_sensitive(lookup_widget(propdialog, "feedrefreshcount"), TRUE);	
		
		gtk_adjustment_set_value(updateInterval, getFeedUpdateInterval(fp));

		defaultInterval = getFeedDefaultInterval(fp);
		if(-1 != defaultInterval)
			defaultIntervalStr = g_strdup_printf(_("The provider of this feed suggests an update interval of %d minutes"), defaultInterval);
		else
			defaultIntervalStr = g_strdup(_("This feed specifies no default update interval."));
		gtk_label_set_text(GTK_LABEL(lookup_widget(propdialog, "feedupdateinfo")), defaultIntervalStr);
		g_free(defaultIntervalStr);		
	}

	gtk_widget_show(propdialog);
}

void on_propchangebtn_clicked(GtkButton *button, gpointer user_data) {
	gchar		*feedurl, *feedname;
	GtkWidget 	*feedurlentry;
	GtkWidget 	*feednameentry;
	GtkWidget 	*updateIntervalBtn;
	GtkAdjustment	*updateInterval;
	gint		interval;
	feedPtr		fp = selected_fp;
	
	g_assert(NULL != propdialog);
		
	if(NULL != fp) {
		feednameentry = lookup_widget(propdialog, "feednameentry");
		feedurlentry = lookup_widget(propdialog, "feedurlentry");

		feedurl = (gchar *)gtk_entry_get_text(GTK_ENTRY(feedurlentry));
		feedname = (gchar *)gtk_entry_get_text(GTK_ENTRY(feednameentry));
	
		setFeedTitle(fp, g_strdup(feedname));  

		/* if URL has changed... */
		if(strcmp(feedurl, getFeedSource(fp))) {
			setFeedSource(fp, g_strdup(feedurl));
			updateFeed(fp);
		}
		
		if(IS_FEED(getFeedType(fp))) {
			updateIntervalBtn = lookup_widget(propdialog, "feedrefreshcount");
			updateInterval = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(updateIntervalBtn));

			interval = gtk_adjustment_get_value(updateInterval);
			
			if(0 == interval)
				interval = -1;	/* this is due to ignore this feed while updating */
			setFeedUpdateInterval(fp, interval);
			setFeedUpdateCounter(fp, interval);
		}
		
		redrawFeedList();
	} else {
		g_warning(_("Internal error! No feed selected, but property change requested...\n"));
	}
}

/*------------------------------------------------------------------------------*/
/* new entry dialog callbacks 							*/
/*------------------------------------------------------------------------------*/

void subscribeTo(gint type, gchar *source, gchar * keyprefix, gboolean showPropDialog) {
	GtkWidget	*updateIntervalBtn;
	feedPtr		fp;
	gint		interval;
	gchar		*tmp;

	if(NULL != (fp = newFeed(type, source, keyprefix))) {
		addToFeedList(fp, FALSE);	/* add feed */
		checkForEmptyFolders();		/* remove empty entry if necessary */
		saveFolderFeedList(keyprefix);	/* save new folder contents order */
		if(FALSE == getFeedAvailable(fp)) {
			tmp = g_strdup_printf(_("Could not download \"%s\"!\n\n Maybe the URL is invalid or the feed is temporarily not available. You can retry downloading or remove the feed subscription via the context menu from the feed list.\n"), source);
			showErrorBox(tmp);
			g_free(tmp);
		} else {
			if(TRUE == showPropDialog) {
				/* prop dialog may not yet exist */
				build_prop_dialog();
				
				if(-1 != (interval = getFeedDefaultInterval(fp))) {
					updateIntervalBtn = lookup_widget(propdialog, "feedrefreshcount");
					gtk_spin_button_set_value(GTK_SPIN_BUTTON(updateIntervalBtn), (gfloat)interval);
				}

				on_popup_prop_selected();		/* prepare prop dialog */
			}
		}
	}
}

void on_newbtn_clicked(GtkButton *button, gpointer user_data) {	
	GtkWidget 	*sourceentry;	
	
	if(NULL == newdialog || !G_IS_OBJECT(newdialog)) 
		newdialog = create_newdialog();
		
	if(NULL == propdialog || !G_IS_OBJECT(propdialog))
		propdialog = create_propdialog();

	sourceentry = lookup_widget(newdialog, "newfeedentry");
	gtk_entry_set_text(GTK_ENTRY(sourceentry), "");

	g_assert(NULL != newdialog);
	g_assert(NULL != propdialog);
	gtk_widget_show(newdialog);
}

void on_newfeedbtn_clicked(GtkButton *button, gpointer user_data) {
	gchar		*source;
	GtkWidget 	*sourceentry;	
	GtkWidget 	*titleentry, *typeoptionmenu;
	gint		type;
	
	g_assert(newdialog != NULL);
	g_assert(propdialog != NULL);

	sourceentry = lookup_widget(newdialog, "newfeedentry");
	titleentry = lookup_widget(propdialog, "feednameentry");
	typeoptionmenu = lookup_widget(newdialog, "typeoptionmenu");
		
	g_assert(NULL != selected_keyprefix);
	source = g_strdup(gtk_entry_get_text(GTK_ENTRY(sourceentry)));
	type = gtk_option_menu_get_history(GTK_OPTION_MENU(typeoptionmenu));
	
	/* the retrieved number is not yet the real feed type! */
	if(type > MAX_TYPE_SELECT) {
			g_error(_("internal error! invalid type selected! This should never happen!\n"));
			return;
	} else
		type = selectableTypes[type];

	/* It is possible, that there is no selected folder when we are
	   called from the menu! In this case we default to the root folder */
	if(NULL != selected_keyprefix) 
		subscribeTo(type, source, g_strdup(selected_keyprefix), TRUE);	
	else
		subscribeTo(type, source, g_strdup(""), TRUE);	
	/* don't free source for it is reused by newFeed! */
}

void on_localfileselect_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*source;
	
	gtk_widget_hide(filedialog);
	g_assert(NULL != newdialog);
	if(NULL != (source = lookup_widget(newdialog, "newfeedentry")))
		gtk_entry_set_text(GTK_ENTRY(source), gtk_file_selection_get_filename(GTK_FILE_SELECTION(filedialog)));
}

void on_localfilebtn_pressed(GtkButton *button, gpointer user_data) {
	GtkWidget	*okbutton;
	
	if(NULL == filedialog || !G_IS_OBJECT(filedialog))
		filedialog = create_fileselection();
		
	if(NULL == (okbutton = lookup_widget(filedialog, "fileselectbtn")))
		g_error(_("internal error! could not find file dialog select button!"));

	g_signal_connect((gpointer) okbutton, "clicked", G_CALLBACK (on_localfileselect_clicked), NULL);
	gtk_widget_show(filedialog);
}

/*------------------------------------------------------------------------------*/
/* function for finding next unread item 					*/
/*------------------------------------------------------------------------------*/

feedPtr ui_feed_find_unread(GtkTreeIter *iter) {
	GtkTreeSelection	*selection;
	GtkTreePath		*path;
	GtkWidget		*treeview;
	GtkTreeIter		childiter;
	gboolean		valid;
	feedPtr			fp;
	gchar			*tmp_key;
	gint			tmp_type;
	
	if(NULL == iter)
		valid = gtk_tree_model_get_iter_root(GTK_TREE_MODEL(feedstore), &childiter);
	else
		valid = gtk_tree_model_iter_children(GTK_TREE_MODEL(feedstore), &childiter, iter);
		
	while(valid) {
               	gtk_tree_model_get(GTK_TREE_MODEL(feedstore), &childiter, FS_KEY, &tmp_key, FS_TYPE, &tmp_type, -1);

		if(IS_FEED(tmp_type)) {
			g_assert(tmp_key != NULL);

			fp = getFeed(tmp_key);
			g_assert(fp != NULL);

			if(getFeedUnreadCount(fp) > 0) {
				/* select the feed entry... */
				if(NULL != (treeview = lookup_widget(mainwindow, "feedlist"))) {
					if(NULL != (selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)))) {
						/* expand tree to iter and select it ... */
						path = gtk_tree_model_get_path(GTK_TREE_MODEL(feedstore), &childiter);
						gtk_tree_view_expand_to_path(GTK_TREE_VIEW(treeview), path);
						gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview), path, NULL, FALSE, FALSE, FALSE);
						gtk_tree_path_free(path);
						gtk_tree_selection_select_iter(selection, &childiter);
					} else
						g_warning(_("internal error! could not get feed tree view selection!\n"));
				} else {
					g_warning(_("internal error! could not find feed tree view widget!\n"));
				}			

				return fp;
			}		
		} else {
			/* must be a folder, so recursivly go down... */
			if(NULL != (fp = ui_feed_find_unread(&childiter)))
				return fp;
		}
		
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(feedstore), &childiter);
	}

	return NULL;	
}

void ui_feed_mark_items_as_unread(GtkTreeIter *iter) {
	GtkTreeModel	*model;
	gint		tmp_type;
	gchar		*tmp_key;
	feedPtr		fp;
	
	model = GTK_TREE_MODEL(getFeedStore());
	g_assert(NULL != model);
	
	gtk_tree_model_get(model, iter, 
			   FS_KEY, &tmp_key,
			   FS_TYPE, &tmp_type,
			   -1);	
	if(!IS_NODE(tmp_type)) {
		fp = getFeed(tmp_key);
		g_assert(NULL != fp);
		markAllItemsAsRead(fp);
	}
	g_free(tmp_key);
}
