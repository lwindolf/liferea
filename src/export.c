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

#include "ui_folder.h"
extern GtkWidget * filedialog;
static GtkWidget * importdialog = NULL;
static GtkWidget * exportdialog = NULL;

/* Used for exporting, this adds a folder or feed's node to the XML tree */
static void append_node_tag(nodePtr ptr, gpointer userdata) {
	folderPtr		folder = (folderPtr)folder;
	xmlNodePtr 	cur = (xmlNodePtr)userdata;
	xmlNodePtr	childNode;
	
	if (IS_FOLDER(ptr->type)) {
		folderPtr folder = (folderPtr)ptr;
		childNode = xmlNewChild(cur, NULL, BAD_CAST"outline", NULL);
		xmlNewProp(childNode, BAD_CAST"text", BAD_CAST folder_get_title(folder));
		if (ptr->type == FST_HELPFOLDER) {
			xmlNewProp(childNode, BAD_CAST"helpFolder", NULL);
		} else {
			ui_feedlist_do_for_all_data(ptr,ACTION_FILTER_CHILDREN, append_node_tag, (gpointer)childNode);
		}
	} else {
		feedPtr fp = (feedPtr)ptr;
		gchar *type = g_strdup_printf("%d",feed_get_type(fp));
		gchar *interval = g_strdup_printf("%d",feed_get_update_interval(fp));

		childNode = xmlNewChild(cur, NULL, BAD_CAST"outline", NULL);
		xmlNewProp(childNode, BAD_CAST"text", BAD_CAST feed_get_title(fp));
		xmlNewProp(childNode, BAD_CAST"type", BAD_CAST type);
		xmlNewProp(childNode, BAD_CAST"htmlUrl", BAD_CAST "");
		xmlNewProp(childNode, BAD_CAST"xmlUrl", BAD_CAST feed_get_source(fp));
		xmlNewProp(childNode, BAD_CAST"id", BAD_CAST feed_get_id(fp));
		xmlNewProp(childNode, BAD_CAST"updateInterval", BAD_CAST interval);

		g_free(interval);
		g_free(type);
	}
}


int exportOPMLFeedList(gchar *filename) {
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
				ui_feedlist_do_for_all_data(NULL, ACTION_FILTER_CHILDREN, append_node_tag, (gpointer)cur);
			}
			
			xmlDocSetRootElement(doc, opmlNode);		
		} else {
			g_warning(_("could not create XML feed node for feed cache document!"));
			error = 1;
		}
		if (-1 == xmlSaveFormatFileEnc(filename, doc, NULL, 1)) {
			g_warning(_("Could not export to OPML file!!"));
			error = 1;
		}
	} else {
		g_warning(_("could not create XML document!"));
		error = 1;
	}
	return error;
}

static int parse_integer(gchar *str, int def) {
	int num;
	if (str == NULL)
		return def;
	if (0==(sscanf(str,"%d",&num)))
		num = def;
	
	return num;
}


static void parseOutline(xmlNodePtr cur, folderPtr folder) {
	gchar		*title, *source, *typeStr, *intervalStr;
	feedPtr		fp;
	gint		type, interval;
	gchar		*id;
	
	/* process the outline node */	
	title = xmlGetProp(cur, BAD_CAST"text");

	if(NULL == (source = xmlGetProp(cur, BAD_CAST"xmlUrl")))
		source = xmlGetProp(cur, BAD_CAST"xmlurl");	/* e.g. for AmphetaDesk */
	
	if(NULL != source) { /* Reading a feed */
		/* type */
		typeStr = xmlGetProp(cur, BAD_CAST"type");
		type = parse_integer(typeStr, FST_AUTODETECT);
		xmlFree(typeStr);

		intervalStr = xmlGetProp(cur, BAD_CAST"updateInterval");
		interval = parse_integer(intervalStr, -1);
		xmlFree(intervalStr);

		id = xmlGetProp(cur, BAD_CAST"id");
		if(NULL != (fp = feed_add(type, source, folder, title, id, interval, FALSE))) {
			if(NULL != title)
				feed_set_title(fp, title);
		}
		xmlFree(id);
		xmlFree(source);
	} else { /* It is a folder */
		if (NULL != xmlHasProp(cur, BAD_CAST"helpFolder"))
			g_assert(NULL != (folder = feedlist_insert_help_folder(folder)));
		else {
			g_assert(NULL != (folder = restore_folder(folder, -1, title, NULL, FST_FOLDER)));
			ui_add_folder(folder);
		}
		if (NULL != xmlHasProp(cur, BAD_CAST"expanded"))
			ui_folder_set_expansion(folder, TRUE);
		if (NULL != xmlHasProp(cur, BAD_CAST"collapsed"))
			ui_folder_set_expansion(folder, FALSE);
	}
	xmlFree(title);

	/* process any children */
	cur = cur->xmlChildrenNode;
	while(cur != NULL) {
		if((!xmlStrcmp(cur->name, BAD_CAST"outline")))
			parseOutline(cur, folder);
		
		cur = cur->next;
	}
}

static void parseBODY(xmlNodePtr n, folderPtr parent) {
	xmlNodePtr cur;
	cur = n->xmlChildrenNode;
	while(cur != NULL) {
		if((!xmlStrcmp(cur->name, BAD_CAST"outline")))
			parseOutline(cur, parent);
		cur = cur->next;
	}
}

static void parseOPML(xmlNodePtr n, folderPtr parent) {
	xmlNodePtr cur;
	cur = n->xmlChildrenNode;
	while(cur != NULL) {
		/* we ignore the head */
		if((!xmlStrcmp(cur->name, BAD_CAST"body"))) {
			parseBODY(cur, parent);
		}
		cur = cur->next;
	}	
}

void importOPMLFeedList(gchar *filename, folderPtr parent, gboolean showErrors) {
	xmlDocPtr 	doc;
	xmlNodePtr 	cur;
	
	g_message("Importing OPML file: %s", filename);
	/* read the feed list */
	doc = xmlParseFile(filename);

	if(NULL == doc) {
		if (showErrors)
			ui_show_error_box(_("XML error while reading cache file! Could not import \"%s\"!"), filename);
		else
			g_message(_("XML error while reading cache file! Could not import \"%s\"!"), filename);
	} else {
		if(NULL == (cur = xmlDocGetRootElement(doc))) {
			if (showErrors)
				ui_show_error_box(_("Empty document! OPML document \"%s\" should not be empty when importing."), filename);
			else
				g_message(_("Empty document! OPML document \"%s\" should not be empty when importing."), filename);
		} else {
			while (cur != NULL) {
				if(!xmlIsBlankNode(cur)) {
					if(!xmlStrcmp(cur->name, BAD_CAST"opml")) {
						parseOPML(cur, parent);
					} else {
						if (showErrors)
							ui_show_error_box(_("\"%s\" is no valid OPML document! Liferea cannot import this file!"), filename);
						else
							g_message(_("\"%s\" is no valid OPML document! Liferea cannot import this file!"), filename);
					}
				}
				cur = cur->next;
			}
		}
		xmlFreeDoc(doc);
	}
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
	const gchar *name;
	gchar *utfname;

	gtk_widget_hide(filedialog);	

	if(NULL != (source = lookup_widget(exportdialog, "exportfileentry"))) {
		name = gtk_file_selection_get_filename(GTK_FILE_SELECTION(filedialog));
		utfname = g_filename_to_utf8(name, -1, NULL, NULL, NULL);
		if (utfname!= NULL) {
			gtk_entry_set_text(GTK_ENTRY(source), utfname);
		}
		g_free(utfname);
	}
}

void on_importfileselected_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*source;
	const gchar *name;
	gchar *utfname;
	
	gtk_widget_hide(filedialog);	

	if(NULL != (source = lookup_widget(importdialog, "importfileentry"))) {
		name = gtk_file_selection_get_filename(GTK_FILE_SELECTION(filedialog));
		utfname = g_filename_to_utf8(name, -1, NULL, NULL, NULL);
		if (utfname!= NULL) {
			gtk_entry_set_text(GTK_ENTRY(source), utfname);
		}
		g_free(utfname);
	}
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
	GtkWidget	*source;
	const gchar *utfname;
	gchar *name;
	gint error;

	gtk_widget_hide(exportdialog);

	if(NULL != (source = lookup_widget(exportdialog, "exportfileentry"))) {
		utfname = gtk_entry_get_text(GTK_ENTRY(source));
		name = g_filename_from_utf8(utfname,-1,NULL, NULL, NULL);
		if (name != NULL) {
			error = exportOPMLFeedList(name);
			g_free(name);
		}
	}
	if(0 != error)
		ui_show_error_box(_("Error while exporting feed list!"));
	else 
		ui_show_info_box(_("Feed List exported!"));
}

void on_importfile_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*source;
	const gchar *utfname;
	gchar *name;
	GError *err = NULL;
	gtk_widget_hide(importdialog);
	if(NULL != (source = lookup_widget(importdialog, "importfileentry"))) {
		utfname = gtk_entry_get_text(GTK_ENTRY(source));
		name = g_filename_from_utf8(utfname,-1,NULL, NULL, &err);
		if (name != NULL) {
			gchar *foldertitle;
			folderPtr folder;
			
			foldertitle = g_strdup(_("imported feed list"));
			
			if(NULL != (folder = restore_folder(folder_get_root(), -1, foldertitle, NULL, FST_FOLDER))) {
				/* add the new folder to the model */
				ui_add_folder(folder);
			} else {
				ui_mainwindow_set_status_bar(_("internal error! could not get a new folder key!"));
				return;
			}
			g_free(foldertitle);
			
			importOPMLFeedList(name, folder, TRUE);
			g_free(name);
		}
	}
}
