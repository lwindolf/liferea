/*
   search GUI dialogs
   
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

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

#include "callbacks.h"
#include "interface.h"
#include "feed.h"
#include "folder.h"
#include "filter.h"
#include "support.h"
#include "common.h"

extern GtkWidget 	*mainwindow;
static GtkWidget 	*feedsterdialog = NULL;

extern feedPtr          allItems;

extern feedPtr		selected_fp;
extern itemPtr		selected_ip;
extern gchar 		*selected_keyprefix;
extern gint		selected_type;

/*------------------------------------------------------------------------------*/
/* search dialog callbacks								*/
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
		print_status(g_strdup_printf(_("searching for \"%s\""), searchstring));
		selected_fp = NULL;
		selected_ip = NULL;
		selected_type = FST_VFOLDER;
		loadItemList(allItems, (gchar *)searchstring);
	}
}

/*------------------------------------------------------------------------------*/
/* feedster support								*/
/*------------------------------------------------------------------------------*/

void on_feedsterbtn_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*keywords, *resultCountButton;
	GtkAdjustment	*resultCount;
	feedPtr		fp;
	gchar		*tmp, *searchtext = NULL;
	
	keywords = lookup_widget(feedsterdialog, "feedsterkeywords");
	resultCountButton = lookup_widget(feedsterdialog, "feedsterresultcount");
	if((NULL != keywords) && (NULL != resultCountButton)) {
		resultCount = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(resultCountButton));
		searchtext = (gchar *)gtk_entry_get_text(GTK_ENTRY(keywords));
		searchtext = encodeURIString(searchtext);
		searchtext = g_strdup_printf("http://www.feedster.com/rss.php?q=%s&sort=date&type=rss&ie=UTF-8&limit=%d", 
					    searchtext, (int)gtk_adjustment_get_value(resultCount));

		/* It is possible, that there is no selected folder when we are
		   called from the menu! In this case we default to the root folder */
		if(NULL != selected_keyprefix) 
			subscribeTo(FST_RSS, searchtext, g_strdup(selected_keyprefix), FALSE);
		else
			subscribeTo(FST_RSS, searchtext, g_strdup(""), FALSE);	
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

void on_newVFolder_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget		*searchentry;
	G_CONST_RETURN gchar	*searchstring;
//	rulePtr			rp;	// FIXME: this really does not belong here!!! -> vfolder.c
	feedPtr			fp;
	
	g_assert(mainwindow != NULL);
		
	if(NULL != (searchentry = lookup_widget(mainwindow, "searchentry"))) {
		searchstring = gtk_entry_get_text(GTK_ENTRY(searchentry));
		print_status(g_strdup_printf(_("creating VFolder for search term \"%s\""), searchstring));

		if(NULL != selected_keyprefix) {

			if(NULL != (fp = newFeed(FST_VFOLDER, "", g_strdup(selected_keyprefix)))) {
				checkForEmptyFolders();
				
				// FIXME: this really does not belong here!!! -> vfolder.c
				/* setup a rule */
//				if(NULL == (rp = (rulePtr)g_malloc(sizeof(struct rule)))) 
//					g_error(_("Could not allocate memory!"));

				/* we set the searchstring as a default title */
				setFeedTitle(fp, (gpointer)g_strdup_printf(_("VFolder %s"),searchstring));
				/* and set the rules... */
/*				rp->value = g_strdup((gchar *)searchstring);
				setVFolderRules(fp, rp);*/

				/* FIXME: brute force: update of all vfolders redundant */
//				loadVFolders();
				
				addToFeedList(fp, FALSE);
			}
		} else {
			print_status(_("internal error! could not get folder key prefix!"));
		}
		
	}
}

