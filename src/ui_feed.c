/**
 * @file ui_feed.c	UI actions concerning a single feed
 *
 * Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>
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
#include "update.h"
#include "interface.h"
#include "ui_feed.h"
#include "ui_queue.h"
#include "favicon.h"
#include "ui_notification.h"

/**
 * Creates a new error description according to the passed
 * HTTP status and the feeds parser errors. If the HTTP
 * status is a success status and no parser errors occurred
 * no error messages is created. The created error message 
 * can be queried with feed_get_error_description().
 *
 * @param fp		feed
 * @param httpstatus	HTTP status
 * @param resultcode the update code's return code (see update.h)
 */
static void ui_feed_set_error_description(feedPtr fp, gint httpstatus, gint resultcode) {
	gchar		*tmp1, *tmp2 = NULL, *buffer = NULL;
	gboolean	errorFound = FALSE;

	g_assert(NULL != fp);
	g_free(fp->errorDescription);
	fp->errorDescription = NULL;
	
	if(((httpstatus >= 200) && (httpstatus < 400)) && /* HTTP codes starting with 2 and 3 mean no error */
	   (NULL == fp->parseErrors))
		return;
	addToHTMLBuffer(&buffer, UPDATE_ERROR_START);
	
	if((200 != httpstatus) || (resultcode != NET_ERR_OK)) {
		/* first specific codes */
		switch(httpstatus) {
			case 401:tmp2 = g_strdup(_("You are unauthorized to download this feed. Please update your username and "
								  "password in the feed properties dialog box."));break;
			case 402:tmp2 = g_strdup(_("Payment Required"));break;
			case 403:tmp2 = g_strdup(_("Access Forbidden"));break;
			case 404:tmp2 = g_strdup(_("Resource Not Found"));break;
			case 405:tmp2 = g_strdup(_("Method Not Allowed"));break;
			case 406:tmp2 = g_strdup(_("Not Acceptable"));break;
			case 407:tmp2 = g_strdup(_("Proxy Authentication Required"));break;
			case 408:tmp2 = g_strdup(_("Request Time-Out"));break;
			case 410:tmp2 = g_strdup(_("Gone. Resource doesn't exist. Please unsubscribe!"));break;
		}
		/* Then, netio errors */
		if(tmp2 == NULL) {
			switch(resultcode) {
			case NET_ERR_URL_INVALID:    tmp2 = g_strdup(_("URL is invalid")); break;
			case NET_ERR_UNKNOWN:
			case NET_ERR_CONN_FAILED:
			case NET_ERR_SOCK_ERR:       tmp2 = g_strdup(_("Error connecting to remote host")); break;
			case NET_ERR_HOST_NOT_FOUND: tmp2 = g_strdup(_("Hostname could not be found")); break;
			case NET_ERR_CONN_REFUSED:   tmp2 = g_strdup(_("Network connection was refused by the remote host")); break;
			case NET_ERR_TIMEOUT:        tmp2 = g_strdup(_("Remote host did not finish sending data")); break;
				/* Transfer errors */
			case NET_ERR_REDIRECT_COUNT_ERR: tmp2 = g_strdup(_("Too many HTTP redirects were encountered")); break;
			case NET_ERR_REDIRECT_ERR:
			case NET_ERR_HTTP_PROTO_ERR: 
			case NET_ERR_GZIP_ERR:           tmp2 = g_strdup(_("Remote host sent an invalid response")); break;
				/* These are handled above	
				   case NET_ERR_HTTP_410:
				   case NET_ERR_HTTP_404:
				   case NET_ERR_HTTP_NON_200:
				*/
			case NET_ERR_AUTH_FAILED:
			case NET_ERR_AUTH_NO_AUTHINFO: tmp2 = g_strdup(_("Authentication failed")); break;
			case NET_ERR_AUTH_GEN_AUTH_ERR:
			case NET_ERR_AUTH_UNSUPPORTED: tmp2 = g_strdup(_("Webserver's authentication method incompatible with Liferea")); break;
			}
		}
		/* And generic messages in the unlikely event that the above didn't work */
		if(NULL == tmp2) {
			switch(httpstatus / 100) {
			case 3:tmp2 = g_strdup(_("Feed not available: Server requested unsupported redirection!"));break;
			case 4:tmp2 = g_strdup(_("Client Error"));break;
			case 5:tmp2 = g_strdup(_("Server Error"));break;
			default:tmp2 = g_strdup(_("(unknown networking error happened)"));break;
			}
		}
		errorFound = TRUE;
		tmp1 = g_strdup_printf(HTTP_ERROR_TEXT, httpstatus, tmp2);
		addToHTMLBuffer(&buffer, tmp1);
		g_free(tmp1);
		g_free(tmp2);
	}
	
	/* add parsing error messages */
	if(NULL != fp->parseErrors) {
		if(errorFound)
			addToHTMLBuffer(&buffer, HTML_NEWLINE);			
		errorFound = TRUE;
		tmp1 = g_strdup_printf(PARSE_ERROR_TEXT, fp->parseErrors);
		addToHTMLBuffer(&buffer, tmp1);
		g_free(tmp1);
	}
	
	/* if none of the above error descriptions matched... */
	if(!errorFound) {
		tmp1 = g_strdup_printf(_("There was a problem while reading this subscription. Please check the URL and console output."));
		addToHTMLBuffer(&buffer, tmp1);
		g_free(tmp1);
	}
	
	addToHTMLBuffer(&buffer, UPDATE_ERROR_END);
	fp->errorDescription = buffer;
}

/** handles completed feed update requests */
void ui_feed_process_update_result(struct request *request) {
	feedPtr			fp = (feedPtr)request->user_data;
	feedHandlerPtr		fhp;
	gchar			*old_title, *old_source;
	gint			old_update_interval;
	
	ui_lock();
	g_assert(NULL != request);

	feed_load(fp);

	/* no matter what the result of the update is we need to save update
	   status and the last update time to cache */
	fp->needsCacheSave = TRUE;
	
	feed_set_available(fp, TRUE);

	if(401 == request->httpstatus) { /* unauthorized */
		feed_set_available(fp, FALSE);
		if(request->flags & FEED_REQ_AUTH_DIALOG)
			ui_feed_authdialog_new(GTK_WINDOW(mainwindow), fp, request->flags);
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

		/* parse the new downloaded feed into new_fp */
		fhp = feed_parse(fp, request->data, request->size, request->flags & FEED_REQ_AUTO_DISCOVER);
		if(fhp == NULL) {
			feed_set_available(fp, FALSE);
			fp->parseErrors = g_strdup_printf(_("<p>Could not detect the type of this feed! Please check if the source really points to a resource provided in one of the supported syndication formats!</p>%s"), fp->parseErrors);
		} else {
			fp->fhp = fhp;
			
			/* restore user defined properties if necessary */
			if(!(request->flags & FEED_REQ_RESET_TITLE))
				feed_set_title(fp, old_title);
				
			if(!(request->flags & FEED_REQ_AUTO_DISCOVER))
				feed_set_source(fp, old_source);

			if(!(request->flags & FEED_REQ_RESET_UPDATE_INT))
				feed_set_update_interval(fp, old_update_interval);
				
			g_free(old_title);
			g_free(old_source);

			ui_mainwindow_set_status_bar(_("\"%s\" updated..."), feed_get_title(fp));

			if((feedPtr)ui_feedlist_get_selected() == fp)
				ui_itemlist_load((nodePtr)fp);
			
			if(request->flags & FEED_REQ_SHOW_PROPDIALOG)
				ui_feed_propdialog_new(GTK_WINDOW(mainwindow),fp);
		}
	} else {	
		ui_mainwindow_set_status_bar(_("\"%s\" is not available"), feed_get_title(fp));
		feed_set_available(fp, FALSE);
	}
	
	ui_feed_set_error_description(fp, request->httpstatus, request->returncode);

	fp->request = NULL; 
	ui_notification_update(fp);	
	ui_feedlist_update();
	
	if(request->flags & FEED_REQ_DOWNLOAD_FAVICON)
		favicon_download(fp);
	
	feed_unload(fp);
	
	ui_unlock();
}

/** determines the feeds favicon or default icon */
static GdkPixbuf* ui_feed_get_icon(feedPtr fp) {
	gpointer	favicon;
	
	g_assert(FST_FOLDER != fp->type);
		
	if(!feed_get_available(fp))
		return icons[ICON_UNAVAILABLE];

	if(NULL != (favicon = feed_get_favicon(fp)))
		return favicon;
	
	if(fp->fhp != NULL && fp->fhp->icon < MAX_ICONS)
		return icons[fp->fhp->icon];

	/* And default to the available icon.... */
	return icons[ICON_AVAILABLE];
}

/** updating of a single feed list entry */
void ui_feed_update(feedPtr fp) {
	GtkTreeModel	*model;
	GtkTreeIter	*iter;
	gchar		*label, *tmp;
	int		count;
	
	if(fp->ui_data == NULL)
		return;
	
	iter = &((ui_data*)fp->ui_data)->row;
	model =  GTK_TREE_MODEL(feedstore);
	
	g_assert(FST_FOLDER != fp->type);
	
	count = feed_get_unread_counter(fp);
	label = unhtmlize(g_strdup(feed_get_title(fp)));
	/* FIXME: Unescape text here! */
	tmp = g_markup_escape_text(label,-1);
	g_free(label);
	if(count > 0)
		label = g_strdup_printf("<span weight=\"bold\">%s (%d)</span>", tmp, count);
	else
		label = g_strdup_printf("%s", tmp);
	g_free(tmp);
	
	gtk_tree_store_set(feedstore, iter, FS_LABEL, label,
	                                    FS_UNREAD, count,
	                                    FS_ICON, ui_feed_get_icon(fp),
	                                    -1);
	g_free(label);
}

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
