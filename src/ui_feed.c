/**
 * @file ui_feed.c UI-related feed processing
 * 
 * Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include <gtk/gtk.h>
#include <libxml/uri.h>
#include "support.h"
#include "feed.h"
#include "conf.h"
#include "callbacks.h"
#include "interface.h"
#include "ui_feed.h"

/********************************************************************
 * Propdialog                                                       *
 *******************************************************************/

struct fp_prop_ui_data {
	feedPtr fp;
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
	GtkWidget *filter;
	GtkWidget *filterCheckbox;
	GtkWidget *filterbox;
	GtkWidget *filterSelectFile;
	GtkWidget *cacheDefaultRadio;
	GtkWidget *cacheDisableRadio;
	GtkWidget *cacheUnlimitedRadio;
	GtkWidget *cacheLimitedRadio;
	GtkWidget *cacheItemLimit;
};

static void on_feed_prop_url_radio(GtkToggleButton *button, gpointer user_data);
static void on_selectfile_pressed(GtkButton *button, gpointer user_data);
static void ui_feed_prop_enable_httpauth(struct fp_prop_ui_data *ui_data, gboolean enable);
static void on_feed_prop_cache_radio(GtkToggleButton *button, gpointer user_data);
static void on_feed_prop_authcheck(GtkToggleButton *button, gpointer user_data);
static void on_feed_prop_filtercheck(GtkToggleButton *button, gpointer user_data);
static void on_propdialog_response(GtkDialog *dialog, gint response_id, gpointer user_data);
static void on_newdialog_response(GtkDialog *dialog, gint response_id, gpointer user_data);
static void on_authdialog_response(GtkDialog *dialog, gint response_id, gpointer user_data);

GtkWidget* ui_feed_authdialog_new (GtkWindow *parent, feedPtr fp, gint flags) {
	GtkWidget		*authdialog;
	struct fp_prop_ui_data	*ui_data;
	gchar *promptStr;
	gchar *source = NULL;
	xmlURIPtr uri;	
	
	ui_data = g_new0(struct fp_prop_ui_data, 1);
	
	/* Create the dialog */
	ui_data->dialog = authdialog = create_authdialog();
	ui_data->fp = fp;
	ui_data->flags = flags;
	gtk_window_set_transient_for(GTK_WINDOW(authdialog), GTK_WINDOW(parent));
	
	/* Auth check box */
	ui_data->username = lookup_widget(authdialog, "usernameEntry");
	ui_data->password = lookup_widget(authdialog, "passwordEntry");
	
	uri = xmlParseURI(BAD_CAST feed_get_source(fp));
	
	if (uri != NULL) {
		if (uri->user != NULL) {
			gchar *user = uri->user;
			gchar *pass = strstr(user, ":");
			if (pass != NULL) {
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
						   feed_get_title(fp), (source != NULL) ? source : _("Unknown source"));
	gtk_label_set_text(GTK_LABEL(lookup_widget(authdialog, "prompt")), promptStr);
	g_free(promptStr);
	if (source != NULL)
		xmlFree(source);
	
	g_signal_connect(G_OBJECT(authdialog), "response",
				  G_CALLBACK (on_authdialog_response), ui_data);
	
	gtk_widget_show_all(authdialog);
	
	return authdialog;
}

GtkWidget* ui_feed_newdialog_new (GtkWindow *parent) {
	GtkWidget *newdialog;
	struct fp_prop_ui_data *ui_data;
	
	ui_data = g_new0(struct fp_prop_ui_data, 1);

	/* Create the dialog */
	ui_data->dialog = newdialog = create_newdialog();
	gtk_window_set_transient_for(GTK_WINDOW(newdialog), GTK_WINDOW(parent));

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

	ui_data->filter = lookup_widget(newdialog, "filterEntry");
	ui_data->filterCheckbox = lookup_widget(newdialog, "filterCheckbox");
	ui_data->filterbox = lookup_widget(newdialog, "filterbox");
	ui_data->filterSelectFile = lookup_widget(newdialog, "filterSelectFile");

	on_feed_prop_filtercheck(GTK_TOGGLE_BUTTON(ui_data->filterCheckbox), ui_data);
	g_signal_connect(ui_data->filterCheckbox, "toggled", G_CALLBACK(on_feed_prop_filtercheck), ui_data);
	g_signal_connect(ui_data->filterSelectFile, "clicked", G_CALLBACK (on_selectfile_pressed), ui_data);

	/* Sensitivity */
	gtk_widget_set_sensitive(ui_data->selectFile, FALSE);

	gtk_widget_grab_default(lookup_widget(newdialog, "newfeedbtn"));
	g_signal_connect (G_OBJECT (newdialog), "response",
				   G_CALLBACK (on_newdialog_response), ui_data);
	
	gtk_widget_show_all(newdialog);

	return newdialog;
}

GtkWidget* ui_feed_propdialog_new (GtkWindow *parent, feedPtr fp) {
	GtkWidget		*propdialog;
	struct fp_prop_ui_data	*ui_data;
	int 			defaultInterval;
	gchar 			*defaultIntervalStr;

	ui_data = g_new0(struct fp_prop_ui_data, 1);
	ui_data->fp = fp;
	
	/* Create the dialog */
	ui_data->dialog = propdialog = create_propdialog();
	gtk_window_set_transient_for(GTK_WINDOW(propdialog), GTK_WINDOW(parent));

	/***********************************************************************
	 * General                                                             *
	 **********************************************************************/

	/* Setup feed name */
	ui_data->feedNameEntry = lookup_widget(propdialog,"feedNameEntry");
	gtk_entry_set_text(GTK_ENTRY(ui_data->feedNameEntry), feed_get_title(fp));

	/* Setup refresh interval */
	ui_data->refreshInterval = lookup_widget(propdialog,"refreshIntervalSpinButton");
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui_data->refreshInterval), feed_get_update_interval(fp));
	
	defaultInterval = feed_get_default_update_interval(fp);
	if (fp->fhp != NULL && fp->fhp->directory == TRUE) {
		defaultIntervalStr = g_strdup(_("Directories cannot be autoupdated."));
		gtk_widget_set_sensitive(ui_data->refreshInterval, FALSE);
	} else if(-1 != defaultInterval)
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

	if (feed_get_source(fp)[0] == '|') {
		gtk_entry_set_text(GTK_ENTRY(ui_data->sourceEntry), &(feed_get_source(fp)[1]));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui_data->cmdRadio), TRUE);
		ui_feed_prop_enable_httpauth(ui_data, FALSE);
		gtk_widget_set_sensitive(ui_data->selectFile, TRUE);
	} else if (strstr(feed_get_source(fp), "://") != NULL) {
		xmlURIPtr uri = xmlParseURI(BAD_CAST feed_get_source(fp));
		xmlChar *parsedUrl;
		if (uri != NULL) {
			if (uri->user != NULL) {
				gchar *user = uri->user;
				gchar *pass = strstr(user, ":");
				if (pass != NULL) {
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
	
	ui_data->filter = lookup_widget(propdialog, "filterEntry");
	ui_data->filterCheckbox = lookup_widget(propdialog, "filterCheckbox");
	ui_data->filterbox = lookup_widget(propdialog, "filterbox");
	ui_data->filterSelectFile = lookup_widget(propdialog, "filterSelectFile");

	if (feed_get_filter(fp) != NULL) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui_data->filterCheckbox), TRUE);
		gtk_entry_set_text(GTK_ENTRY(ui_data->filter), feed_get_filter(fp));
	}
	on_feed_prop_filtercheck(GTK_TOGGLE_BUTTON(ui_data->filterCheckbox), ui_data);
	g_signal_connect(ui_data->filterCheckbox, "toggled", G_CALLBACK(on_feed_prop_filtercheck), ui_data);
	g_signal_connect(ui_data->filterSelectFile, "clicked", G_CALLBACK (on_selectfile_pressed), ui_data);

	/***********************************************************************
	 * Cache                                                               *
	 **********************************************************************/

	/* Cache size radio buttons */
	ui_data->cacheDefaultRadio = lookup_widget(propdialog, "feedCacheDefault");
	ui_data->cacheDisableRadio = lookup_widget(propdialog, "feedCacheDisable");
	ui_data->cacheLimitedRadio = lookup_widget(propdialog, "feedCacheLimited");
	ui_data->cacheUnlimitedRadio = lookup_widget(propdialog, "feedCacheUnlimited");
	ui_data->cacheItemLimit = lookup_widget(propdialog, "cacheItemLimit");

	if (fp->cacheLimit == CACHE_DISABLE)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui_data->cacheDisableRadio), TRUE);
	else if (fp->cacheLimit == CACHE_DEFAULT)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui_data->cacheDefaultRadio), TRUE);
	else if (fp->cacheLimit == CACHE_UNLIMITED)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui_data->cacheUnlimitedRadio), TRUE);
	else {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui_data->cacheLimitedRadio), TRUE);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui_data->cacheItemLimit), fp->cacheLimit);
	}

	gtk_widget_set_sensitive(ui_data->cacheItemLimit, fp->cacheLimit > 0);
	g_signal_connect(ui_data->cacheLimitedRadio, "toggled", G_CALLBACK(on_feed_prop_cache_radio), ui_data);


	g_signal_connect (G_OBJECT (propdialog), "response",
				   G_CALLBACK (on_propdialog_response), ui_data);

	gtk_widget_show_all(propdialog);

	return propdialog;
}

static gchar * ui_feed_dialog_decode_source(struct fp_prop_ui_data *ui_data) {
	gchar	*source = NULL;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->fileRadio))) {
		source = g_strdup(gtk_entry_get_text(GTK_ENTRY(ui_data->sourceEntry)));
	} else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->urlRadio))) {
		/* Add http:// if needed: */
		const gchar *tmp = gtk_entry_get_text(GTK_ENTRY(ui_data->sourceEntry));
		gchar *str, *tmp2;
		if (strstr(tmp, "://") == NULL)
			str = g_strdup_printf("http://%s",tmp);
		else
			str = g_strdup(tmp);

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

	if (response_id == GTK_RESPONSE_OK) {
		xmlURIPtr uri;
		xmlChar *user, *pass, *sourceUrl;

		/* Source */
		uri = xmlParseURI(BAD_CAST feed_get_source(ui_data->fp));
		
		if (uri == NULL) {
			/* FIXME: message dialog to tell user that something very unexpected happened. */
			g_free(ui_data);
			return;
		}
		if (uri->user != NULL)
			xmlFree(uri->user);

		user = BAD_CAST gtk_entry_get_text(GTK_ENTRY(ui_data->username));
		pass = BAD_CAST gtk_entry_get_text(GTK_ENTRY(ui_data->password));
		uri->user = g_strdup_printf("%s:%s", user, pass);

		sourceUrl = xmlSaveUri(uri);
		if (sourceUrl != NULL) {
			feed_set_source(ui_data->fp, sourceUrl);
			xmlFree(sourceUrl);
		}
		/* Filter handling */
		feed_schedule_update(ui_data->fp, ui_data->flags);
		xmlFreeURI(uri);
	}


	gtk_widget_destroy(GTK_WIDGET(dialog));
	g_free(ui_data);
}


static void on_newdialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
	struct fp_prop_ui_data *ui_data = (struct fp_prop_ui_data*)user_data;

	if (response_id == GTK_RESPONSE_OK) {
		gchar *source = NULL;
		const gchar *filter = NULL;

		/* Source */
		source = ui_feed_dialog_decode_source(ui_data);

		/* Filter handling */
		filter = gtk_entry_get_text(GTK_ENTRY(ui_data->filter));
		if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->filterCheckbox)) ||
		    !strcmp(filter,"")) { /* Maybe this should be a test to see if the file exists? */
			filter = NULL;
		} 
		ui_feedlist_new_subscription(source, filter, FEED_REQ_SHOW_PROPDIALOG | FEED_REQ_RESET_TITLE | FEED_REQ_RESET_UPDATE_INT | FEED_REQ_AUTO_DISCOVER);
		g_free(source);
	}


	gtk_widget_destroy(GTK_WIDGET(dialog));
	g_free(ui_data);
}

static void on_propdialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
	struct fp_prop_ui_data *ui_data = (struct fp_prop_ui_data*)user_data;
	
	if (response_id == GTK_RESPONSE_OK) {
		gchar *newSource;
		const gchar *newFilter;
		gboolean needsUpdate = FALSE;
		
		/* General*/
		feed_set_title(ui_data->fp, gtk_entry_get_text(GTK_ENTRY(ui_data->feedNameEntry)));
		feed_set_update_interval(ui_data->fp, gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ui_data->refreshInterval)));
		
		/* Source */
		newSource = ui_feed_dialog_decode_source(ui_data);
		
		/* Filter handling */
		newFilter = gtk_entry_get_text(GTK_ENTRY(ui_data->filter));
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->filterCheckbox)) &&
		    strcmp(newFilter,"")) { /* Maybe this should be a test to see if the file exists? */
			if (feed_get_filter(ui_data->fp) == NULL ||
			    strcmp(newFilter, feed_get_filter(ui_data->fp))) {
				feed_set_filter(ui_data->fp, newFilter);
				needsUpdate = TRUE;
			}
		} else {
			if (feed_get_filter(ui_data->fp) != NULL) {
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

		/* Cache handling */
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->cacheDefaultRadio)))
			ui_data->fp->cacheLimit = CACHE_DEFAULT;
		else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->cacheDisableRadio)))
			ui_data->fp->cacheLimit = CACHE_DISABLE;
		else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->cacheUnlimitedRadio)))
			ui_data->fp->cacheLimit = CACHE_UNLIMITED;
		else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->cacheLimitedRadio))) {
			ui_data->fp->cacheLimit = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ui_data->cacheItemLimit));
		}

		ui_feedlist_update();
		conf_feedlist_schedule_save();
		if (needsUpdate)
			feed_schedule_update(ui_data->fp, FEED_REQ_AUTH_DIALOG | FEED_REQ_PRIORITY_HIGH);
	}

	g_free(ui_data);
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void on_feed_prop_filtercheck(GtkToggleButton *button, gpointer user_data) {
	struct fp_prop_ui_data *ui_data = (struct fp_prop_ui_data*)user_data;
	gboolean filter = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->filterCheckbox));
	gtk_widget_set_sensitive(ui_data->filterbox, filter);
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
	
	if (filename == NULL)
		return;
	
	utfname = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);

	if (utfname != NULL) {
		if (ui_data->selector == 'u')
			gtk_entry_set_text(GTK_ENTRY(ui_data->sourceEntry), utfname);
		else
			gtk_entry_set_text(GTK_ENTRY(ui_data->filter), utfname);
	}
	
	g_free(utfname);
}

static void on_selectfile_pressed(GtkButton *button, gpointer user_data) {
	struct fp_prop_ui_data *ui_data = (struct fp_prop_ui_data*)user_data;
	const gchar *utfname;
	gchar *name;
	
	if (GTK_WIDGET(button) == ui_data->selectFile) {
		ui_data->selector = 'u';
		utfname =  gtk_entry_get_text(GTK_ENTRY(ui_data->sourceEntry));
	} else {
		ui_data->selector = 'f';
		utfname =  gtk_entry_get_text(GTK_ENTRY(ui_data->filter));
	}
	
	name = g_filename_from_utf8(utfname,-1,NULL, NULL, NULL);
	ui_choose_file(_("Choose File"), GTK_WINDOW(ui_data->dialog), GTK_STOCK_OPEN, FALSE, on_selectfileok_clicked, name, ui_data);
	g_free(name);
}
 
static void ui_feed_prop_enable_httpauth(struct fp_prop_ui_data *ui_data, gboolean enable) {
	gboolean on;
	if (ui_data->authcheckbox != NULL) {
		on = enable && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->authcheckbox));
		gtk_widget_set_sensitive(ui_data->authcheckbox,enable);
		gtk_widget_set_sensitive(ui_data->credTable,on);
	}
}

static void on_feed_prop_cache_radio(GtkToggleButton *button, gpointer user_data) {
	struct fp_prop_ui_data *ui_data = (struct fp_prop_ui_data*)user_data;
	gboolean limited = gtk_toggle_button_get_active(button);
	gtk_widget_set_sensitive(ui_data->cacheItemLimit, limited);
}
