/**
 * @file ui_search.c everything about searching
 *
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.com>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>

#include "common.h"
#include "feedlist.h"
#include "itemlist.h"
#include "node.h"
#include "rule.h"
#include "vfolder.h"
#include "ui/ui_dialog.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_htmlview.h"
#include "ui/ui_itemlist.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_search.h"
#include "ui/ui_vfolder.h"

static GtkWidget	*searchdialog = NULL;
static GtkWidget 	*searchEngineDialog = NULL;
static nodePtr		searchResult = NULL;

typedef struct searchEngine {
	const gchar	*name;		/**< descriptive name for menu options */
	GCallback	func;		/**< search URI generation function */
} *searchEnginePtr;

static GSList	*searchEngines = NULL;

/*------------------------------------------------------------------------------*/
/* search dialog callbacks							*/
/*------------------------------------------------------------------------------*/

static void
ui_search_destroyed_cb(GtkWidget *widget, void *data)
{
	searchdialog = NULL;
}

void
on_searchbtn_clicked (GtkButton *button, gpointer user_data)
{
	GtkWidget	*searchentry;

	if (!searchdialog) {
		searchdialog = liferea_dialog_new (NULL, "searchdialog");
		g_signal_connect (G_OBJECT (searchdialog), "destroy", G_CALLBACK (ui_search_destroyed_cb), NULL);
	}
	
	searchentry = liferea_dialog_lookup (searchdialog, "searchentry");
	gtk_window_set_focus (GTK_WINDOW (searchdialog), searchentry);
}

void
on_searchentry_activate (GtkEntry *entry, gpointer user_data)
{
	/* do not use passed entry because callback is used from a button too */
	GtkWidget		*searchentry;
	G_CONST_RETURN gchar	*searchstring;
	GString			*buffer;
	vfolderPtr		vfolder;
	
	searchentry = liferea_dialog_lookup (searchdialog, "searchentry");
	searchstring = gtk_entry_get_text (GTK_ENTRY(searchentry));

	/* remove last search */
	ui_itemlist_clear ();
	
	if (searchResult) {
		/* Unload from itemlist (necessary on subsequent loads */
		if (searchResult == itemlist_get_displayed_node ())
			itemlist_unload (FALSE);
			
		node_free (searchResult);
	}

	/* create new search */
	searchResult = node_new ();
	vfolder = vfolder_new (searchResult);
	
	node_set_title (searchResult, searchstring);
	vfolder_add_rule (vfolder, "exact", searchstring, TRUE);

	/* calculate vfolder item set */
	vfolder_refresh (vfolder);

	/* switch to item list view and inform user in HTML view */
	ui_feedlist_select (NULL);
	itemlist_set_view_mode (0);
	itemlist_unload (FALSE);
	itemlist_load (searchResult);

	buffer = g_string_new (NULL);
	htmlview_start_output (buffer, NULL, TRUE, FALSE);
	g_string_append_printf (buffer, "<div class='content'><h2>");
	g_string_append_printf (buffer, ngettext("%d Search Result for \"%s\"", 
	                                         "%d Search Results for \"%s\"",
	                                         searchResult->itemCount),
	                        searchResult->itemCount, searchstring);
	g_string_append_printf (buffer, "</h2><p>");
	g_string_append_printf (buffer, _("The item list now contains all items matching the "
	                                "specified search pattern. If you want to save this search "
	                                "result permanently you can click the \"Search Folder\" button in "
	                                "the search dialog and Liferea will add a search folder to your "
	                                "feed list."));
	g_string_append_printf (buffer, "</p></div>");
	htmlview_finish_output (buffer);
	liferea_htmlview_write (ui_mainwindow_get_active_htmlview (), buffer->str, NULL);
	g_string_free (buffer, TRUE);

	/* enable vfolder add button */	
	gtk_widget_set_sensitive (liferea_dialog_lookup (searchdialog, "vfolderaddbtn"), TRUE);
}

void on_searchentry_changed(GtkEditable *editable, gpointer user_data) {
	gchar *searchtext;
	
	/* just to disable the start search button when search string is empty... */
	searchtext = gtk_editable_get_chars(editable,0,-1);
	gtk_widget_set_sensitive(liferea_dialog_lookup(searchdialog, "searchstartbtn"), searchtext && (0 < strlen(searchtext)));
		
}

void on_newVFolder_clicked(GtkButton *button, gpointer user_data) {
	
	if(searchResult) {
		nodePtr node = searchResult;
		searchResult = NULL;
		node_add_child(NULL, node, 0);
		feedlist_schedule_save();
		ui_feedlist_select(node);
	}
}

void
on_new_vfolder_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	node_type_request_interactive_add (vfolder_get_node_type ());
}

/*------------------------------------------------------------------------------*/
/* search engine support							*/
/*------------------------------------------------------------------------------*/

void on_search_engine_btn_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*keywords, *resultCountButton;
	GtkAdjustment	*resultCount;
	gchar		*searchtext, *searchUri, *uriFmt;
	gboolean	limitSupported;
	
	uriFmt = g_object_get_data(G_OBJECT(searchEngineDialog), "uriFmt");
	limitSupported = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(searchEngineDialog), "limitSupported"));

	keywords = liferea_dialog_lookup(searchEngineDialog, "searchkeywords");
	resultCountButton = liferea_dialog_lookup(searchEngineDialog, "resultcount");
	if((NULL != keywords) && (NULL != resultCountButton)) {
		resultCount = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(resultCountButton));
		searchtext = (gchar *)g_strdup(gtk_entry_get_text(GTK_ENTRY(keywords)));
		searchtext = common_encode_uri_string(searchtext);
		if(limitSupported)
			searchUri = g_strdup_printf(uriFmt, searchtext, (int)gtk_adjustment_get_value(resultCount));
		else
			searchUri = g_strdup_printf(uriFmt, searchtext);

		node_request_automatic_add(searchUri, 
					   NULL, 
					   NULL, 
					   NULL,
		                           /*FEED_REQ_SHOW_PROPDIALOG | <- not needed*/
		                           FEED_REQ_RESET_TITLE |
		                           FEED_REQ_RESET_UPDATE_INT | 
		                           FEED_REQ_AUTO_DISCOVER | 
					   FEED_REQ_PRIORITY_HIGH |
					   FEED_REQ_DOWNLOAD_FAVICON |
					   FEED_REQ_AUTH_DIALOG);

		g_free(searchUri);
		g_free(searchtext);
	}
}

static void ui_search_engine_dialog_destroyed_cb(GtkWidget *widget, void *data) {

	searchEngineDialog = NULL;
}

void ui_search_engine_new_feed(const gchar *uriFmt, gboolean limitSupported) {
	GtkWidget	*keywords;
	
	if (!searchEngineDialog) {
		searchEngineDialog = liferea_dialog_new (NULL, "searchenginedialog");
		g_signal_connect (G_OBJECT (searchEngineDialog), "destroy", G_CALLBACK (ui_search_engine_dialog_destroyed_cb), NULL);
	}
		
	keywords = liferea_dialog_lookup(searchEngineDialog, "searchkeywords");
	gtk_window_set_focus(GTK_WINDOW(searchEngineDialog), keywords);
	gtk_entry_set_text(GTK_ENTRY(keywords), "");
	gtk_widget_show(searchEngineDialog);
	
	gtk_widget_set_sensitive(liferea_dialog_lookup(searchEngineDialog, "resultcount"), limitSupported);
	
	g_object_set_data(G_OBJECT(searchEngineDialog), "uriFmt", (gpointer)uriFmt);
	g_object_set_data(G_OBJECT(searchEngineDialog), "limitSupported", GINT_TO_POINTER(limitSupported));
}

void ui_search_engines_setup_menu(GtkUIManager *ui_manager) {
	GtkActionEntry	*entry, *entries = NULL;
	GSList		*iter = searchEngines;
	GError		*error = NULL;
	GString		*uiDesc;

	uiDesc = g_string_new(NULL);

	entries = g_new(GtkActionEntry, g_slist_length(searchEngines));
	entry = entries;
	while(iter) {
		searchEnginePtr searchEngine = (searchEnginePtr)iter->data;
		entry->name = searchEngine->name;
		entry->stock_id = NULL;
		entry->label = searchEngine->name;
		entry->accelerator = NULL;
		entry->tooltip = _("Create a new search feed.");
		entry->callback = searchEngine->func;		
		g_string_append_printf(uiDesc, "<menuitem action='%s'/>", searchEngine->name);
		entry++;
		iter = g_slist_next(iter);
	}	


	g_string_prepend(uiDesc, "<ui>"
	                         "<menubar name='MainwindowMenubar'>"
				 "<menu action='SearchMenu'>"
				 "<menu action='CreateEngineSearch'>");
	g_string_append(uiDesc, "</menu>"
	                        "</menu>"
	                        "</menubar>"
				"</ui>");

	if(gtk_ui_manager_add_ui_from_string(ui_manager, uiDesc->str, -1, &error)) {
		GtkActionGroup *ag = gtk_action_group_new("SearchEngineActions");
		gtk_action_group_set_translation_domain(ag, PACKAGE);
		gtk_action_group_add_actions(ag, entries, g_slist_length(searchEngines), NULL);
		gtk_ui_manager_insert_action_group (ui_manager, ag, 0);
	} else {
		g_warning("building search engine menus failed: %s", error->message);
		g_error_free(error);
	}
	g_string_free(uiDesc, TRUE);
	g_free(entries);
}

static void create_delicious_search_url(void) {
	ui_search_engine_new_feed("http://del.icio.us/rss/tag/%s", FALSE);
}
static void create_feedster_search_url(void) {
	ui_search_engine_new_feed("http://www.feedster.com/search.php?q=%s&sort=date&type=rss&ie=UTF-8&limit=%d", TRUE);
}
static void create_google_blog_search_url(void) {
	ui_search_engine_new_feed("http://blogsearch.google.com/blogsearch_feeds?hl=en&q=%s&ie=utf-8&num=%d&output=atom", TRUE);
}
static void create_icerocket_search_url(void) {
	ui_search_engine_new_feed("http://www.icerocket.com/search?tab=blog&q=%s&rss=1", FALSE);
} 
static void create_reddit_search_url(void) {
	ui_search_engine_new_feed("http://reddit.com/search.rss?q=%s", FALSE);
}
static void create_technorati_search_url(void) {
	ui_search_engine_new_feed("http://feeds.technorati.com/feed/posts/tag/%s", FALSE);
}
static void create_yahoo_search_url(void) {
	ui_search_engine_new_feed("http://api.search.yahoo.com/WebSearchService/rss/webSearch.xml?appid=yahoosearchwebrss&query=%s&adult_ok=1", FALSE);
}

static void search_engine_register(const gchar *name, GCallback func) {
	struct searchEngine *searchEngine;
	
	searchEngine = (struct searchEngine *)g_new0(struct searchEngine, 1);
	searchEngine->name = name;
	searchEngine->func = func;
	searchEngines = g_slist_append(searchEngines, searchEngine);	
}

void ui_search_init(void) {
	search_engine_register("Del.icio.us",	create_delicious_search_url);
	search_engine_register("Feedster",	create_feedster_search_url);
	search_engine_register("Google Blog",	create_google_blog_search_url);
	search_engine_register("Ice Rocket",	create_icerocket_search_url);
	search_engine_register("Reddit.com",	create_reddit_search_url);
	search_engine_register("Technorati",	create_technorati_search_url);
	search_engine_register("Yahoo",		create_yahoo_search_url);
}
