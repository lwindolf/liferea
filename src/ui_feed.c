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
	GtkWidget *filters;
	GtkWidget *cacheDefaultRadio;
	GtkWidget *cacheDisableRadio;
	GtkWidget *cacheUnlimitedRadio;
	GtkWidget *cacheLimitedRadio;
	GtkWidget *cacheItemLimit;
};

static void on_feed_prop_url_radio(GtkToggleButton *button, gpointer user_data);
static void on_selectfile_pressed(GtkButton *button, gpointer user_data);
static void on_selectfileok_clicked(GtkButton *button, gpointer user_data);
static void ui_feed_prop_enable_httpauth(struct fp_prop_ui_data *ui_data, gboolean enable);
static void on_feed_prop_cache_radio(GtkToggleButton *button, gpointer user_data);
static void on_feed_prop_authcheck(GtkToggleButton *button, gpointer user_data);
static void on_propdialog_response(GtkDialog       *dialog,
							gint             response_id,
							gpointer         user_data);
static void feed_prop_list_filters(GtkWidget *menu);

GtkWidget* ui_feed_propdialog_new (GtkWindow *parent, feedPtr fp) {
	GtkWidget *propdialog;
	struct fp_prop_ui_data *ui_data = g_new0(struct fp_prop_ui_data, 1);
	int defaultInterval;
	gchar *defaultIntervalStr;

	ui_data->fp = fp;
	
	/* Create the dialog */
	propdialog = create_propdialog();
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
	} else {
		/* File */
		gtk_entry_set_text(GTK_ENTRY(ui_data->sourceEntry), feed_get_source(fp));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui_data->fileRadio), TRUE);
		ui_feed_prop_enable_httpauth(ui_data, FALSE);
	}
	
	ui_data->filters = lookup_widget(propdialog, "filtermenu");
	feed_prop_list_filters(gtk_option_menu_get_menu(GTK_OPTION_MENU(ui_data->filters)));
	
	/* Sensitivity */
	gtk_widget_set_sensitive(ui_data->selectFile, FALSE);
	
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

void on_propdialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
	struct fp_prop_ui_data *ui_data = (struct fp_prop_ui_data*)user_data;

	if (response_id == GTK_RESPONSE_OK) {
		gchar *newSource = NULL;
		const gchar *filter;

		/* General*/
		feed_set_title(ui_data->fp, gtk_editable_get_chars(GTK_EDITABLE(ui_data->feedNameEntry), 0, -1));
		feed_set_update_interval(ui_data->fp, gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ui_data->refreshInterval)));

		/* Source */
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->fileRadio))) {
			newSource = g_strdup(gtk_entry_get_text(GTK_ENTRY(ui_data->sourceEntry)));
		} else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->urlRadio))) {
			
			/* Use the values in the textboxes if also specified in the URL! */
			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->authcheckbox))) {
				xmlURIPtr uri = xmlParseURI(BAD_CAST gtk_entry_get_text(GTK_ENTRY(ui_data->sourceEntry)));
				xmlChar *source;
				if (uri != NULL) {
					xmlFree(uri->user);
					uri->user = g_strdup_printf("%s:%s",
										   gtk_entry_get_text(GTK_ENTRY(ui_data->username)),
										   gtk_entry_get_text(GTK_ENTRY(ui_data->password)));
					source = xmlSaveUri(uri);
					newSource = g_strdup(BAD_CAST source);
					g_free(uri->user);
					uri->user = NULL;
					xmlFree(source);
					xmlFreeURI(uri);
				} else
					newSource = g_strdup(gtk_entry_get_text(GTK_ENTRY(ui_data->sourceEntry)));
			} else {
				newSource = g_strdup(gtk_entry_get_text(GTK_ENTRY(ui_data->sourceEntry)));
			}
			
		} else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->cmdRadio))) {
			newSource = g_strdup_printf("|%s", gtk_entry_get_text(GTK_ENTRY(ui_data->sourceEntry)));
		}
		
		filter = ui_data->filters;
		
		/* if URL has changed... */
		if(strcmp(newSource, feed_get_source(ui_data->fp))) {
			feed_set_source(ui_data->fp, newSource);
			feed_schedule_update(ui_data->fp);
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
	}

	g_free(ui_data);
	gtk_widget_destroy(GTK_WIDGET(dialog));
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

	ui_feed_prop_enable_httpauth(ui_data, url);
	gtk_widget_set_sensitive(ui_data->selectFile, file);
}

static void on_selectfileok_clicked(GtkButton *button, gpointer user_data) {
	struct fp_prop_ui_data *ui_data = (struct fp_prop_ui_data*)user_data;
	GtkWidget *filedialog = g_object_get_data(G_OBJECT(button), "dialog");
	const gchar *name;
	gchar *utfname;

	name = gtk_file_selection_get_filename(GTK_FILE_SELECTION(filedialog));
	utfname = g_filename_to_utf8(name, -1, NULL, NULL, NULL);
	if (utfname != NULL)
		gtk_entry_set_text(GTK_ENTRY(ui_data->sourceEntry), utfname);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui_data->fileRadio), TRUE);
	gtk_widget_destroy(filedialog);

	g_free(utfname);
}

static void on_selectfile_pressed(GtkButton *button, gpointer user_data) {
	GtkWidget *filedialog;
	GtkWidget	*okbutton;
	
	struct fp_prop_ui_data *ui_data = (struct fp_prop_ui_data*)user_data;
	const gchar *utfname = gtk_entry_get_text(GTK_ENTRY(ui_data->sourceEntry));
	gchar *name = g_filename_from_utf8(utfname,-1,NULL, NULL, NULL);

	filedialog = create_fileselection();

	if(NULL == (okbutton = lookup_widget(filedialog, "fileselectbtn")))
		g_error(_("internal error! could not find file dialog select button!"));

	if (name != NULL)
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(filedialog), name);
	
	gtk_object_set_data(GTK_OBJECT(okbutton), "dialog", filedialog);
	g_signal_connect((gpointer) okbutton, "clicked", G_CALLBACK (on_selectfileok_clicked), user_data);
	gtk_widget_show(filedialog);
	g_free(name);
}
 
static void ui_feed_prop_enable_httpauth(struct fp_prop_ui_data *ui_data, gboolean enable) {
	gboolean on = enable && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->authcheckbox));

	gtk_widget_set_sensitive(ui_data->authcheckbox,enable);
	gtk_widget_set_sensitive(ui_data->credTable,on);
}

static void on_feed_prop_cache_radio(GtkToggleButton *button, gpointer user_data) {
	struct fp_prop_ui_data *ui_data = (struct fp_prop_ui_data*)user_data;
	gboolean limited = gtk_toggle_button_get_active(button);
	gtk_widget_set_sensitive(ui_data->cacheItemLimit, limited);
}

static void feed_prop_list_filters(GtkWidget *menu) {
	gchar *dirpath = g_strdup_printf("%s/filters/", getCachePath());
	GDir  *dir = g_dir_open(dirpath, 0, NULL);
	if (dir != NULL) {
		const gchar *entry;
		while ((entry = g_dir_read_name(dir)) != NULL) {
			gchar *full = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s", dirpath, entry);
			if (g_file_test(full, G_FILE_TEST_IS_REGULAR) &&
			    g_file_test(full, G_FILE_TEST_IS_EXECUTABLE)) {
				GtkWidget *item = gtk_menu_item_new_with_label(entry);
				gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
			}
			g_free(full);
		}
	g_dir_close(dir);
	}
	g_free(dirpath);
}
