/**
 * @file ui_feed.c	UI actions concerning a single feed
 *
 * Copyright (C) 2004-2005 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004-2005 Nathan J. Conrad <t98502@users.sourceforge.net>
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
#include <libxml/uri.h>
#include <string.h> 
#include "support.h"
#include "feed.h"
#include "node.h"
#include "conf.h"
#include "callbacks.h"
#include "update.h"
#include "interface.h"
#include "itemlist.h"
#include "favicon.h"
#include "debug.h"
#include "ui/ui_feed.h"
#include "ui/ui_notification.h"
#include "fl_providers/fl_default.h"

extern GtkWidget *mainwindow;

/********************************************************************
 * Propdialog                                                       *
 *******************************************************************/

struct fp_prop_ui_data {
	feedPtr fp;
	nodePtr np;
	gint flags; /* Used by the authdialog to know how to request the feed update */
	gint selector; /* Desiginates which fileselection dialog box is open.
				   Set to 'u' for source
				   Set to 'f' for filter */
	
	GtkWidget *dialog;
	GtkWidget *feedNameEntry;
	GtkWidget *refreshInterval;
	GtkWidget *sourceEntry;
	GtkWidget *selectFile;
	GtkWidget *fileRadio;
	GtkWidget *urlRadio;
	GtkWidget *cmdRadio;
	GtkWidget *authcheckbox;
	GtkWidget *credTable;
	GtkWidget *username;
	GtkWidget *password;
};

/* dialog callbacks */

static gchar * ui_feed_dialog_decode_source(struct fp_prop_ui_data *ui_data) {
	gchar	*source = NULL;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->fileRadio))) {
		source = g_strdup(gtk_entry_get_text(GTK_ENTRY(ui_data->sourceEntry)));
	} else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->urlRadio))) {
		gchar *str, *tmp2;
		/* First, strip leading and trailing whitespace */
		str = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(ui_data->sourceEntry))));
		
		/* Add http:// if needed */
		if (strstr(str, "://") == NULL) {
			tmp2 = g_strdup_printf("http://%s",str);
			g_free(str);
			str = tmp2;
		}
		
		/* Add trailing / if needed */
		if (strstr(strstr(str, "://") + 3, "/") == NULL) {
			tmp2 = g_strdup_printf("%s/", str);
			g_free(str);
			str = tmp2;
		}

		/* Use the values in the textboxes if also specified in the URL! */
		if((NULL != ui_data->authcheckbox) && 
		   gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->authcheckbox))) {
			xmlURIPtr uri = xmlParseURI(BAD_CAST str);
			if (uri != NULL) {
				xmlChar *sourceUrl;
				xmlFree(uri->user);
				uri->user = g_strdup_printf("%s:%s",
									   gtk_entry_get_text(GTK_ENTRY(ui_data->username)),
									   gtk_entry_get_text(GTK_ENTRY(ui_data->password)));
				sourceUrl = xmlSaveUri(uri);
				source = g_strdup(BAD_CAST sourceUrl);
				g_free(uri->user);
				uri->user = NULL;
				xmlFree(sourceUrl);
				xmlFreeURI(uri);
			} else
				source = g_strdup(str);
		} else {
			source = g_strdup(str);
		}
		g_free(str);
	} else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->cmdRadio))) {
		source = g_strdup_printf("|%s", gtk_entry_get_text(GTK_ENTRY(ui_data->sourceEntry)));
	}

	return source;
}

static void on_authdialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
	struct fp_prop_ui_data *ui_data = (struct fp_prop_ui_data*)user_data;

	if(response_id == GTK_RESPONSE_OK) {
		xmlURIPtr uri;
		xmlChar *user, *pass, *sourceUrl;

		/* Source */
		uri = xmlParseURI(BAD_CAST feed_get_source(ui_data->fp));
		
		if(uri == NULL) {
			g_warning("Error when parsing authentication URL! Authentication settings lost.");
			g_free(ui_data);
			return;
		}
		if(uri->user != NULL)
			xmlFree(uri->user);

		user = BAD_CAST gtk_entry_get_text(GTK_ENTRY(ui_data->username));
		pass = BAD_CAST gtk_entry_get_text(GTK_ENTRY(ui_data->password));
		uri->user = g_strdup_printf("%s:%s", user, pass);

		sourceUrl = xmlSaveUri(uri);
		if(sourceUrl != NULL) {
			feed_set_source(ui_data->fp, sourceUrl);
			xmlFree(sourceUrl);
		}

		node_schedule_update(ui_data->np, ui_feed_process_update_result, ui_data->flags);
		xmlFreeURI(uri);
	}

	gtk_widget_destroy(GTK_WIDGET(dialog));
	g_free(ui_data);
}


static void on_newdialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
	struct fp_prop_ui_data *ui_data = (struct fp_prop_ui_data*)user_data;

	if(response_id == GTK_RESPONSE_OK) {
		gchar *source = NULL;
		const gchar *filter = NULL;

		/* Source */
		source = ui_feed_dialog_decode_source(ui_data);

		/* Filter handling */
		filter = gtk_entry_get_text(GTK_ENTRY(lookup_widget(ui_data->dialog, "filterEntry")));
		if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(ui_data->dialog, "filterCheckbox"))) ||
		   !strcmp(filter,"")) { /* Maybe this should be a test to see if the file exists? */
			filter = NULL;
		} 
		ui_feed_add(ui_data->np, source, filter, FEED_REQ_SHOW_PROPDIALOG | FEED_REQ_RESET_TITLE | FEED_REQ_RESET_UPDATE_INT | FEED_REQ_AUTO_DISCOVER);
		g_free(source);
	}

	gtk_widget_destroy(GTK_WIDGET(dialog));
	g_free(ui_data);
}

static void on_propdialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
	struct fp_prop_ui_data *ui_data = (struct fp_prop_ui_data*)user_data;
	
	if(response_id == GTK_RESPONSE_OK) {
		gchar *newSource;
		const gchar *newFilter;
		gboolean needsUpdate = FALSE;
		
		/* General*/
		feed_set_title(ui_data->fp, gtk_entry_get_text(GTK_ENTRY(ui_data->feedNameEntry)));
		feed_set_update_interval(ui_data->fp, gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ui_data->refreshInterval)));
		
		/* Source */
		newSource = ui_feed_dialog_decode_source(ui_data);
		
		/* Filter handling */
		newFilter = gtk_entry_get_text(GTK_ENTRY(lookup_widget(ui_data->dialog, "filterEntry")));
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(ui_data->dialog, "filterCheckbox"))) &&
		   strcmp(newFilter,"")) { /* Maybe this should be a test to see if the file exists? */
			if(feed_get_filter(ui_data->fp) == NULL ||
			   strcmp(newFilter, feed_get_filter(ui_data->fp))) {
				feed_set_filter(ui_data->fp, newFilter);
				needsUpdate = TRUE;
			}
		} else {
			if(feed_get_filter(ui_data->fp) != NULL) {
				feed_set_filter(ui_data->fp, NULL);
				needsUpdate = TRUE;
			}
		}
		
		/* if URL has changed... */
		if(strcmp(newSource, feed_get_source(ui_data->fp))) {
			feed_set_source(ui_data->fp, newSource);
			needsUpdate = TRUE;
		}
		g_free(newSource);

		/* Update interval handling */
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(dialog), "updateIntervalNever"))))
			feed_set_update_interval(ui_data->fp, -2);
		else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(dialog), "updateIntervalDefault"))))
			feed_set_update_interval(ui_data->fp, -1);
		else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(dialog), "updateIntervalSpecific"))))
			feed_set_update_interval(ui_data->fp, gtk_spin_button_get_value(GTK_SPIN_BUTTON(lookup_widget(GTK_WIDGET(dialog), "refreshIntervalSpinButton"))));
		
		
		/* Cache handling */
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(dialog), "feedCacheDefault"))))
			ui_data->fp->cacheLimit = CACHE_DEFAULT;
		else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(dialog), "feedCacheDisable"))))
			ui_data->fp->cacheLimit = CACHE_DISABLE;
		else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(dialog), "feedCacheUnlimited"))))
			ui_data->fp->cacheLimit = CACHE_UNLIMITED;
		else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(dialog), "feedCacheLimited"))))
			ui_data->fp->cacheLimit = gtk_spin_button_get_value(GTK_SPIN_BUTTON(lookup_widget(GTK_WIDGET(dialog), "cacheItemLimit")));

		/* Enclosures */
		ui_data->fp->encAutoDownload = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(dialog), "enclosureDownloadCheck")));

		ui_node_update(ui_data->np);
		feedlist_schedule_save();
		if(needsUpdate)
			node_schedule_update(ui_data->np, ui_feed_process_update_result, FEED_REQ_AUTH_DIALOG | FEED_REQ_PRIORITY_HIGH);
	}

	g_free(ui_data);
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void on_feed_prop_filtercheck(GtkToggleButton *button, gpointer user_data) {
	struct fp_prop_ui_data *ui_data = (struct fp_prop_ui_data*)user_data;
	
	gboolean filter = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(ui_data->dialog, "filterCheckbox")));
	if(filter)
		gtk_widget_show(lookup_widget(ui_data->dialog, "innerfiltervbox"));
	else
		gtk_widget_hide(lookup_widget(ui_data->dialog, "innerfiltervbox"));
}

static void ui_feed_prop_enable_httpauth(struct fp_prop_ui_data *ui_data, gboolean enable) {
	gboolean on;
	if (ui_data->authcheckbox != NULL) {
		on = enable && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->authcheckbox));
		gtk_widget_set_sensitive(ui_data->authcheckbox,enable);
		gtk_widget_set_sensitive(ui_data->credTable,on);
	}
}

static void on_feed_prop_authcheck(GtkToggleButton *button, gpointer user_data) {
	struct fp_prop_ui_data *ui_data = (struct fp_prop_ui_data*)user_data;
	gboolean url = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->urlRadio));

	ui_feed_prop_enable_httpauth(ui_data, url);
}

static void on_feed_prop_url_radio(GtkToggleButton *button, gpointer user_data) {
	struct fp_prop_ui_data *ui_data = (struct fp_prop_ui_data*)user_data;
	gboolean url = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->urlRadio));
	gboolean file = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->fileRadio));
	gboolean cmd = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->cmdRadio));
	
	ui_feed_prop_enable_httpauth(ui_data, url);
	gtk_widget_set_sensitive(ui_data->selectFile, file || cmd);
}

static void on_selectfileok_clicked(const gchar *filename, gpointer user_data) {
	struct fp_prop_ui_data *ui_data = (struct fp_prop_ui_data*)user_data;
	gchar *utfname;
	
	if(filename == NULL)
		return;
	
	utfname = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);

	if(utfname != NULL) {
		if(ui_data->selector == 'u')
			gtk_entry_set_text(GTK_ENTRY(ui_data->sourceEntry), utfname);
		else
			gtk_entry_set_text(GTK_ENTRY(lookup_widget(ui_data->dialog, "filterEntry")), utfname);
	}
	
	g_free(utfname);
}

static void on_selectfile_pressed(GtkButton *button, gpointer user_data) {
	struct fp_prop_ui_data *ui_data = (struct fp_prop_ui_data*)user_data;
	const gchar *utfname;
	gchar *name;
	
	if(GTK_WIDGET(button) == ui_data->selectFile) {
		ui_data->selector = 'u';
		utfname = gtk_entry_get_text(GTK_ENTRY(ui_data->sourceEntry));
	} else {
		ui_data->selector = 'f';
		utfname = gtk_entry_get_text(GTK_ENTRY(lookup_widget(ui_data->dialog, "filterEntry")));
	}
	
	name = g_filename_from_utf8(utfname,-1,NULL, NULL, NULL);
	ui_choose_file(_("Choose File"), GTK_WINDOW(ui_data->dialog), GTK_STOCK_OPEN, FALSE, on_selectfileok_clicked, name, name, ui_data);
	g_free(name);
}
 
static void on_feed_prop_cache_radio(GtkToggleButton *button, gpointer user_data) {
	struct fp_prop_ui_data *ui_data = (struct fp_prop_ui_data*)user_data;
	gboolean limited = gtk_toggle_button_get_active(button);
	
	gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(ui_data->dialog), "cacheItemLimit"), limited);
}

static void on_feed_prop_update_radio(GtkToggleButton *button, gpointer user_data) {
	struct fp_prop_ui_data *ui_data = (struct fp_prop_ui_data*)user_data;
	gboolean limited = gtk_toggle_button_get_active(button);
	
	gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(ui_data->dialog), "refreshIntervalSpinButton"), limited);
}

/* dialog preparation */

GtkWidget* ui_feed_authdialog_new(nodePtr np, gint flags) {
	GtkWidget		*authdialog;
	struct fp_prop_ui_data	*ui_data;
	gchar			*promptStr;
	gchar			*source = NULL;
	xmlURIPtr		uri;	
	
	ui_data = g_new0(struct fp_prop_ui_data, 1);
	
	/* Create the dialog */
	ui_data->dialog = authdialog = create_authdialog();
	ui_data->np = np;
	ui_data->fp = (feedPtr)np->data;
	ui_data->flags = flags;
	gtk_window_set_transient_for(GTK_WINDOW(authdialog), GTK_WINDOW(mainwindow));
	
	/* Auth check box */
	ui_data->username = lookup_widget(authdialog, "usernameEntry");
	ui_data->password = lookup_widget(authdialog, "passwordEntry");
	
	uri = xmlParseURI(BAD_CAST feed_get_source(ui_data->fp));
	
	if(uri != NULL) {
		if(uri->user != NULL) {
			gchar *user = uri->user;
			gchar *pass = strstr(user, ":");
			if(pass != NULL) {
				pass[0] = '\0';
				pass++;
				gtk_entry_set_text(GTK_ENTRY(ui_data->password), pass);
			}
			gtk_entry_set_text(GTK_ENTRY(ui_data->username), user);
			xmlFree(uri->user);
			uri->user = NULL;
		}
		xmlFree(uri->user);
		uri->user = NULL;
		source = xmlSaveUri(uri);
		xmlFreeURI(uri);
	}
	
	promptStr = g_strdup_printf(_("Enter the username and password for \"%s\" (%s):"),
	                            feed_get_title(ui_data->fp), (source != NULL) ? source : _("Unknown source"));
	gtk_label_set_text(GTK_LABEL(lookup_widget(authdialog, "prompt")), promptStr);
	g_free(promptStr);
	if(source != NULL)
		xmlFree(source);
	
	g_signal_connect(G_OBJECT(authdialog), "response",
	                 G_CALLBACK(on_authdialog_response), ui_data);

	gtk_widget_show_all(authdialog);
	
	return authdialog;
}

void ui_feed_newdialog(nodePtr np) {
	GtkWidget *newdialog;
	struct fp_prop_ui_data *ui_data;

	ui_data = g_new0(struct fp_prop_ui_data, 1);
	ui_data->np = np;

	/* Create the dialog */
	ui_data->dialog = newdialog = create_newdialog();
	gtk_window_set_transient_for(GTK_WINDOW(newdialog), GTK_WINDOW(mainwindow));

	/***********************************************************************
	 * Source                                                              *
	 **********************************************************************/
		
	/* Setup source entry */
	ui_data->sourceEntry = lookup_widget(newdialog,"sourceEntry");
	gtk_widget_grab_focus(GTK_WIDGET(ui_data->sourceEntry));
	gtk_entry_set_activates_default(GTK_ENTRY(ui_data->sourceEntry), TRUE);
		
	ui_data->selectFile = lookup_widget(newdialog,"selectSourceFileButton");
	g_signal_connect(ui_data->selectFile, "clicked", G_CALLBACK (on_selectfile_pressed), ui_data);
	
	/* Feed location radio buttons */
	ui_data->fileRadio = lookup_widget(newdialog, "feed_loc_file");
	ui_data->urlRadio = lookup_widget(newdialog, "feed_loc_url");
	ui_data->cmdRadio = lookup_widget(newdialog, "feed_loc_command");
	g_signal_connect(ui_data->urlRadio, "toggled", G_CALLBACK(on_feed_prop_url_radio), ui_data);
	g_signal_connect(ui_data->fileRadio, "toggled", G_CALLBACK(on_feed_prop_url_radio), ui_data);
	g_signal_connect(ui_data->cmdRadio, "toggled", G_CALLBACK(on_feed_prop_url_radio), ui_data);

	g_signal_connect(lookup_widget(newdialog, "filterCheckbox"), "toggled", G_CALLBACK(on_feed_prop_filtercheck), ui_data);
	g_signal_connect(lookup_widget(newdialog, "filterSelectFile"), "clicked", G_CALLBACK(on_selectfile_pressed), ui_data);

	gtk_widget_grab_default(lookup_widget(newdialog, "newfeedbtn"));
	g_signal_connect(G_OBJECT(newdialog), "response",
	                 G_CALLBACK(on_newdialog_response), ui_data);
	
	gtk_widget_show_all(newdialog);
	on_feed_prop_filtercheck(GTK_TOGGLE_BUTTON(lookup_widget(newdialog, "filterCheckbox")), ui_data);
	on_feed_prop_url_radio(GTK_TOGGLE_BUTTON(ui_data->urlRadio), ui_data);
	
	gtk_widget_show(newdialog);
}

GtkWidget* ui_feed_propdialog_new(nodePtr np) {
	GtkWidget		*propdialog;
	struct fp_prop_ui_data	*ui_data;
	int 			interval, defaultInterval;
	gchar 			*defaultIntervalStr;
	feedPtr			fp = (feedPtr)np->data;

	ui_data = g_new0(struct fp_prop_ui_data, 1);
	ui_data->np = np;
	ui_data->fp = fp;
	
	/* Create the dialog */
	ui_data->dialog = propdialog = create_propdialog();
	gtk_window_set_transient_for(GTK_WINDOW(propdialog), GTK_WINDOW(mainwindow));

	/***********************************************************************
	 * General                                                             *
	 **********************************************************************/

	/* Setup feed name */
	ui_data->feedNameEntry = lookup_widget(propdialog,"feedNameEntry");
	gtk_entry_set_text(GTK_ENTRY(ui_data->feedNameEntry), feed_get_title(fp));

	/* Setup refresh interval */
	ui_data->refreshInterval = lookup_widget(propdialog,"refreshIntervalSpinButton");
	
	/* interval radio buttons */
	interval = feed_get_update_interval(fp);
	defaultInterval = feed_get_default_update_interval(fp);
	
	if(-2 >= interval) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(propdialog, "updateIntervalNever")), TRUE);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(lookup_widget(propdialog, "refreshIntervalSpinButton")), defaultInterval);
	} else if(-1 == interval) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(propdialog, "updateIntervalDefault")), TRUE);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(lookup_widget(propdialog, "refreshIntervalSpinButton")), defaultInterval);
	} else {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(propdialog, "updateIntervalSpecific")), TRUE);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(lookup_widget(propdialog, "refreshIntervalSpinButton")), interval);
	}
	
	gtk_widget_set_sensitive(lookup_widget(propdialog, "refreshIntervalSpinButton"), interval > 0);
	g_signal_connect(lookup_widget(propdialog, "updateIntervalSpecific"), "toggled", G_CALLBACK(on_feed_prop_update_radio), ui_data);
	
	/* setup info label about default update interval */
	if(-1 != defaultInterval)
		defaultIntervalStr = g_strdup_printf(_("The provider of this feed suggests an update interval of %d minutes."), defaultInterval);
	else
		defaultIntervalStr = g_strdup(_("This feed specifies no default update interval."));

	gtk_label_set_text(GTK_LABEL(lookup_widget(propdialog, "feedUpdateInfo")), defaultIntervalStr);
	g_free(defaultIntervalStr);

	/***********************************************************************
	 * Source                                                              *
	 **********************************************************************/
		
	/* Setup source entry */
	ui_data->sourceEntry = lookup_widget(propdialog,"sourceEntry");
	
	ui_data->selectFile = lookup_widget(propdialog,"selectSourceFileButton");
	g_signal_connect(ui_data->selectFile, "clicked", G_CALLBACK (on_selectfile_pressed), ui_data);

	/* Feed location radio buttons */
	ui_data->fileRadio = lookup_widget(propdialog, "feed_loc_file");
	ui_data->urlRadio = lookup_widget(propdialog, "feed_loc_url");
	ui_data->cmdRadio = lookup_widget(propdialog, "feed_loc_command");
	g_signal_connect(ui_data->urlRadio, "toggled", G_CALLBACK(on_feed_prop_url_radio), ui_data);
	g_signal_connect(ui_data->fileRadio, "toggled", G_CALLBACK(on_feed_prop_url_radio), ui_data);
	g_signal_connect(ui_data->cmdRadio, "toggled", G_CALLBACK(on_feed_prop_url_radio), ui_data);

	/* Auth check box */
	ui_data->authcheckbox = lookup_widget(propdialog, "HTTPauthCheck");
	ui_data->username = lookup_widget(propdialog, "usernameEntry");
	ui_data->password = lookup_widget(propdialog, "passwordEntry");
	ui_data->credTable = lookup_widget(propdialog, "table4");
	g_signal_connect(ui_data->authcheckbox, "toggled", G_CALLBACK(on_feed_prop_authcheck), ui_data);

	if(feed_get_source(fp)[0] == '|') {
		gtk_entry_set_text(GTK_ENTRY(ui_data->sourceEntry), &(feed_get_source(fp)[1]));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui_data->cmdRadio), TRUE);
		ui_feed_prop_enable_httpauth(ui_data, FALSE);
		gtk_widget_set_sensitive(ui_data->selectFile, TRUE);
	} else if(strstr(feed_get_source(fp), "://") != NULL) {
		xmlURIPtr uri = xmlParseURI(BAD_CAST feed_get_source(fp));
		xmlChar *parsedUrl;
		if(uri != NULL) {
			if(uri->user != NULL) {
				gchar *user = uri->user;
				gchar *pass = strstr(user, ":");
				if(pass != NULL) {
					pass[0] = '\0';
					pass++;
					gtk_entry_set_text(GTK_ENTRY(ui_data->password), pass);
				}
				gtk_entry_set_text(GTK_ENTRY(ui_data->username), user);
				xmlFree(uri->user);
				uri->user = NULL;
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui_data->authcheckbox), TRUE);
			}
			parsedUrl = xmlSaveUri(uri);
			gtk_entry_set_text(GTK_ENTRY(ui_data->sourceEntry), parsedUrl);
			xmlFree(parsedUrl);
			xmlFreeURI(uri);
		} else {
			gtk_entry_set_text(GTK_ENTRY(ui_data->sourceEntry), feed_get_source(fp));
		}
		ui_feed_prop_enable_httpauth(ui_data, TRUE);
		gtk_widget_set_sensitive(ui_data->selectFile, FALSE);
	} else {
		/* File */
		gtk_entry_set_text(GTK_ENTRY(ui_data->sourceEntry), feed_get_source(fp));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui_data->fileRadio), TRUE);
		ui_feed_prop_enable_httpauth(ui_data, FALSE);
		gtk_widget_set_sensitive(ui_data->selectFile, TRUE);
	}

	if(feed_get_filter(fp) != NULL) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(propdialog, "filterCheckbox")), TRUE);
		gtk_entry_set_text(GTK_ENTRY(lookup_widget(propdialog, "filterEntry")), feed_get_filter(fp));
	}
	g_signal_connect(lookup_widget(propdialog, "filterCheckbox"), "toggled", G_CALLBACK(on_feed_prop_filtercheck), ui_data);
	g_signal_connect(lookup_widget(propdialog, "filterSelectFile"), "clicked", G_CALLBACK (on_selectfile_pressed), ui_data);

	/***********************************************************************
	 * Cache                                                               *
	 **********************************************************************/

	/* Cache size radio buttons */
	if(fp->cacheLimit == CACHE_DISABLE)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(propdialog, "feedCacheDisable")), TRUE);
	else if(fp->cacheLimit == CACHE_DEFAULT)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(propdialog, "feedCacheDefault")), TRUE);
	else if(fp->cacheLimit == CACHE_UNLIMITED)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(propdialog, "feedCacheUnlimited")), TRUE);
	else {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(propdialog, "feedCacheLimited")), TRUE);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(lookup_widget(propdialog, "cacheItemLimit")), fp->cacheLimit);
	}

	gtk_widget_set_sensitive(lookup_widget(propdialog, "cacheItemLimit"), fp->cacheLimit > 0);
	g_signal_connect(lookup_widget(propdialog, "feedCacheLimited"), "toggled", G_CALLBACK(on_feed_prop_cache_radio), ui_data);

	g_signal_connect(G_OBJECT (propdialog), "response", G_CALLBACK (on_propdialog_response), ui_data);

	gtk_widget_show_all(propdialog);
	on_feed_prop_filtercheck(GTK_TOGGLE_BUTTON(lookup_widget(propdialog, "filterCheckbox")), ui_data);

	/***********************************************************************
	 * Enclosures                                                          *
	 **********************************************************************/
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(propdialog, "enclosureDownloadCheck")), fp->encAutoDownload);

	return propdialog;
}

/* used by fl_default_node_add but also from ui_search.c! */
void ui_feed_add(nodePtr np, const gchar *source, const gchar *filter, gint flags) {
	feedPtr			fp;
	int			pos;
	nodePtr			parent;
	
	debug_enter("ui_feed_add");	
	
	fp = feed_new();
	feed_set_source(fp, source);
	feed_set_title(fp, _("New subscription"));
	feed_set_filter(fp, filter);

	node_set_title(np, feed_get_title(fp));
	node_add_data(np, FST_FEED, (gpointer)fp);
	parent = ui_feedlist_get_target_folder(&pos);
	feedlist_add_node(parent, np, pos);

	node_schedule_update(np, ui_feed_process_update_result, flags | FEED_REQ_PRIORITY_HIGH | FEED_REQ_DOWNLOAD_FAVICON | FEED_REQ_AUTH_DIALOG);

	debug_exit("ui_feed_add");	
}

/** handles completed feed update requests */
void ui_feed_process_update_result(struct request *request) {
	nodePtr			np = (nodePtr)request->user_data;
	feedPtr			fp = (feedPtr)np->data;
	feedHandlerPtr		fhp;
	itemSetPtr		sp;
	gchar			*old_title, *old_source;
	gint			old_update_interval;

	debug_enter("ui_feed_process_update_result");
	
	node_load(np);

	/* no matter what the result of the update is we need to save update
	   status and the last update time to cache */
	np->needsCacheSave = TRUE;
	
	feed_set_available(fp, TRUE);

	if(401 == request->httpstatus) { /* unauthorized */
		feed_set_available(fp, FALSE);
		if(request->flags & FEED_REQ_AUTH_DIALOG)
			ui_feed_authdialog_new(np, request->flags);
	} else if(410 == request->httpstatus) { /* gone */
		feed_set_available(fp, FALSE);
		feed_set_discontinued(fp, TRUE);
		ui_mainwindow_set_status_bar(_("\"%s\" is discontinued. Liferea won't updated it anymore!"), feed_get_title(fp));
	} else if(304 == request->httpstatus) {
		ui_mainwindow_set_status_bar(_("\"%s\" has not changed since last update"), feed_get_title(fp));
	} else if(NULL != request->data) {
		feed_set_lastmodified(fp, request->lastmodified);
		feed_set_etag(fp, request->etag);
		
		/* note this is to update the feed URL on permanent redirects */
		if(0 != strcmp(request->source, feed_get_source(fp))) {
			feed_set_source(fp, request->source);
			ui_mainwindow_set_status_bar(_("The URL of \"%s\" has changed permanently and was updated"), feed_get_title(fp));
		}
		
		/* we save all properties that should not be overwritten in all cases */
		old_update_interval = feed_get_update_interval(fp);
		old_title = g_strdup(feed_get_title(fp));
		old_source = g_strdup(feed_get_source(fp));

		/* parse the new downloaded feed into fp and sp */
		sp = (itemSetPtr)g_new0(struct itemSet, 1);
		fhp = feed_parse(fp, sp, request->data, request->size, request->flags & FEED_REQ_AUTO_DISCOVER);

		if(fhp == NULL) {
			feed_set_available(fp, FALSE);
			fp->parseErrors = g_strdup_printf(_("<p>Could not detect the type of this feed! Please check if the source really points to a resource provided in one of the supported syndication formats!</p>%s"), fp->parseErrors);
		} else {
			fp->fhp = fhp;
			
			/* merge the resulting items into the node's item set */
			node_merge_items(np, sp->items);
		
			/* restore user defined properties if necessary */
			if(!(request->flags & FEED_REQ_RESET_TITLE)) {
				feed_set_title(fp, old_title);
				node_set_title(np, old_title);
			}
				
			if(!(request->flags & FEED_REQ_AUTO_DISCOVER))
				feed_set_source(fp, old_source);

			if(request->flags & FEED_REQ_RESET_UPDATE_INT)
				feed_set_update_interval(fp, feed_get_default_update_interval(fp));
			else
				feed_set_update_interval(fp, old_update_interval);
				
			g_free(old_title);
			g_free(old_source);

			ui_mainwindow_set_status_bar(_("\"%s\" updated..."), feed_get_title(fp));

			itemlist_reload(np->itemSet);
			
			if(request->flags & FEED_REQ_SHOW_PROPDIALOG)
				ui_feed_propdialog_new(np);
		}
	} else {	
		ui_mainwindow_set_status_bar(_("\"%s\" is not available"), feed_get_title(fp));
		feed_set_available(fp, FALSE);
	}
	
	feed_set_error_description(fp, request->httpstatus, request->returncode, request->filterErrors);

	fp->request = NULL; 

	if(request->flags & FEED_REQ_DOWNLOAD_FAVICON)
		favicon_download(np);

	ui_node_update(np);
	ui_notification_update(np);
	node_unload(np);

	debug_exit("ui_feed_process_update_result");
}

