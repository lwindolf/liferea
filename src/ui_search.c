/**
 * @file ui_search.c everything about searching
 *
 * Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>
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

#include "callbacks.h"
#include "interface.h"
#include "feed.h"
#include "folder.h"
#include "rule.h"
#include "vfolder.h"
#include "support.h"
#include "common.h"

extern GtkWidget 	*mainwindow;
static GtkWidget 	*feedsterdialog = NULL;
static feedPtr		searchFeed = NULL;

/*------------------------------------------------------------------------------*/
/* search dialog callbacks							*/
/*------------------------------------------------------------------------------*/

/* called when toolbar search button is clicked */
void on_searchbtn_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*searchbox;
	gboolean	visible;

	g_assert(mainwindow != NULL);

	if(NULL != (searchbox = lookup_widget(mainwindow, "searchbox"))) {
		g_object_get(searchbox, "visible", &visible, NULL);
		g_object_set(searchbox, "visible", !visible, NULL);
	}
}

/* called when close button in search dialog is clicked */
void on_hidesearch_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*searchbox;

	g_assert(mainwindow != NULL);
	
	if(NULL != (searchbox = lookup_widget(mainwindow, "searchbox"))) {
		g_object_set(searchbox, "visible", FALSE, NULL);
	}
}

void on_searchentry_activate(GtkButton *button, gpointer user_data) {
	GtkWidget		*searchentry;
	G_CONST_RETURN gchar	*searchstring;

	g_assert(mainwindow != NULL);
	if(NULL != (searchentry = lookup_widget(mainwindow, "searchentry"))) {
		searchstring = gtk_entry_get_text(GTK_ENTRY(searchentry));
		ui_mainwindow_set_status_bar(_("Searching for \"%s\""), searchstring);
		ui_itemlist_clear();
		if(NULL != searchFeed)
			feed_free(searchFeed);
		searchFeed = vfolder_new();
		vfolder_add_rule(searchFeed, "exact", searchstring, TRUE);
		vfolder_refresh(searchFeed);
		ui_feedlist_select(NULL);
		ui_itemlist_load((nodePtr)searchFeed);
g_print("==============================================================\n");
	}
}

void on_newVFolder_clicked(GtkButton *button, gpointer user_data) {
	G_CONST_RETURN gchar	*searchstring;
	gint			pos;
	feedPtr			fp;
	folderPtr		folder = NULL;
	
	if(NULL != searchFeed) {
		folder = ui_feedlist_get_target_folder(&pos);
		fp = searchFeed;
		searchFeed = NULL;
		ui_folder_add_feed(folder, fp, pos);
		ui_feedlist_update();
		ui_feedlist_select((nodePtr)fp);
	} else {
		ui_show_info_box(_("Please do a search first!"));
	}
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

