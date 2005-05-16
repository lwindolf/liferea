/**
 * @file ui_search.c everything about searching
 *
 * Copyright (C) 2003-2005 Lars Lindner <lars.lindner@gmx.net>
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

#include "callbacks.h"
#include "interface.h"
#include "ui_mainwindow.h"
#include "feed.h"
#include "folder.h"
#include "rule.h"
#include "vfolder.h"
#include "support.h"
#include "common.h"

static GtkWidget	*searchdialog = NULL;
static GtkWidget 	*feedsterdialog = NULL;
static feedPtr		searchFeed = NULL;

/*------------------------------------------------------------------------------*/
/* search dialog callbacks							*/
/*------------------------------------------------------------------------------*/

void on_searchbtn_clicked(GtkButton *button, gpointer user_data) {
	gboolean	visible;

	if(NULL == searchdialog)
		searchdialog = create_searchdialog();
	
	g_object_get(searchdialog, "visible", &visible, NULL);
	g_object_set(searchdialog, "visible", !visible, NULL);
}

void on_hidesearch_clicked(GtkButton *button, gpointer user_data) {

	gtk_widget_hide(searchdialog);
}

void on_searchentry_activate(GtkEntry *entry, gpointer user_data) {
	/* do not use passed entry because callback is used from a button too */
	GtkWidget		*searchentry;
	G_CONST_RETURN gchar	*searchstring;
	gchar			*buffer = NULL, *tmp;
	
	searchentry = lookup_widget(searchdialog, "searchentry");
	searchstring = gtk_entry_get_text(GTK_ENTRY(searchentry));
	ui_mainwindow_set_status_bar(_("Searching for \"%s\""), searchstring);
	ui_itemlist_clear();
	if(NULL != searchFeed)
		feed_free(searchFeed);
	searchFeed = vfolder_new();
	feed_set_title(searchFeed, searchstring);
	vfolder_add_rule(searchFeed, "exact", searchstring, TRUE);
	vfolder_refresh(searchFeed);
	ui_feedlist_select(NULL);
	itemlist_load((nodePtr)searchFeed);
	
	/* switch to item list view and inform user in HTML view */
	ui_itemlist_set_two_pane_mode(FALSE);

	ui_htmlview_start_output(&buffer, NULL, TRUE);
	tmp = g_strdup_printf(_("%s<h2>%d Search Results for \"%s\"</h2>"
	                         "<p>The item list now contains all items matching the "
	                         "specified search pattern. If you want to save this search "
	                         "result permanently you can click the VFolder button in "
	                         "the search dialog and Liferea will add a VFolder to your "
	                         "feed list.</h2>"), buffer, g_slist_length(feed_get_item_list(searchFeed)), searchstring);
	addToHTMLBufferFast(&buffer, tmp);
	g_free(tmp);
	ui_htmlview_finish_output(&buffer);
	ui_htmlview_write(ui_mainwindow_get_active_htmlview(), buffer, NULL);
	g_free(buffer);

	/* enable vfolder add button */	
	gtk_widget_set_sensitive(lookup_widget(searchdialog, "vfolderaddbtn"), TRUE);
}

void on_searchentry_changed(GtkEditable *editable, gpointer user_data) {
	gchar *searchtext;
	
	/* just to disable the start search button when search string is empty... */
	searchtext = gtk_editable_get_chars(editable,0,-1);
	gtk_widget_set_sensitive(lookup_widget(searchdialog, "searchstartbtn"), (NULL != searchtext) && (0 < strlen(searchtext)));
		
}

void on_newVFolder_clicked(GtkButton *button, gpointer user_data) {
	gint			pos;
	feedPtr			fp;
	folderPtr		folder = NULL;
	
	if(NULL != searchFeed) {
		folder = ui_feedlist_get_target_folder(&pos);
		fp = searchFeed;
		searchFeed = NULL;
		ui_feedlist_add(folder, (nodePtr)fp, pos);
		ui_feedlist_update();
		ui_feedlist_select((nodePtr)fp);
	} else {
		ui_show_info_box(_("Please do a search first!"));
	}
}

void on_new_vfolder_activate(GtkMenuItem *menuitem, gpointer user_data) {
	gint			pos;
	feedPtr			fp;
	folderPtr		folder = NULL;
	
	fp = vfolder_new();
	feed_set_title(fp, _("New VFolder"));
	folder = ui_feedlist_get_target_folder(&pos);
	ui_feedlist_add(folder, (nodePtr)fp, pos);
	ui_feedlist_update();
	ui_feedlist_select((nodePtr)fp);
}


/*------------------------------------------------------------------------------*/
/* feedster support								*/
/*------------------------------------------------------------------------------*/

void on_feedsterbtn_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*keywords, *resultCountButton;
	GtkAdjustment	*resultCount;
	gchar		*searchtext;

	keywords = lookup_widget(feedsterdialog, "feedsterkeywords");
	resultCountButton = lookup_widget(feedsterdialog, "feedsterresultcount");
	if((NULL != keywords) && (NULL != resultCountButton)) {
		resultCount = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(resultCountButton));
		searchtext = (gchar *)g_strdup(gtk_entry_get_text(GTK_ENTRY(keywords)));
		searchtext = encode_uri_string(searchtext);
		searchtext = g_strdup_printf("http://www.feedster.com/search.php?q=%s&sort=date&type=rss&ie=UTF-8&limit=%d", 
					    searchtext, (int)gtk_adjustment_get_value(resultCount));

		ui_feedlist_new_subscription(searchtext, NULL, FALSE);

		g_free(searchtext);
	}
}

void on_search_with_feedster_activate(GtkMenuItem *menuitem, gpointer user_data) {
	GtkWidget	*keywords;
	
	if(NULL == feedsterdialog || !G_IS_OBJECT(feedsterdialog))
		feedsterdialog = create_feedsterdialog();
		
	keywords = lookup_widget(feedsterdialog, "feedsterkeywords");
	gtk_entry_set_text(GTK_ENTRY(keywords), "");
	gtk_widget_show(feedsterdialog);
}

