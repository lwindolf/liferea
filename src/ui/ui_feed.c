/**
 * @file ui_feed.c	UI actions concerning a single feed
 *
 * Copyright (C) 2004-2006 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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

extern GtkWidget *mainwindow;

/********************************************************************
 * general callbacks for "New" and "Properties" dialog              *
 ********************************************************************/

struct fp_prop_ui_data {
	feedPtr feed;
	nodePtr node;
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

static gchar * ui_feed_create_url(gchar *url, gboolean auth, const gchar *username, const gchar *password) {
	gchar	*source = NULL;
	gchar *str, *tmp2;
	
	/* First, strip leading and trailing whitespace */
	str = g_strstrip(url);

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
	if(auth) {
		xmlURIPtr uri = xmlParseURI(BAD_CAST str);
		if (uri != NULL) {
			xmlChar *sourceUrl;
			xmlFree(uri->user);
			uri->user = g_strdup_printf("%s:%s", username, password);
			sourceUrl = xmlSaveUri(uri);
			source = g_strdup(sourceUrl);
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
	
	return source;
}

static gchar * ui_feed_dialog_decode_source(struct fp_prop_ui_data *ui_data) {
	gchar	*source = NULL;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->fileRadio)))
		source = g_strdup(gtk_entry_get_text(GTK_ENTRY(ui_data->sourceEntry)));
		
	else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->urlRadio)))
		source = ui_feed_create_url(g_strdup(gtk_entry_get_text(GTK_ENTRY(ui_data->sourceEntry))),
		                            ui_data->authcheckbox &&
		                            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->authcheckbox)),
		                            gtk_entry_get_text(GTK_ENTRY(ui_data->username)),
					    gtk_entry_get_text(GTK_ENTRY(ui_data->password)));
					    
	else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->cmdRadio)))
		source = g_strdup_printf("|%s", gtk_entry_get_text(GTK_ENTRY(ui_data->sourceEntry)));

	return source;
}

static void on_propdialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
	struct fp_prop_ui_data *ui_data = (struct fp_prop_ui_data*)user_data;
	
	if(response_id == GTK_RESPONSE_OK) {
		gchar		*newSource;
		const gchar	*newFilter;
		gboolean	needsUpdate = FALSE;
		feedPtr		feed = ui_data->feed;
		nodePtr		node = ui_data->node;
		
		/* "General" */
		node_set_title(node, gtk_entry_get_text(GTK_ENTRY(ui_data->feedNameEntry)));
		feed_set_update_interval(feed, gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ui_data->refreshInterval)));
		
		/* Source */
		newSource = ui_feed_dialog_decode_source(ui_data);
		
		/* Filter handling */
		newFilter = gtk_entry_get_text(GTK_ENTRY(lookup_widget(ui_data->dialog, "filterEntry")));
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(ui_data->dialog, "filterCheckbox"))) &&
		   strcmp(newFilter,"")) { /* Maybe this should be a test to see if the file exists? */
			if(feed_get_filter(feed) == NULL ||
			   strcmp(newFilter, feed_get_filter(feed))) {
				feed_set_filter(feed, newFilter);
				needsUpdate = TRUE;
			}
		} else {
			if(feed_get_filter(feed)) {
				feed_set_filter(feed, NULL);
				needsUpdate = TRUE;
			}
		}
		
		/* if URL has changed... */
		if(strcmp(newSource, feed_get_source(feed))) {
			feed_set_source(feed, newSource);
			needsUpdate = TRUE;
		}
		g_free(newSource);

		/* Update interval handling */
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(dialog), "updateIntervalNever"))))
			feed_set_update_interval(feed, -2);
		else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(dialog), "updateIntervalDefault"))))
			feed_set_update_interval(feed, -1);
		else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(dialog), "updateIntervalSpecific"))))
			feed_set_update_interval(feed, gtk_spin_button_get_value(GTK_SPIN_BUTTON(lookup_widget(GTK_WIDGET(dialog), "refreshIntervalSpinButton"))));
		
		
		/* "Archive" handling */
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(dialog), "feedCacheDefault"))))
			feed->cacheLimit = CACHE_DEFAULT;
		else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(dialog), "feedCacheDisable"))))
			feed->cacheLimit = CACHE_DISABLE;
		else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(dialog), "feedCacheUnlimited"))))
			feed->cacheLimit = CACHE_UNLIMITED;
		else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(dialog), "feedCacheLimited"))))
			feed->cacheLimit = gtk_spin_button_get_value(GTK_SPIN_BUTTON(lookup_widget(GTK_WIDGET(dialog), "cacheItemLimit")));

		/* "Download" Options */
		feed->updateOptions->dontUseProxy = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(dialog), "dontUseProxyCheck")));

		/* "Advanced" options */
		feed->encAutoDownload = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(dialog), "enclosureDownloadCheck")));
		feed->loadItemLink = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(dialog), "loadItemLinkCheck")));

		ui_node_update(node);
		feedlist_schedule_save();
		if(needsUpdate)
			node_request_update(node, FEED_REQ_AUTH_DIALOG | FEED_REQ_PRIORITY_HIGH);
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

	if(ui_data->authcheckbox) {
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
	
	if(!filename)
		return;
	
	utfname = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);

	if(utfname) {
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
	ui_choose_file(_("Choose File"), GTK_WINDOW(ui_data->dialog), GTK_STOCK_OPEN, FALSE, on_selectfileok_clicked, name, NULL, ui_data);
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

/********************************************************************
 * "HTTP Authentication" dialog                                     *
 ********************************************************************/

static void on_authdialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
	struct fp_prop_ui_data *ui_data = (struct fp_prop_ui_data*)user_data;

	if(response_id == GTK_RESPONSE_OK) {
		xmlURIPtr uri;
		xmlChar *user, *pass, *sourceUrl;

		/* Source */
		uri = xmlParseURI(BAD_CAST feed_get_source(ui_data->feed));
		
		if(!uri) {
			g_warning("Error when parsing authentication URL! Authentication settings lost.");
			g_free(ui_data);
			return;
		}
		if(uri->user)
			xmlFree(uri->user);

		user = BAD_CAST gtk_entry_get_text(GTK_ENTRY(ui_data->username));
		pass = BAD_CAST gtk_entry_get_text(GTK_ENTRY(ui_data->password));
		uri->user = g_strdup_printf("%s:%s", user, pass);

		sourceUrl = xmlSaveUri(uri);
		if(sourceUrl) {
			feed_set_source(ui_data->feed, sourceUrl);
			xmlFree(sourceUrl);
		}

		node_request_update(ui_data->node, ui_data->flags);
		xmlFreeURI(uri);
	}

	gtk_widget_destroy(GTK_WIDGET(dialog));
	g_free(ui_data);
}

void ui_feed_authdialog_new(nodePtr node, gint flags) {
	GtkWidget		*authdialog;
	struct fp_prop_ui_data	*ui_data;
	gchar			*promptStr;
	gchar			*source = NULL;
	xmlURIPtr		uri;	
	
	ui_data = g_new0(struct fp_prop_ui_data, 1);
	
	/* Create the dialog */
	ui_data->dialog = authdialog = create_authdialog();
	ui_data->node = node;
	ui_data->feed = (feedPtr)node->data;
	ui_data->flags = flags;
	gtk_window_set_transient_for(GTK_WINDOW(authdialog), GTK_WINDOW(mainwindow));
	
	/* Auth check box */
	ui_data->username = lookup_widget(authdialog, "usernameEntry");
	ui_data->password = lookup_widget(authdialog, "passwordEntry");
	
	uri = xmlParseURI(BAD_CAST feed_get_source(ui_data->feed));
	
	if(uri) {
		if(uri->user) {
			gchar *user = uri->user;
			gchar *pass = strstr(user, ":");
			if(pass) {
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
	                            node_get_title(node), source?source:_("Unknown source"));
	gtk_label_set_text(GTK_LABEL(lookup_widget(authdialog, "prompt")), promptStr);
	g_free(promptStr);
	if(source)
		xmlFree(source);
	
	g_signal_connect(G_OBJECT(authdialog), "response",
	                 G_CALLBACK(on_authdialog_response), ui_data);

	gtk_widget_show_all(authdialog);
}

/********************************************************************
 * complex "New" dialog                                             *
 ********************************************************************/

static void on_newdialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
	struct fp_prop_ui_data *ui_data = (struct fp_prop_ui_data*)user_data;

	if(response_id == GTK_RESPONSE_OK) {
		gchar *source = NULL;
		const gchar *filter = NULL;
		updateOptionsPtr options;

		/* Source */
		source = ui_feed_dialog_decode_source(ui_data);

		/* Filter handling */
		filter = gtk_entry_get_text(GTK_ENTRY(lookup_widget(ui_data->dialog, "filterEntry")));
		if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(ui_data->dialog, "filterCheckbox"))) ||
		   !strcmp(filter,"")) { /* Maybe this should be a test to see if the file exists? */
			filter = NULL;
		} 
		
		options = g_new0(struct updateOptions, 1);
		options->dontUseProxy = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(dialog), "dontUseProxyCheck")));
		
		node_request_automatic_add(source, NULL, filter, options,
					   FEED_REQ_RESET_TITLE | 
					   FEED_REQ_RESET_UPDATE_INT | 
					   FEED_REQ_AUTO_DISCOVER | 
					   FEED_REQ_PRIORITY_HIGH | 
					   FEED_REQ_DOWNLOAD_FAVICON | 
					   FEED_REQ_AUTH_DIALOG);
		g_free(source);
	}

	gtk_widget_destroy(GTK_WIDGET(dialog));
	g_free(ui_data);
}

static void on_complex_newdialog(struct fp_prop_ui_data *ui_data) {
	GtkWidget		*newdialog;

	gtk_widget_destroy(ui_data->dialog);

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

/********************************************************************
 * simple "New" dialog                                             *
 ********************************************************************/

static void on_simple_newdialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
	struct fp_prop_ui_data *ui_data = (struct fp_prop_ui_data*)user_data;
	gchar *source = NULL;	
	
	switch(response_id) {
		case GTK_RESPONSE_OK:
			source = ui_feed_create_url(g_strdup(gtk_entry_get_text(GTK_ENTRY(ui_data->sourceEntry))),
			                            FALSE /* auth */, NULL /* user */, NULL /* passwd */);

			node_request_automatic_add(source, NULL, NULL, NULL,
						   FEED_REQ_RESET_TITLE | 
						   FEED_REQ_RESET_UPDATE_INT | 
						   FEED_REQ_AUTO_DISCOVER | 
						   FEED_REQ_PRIORITY_HIGH | 
						   FEED_REQ_DOWNLOAD_FAVICON | 
						   FEED_REQ_AUTH_DIALOG);
			g_free(source);
			gtk_widget_destroy(GTK_WIDGET(dialog));
			g_free(ui_data);
			break;
		case GTK_RESPONSE_APPLY: /* misused for "Advanced" */
			on_complex_newdialog(ui_data);
			break;
	}
}

void ui_feed_add(nodePtr parent) {
	GtkWidget		*newdialog;
	struct fp_prop_ui_data	*ui_data;

	ui_data = g_new0(struct fp_prop_ui_data, 1);
	ui_data->node = parent;

	/* Create the dialog */
	ui_data->dialog = newdialog = create_simplenewdialog();
	gtk_window_set_transient_for(GTK_WINDOW(newdialog), GTK_WINDOW(mainwindow));
	
	/* Setup source entry */
	ui_data->sourceEntry = lookup_widget(newdialog,"sourceEntry");
	gtk_widget_grab_focus(GTK_WIDGET(ui_data->sourceEntry));
	gtk_entry_set_activates_default(GTK_ENTRY(ui_data->sourceEntry), TRUE);

	g_signal_connect(G_OBJECT(newdialog), "response",
	                 G_CALLBACK(on_simple_newdialog_response), ui_data);
	
	gtk_widget_show(newdialog);
}

/********************************************************************
 * "Properties" dialog                                              *
 ********************************************************************/

void ui_feed_properties(nodePtr node) {
	GtkWidget		*propdialog;
	struct fp_prop_ui_data	*ui_data;
	int 			interval, defaultInterval;
	gchar 			*defaultIntervalStr;
	feedPtr			feed = (feedPtr)node->data;

	node_load(node);

	ui_data = g_new0(struct fp_prop_ui_data, 1);
	ui_data->node = node;
	ui_data->feed = feed;
	
	/* Create the dialog */
	ui_data->dialog = propdialog = create_propdialog();
	gtk_window_set_transient_for(GTK_WINDOW(propdialog), GTK_WINDOW(mainwindow));

	/***********************************************************************
	 * General                                                             *
	 ***********************************************************************/

	/* Setup feed name */
	ui_data->feedNameEntry = lookup_widget(propdialog,"feedNameEntry");
	gtk_entry_set_text(GTK_ENTRY(ui_data->feedNameEntry), node_get_title(node));

	/* Setup refresh interval */
	ui_data->refreshInterval = lookup_widget(propdialog,"refreshIntervalSpinButton");
	
	/* interval radio buttons */
	interval = feed_get_update_interval(feed);
	defaultInterval = feed_get_default_update_interval(feed);
	
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
	 ***********************************************************************/
		
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

	if(feed_get_source(feed)[0] == '|') {
		gtk_entry_set_text(GTK_ENTRY(ui_data->sourceEntry), &(feed_get_source(feed)[1]));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui_data->cmdRadio), TRUE);
		ui_feed_prop_enable_httpauth(ui_data, FALSE);
		gtk_widget_set_sensitive(ui_data->selectFile, TRUE);
	} else if(strstr(feed_get_source(feed), "://") != NULL) {
		xmlURIPtr uri = xmlParseURI(BAD_CAST feed_get_source(feed));
		xmlChar *parsedUrl;
		if(uri) {
			if(uri->user) {
				gchar *user = uri->user;
				gchar *pass = strstr(user, ":");
				if(pass) {
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
			gtk_entry_set_text(GTK_ENTRY(ui_data->sourceEntry), feed_get_source(feed));
		}
		ui_feed_prop_enable_httpauth(ui_data, TRUE);
		gtk_widget_set_sensitive(ui_data->selectFile, FALSE);
	} else {
		/* File */
		gtk_entry_set_text(GTK_ENTRY(ui_data->sourceEntry), feed_get_source(feed));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui_data->fileRadio), TRUE);
		ui_feed_prop_enable_httpauth(ui_data, FALSE);
		gtk_widget_set_sensitive(ui_data->selectFile, TRUE);
	}

	if(feed_get_filter(feed)) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(propdialog, "filterCheckbox")), TRUE);
		gtk_entry_set_text(GTK_ENTRY(lookup_widget(propdialog, "filterEntry")), feed_get_filter(feed));
	}
	g_signal_connect(lookup_widget(propdialog, "filterCheckbox"), "toggled", G_CALLBACK(on_feed_prop_filtercheck), ui_data);
	g_signal_connect(lookup_widget(propdialog, "filterSelectFile"), "clicked", G_CALLBACK (on_selectfile_pressed), ui_data);

	/***********************************************************************
	 * Archive                                                             *
	 ***********************************************************************/

	/* Cache size radio buttons */
	if(feed->cacheLimit == CACHE_DISABLE) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(propdialog, "feedCacheDisable")), TRUE);
	} else if(feed->cacheLimit == CACHE_DEFAULT) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(propdialog, "feedCacheDefault")), TRUE);
	} else if(feed->cacheLimit == CACHE_UNLIMITED) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(propdialog, "feedCacheUnlimited")), TRUE);
	} else {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(propdialog, "feedCacheLimited")), TRUE);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(lookup_widget(propdialog, "cacheItemLimit")), feed->cacheLimit);
	}

	gtk_widget_set_sensitive(lookup_widget(propdialog, "cacheItemLimit"), feed->cacheLimit > 0);
	g_signal_connect(lookup_widget(propdialog, "feedCacheLimited"), "toggled", G_CALLBACK(on_feed_prop_cache_radio), ui_data);

	g_signal_connect(G_OBJECT (propdialog), "response", G_CALLBACK (on_propdialog_response), ui_data);

	on_feed_prop_filtercheck(GTK_TOGGLE_BUTTON(lookup_widget(propdialog, "filterCheckbox")), ui_data);
	
	/***********************************************************************
	 * Download                                                            *
	 ***********************************************************************/
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(propdialog, "dontUseProxyCheck")), feed->updateOptions->dontUseProxy);

	/***********************************************************************
	 * Advanced                                                            *
	 ***********************************************************************/
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(propdialog, "enclosureDownloadCheck")), feed->encAutoDownload);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(propdialog, "loadItemLinkCheck")), feed->loadItemLink);

	gtk_widget_show_all(propdialog);

	node_unload(node);
}

