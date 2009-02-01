/**
 * @file ui_search.c  search engine handling
 *
 * Copyright (C) 2003-2008 Lars Lindner <lars.lindner@gmail.com>
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
 
#include "ui/ui_search.h"

#include "common.h"
#include "feedlist.h"
#include "itemlist.h"
#include "node.h"
#include "vfolder.h"
#include "ui/liferea_dialog.h"
#include "ui/search_engine_dialog.h"

static GtkWidget	*searchdialog = NULL;
static GtkWidget 	*searchEngineDialog = NULL;
static nodePtr		searchResult = NULL;

typedef struct searchEngine {
	const gchar	*name;		/**< descriptive name for menu options */
	GCallback	func;		/**< search URI generation function */
} *searchEnginePtr;

static GSList	*searchEngines = NULL;

void
on_new_vfolder_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	node_type_request_interactive_add (vfolder_get_node_type ());
}

/*------------------------------------------------------------------------------*/
/* search engine support							*/
/*------------------------------------------------------------------------------*/

static void
create_delicious_search_url (void)
{
	search_engine_dialog_new ("http://del.icio.us/rss/tag/%s", FALSE);
}

static void
create_feedster_search_url (void)
{
	search_engine_dialog_new ("http://www.feedster.com/search.php?q=%s&sort=date&type=rss&ie=UTF-8&limit=%d", TRUE);
}

static void
create_google_blog_search_url (void)
{
	search_engine_dialog_new ("http://blogsearch.google.com/blogsearch_feeds?hl=en&q=%s&ie=utf-8&num=%d&output=atom", TRUE);
}

static void
create_icerocket_search_url (void)
{
	search_engine_dialog_new ("http://www.icerocket.com/search?tab=blog&q=%s&rss=1", FALSE);
} 

static void
create_reddit_search_url (void)
{
	search_engine_dialog_new ("http://reddit.com/search.rss?q=%s", FALSE);
}

static void
create_technorati_search_url (void)
{
	search_engine_dialog_new ("http://feeds.technorati.com/feed/posts/tag/%s", FALSE);
}

static void
create_yahoo_search_url (void)
{
	search_engine_dialog_new ("http://api.search.yahoo.com/WebSearchService/rss/webSearch.xml?appid=yahoosearchwebrss&query=%s&adult_ok=1", FALSE);
}

static void
search_engine_register (const gchar *name, GCallback func)
{
	struct searchEngine *searchEngine;

	searchEngine = (struct searchEngine *)g_new0 (struct searchEngine, 1);
	searchEngine->name = name;
	searchEngine->func = func;
	searchEngines = g_slist_append (searchEngines, searchEngine);	
}

static void
search_engines_init (void)
{
	search_engine_register ("Del.icio.us",	create_delicious_search_url);
	search_engine_register ("Feedster",	create_feedster_search_url);
	search_engine_register ("Google Blog",	create_google_blog_search_url);
	search_engine_register ("Ice Rocket",	create_icerocket_search_url);
	search_engine_register ("Reddit.com",	create_reddit_search_url);
	search_engine_register ("Technorati",	create_technorati_search_url);
	search_engine_register ("Yahoo",	create_yahoo_search_url);
}

void
ui_search_engines_setup_menu (GtkUIManager *ui_manager)
{
	GtkActionEntry	*entry, *entries = NULL;
	GSList		*iter;
	GError		*error = NULL;
	GString		*uiDesc;
	
	if (!searchEngines)
		search_engines_init ();

	uiDesc = g_string_new (NULL);

	entries = g_new (GtkActionEntry, g_slist_length (searchEngines));
	entry = entries;
	iter = searchEngines;
	while (iter) {
		searchEnginePtr searchEngine = (searchEnginePtr)iter->data;
		entry->name = searchEngine->name;
		entry->stock_id = NULL;
		entry->label = searchEngine->name;
		entry->accelerator = NULL;
		entry->tooltip = _("Create a new search feed.");
		entry->callback = searchEngine->func;	
		g_string_append_printf (uiDesc, "<menuitem action='%s'/>", searchEngine->name);
		entry++;
		iter = g_slist_next (iter);
	}	


	g_string_prepend (uiDesc, "<ui>"
	                          "<menubar name='MainwindowMenubar'>"
				  "<menu action='SearchMenu'>"
				  "<menu action='CreateEngineSearch'>");
	g_string_append (uiDesc, "</menu>"
	                         "</menu>"
	                         "</menubar>"
				 "</ui>");

	if (gtk_ui_manager_add_ui_from_string (ui_manager, uiDesc->str, -1, &error)) {
		GtkActionGroup *ag = gtk_action_group_new ("SearchEngineActions");
		gtk_action_group_set_translation_domain (ag, PACKAGE);
		gtk_action_group_add_actions (ag, entries, g_slist_length (searchEngines), NULL);
		gtk_ui_manager_insert_action_group (ui_manager, ag, 0);
	} else {
		g_warning ("building search engine menus failed: %s", error->message);
		g_error_free (error);
	}
	g_string_free (uiDesc, TRUE);
	g_free (entries);
}
