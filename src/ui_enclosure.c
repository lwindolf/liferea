/**
 * @file ui_enclosures.c enclosures user interface
 *
 * Copyright (C) 2005 Lars Lindner <lars.lindner@gmx.net>
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

#include <gtk/gtk.h>
#include <libxml/tree.h>
#include <libxml/uri.h>
#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "conf.h"
#include "ui_popup.h"
#include "ui_enclosure.h"
#include "debug.h"

static GSList *types = NULL;

/* some prototypes */
static void ui_enclosure_add(encTypePtr type, gchar *url, gchar *typestr);

void ui_enclosure_init(void) {
	xmlDocPtr	doc;
	xmlNodePtr	cur;
	encTypePtr	etp;
	gchar		*filename;
	
	filename = g_strdup_printf("%s" G_DIR_SEPARATOR_S "mime.xml", common_get_cache_path());
	if(NULL == (doc = xmlParseFile(filename))) {
		debug0(DEBUG_CONF, "could not load enclosure type config file!");
	} else {
		if(NULL == (cur = xmlDocGetRootElement(doc))) {
			g_warning("could not read root element from enclosure type config file!");
		} else {
			while(cur != NULL) {
				if(!xmlIsBlankNode(cur)) {
					if(!xmlStrcmp(cur->name, BAD_CAST"types")) {
						cur = cur->xmlChildrenNode;
						while(cur != NULL) {
							if((!xmlStrcmp(cur->name, BAD_CAST"type"))) {
								etp = g_new0(struct encType, 1);
								etp->mime = xmlGetProp(cur, BAD_CAST"mime");
								etp->extension = xmlGetProp(cur, BAD_CAST"extension");
								etp->cmd = xmlGetProp(cur, BAD_CAST"cmd");
								etp->permanent = TRUE;
								types = g_slist_append(types, etp);
							}
							cur = cur->next;
						}
						break;
					} else {
						g_warning(_("\"%s\" is not a valid enclosure type config file!"), filename);
					}
				}
				cur = cur->next;
			}
		}
		xmlFreeDoc(doc);
	}
	g_free(filename);
}

/* saves the enclosure type configurations to disk */
void ui_enclosure_save(void) {
	xmlDocPtr	doc;
	xmlNodePtr	root, cur;
	encTypePtr	etp;
	GSList		*iter;
	gchar		*filename;

	doc = xmlNewDoc("1.0");
	
	root = xmlNewDocNode(doc, NULL, BAD_CAST"types", NULL);
	
	iter = types;
	while(NULL != iter) {
		etp = (encTypePtr)iter->data;
		cur = xmlNewChild(root, NULL, BAD_CAST"type", NULL);
		xmlNewProp(cur, BAD_CAST"cmd", etp->cmd);
		if(NULL != etp->mime)
			xmlNewProp(cur, BAD_CAST"mime", etp->mime);
		if(NULL != etp->extension)
			xmlNewProp(cur, BAD_CAST"extension", etp->extension);
		iter = g_slist_next(iter);
	}
	
	xmlDocSetRootElement(doc, root);
	
	filename = g_strdup_printf("%s" G_DIR_SEPARATOR_S "mime.xml", common_get_cache_path());
	if(-1 == xmlSaveFormatFileEnc(filename, doc, NULL, 1))
		g_warning("Could not save to enclosure type config file!");
	g_free(filename);
	
	xmlFreeDoc(doc);
}

/* returns all configured enclosure types */
GSList * ui_enclosure_get_types(void) {

	return types;
}

void ui_enclosure_remove_type(gpointer type) {

	types = g_slist_remove(types, type);
	g_free(((encTypePtr)type)->cmd);
	g_free(((encTypePtr)type)->mime);
	g_free(((encTypePtr)type)->extension);
	g_free(type);
	ui_enclosure_save();
}

void ui_enclosure_change_type(gpointer type) {

	ui_enclosure_add((encTypePtr)type, NULL, NULL);
}

typedef struct encJob {
	gchar	*download;	/* command to download */
	gchar	*run;		/* command to run after download */
	gchar	*filename;	/* filename the result is saved to */
} *encJobPtr;

static void *ui_enclosure_exec(void *data) {
	encJobPtr	ejp = (encJobPtr)data;
	GError		*error = NULL;
	gint		status;
	
	g_print("running \"%s\"\n", ejp->download);
	g_spawn_command_line_sync(ejp->download, NULL, NULL, &status, &error);
	if((NULL != error) && (0 != error->code)) {
		g_warning("command \"%s\" failed with exitcode %d!", ejp->download, status);
	} else {
		if(NULL != ejp->run) {
			/* execute */
			g_print("running \"%s\"\n", ejp->run);
			g_spawn_command_line_async(ejp->run, &error);
			if((NULL != error) && (0 != error->code))
				g_warning("command \"%s\" failed!", ejp->run);
		} else {
			/* just saving */
			g_print("saved as \"%s\"\n", ejp->filename);			
		}
	}
	g_free(ejp->download);
	g_free(ejp->run);
	g_free(ejp->filename);
	g_free(ejp);
}

/* etp is optional, if it is missing we are in save mode */
void ui_enclosure_download(encTypePtr etp, gchar *url, gchar *path) {
	gchar		*filename;
	encJobPtr	ejp;
	
	if(NULL == (filename = strrchr(url, '/')))
		filename = g_strdup(url + 1);
		
	filename = g_strdup_printf("%s%s%s", path, G_DIR_SEPARATOR_S, filename);
	
	/* prepare job structure */
	ejp = g_new0(struct encJob, 1);
	ejp->filename = filename;
	ejp->download = g_strdup_printf(prefs_get_download_cmd(), filename, url);
	if(NULL != etp)
		ejp->run = g_strdup_printf("%s %s", etp->cmd, filename);

	g_print("downloading %s to %s...\n", url, filename);

	/* free now unnecessary stuff */
	if(FALSE == etp->permanent)
		ui_enclosure_remove_type(etp);
	g_free(path);
	g_free(url);
	
	g_thread_create(ui_enclosure_exec, ejp, FALSE, NULL);
}

/* opens a popup menu for the given link */
void ui_enclosure_new_popup(gchar *url) {
	gchar	*enclosure;
	
	if(NULL != (enclosure = xmlURIUnescapeString(url + strlen(ENCLOSURE_PROTOCOL "load?"), 0, NULL))) {
		gtk_menu_popup(ui_popup_make_enclosure_menu(enclosure), NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
		g_free(enclosure);
	}
}

static void on_selectcmdok_clicked(const gchar *filename, gpointer user_data) {
	GtkWidget	*dialog = (GtkWidget *)user_data;
	gchar		*utfname;
	
	if(filename == NULL)
		return;
	
	utfname = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);

	if(utfname != NULL)
		gtk_entry_set_text(GTK_ENTRY(lookup_widget(dialog, "enc_cmd_entry")), utfname);

	
	g_free(utfname);
}

static void on_selectcmd_pressed(GtkButton *button, gpointer user_data) {
	GtkWidget	*dialog = (GtkWidget *)user_data;
	const gchar	*utfname;
	gchar *name;
	
	utfname =  gtk_entry_get_text(GTK_ENTRY(lookup_widget(dialog,"enc_cmd_entry")));
	name = g_filename_from_utf8(utfname, -1, NULL, NULL, NULL);
	ui_choose_file(_("Choose File"), GTK_WINDOW(dialog), GTK_STOCK_OPEN, FALSE, on_selectcmdok_clicked, name, dialog);
	g_free(name);
}

/* dialog used for both changing and adding new definitions */
static void on_adddialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
	gchar		*tmp, *url;
	gboolean	new = FALSE;
	encTypePtr	etp;
	
	if(response_id == GTK_RESPONSE_OK) {
		etp = g_object_get_data(G_OBJECT(dialog), "type");
		tmp = g_object_get_data(G_OBJECT(dialog), "typestr");
		url = g_object_get_data(G_OBJECT(dialog), "url");

		if(NULL == etp)	{
			new = TRUE;
			etp = g_new0(struct encType, 1);
			if(NULL == strchr(tmp, '/'))
				etp->extension = tmp;
			else
				etp->mime = tmp;
		} else {
			g_free(etp->cmd);
		}
		etp->cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "enc_cmd_entry"))));
		etp->permanent = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(dialog), "enc_always_btn")));
		if(TRUE == new)
			types = g_slist_append(types, etp);
		
		/* now we have ensured an existing type configuration and
		   can launch the URL for which we configured the type */
		if(NULL != url)
			on_popup_open_enclosure(g_strdup_printf("%s%s%s", url, (NULL == etp->mime)?"":",", (NULL == etp->mime)?"":etp->mime), 0, NULL);
	}
	gtk_widget_destroy(GTK_WIDGET(dialog));
	
	ui_enclosure_save();
}

/* either type or url and typestr are optional */
static void ui_enclosure_add(encTypePtr type, gchar *url, gchar *typestr) {
	GtkWidget	*dialog, *widget;
	gchar		*tmp;
	
	dialog = create_enchandlerdialog();
	
	if(type != NULL) {
		typestr = (NULL != type->mime)?type->mime:type->extension;
		gtk_entry_set_text(GTK_ENTRY(lookup_widget(dialog, "enc_cmd_entry")), type->cmd);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(dialog), "enc_always_btn")), TRUE);
	}
	
	if(NULL == strchr(typestr, '/')) 
		tmp = g_strdup_printf(_("<b>File Extension .%s</b>"), typestr);
	else
		tmp = g_strdup_printf(_("<b>%s</b>"), typestr);
	gtk_label_set_markup_with_mnemonic(GTK_LABEL(lookup_widget(dialog, "enc_type_label")), tmp);
	g_free(tmp);
	
	g_object_set_data(G_OBJECT(dialog), "typestr", typestr);
	g_object_set_data(G_OBJECT(dialog), "url", url);
	g_object_set_data(G_OBJECT(dialog), "type", type);
	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(on_adddialog_response), type);
	g_signal_connect(G_OBJECT(lookup_widget(dialog, "enc_cmd_select_btn")), "clicked", G_CALLBACK(on_selectcmd_pressed), dialog);
	gtk_widget_show(dialog);
	
}

void on_popup_open_enclosure(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	gchar		*typestr, *url = (gchar *)callback_data;
	gboolean	found = FALSE, mime = FALSE;
	encTypePtr	etp = NULL;
	GSList		*iter;
	
	/* When opening enclosures we need the type to determine
	   the configured launch command. The format of the enclosure
	   info: <url>[,<mime type] */	
	if(NULL != (typestr = strrchr(url, ','))) {
		*typestr = 0;
		typestr++;
		mime = TRUE;
	}
	
	/* without a type we use the file extension */
	if(FALSE == mime) {
		typestr = strrchr(url, '.');
		typestr = g_strdup(typestr + 1);
	}
	
	/* if we have no such thing we map to "data" */
	if(NULL == typestr)
		typestr = "data";

	g_print("url:%s, type:%s mime:%s\n", url, typestr, mime?"TRUE":"FALSE");
		
	/* search for type configuration */
	iter = types;
	while(NULL != iter) {
		etp = (encTypePtr)(iter->data);
		if((NULL != ((TRUE == mime)?etp->mime:etp->extension)) &&
		   (0 == strcmp(typestr, (TRUE == mime)?etp->mime:etp->extension))) {
		   	found = TRUE;
		   	break;
		}
		iter = g_slist_next(iter);
	}
	
	if(TRUE == found)
		ui_enclosure_download(etp, url, g_strdup(getStringConfValue(ENCLOSURE_DOWNLOAD_PATH)));
	else
		ui_enclosure_add(NULL, url, typestr);
}

void on_popup_save_enclosure(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	
	g_print("save: %s\n", (gchar *)callback_data);
	// FIXME: open save as dialog
	// ui_enclose_download(NULL, url, path);
}
