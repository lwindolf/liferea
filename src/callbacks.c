/*
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "backend.h"
#include "conf.h"
// FIXME: the following should not be necessary!
#include "rss_channel.h"
#include "rss_item.h"

#include "cdf_channel.h"
#include "cdf_item.h"

GtkWidget	*mainwindow;
GtkWidget	*newdialog = NULL;
GtkWidget	*feednamedialog = NULL;
GtkWidget	*propdialog = NULL;
GtkWidget	*prefdialog = NULL;
GtkWidget	*newfolderdialog = NULL;

extern GdkPixbuf	*unreadIcon;
extern GdkPixbuf	*readIcon;
extern GdkPixbuf	*directoryIcon;
extern GdkPixbuf	*listIcon;
extern GdkPixbuf	*availableIcon;
extern GdkPixbuf	*unavailableIcon;

extern GThread	*updateThread;

static gint	itemlist_loading = 0;	/* freaky workaround */
static gchar	*new_key;		/* used by new feed dialog */


/*------------------------------------------------------------------------------*/
/* helper functions								*/
/*------------------------------------------------------------------------------*/

static gchar * getEntryViewSelection(GtkWidget *feedlist) {
	GtkTreeSelection	*select;
        GtkTreeModel		*model;
	GtkTreeIter		iter;	
	gchar			*feedkey = NULL;
		
	if(NULL == feedlist) {
		/* this is possible for the feed list editor window */
		return NULL;
	}
			
	if(NULL == (select = gtk_tree_view_get_selection(GTK_TREE_VIEW(feedlist)))) {
		print_status(_("could not retrieve selection of feed list!"));
		return NULL;
	}

        if(gtk_tree_selection_get_selected (select, &model, &iter))
                gtk_tree_model_get(model, &iter, FS_KEY, &feedkey, -1);
	else {
		return NULL;
	}
	
	return feedkey;
}

gchar * getMainFeedListViewSelection(void) {
	GtkWidget	*feedlistview;
	
	if(NULL == mainwindow)
		return NULL;
	
	if(NULL == (feedlistview = lookup_widget(mainwindow, "feedlist"))) {
		g_warning(_("feed list widget lookup failed!\n"));
		return NULL;
	}
	
	return getEntryViewSelection(feedlistview);
}

gchar * getEntryViewSelectionPrefix(GtkWidget *window) {
	GtkWidget		*treeview;
	GtkTreeSelection	*select;
        GtkTreeModel		*model;
	GtkTreeIter		iter;
	GtkTreeIter		topiter;	
	gchar			*keyprefix;
	gboolean		valid;
	
	if(NULL == window)
		return NULL;
	
	if(NULL == (treeview = lookup_widget(window, "feedlist"))) {
		g_warning(_("entry list widget lookup failed!\n"));
		return NULL;
	}
		
	if(NULL == (select = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)))) {
		print_status(_("could not retrieve selection of entry list!"));
		return NULL;
	}

        if(gtk_tree_selection_get_selected (select, &model, &iter)) {
		
		/* the selected iter is usually not the directory iter... */
		if(0 != gtk_tree_store_iter_depth(GTK_TREE_STORE(model), &iter)) {
		
			/* scan through top level iterators till we find
			   the correct ancestor */
			valid = gtk_tree_model_get_iter_first(model, &topiter);
			while(valid) {	
				if(gtk_tree_store_is_ancestor(GTK_TREE_STORE(model), &topiter, &iter)) {
			                gtk_tree_model_get(model, &topiter, FS_KEY, &keyprefix, -1);
					return keyprefix;
				}
				valid = gtk_tree_model_iter_next(model, &topiter);
			}
		}
                gtk_tree_model_get(model, &iter, FS_KEY, &keyprefix, -1);
		
	} else {
		return NULL;
	}
	
	return keyprefix;
}

gint getEntryViewSelectionType(GtkWidget *window) {
	GtkWidget		*treeview;
	GtkTreeSelection	*select;
        GtkTreeModel		*model;
	GtkTreeIter		iter;	
	gint			type;
	
	if(NULL == window)
		return FST_INVALID;
	
	if(NULL == (treeview = lookup_widget(window, "feedlist"))) {
		g_warning(_("entry list widget lookup failed!\n"));
		return FST_INVALID;
	}
		
	if(NULL == (select = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)))) {
		print_status(_("could not retrieve selection of entry list!"));
		return FST_INVALID;
	}

        if(gtk_tree_selection_get_selected (select, &model, &iter))
                gtk_tree_model_get(model, &iter, FS_TYPE, &type, -1);
	else {
		return FST_INVALID;
	}
	
	return type;
}

GtkTreeIter * getEntryViewSelectionIter(GtkWidget *window) {
	GtkTreeIter		*iter;
	GtkWidget		*treeview;
	GtkTreeSelection	*select;
        GtkTreeModel		*model;
	
	if(NULL == (iter = (GtkTreeIter *)g_malloc(sizeof(GtkTreeIter)))) 
		g_error("could not allocate memory!\n");
	
	if(NULL == window)
		return NULL;
	
	if(NULL == (treeview = lookup_widget(window, "feedlist"))) {
		g_warning(_("entry list widget lookup failed!\n"));
		return NULL;
	}
		
	if(NULL == (select = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)))) {
		print_status(_("could not retrieve selection of entry list!"));
		return NULL;
	}

        gtk_tree_selection_get_selected(select, &model, iter);
	
	return iter;
}

static void selectFeedViewItem(GtkWidget *window, gchar *viewname, gchar *feedkey) {
	GtkWidget		*view;
	GtkTreeSelection	*select;
        GtkTreeModel		*model;
	GtkTreeIter		iter;
	GtkTreeStore		*feedstore;	
	gboolean		valid;
	gchar			*tmp_key;
	gint			tmp_type;
		
	if(NULL == window) {
		/* this is possible for the feed list editor window */
		return;	
	}
	
	if(NULL == (view = lookup_widget(window, viewname))) {
		g_warning(_("feed list widget lookup failed!\n"));
		return;
	}
		
	if(NULL == (select = gtk_tree_view_get_selection(GTK_TREE_VIEW(view)))) {
		g_warning(_("could not retrieve selection of feed list!\n"));
		return;
	}

	feedstore = getEntryStore();
	valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(feedstore), &iter);
	while(valid) {
		gtk_tree_model_get(GTK_TREE_MODEL(feedstore), &iter, 
		                   FS_KEY, &tmp_key,
				   FS_TYPE, &tmp_type,
				   -1);
		
		// FIXME: this should not be feed specific (OCS support!)
		if(IS_FEED(tmp_type)) {
			g_assert(NULL != tmp_key);
			if(0 == strcmp(tmp_key, feedkey)) {
				gtk_tree_selection_select_iter(select, &iter);
				break;
			}
		}

		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(feedstore), &iter);
	}
}

void redrawFeedList(void) {
	GtkWidget	*list;
	
	if(NULL == mainwindow)
		return;
	
	list = lookup_widget(mainwindow, "feedlist");
	if(NULL != list)  {
		gtk_widget_queue_draw(list);
	}
}

void redrawItemList(void) {
	GtkWidget	*list;
	
	if(NULL == mainwindow)
		return;
	
	list = lookup_widget(mainwindow, "Itemlist");
	if(NULL != list)  {
		gtk_widget_queue_draw(list);
	}
}


/*------------------------------------------------------------------------------*/
/* callbacks 									*/
/*------------------------------------------------------------------------------*/

void on_refreshbtn_clicked(GtkButton *button, gpointer user_data) {

	resetAllUpdateCounters();
	updateNow();	
}


void on_popup_refresh_selected(void) {
	gchar	*feedkey;
	
	if(NULL != (feedkey = getEntryViewSelection(lookup_widget(mainwindow, "feedlist"))))
		updateEntry(feedkey);
}

void on_prefbtn_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*browsercmdentry, 
			*timeformatentry,
			*proxyhostentry,
			*proxyportentry,
			*useproxycheck,
			*dccheck,
			*contentcheck,
			*slashcheck,
			*fmcheck;
				
	if(NULL == prefdialog || !G_IS_OBJECT(prefdialog))
		prefdialog = create_prefdialog ();		

	// FIXME: bad solution, design a generic pref interface!
	browsercmdentry = lookup_widget(prefdialog, "browsercmd");
	timeformatentry = lookup_widget(prefdialog, "timeformat");
	proxyhostentry = lookup_widget(prefdialog, "proxyhost");
	proxyportentry = lookup_widget(prefdialog, "proxyport");
	useproxycheck =  lookup_widget(prefdialog, "useproxy");
	dccheck =  lookup_widget(prefdialog, "usedc");
	contentcheck =  lookup_widget(prefdialog, "usecontent");
	slashcheck =  lookup_widget(prefdialog, "useslash");
	fmcheck =  lookup_widget(prefdialog, "usefm");	
		
	gtk_entry_set_text(GTK_ENTRY(browsercmdentry), getStringConfValue(BROWSER_COMMAND));
	gtk_entry_set_text(GTK_ENTRY(timeformatentry), getStringConfValue(TIME_FORMAT));
	gtk_entry_set_text(GTK_ENTRY(proxyhostentry), getStringConfValue(PROXY_HOST));
	gtk_entry_set_text(GTK_ENTRY(proxyportentry), g_strdup_printf("%d", getNumericConfValue(PROXY_PORT)));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(useproxycheck), getBooleanConfValue(USE_PROXY));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dccheck), getBooleanConfValue(USE_DC));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(contentcheck), getBooleanConfValue(USE_CONTENT));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(slashcheck), getBooleanConfValue(USE_SLASH));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fmcheck), getBooleanConfValue(USE_FM));	
				
	gtk_widget_show(prefdialog);
}

void on_prefsavebtn_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*browsercmdentry, 
			*timeformatentry,
			*proxyhostentry,
			*proxyportentry,
			*useproxycheck,
			*dccheck,
			*contentcheck,
			*slashcheck,
			*fmcheck;

	// FIXME: bad solution, design a generic pref interface!
	browsercmdentry = lookup_widget(prefdialog, "browsercmd");
	timeformatentry = lookup_widget(prefdialog, "timeformat");
	proxyhostentry = lookup_widget(prefdialog, "proxyhost");
	proxyportentry = lookup_widget(prefdialog, "proxyport");
	useproxycheck =  lookup_widget(prefdialog, "useproxy");
	dccheck =  lookup_widget(prefdialog, "usedc");
	contentcheck =  lookup_widget(prefdialog, "usecontent");
	slashcheck =  lookup_widget(prefdialog, "useslash");
	fmcheck =  lookup_widget(prefdialog, "usefm");	
		
	setStringConfValue(BROWSER_COMMAND,
			   (gchar *)gtk_entry_get_text(GTK_ENTRY(browsercmdentry)));
	setStringConfValue(TIME_FORMAT,
			   (gchar *)gtk_entry_get_text(GTK_ENTRY(timeformatentry)));
	setStringConfValue(PROXY_HOST,
			   (gchar *)gtk_entry_get_text(GTK_ENTRY(proxyhostentry)));
	setNumericConfValue(PROXY_PORT,
			   atoi(gtk_entry_get_text(GTK_ENTRY(proxyportentry))));
	setBooleanConfValue(USE_PROXY,
			   (gboolean)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(useproxycheck)));
	setBooleanConfValue(USE_DC,
			   (gboolean)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dccheck)));
	setBooleanConfValue(USE_CONTENT,
			   (gboolean)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(contentcheck)));
	setBooleanConfValue(USE_SLASH,
			   (gboolean)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(slashcheck)));
	setBooleanConfValue(USE_FM,
			   (gboolean)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(fmcheck)));			   
	/* reinitialize */
	loadConfig();
	initBackend();
			
	gtk_widget_hide(prefdialog);
}

void on_deletebtn(GtkWidget *feedlist) {
	GtkTreeIter	*iter;
	gchar		*keyprefix;
	gchar		*key;

	/* user_data has to contain the feed list widget reference */
	key = getEntryViewSelection(feedlist);
	keyprefix = getEntryViewSelectionPrefix(mainwindow);
	iter = getEntryViewSelectionIter(mainwindow);

	/* make sure thats no grouping iterator */
	if(NULL != key) {
		print_status(g_strdup_printf("%s \"%s\"",_("Deleting entry"), getDefaultEntryTitle(key)));
		removeEntry(keyprefix, key);
		gtk_tree_store_remove(getEntryStore(), iter);
		g_free(key);
		g_free(iter);
				
		clearItemList();
	} else {
		print_status(_("Error: Cannot delete this list entry!"));
	}
}

void on_popup_delete_selected(void) {
	on_deletebtn(lookup_widget(mainwindow, "feedlist"));
}

void on_deletebtn_clicked(GtkButton *button, gpointer user_data) {
//	on_deletebtn(lookup_widget(feedlistdialog, "feedlist"));
}

void on_flupbtn_clicked(GtkButton *button, gpointer user_data) {
//	gchar	*key;
//	gchar	*keyprefix;
	
	/* user_data has to contain the entry list widget reference */
//	key = getEntryViewSelection(GTK_WIDGET(lookup_widget(feedlistdialog, "feedlist")));
//	keyprefix = getEntryViewSelectionPrefix(feedlistdialog);
		
	/* make sure thats no grouping iterator */
//	if(NULL != key) {
//		print_status(g_strdup_printf("%s \"%s\"",_("Moving feed up"), getDefaultEntryTitle(key))); // FIXME: g_free...
//		moveUpEntryPosition(keyprefix, key);
//	} else {
//		print_status(_("Error: Cannot move up this list entry!"));
//        }
}



void on_fldownbtn_clicked(GtkButton *button, gpointer user_data) {
//	gchar	*key;
//	gchar	*keyprefix;

	/* user_data has to contain the feed list widget reference */
//	key = getEntryViewSelection(GTK_WIDGET(lookup_widget(feedlistdialog, "feedlist")));	
//	keyprefix = getEntryViewSelectionPrefix(feedlistdialog);
	
	/* make sure thats no grouping iterator */
//	if(NULL != key) {
//		print_status(g_strdup_printf("%s \"%s\"",_("Moving feed down"), getDefaultEntryTitle(key))); // FIXME: g_free...
//		moveDownEntryPosition(keyprefix, key);
//	} else {
//		print_status(_("Error: Cannot move down this list entry!"));
//	}
}

void on_flsortbtn_clicked(GtkButton *button, gpointer user_data) {

	print_status(_("Sorting not yet implemented!\n"));
//	sortEntries();
}

void on_propbtn(GtkWidget *feedlist) {
	gint		type;
	gchar		*feedkey;
	GtkWidget 	*feednameentry;
	GtkWidget 	*feedurlentry;
	GtkWidget 	*updateIntervalBtn;
	GtkAdjustment	*updateInterval;

	/* user_data has to contain the feed list widget reference */
	if(NULL == (feedkey = getEntryViewSelection(feedlist))) {
		print_status(_("Internal Error! Feed pointer NULL!\n"));
		return;
	}
	type = getEntryViewSelectionType(mainwindow);
	
	if(NULL == propdialog || !G_IS_OBJECT(propdialog))
		propdialog = create_propdialog();

	feednameentry = lookup_widget(propdialog, "feednameentry");
	feedurlentry = lookup_widget(propdialog, "feedurlentry");
	updateIntervalBtn = lookup_widget(propdialog, "feedrefreshcount");
	updateInterval = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(updateIntervalBtn));

	gtk_entry_set_text(GTK_ENTRY(feednameentry), getDefaultEntryTitle(feedkey));
	gtk_entry_set_text(GTK_ENTRY(feedurlentry), getEntrySource(feedkey));
	gtk_adjustment_set_value(updateInterval, getFeedUpdateInterval(feedkey));

	if(FST_OCS == type) {
		/* disable the update interval selector for OCS feeds */
		gtk_widget_set_sensitive(lookup_widget(propdialog, "feedrefreshcount"), FALSE);
	} else {
		/* enable it otherwise */
		gtk_widget_set_sensitive(lookup_widget(propdialog, "feedrefreshcount"), TRUE);	
	}
	
	/* note: the OK buttons signal is connected on the fly
	   to pass the correct dialog widget that feedlist was
	   clicked... */

	g_signal_connect((gpointer)lookup_widget(propdialog, "propchangebtn"), 
			 "clicked", G_CALLBACK (on_propchangebtn_clicked), feedlist);
   
	   
	gtk_widget_show(propdialog);
}

void on_propchangebtn_clicked(GtkButton *button, gpointer user_data) {
	gchar		*feedkey;
	GtkWidget 	*feedurlentry;
	GtkWidget 	*feednameentry;
	GtkWidget 	*updateIntervalBtn;
	GtkAdjustment	*updateInterval;
	gint		interval;
	gint		type;

	type = getEntryViewSelectionType(GTK_WIDGET(user_data));
	if(NULL != (feedkey = getEntryViewSelection(GTK_WIDGET(user_data)))) {
		feednameentry = lookup_widget(propdialog, "feednameentry");
		feedurlentry = lookup_widget(propdialog, "feedurlentry");
		updateIntervalBtn = lookup_widget(propdialog, "feedrefreshcount");
		updateInterval = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(updateIntervalBtn));

		gchar *feedurl = (gchar *)gtk_entry_get_text(GTK_ENTRY(feedurlentry));
		gchar *feedname = (gchar *)gtk_entry_get_text(GTK_ENTRY(feednameentry));
		interval = gtk_adjustment_get_value(updateInterval);
	
		setEntryTitle(feedkey, feedname);    
		setEntrySource(feedkey, feedurl);
		if(IS_FEED(type)) 
			setFeedUpdateInterval(feedkey, interval);

//		selectFeedViewItem(feedlistdialog, "feedlist", feedkey);
		selectFeedViewItem(mainwindow, "feedlist", feedkey);
	}
}

void on_popup_prop_selected(void) {

	g_assert(NULL != mainwindow);
	on_propbtn(GTK_WIDGET(lookup_widget(mainwindow, "feedlist")));
}

void on_propbtn_clicked(GtkButton *button, gpointer user_data) {

//	g_assert(NULL != feedlistdialog);
//	on_propbtn(GTK_WIDGET(lookup_widget(feedlistdialog, "feedlist")));
}

/*------------------------------------------------------------------------------*/
/* new entry dialog callbacks 							*/
/*------------------------------------------------------------------------------*/

void on_newbtn_clicked(GtkButton *button, gpointer user_data) {	
	GtkWidget *feedurlentry;
	GtkWidget *feednameentry;

	if(NULL == newdialog || !G_IS_OBJECT(newdialog)) 
		newdialog = create_newdialog();

	if(NULL == feednamedialog || !G_IS_OBJECT(feednamedialog)) 
		feednamedialog = create_feednamedialog();

	/* always clear the edit field */
	feedurlentry = lookup_widget(newdialog, "newfeedentry");
	gtk_entry_set_text(GTK_ENTRY(feedurlentry), "");	

	gtk_widget_show(newdialog);
}

void on_newfeedbtn_clicked(GtkButton *button, gpointer user_data) {
	gchar		*key;
	gchar		*keyprefix;
	gchar		*source;
	GtkWidget 	*sourceentry;	
	GtkWidget 	*titleentry;
	GtkWidget	*feedradiobtn;
	GtkWidget	*ocsradiobtn;	
	GtkWidget	*cdfradiobtn;		
	gint		type = FST_FEED;
	
	sourceentry = lookup_widget(newdialog, "newfeedentry");
	titleentry = lookup_widget(feednamedialog, "feednameentry");
	feedradiobtn = lookup_widget(newdialog, "typeradiobtn");
	cdfradiobtn = lookup_widget(newdialog, "typeradiobtn1");	
	ocsradiobtn = lookup_widget(newdialog, "typeradiobtn2");

	source = (gchar *)gtk_entry_get_text(GTK_ENTRY(sourceentry));
	keyprefix = getEntryViewSelectionPrefix(mainwindow);
		
	/* FIXME: make this more generic! */
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(feedradiobtn))) {
		type = FST_FEED;
	}

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cdfradiobtn))) {
		type = FST_CDF;
	}	

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ocsradiobtn))) {
		type = FST_OCS;
		/* disable the update interval selector */
		gtk_widget_set_sensitive(lookup_widget(feednamedialog, "feedrefreshcount"), FALSE);
	}

	if(NULL != (keyprefix = getEntryViewSelectionPrefix(mainwindow))) {

		if(NULL != (key = newEntry(type, source, keyprefix))) {

			gtk_entry_set_text(GTK_ENTRY(titleentry), getDefaultEntryTitle(key));
			new_key = key;

			gtk_widget_show(feednamedialog);
		}
	} else {
		print_status("could not get entry key prefix! maybe you did not select a group");
	}
	
	/* don't free source/keyprefix for they are reused by newEntry! */
}

void on_feednamebutton_clicked(GtkButton *button, gpointer user_data) {	
	GtkWidget	*feednameentry;
	GtkWidget 	*updateIntervalBtn;
	GtkAdjustment	*updateInterval;
	gint		interval;
	
	gchar		*feedurl;
	gchar		*feedname;

	feednameentry = lookup_widget(feednamedialog, "feednameentry");
	updateIntervalBtn = lookup_widget(feednamedialog, "feedrefreshcount");
	updateInterval = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(updateIntervalBtn));

	/* this button may be disabled, because we added an OCS directory
	   enable it for the next new dialog usage... */
	gtk_widget_set_sensitive(GTK_WIDGET(updateIntervalBtn), TRUE);
	
	feedname = (gchar *)gtk_entry_get_text(GTK_ENTRY(feednameentry));
	interval = gtk_adjustment_get_value(updateInterval);
	
	setEntryTitle(new_key, feedname);
	if(IS_FEED(getEntryType(new_key)))
		setFeedUpdateInterval(new_key, interval);
		
	new_key = NULL;
}

/*------------------------------------------------------------------------------*/
/* new/remove folder dialog callbacks 						*/
/*------------------------------------------------------------------------------*/

void on_popup_newfolder_selected(void) {
	if(NULL == newfolderdialog || !G_IS_OBJECT(newfolderdialog))
		newfolderdialog = create_newfolderdialog();
		
	gtk_widget_show(newfolderdialog);
}

void on_newfolderbtn_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget	*foldertitleentry;
	gchar		*folderkey, *foldertitle;
	
	foldertitleentry = lookup_widget(newfolderdialog, "foldertitleentry");
	foldertitle = (gchar *)gtk_entry_get_text(GTK_ENTRY(foldertitleentry));
	if(NULL != (folderkey = addFolderToConfig(foldertitle))) {
		/* add the new folder to the model */
		addFolder(folderkey, foldertitle);
	} else {
		print_status(_("internal error! could not get a new folder key!"));
	}	
}

void on_popup_removefolder_selected(void) {
	GtkTreeStore	*entrystore;
	GtkTreeIter	*iter;
	gchar		*keyprefix;
	
	keyprefix = getEntryViewSelectionPrefix(mainwindow);
	iter = getEntryViewSelectionIter(mainwindow);
	entrystore = getEntryStore();
	
	g_assert(entrystore != NULL);
	
	/* make sure thats no grouping iterator */
	if(NULL != keyprefix) {
		/* check if folder is empty */
		if(FALSE == gtk_tree_model_iter_has_child(GTK_TREE_MODEL(entrystore), iter)) {
			gtk_tree_store_remove(entrystore, iter);
			removeFolder(keyprefix);
			g_free(keyprefix);
		} else {
			showErrorBox(_("A folder must be empty to delete it!"));
		}
	} else {
		print_status(_("Error: Cannot determine folder key!"));
	}
	g_free(iter);	
}

void on_popup_allunread_selected(void) {
	GtkTreeModel 	*model;
	GtkTreeIter	iter;
	gpointer	tmp_ip;
	gint		tmp_type;
	gboolean 	valid;

	model = GTK_TREE_MODEL(getItemStore());
	valid = gtk_tree_model_get_iter_first(model, &iter);

	while(valid) {
               	gtk_tree_model_get (model, &iter, IS_PTR, &tmp_ip,
		                                  IS_TYPE, &tmp_type, -1);
		g_assert(tmp_ip != NULL);
		markItemAsRead(tmp_type, tmp_ip);

		valid = gtk_tree_model_iter_next(model, &iter);
	}

	/* redraw feed list to update unread items count */
	redrawFeedList();

	/* necessary to rerender the formerly bold text labels */
	redrawItemList();	
}

/*------------------------------------------------------------------------------*/
/* help callback								*/
/*------------------------------------------------------------------------------*/

void
on_helpbtn_clicked                     (GtkButton       *button,
                                        gpointer         user_data)
{	gchar	*key;

	if(NULL != (key = getHelpFeedKey())) {
		clearItemList();	
		loadItemList(key, NULL);
	} else
		showErrorBox(_("Error: Could not load help feed!"));
}

/*------------------------------------------------------------------------------*/
/* search callbacks								*/
/*------------------------------------------------------------------------------*/

/* called when toolbar search button is clicked */
void on_searchbtn_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*searchbox;
	gboolean	visible;

	if(NULL != (searchbox = lookup_widget(mainwindow, "searchbox"))) {
		g_object_get(searchbox, "visible", &visible, NULL);
		g_object_set(searchbox, "visible", !visible, NULL);
	}
}

/* called when close button in search dialog is clicked */
void on_hidesearch_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*searchbox;

	if(NULL != (searchbox = lookup_widget(mainwindow, "searchbox"))) {
		g_object_set(searchbox, "visible", FALSE, NULL);
	}
}


void on_searchentry_activate(GtkEntry *entry, gpointer user_data) {
	GtkWidget		*searchentry;
	G_CONST_RETURN gchar	*searchstring;

	if(NULL != (searchentry = lookup_widget(mainwindow, "searchentry"))) {
		searchstring = gtk_entry_get_text(GTK_ENTRY(searchentry));
		print_status(g_strdup_printf(_("searching for \"%s\""), searchstring));
		searchItems(searchstring);
	}
}

/*------------------------------------------------------------------------------*/
/* selection change callbacks							*/
/*------------------------------------------------------------------------------*/

void feedlist_selection_changed_cb(GtkTreeSelection *selection, gpointer data) {
	GtkWidget		*itemlist;
	GtkTreeSelection	*itemselection;
	GtkTreeIter		iter;
        GtkTreeModel		*model;
	gchar			*tmp_key;
	gint			tmp_type;
	
        if (gtk_tree_selection_get_selected (selection, &model, &iter))
        {
                gtk_tree_model_get (model, &iter, 
				FS_KEY, &tmp_key,
				FS_TYPE, &tmp_type,
				-1);
				
		/* make sure thats no grouping iterator */
		if(FST_NODE != tmp_type) {
			g_assert(NULL != tmp_key);

			clearItemList();
			loadItemList(tmp_key, NULL);

			/* this flag disables the selection changed handler of
			   the itemlist view and so prevents that the item which
			   is marked when you clicked into the itemlist is set to
			   read status... */
			//itemlist_loading = 1;

			/* preselect first item	*/
/*			if(NULL == (itemlist = lookup_widget(mainwindow, "Itemlist"))) {
				g_warning(_("item list widget lookup failed!\n"));
				return;
			}

			if(NULL == (itemselection = gtk_tree_view_get_selection(GTK_TREE_VIEW(itemlist)))) {
				g_warning(_("could not retrieve selection of item list!\n"));
				return;
			}

			if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(getItemStore()), &iter))
				gtk_tree_selection_select_iter(itemselection, &iter);
*/
			g_free(tmp_key);
		}		
       	}
}

void itemlist_selection_changed_cb (GtkTreeSelection *selection, gpointer data) {
	GtkTreeView	*itemlist;
	GtkTreeIter	iter;
        GtkTreeModel	*model;
	gpointer	tmp_ip;
	gint		tmp_type;
	gchar		*feedkey;

       	if(gtk_tree_selection_get_selected(selection, &model, &iter)) {

               	gtk_tree_model_get (model, &iter, IS_PTR, &tmp_ip,
		                                  IS_TYPE, &tmp_type, -1);

		g_assert(tmp_ip != NULL);

		if((0 == itemlist_loading)) {
			if(NULL != (itemlist = GTK_TREE_VIEW(lookup_widget(mainwindow, "Itemlist")))) {
				loadItem(tmp_type, tmp_ip);
			
				/* redraw feed list to update unread items numbers */
				redrawFeedList();
			}
		}
       	}
	itemlist_loading = 0;		
}

/*------------------------------------------------------------------------------*/
/* treeview creation and rendering						*/
/*------------------------------------------------------------------------------*/

static void renderEntryTitle(GtkTreeViewColumn *tree_column,
	             GtkCellRenderer   *cell,
	             GtkTreeModel      *model,
        	     GtkTreeIter       *iter,
	             gpointer           data)
{
	gint		tmp_type;
	gchar		*tmp_key;
	int		count;
	gboolean	unspecific = TRUE;

	gtk_tree_model_get(model, iter, FS_TYPE, &tmp_type, 
	                                FS_KEY, &tmp_key, -1);
	
	if(IS_FEED(tmp_type)) {
		g_assert(NULL != tmp_key);	
		switch(tmp_type) {
			case FST_FEED:
				count = getRSSFeedUnreadCount(tmp_key);
				break;
			case FST_CDF:
				count = getCDFFeedUnreadCount(tmp_key);
				break;
			case FST_OCS:
				count = 0;
				break;				
			default:
				g_print("internal error: unknown type!\n");
				break;
		}
		   
		if(count > 0) {
			unspecific = FALSE;
			g_object_set(GTK_CELL_RENDERER(cell), "font", "bold", NULL);
			g_object_set(GTK_CELL_RENDERER(cell), "text", g_strdup_printf("%s (%d)", getDefaultEntryTitle(tmp_key), count), NULL);
		}
	}
	g_free(tmp_key);
	
	if(unspecific)
		g_object_set(GTK_CELL_RENDERER(cell), "font", "normal", NULL);
}


static void renderEntryStatus(GtkTreeViewColumn *tree_column,
	             GtkCellRenderer   *cell,
	             GtkTreeModel      *model,
        	     GtkTreeIter       *iter,
	             gpointer           data)
{
	GdkPixbuf	*tmp_state;
	gchar		*tmp_key;
	gint		tmp_type;
	
	gtk_tree_model_get(model, iter, FS_TYPE, &tmp_type, 
					FS_KEY, &tmp_key,
	                                FS_STATE, &tmp_state, -1);

	switch(tmp_type) {
		case FST_NODE:
			g_object_set(GTK_CELL_RENDERER(cell), "pixbuf", directoryIcon, NULL);
			break;
		case FST_OCS:
			if(getEntryStatus(tmp_key))	
				g_object_set(GTK_CELL_RENDERER(cell), "pixbuf", listIcon, NULL);
			else
				g_object_set(GTK_CELL_RENDERER(cell), "pixbuf", unavailableIcon, NULL);
			break;
		case FST_FEED:			
		case FST_CDF:
			if(getEntryStatus(tmp_key))
				g_object_set(GTK_CELL_RENDERER(cell), "pixbuf", availableIcon, NULL);
			else
				g_object_set(GTK_CELL_RENDERER(cell), "pixbuf", unavailableIcon, NULL);
			break;
		default:
			g_print(_("internal error! unknown entry type! cannot display appropriate icon!\n"));
			break;
			
	}
	
	g_free(tmp_key);
}

/* set up the entry list store and connects it to the entry list
   view in the main window */
void setupEntryList(GtkWidget *mainview) {
	GtkCellRenderer		*textRenderer;
	GtkCellRenderer		*iconRenderer;	
	GtkTreeViewColumn 	*column;
	GtkTreeSelection	*select;	
	GtkTreeStore		*entrystore;
	
	entrystore = getEntryStore();

	gtk_tree_view_set_model(GTK_TREE_VIEW(mainview), GTK_TREE_MODEL(entrystore));
	
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

	gtk_tree_view_column_set_cell_data_func (column, iconRenderer, 
					   renderEntryStatus, NULL, NULL);
					   
	gtk_tree_view_column_set_cell_data_func (column, textRenderer,
                                           renderEntryTitle, NULL, NULL);			   
		
	/* Setup the selection handler for the main view */
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(mainview));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(select), "changed",
                 	 G_CALLBACK(feedlist_selection_changed_cb),
                	 lookup_widget(mainwindow, "feedlist"));
}

static void renderItemTitle(GtkTreeViewColumn *tree_column,
	             GtkCellRenderer   *cell,
	             GtkTreeModel      *model,
        	     GtkTreeIter       *iter,
	             gpointer           data)
{
	gpointer	*tmp_ip;
	gint		tmp_type;
	gboolean 	result;

	gtk_tree_model_get(model, iter, IS_PTR, &tmp_ip,
	                                IS_TYPE, &tmp_type, -1);
	
	switch(tmp_type) {
		case FST_FEED:
			result = getRSSItemReadStatus(tmp_ip);
			break;
		case FST_CDF:
			result = getCDFItemReadStatus(tmp_ip);
			break;
		case FST_OCS:
			result = TRUE;
			break;			
		default:
			g_print("internal error! unknown type\n");
			break;
	}
	
	if(FALSE == result) {
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
	gpointer	tmp_ip;
	gint		tmp_type;
	gboolean	result = TRUE;

	gtk_tree_model_get(model, iter, IS_PTR, &tmp_ip,
	                                IS_TYPE, &tmp_type, -1);
					
	switch(tmp_type) {
		case FST_FEED:
			result = getRSSItemReadStatus(tmp_ip);
			break;
		case FST_CDF:
			result = getCDFItemReadStatus(tmp_ip);
			break;
		case FST_OCS:
			result = TRUE;
			break;
		default:
			g_print("internal error! unknown type\n");
			break;
	}
		
	if(FALSE == result) {
		g_object_set(GTK_CELL_RENDERER(cell), "pixbuf", unreadIcon, NULL);
	} else {
		g_object_set(GTK_CELL_RENDERER(cell), "pixbuf", readIcon, NULL);
	}
}

void setupItemList(GtkWidget *itemlist) {
	GtkCellRenderer		*renderer;
	GtkTreeViewColumn 	*column;
	GtkTreeSelection	*select;
	GtkTreeStore		*itemstore;	
	
	itemstore = getItemStore();

	gtk_tree_view_set_model(GTK_TREE_VIEW(itemlist), GTK_TREE_MODEL(itemstore));

	/* we only render the state, title and time */
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes("", renderer, "pixbuf", IS_STATE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(itemlist), column);
	/*gtk_tree_view_column_set_sort_column_id(column, IS_STATE); ...leads to segfaults on tab-bing through */
	gtk_tree_view_column_set_cell_data_func (column, renderer, renderItemStatus, NULL, NULL);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Time"), renderer, "text", IS_TIME, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(itemlist), column);
	gtk_tree_view_column_set_sort_column_id(column, IS_TIME);
						
	renderer = gtk_cell_renderer_text_new();						   	
	column = gtk_tree_view_column_new_with_attributes(_("Title"), renderer, "text", IS_TITLE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(itemlist), column);
	gtk_tree_view_column_set_sort_column_id(column, IS_TITLE);
	gtk_tree_view_column_set_cell_data_func (column, renderer, renderItemTitle, NULL, NULL);

	/* Setup the selection handler */
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(itemlist));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(select), "changed",
                  G_CALLBACK(itemlist_selection_changed_cb),
                  lookup_widget(mainwindow, "feedlist"));
	
}

/*------------------------------------------------------------------------------*/
/* popup menu callbacks 							*/
/*------------------------------------------------------------------------------*/

static GtkItemFactoryEntry feedentry_menu_items[] = {
      {"/_Update Entry", NULL, on_popup_refresh_selected, 0, NULL},
      {"/_New Entry", 	NULL, on_newbtn_clicked, 0, NULL},
      {"/New _Folder", 	NULL, on_popup_newfolder_selected, 0, NULL},
      {"/_Remove Entry",	NULL, on_popup_delete_selected, 0, NULL},
      {"/_Properties",	NULL, on_popup_prop_selected, 0, NULL}
};

static GtkItemFactoryEntry ocsentry_menu_items[] = {
      {"/_Update Directory",	NULL, on_popup_refresh_selected, 0, NULL},
      {"/_New Entry", 		NULL, on_newbtn_clicked, 0, NULL},
      {"/New _Folder", 		NULL, on_popup_newfolder_selected, 0, NULL},
      {"/_Remove Directory",	NULL, on_popup_delete_selected, 0, NULL},
      {"/_Properties",		NULL, on_popup_prop_selected, 0, NULL}
};

static GtkItemFactoryEntry node_menu_items[] = {
      {"/_New Entry", 	NULL, on_newbtn_clicked, 0, NULL},
      {"/New _Folder", 	NULL, on_popup_newfolder_selected, 0, NULL},
      {"/_Remove Folder", NULL, on_popup_removefolder_selected, 0, NULL}
};

static GtkItemFactoryEntry default_menu_items[] = {
      {"/New _Folder", 	NULL, on_popup_newfolder_selected, 0, NULL}
};

static GtkMenu *make_entry_menu(gint type) {
	GtkWidget 		*menubar;
	GtkItemFactory 		*item_factory;
	gint 			nmenu_items;
	GtkItemFactoryEntry	*menu_items;
	
	switch(type) {
		case FST_NODE:
			menu_items = node_menu_items;
			nmenu_items = sizeof(node_menu_items)/(sizeof(node_menu_items[0]));
			break;	
		case FST_FEED:
		case FST_CDF:
			menu_items = feedentry_menu_items;
			nmenu_items = sizeof(feedentry_menu_items)/(sizeof(feedentry_menu_items[0]));
			break;
		case FST_OCS:
			menu_items = ocsentry_menu_items;
			nmenu_items = sizeof(ocsentry_menu_items)/(sizeof(ocsentry_menu_items[0]));
			break;
		default:
			menu_items = default_menu_items;
			nmenu_items = sizeof(default_menu_items)/(sizeof(default_menu_items[0]));
			break;
	}

	item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<feedentrypopup>", NULL);
	gtk_item_factory_create_items(item_factory, nmenu_items, menu_items, NULL);
	menubar = gtk_item_factory_get_widget(item_factory, "<feedentrypopup>");

	return GTK_MENU(menubar);
}


static GtkItemFactoryEntry item_menu_items[] = {
      {"/_Mark all as unread", 	NULL, on_popup_allunread_selected, 0, NULL}
};

static GtkMenu *make_item_menu(void) {
	GtkWidget 		*menubar;
	GtkItemFactory 		*item_factory;
	gint 			nmenu_items;
	GtkItemFactoryEntry	*menu_items;
	
	menu_items = item_menu_items;
	nmenu_items = sizeof(item_menu_items)/(sizeof(item_menu_items[0]));

	item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<itempopup>", NULL);
	gtk_item_factory_create_items(item_factory, nmenu_items, menu_items, NULL);
	menubar = gtk_item_factory_get_widget(item_factory, "<itempopup>");

	return GTK_MENU(menubar);
}


gboolean on_mainfeedlist_button_press_event(GtkWidget *widget,
					    GdkEventButton *event,
                                            gpointer user_data)
{
	GdkEventButton 	*eb;
	gboolean 	retval;
	gint		type;
  
	if (event->type != GDK_BUTTON_PRESS) return FALSE;
	eb = (GdkEventButton*) event;

	if (eb->button != 3)
		return FALSE;

	// FIXME: don't use existing selection, but determine
	// which selection would result from the right mouse click
	type = getEntryViewSelectionType(mainwindow);
	gtk_menu_popup(make_entry_menu(type), NULL, NULL, NULL, NULL, eb->button, eb->time);
		
	return TRUE;
}

gboolean on_itemlist_button_press_event(GtkWidget *widget,
					    GdkEventButton *event,
                                            gpointer user_data)
{
	GdkEventButton 	*eb;
	gboolean 	retval;
	gint		type;
  
	if (event->type != GDK_BUTTON_PRESS) return FALSE;
	eb = (GdkEventButton*) event;

	if (eb->button != 3)
		return FALSE;

	gtk_menu_popup(make_item_menu(), NULL, NULL, NULL, NULL, eb->button, eb->time);
		
	return TRUE;
}

/*------------------------------------------------------------------------------*/
/* status bar callback 								*/
/*------------------------------------------------------------------------------*/

void print_status(gchar *statustext) {
	GtkWidget *statusbar = lookup_widget(mainwindow, "statusbar");

	g_print("%s\n", statustext);
	
	/* lock handling, because this method is called from main
	   and update thread */
	if(updateThread == g_thread_self())
		gdk_threads_enter();
	
	gtk_label_set_text(GTK_LABEL(GTK_STATUSBAR(statusbar)->label), statustext);
	
	if(updateThread == g_thread_self())
		gdk_threads_leave();
}

void showErrorBox(gchar *msg) {
	GtkWidget	*dialog;
	
	dialog = gtk_message_dialog_new(GTK_WINDOW(mainwindow),
                  GTK_DIALOG_DESTROY_WITH_PARENT,
                  GTK_MESSAGE_ERROR,
                  GTK_BUTTONS_CLOSE,
                  msg);
	 gtk_dialog_run (GTK_DIALOG (dialog));
	 gtk_widget_destroy (dialog);
}
