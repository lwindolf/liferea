/*
   OPML feedlist import&export
   
   Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>

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

#include "feed.h"
#include "folder.h"
#include "conf.h"
#include "callbacks.h"
#include "interface.h"
#include "support.h"

extern GtkWidget * filedialog;
static GtkWidget * importdialog = NULL;
static GtkWidget * exportdialog = NULL;

extern GMutex * feeds_lock;
extern GHashTable * feeds;

/* the real import/export functions */
static void createFeedTag(gpointer key, gpointer value, gpointer userdata) {
	feedPtr		fp = (feedPtr)value;
	xmlNodePtr 	cur = (xmlNodePtr)userdata;
	xmlNodePtr	feedNode;

	feedNode = xmlNewChild(cur, NULL, BAD_CAST"outline", NULL);
	xmlNewProp(feedNode, BAD_CAST"title", BAD_CAST getFeedTitle(fp));
	xmlNewProp(feedNode, BAD_CAST"description", BAD_CAST getFeedTitle(fp));
	xmlNewProp(feedNode, BAD_CAST"xmlUrl", BAD_CAST getFeedSource(fp));
	xmlNewProp(feedNode, BAD_CAST"htmlUrl", BAD_CAST "");
}


// FIXME: make hierarchical exports as soon as there are folderPtr structures
void exportOPMLFeedList(gchar *filename) {
	xmlDocPtr 	doc;
	xmlNodePtr 	cur, opmlNode;
	gint		error = 0;
	
	if(NULL != (doc = xmlNewDoc("1.0"))) {	
		if(NULL != (opmlNode = xmlNewDocNode(doc, NULL, BAD_CAST"opml", NULL))) {
			xmlNewProp(opmlNode, BAD_CAST"version", BAD_CAST"1.0");
			
			/* create head */
			if(NULL != (cur = xmlNewChild(opmlNode, NULL, BAD_CAST"head", NULL))) {
				xmlNewTextChild(cur, NULL, BAD_CAST"title", BAD_CAST"Liferea Feed List Export");
			}
			
			/* create body with feed list */
			if(NULL != (cur = xmlNewChild(opmlNode, NULL, BAD_CAST"body", NULL))) {
				g_mutex_lock(feeds_lock);
				g_hash_table_foreach(feeds, createFeedTag, (gpointer)cur);
				g_mutex_unlock(feeds_lock);
			}
			
			xmlDocSetRootElement(doc, opmlNode);		
		} else {
			g_warning(_("could not create XML feed node for feed cache document!"));				error = 1;
		}
		xmlSaveFormatFileEnc(filename, doc, NULL, 1);
	} else {
		g_warning(_("could not create XML document!"));
		error = 1;
	}
	
	if(0 != error)
		showErrorBox(_("Error while exporting feed list!"));
	else 
		showInfoBox(_("Feed List exported!"));

}

static void parseOutline(xmlNodePtr cur, gchar *folderkey) {
	gchar		*title, *source;
	feedPtr		fp;
	
	/* process the outline node */	
	title = xmlGetProp(cur, BAD_CAST"title");
	if(NULL == (source = xmlGetProp(cur, BAD_CAST"xmlUrl")))
		source = xmlGetProp(cur, BAD_CAST"xmlurl");	/* e.g. for AmphetaDesk */
		
	if(NULL != source) {
		if(NULL != (fp = newFeed(FST_AUTODETECT, g_strdup(source), g_strdup(folderkey)))) {
			addToFeedList(fp, TRUE);

			if(NULL != title)
				setFeedTitle(fp, g_strdup(title)); 
		}
		updateUI();
	}
	
	/* process any children */
	cur = cur->xmlChildrenNode;
	while(cur != NULL) {
		if((!xmlStrcmp(cur->name, BAD_CAST"outline")))
			parseOutline(cur, folderkey);

		cur = cur->next;
	}
}

void importOPMLFeedList(gchar *filename) {
	xmlDocPtr 	doc;
	xmlNodePtr 	cur;
	gchar		*folderkey, *foldertitle;

	/* create a new import folder */	
	foldertitle = g_strdup(_("imported feed list"));
	if(NULL != (folderkey = addFolderToConfig(foldertitle))) {
		/* add the new folder to the model */
		addFolder(folderkey, foldertitle, FST_NODE);
	} else {
		print_status(_("internal error! could not get a new folder key!"));
		return;
	}
	
	/* read the feed list */
	doc = xmlParseFile(filename);
	
	while(1) {	
		if(NULL == doc) {
			print_status(g_strdup_printf(_("XML error while reading cache file \"%s\" ! Cache file could not be loaded!"), filename));
			break;
		} 

		if(NULL == (cur = xmlDocGetRootElement(doc))) {
			print_status(_("Empty document! OPML document should not be empty..."));
			break;
		}

		while(cur && xmlIsBlankNode(cur))
			cur = cur->next;

		if(xmlStrcmp(cur->name, BAD_CAST"opml")) {
			print_status(g_strdup_printf(_("\"%s\" is no valid cache file! Cannot read OPML file!"), filename));
			break;		
		}

		cur = cur->xmlChildrenNode;
		while(cur != NULL) {
			/* we ignore the head */
			if((!xmlStrcmp(cur->name, BAD_CAST"body"))) {
				cur = cur->xmlChildrenNode;
				while(cur != NULL) {
					if((!xmlStrcmp(cur->name, BAD_CAST"outline")))
						parseOutline(cur, folderkey);

					cur = cur->next;
				}
				break;
			}
			cur = cur->next;
		}

		break;
	}
	
	if(NULL != doc)
		xmlFreeDoc(doc);

	checkForEmptyFolders();
}


/* UI stuff */


void on_import_activate(GtkMenuItem *menuitem, gpointer user_data) {

	if(NULL == importdialog || !G_IS_OBJECT(importdialog))
		importdialog = create_importdialog();
		
	gtk_widget_show(importdialog);
}

void on_export_activate(GtkMenuItem *menuitem, gpointer user_data) {

	if(NULL == exportdialog || !G_IS_OBJECT(exportdialog)) 
		exportdialog = create_exportdialog();
		
	gtk_widget_show(exportdialog);
	
}

void on_exportfileselected_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*source;
	
	gtk_widget_hide(filedialog);	

	if(NULL != (source = lookup_widget(exportdialog, "exportfileentry")))
		gtk_entry_set_text(GTK_ENTRY(source), gtk_file_selection_get_filename(GTK_FILE_SELECTION(filedialog)));
}

void on_importfileselected_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*source;
	
	gtk_widget_hide(filedialog);	

	if(NULL != (source = lookup_widget(importdialog, "importfileentry")))
		gtk_entry_set_text(GTK_ENTRY(source), gtk_file_selection_get_filename(GTK_FILE_SELECTION(filedialog)));
}

void on_exportfileselect_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*okbutton;
	
	if(NULL == filedialog || !G_IS_OBJECT(filedialog))
		filedialog = create_fileselection();
	
	if(NULL == (okbutton = lookup_widget(filedialog, "fileselectbtn")))
		g_error(_("internal error! could not find file dialog select button!"));
		
	g_signal_connect((gpointer)okbutton, "clicked", G_CALLBACK (on_exportfileselected_clicked), NULL);
	gtk_widget_show(filedialog);
}

void on_importfileselect_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*okbutton;
	
	if(NULL == filedialog || !G_IS_OBJECT(filedialog))
		filedialog = create_fileselection();
	
	if(NULL == (okbutton = lookup_widget(filedialog, "fileselectbtn")))
		g_error(_("internal error! could not find file dialog select button!"));
		
	g_signal_connect((gpointer) okbutton, "clicked", G_CALLBACK (on_importfileselected_clicked), NULL);
	gtk_widget_show(filedialog);
}

void on_exportfile_clicked(GtkButton *button, gpointer user_data) {

	gtk_widget_hide(exportdialog);
	exportOPMLFeedList((gchar *)gtk_file_selection_get_filename(GTK_FILE_SELECTION(filedialog)));
}

void on_importfile_clicked(GtkButton *button, gpointer user_data) {

	gtk_widget_hide(importdialog);
	importOPMLFeedList((gchar *)gtk_file_selection_get_filename(GTK_FILE_SELECTION(filedialog)));
}
