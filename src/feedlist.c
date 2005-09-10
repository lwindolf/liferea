/**
 * @file feedlist.c feedlist handling
 *
 * Copyright (C) 2005 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2005 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2005 Raphaël Slinckx <raphael@slinckx.net>
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

#include <libxml/uri.h>
#include "support.h"
#include "feed.h"
#include "feedlist.h"
#include "item.h"
#include "itemlist.h"
#include "ui_mainwindow.h"
#include "ui_feedlist.h"
#include "ui_htmlview.h"
#include "ui_notification.h"
#include "ui_tray.h"
#include "update.h"
#include "conf.h"
#include "debug.h"
#include "favicon.h"
#include "ui_feed.h"
#include "callbacks.h"

static int unreadCount = 0;
static int newCount = 0;

static nodePtr	selectedNode = NULL;
extern nodePtr		displayed_node;

/* helper functions */

typedef enum {
	NEW_ITEM_COUNT,
	UNREAD_ITEM_COUNT
} countType;

/* statistic handling methods */

int feedlist_get_unread_item_count(void) { return unreadCount; }
int feedlist_get_new_item_count(void) { return newCount; }

void feedlist_update_counters(gint unreadDiff, gint newDiff) {

	unreadCount += unreadDiff;
	newCount += newDiff;

	if((0 != newDiff) || (0 != unreadDiff))
		ui_tray_update();
}

static void feedlist_unset_new_items(nodePtr np) {
	GSList *iter;
	
	if(0 == ((feedPtr)np)->newCount)
		return;
		
	feedlist_load_feed((feedPtr)np);
	
	iter = feed_get_item_list((feedPtr)np);
	while(NULL != iter) {
		item_set_new_status((itemPtr)iter->data, FALSE);
		iter = g_slist_next(iter);

	}
		
	feedlist_unload_feed((feedPtr)np);
}

void feedlist_reset_new_item_count(void) {

	if(0 != newCount) {
		ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED | ACTION_FILTER_DIRECTORY, feedlist_unset_new_items);
		newCount = 0;
		ui_tray_update();
	}
}

void feedlist_add_feed(folderPtr parent, feedPtr feed, gint position) {

	ui_feedlist_add(parent, (nodePtr)feed, position);
}

void feedlist_add_folder(folderPtr parent, folderPtr folder, gint position) {

	ui_feedlist_add(parent, (nodePtr)folder, position);
}

void feedlist_update_feed(nodePtr np) {

	if(FST_FEED == np->type)	/* don't process vfolders */
		feed_schedule_update((feedPtr)np, FEED_REQ_PRIORITY_HIGH);
}

static void feedlist_remove_feed(nodePtr np) { 
	
	ui_notification_remove_feed((feedPtr)np);	/* removes an existing notification for this feed */
	ui_folder_remove_node(np);
	ui_feedlist_update();
	
	feedlist_load_feed((feedPtr)np);
	feed_free((feedPtr)np);	
}

static void feedlist_remove_folder(nodePtr np) {

	ui_feedlist_do_for_all(np, ACTION_FILTER_CHILDREN | ACTION_FILTER_ANY, feedlist_remove_node);
	ui_feedlist_update();
	folder_free((folderPtr)np);	
}

void feedlist_remove_node(nodePtr np) {

	if(NULL == np)
		return;
		
	if(np == displayed_node) {
		itemlist_load(NULL);
		ui_htmlview_clear(ui_mainwindow_get_active_htmlview());
	}

	if((FST_FEED == np->type) || (FST_VFOLDER == np->type))		
		feedlist_remove_feed(np);
	else
		feedlist_remove_folder(np);
}

/* methods to load/unload feeds from memory */

gboolean feedlist_load_feed(feedPtr fp) {
	gboolean loaded;

	if(FST_VFOLDER == feed_get_type(fp)) {
		debug0(DEBUG_CACHE, "it's a vfolder, nothing to do...");
		return TRUE;
	}
	
	if(0 != (fp->loaded)++) {
		debug0(DEBUG_CACHE, "feed already loaded!\n");
		return TRUE;
	}
	
	/* the following is necessary to prevent counting unread 
	   or new items multiple times (on each loading) */
	unreadCount -= fp->unreadCount;
	newCount -= fp->newCount;
	
	loaded = feed_load(fp);

	return loaded;
}

void feedlist_unload_feed(feedPtr fp) {

	g_assert(0 <= fp->loaded);	/* could indicate bad loaded reference counting */

	if(FST_VFOLDER == feed_get_type(fp)) {
		debug0(DEBUG_CACHE, "it's a vfolder, nothing to do...");
		return;
	}

	if(0 != fp->loaded)
		feed_unload(fp);
}

/* auto updating methods */

static void feedlist_check_update_counter(feedPtr fp) {
	GTimeVal	now;
	gint		interval;

	g_get_current_time(&now);
	interval = feed_get_update_interval(fp);
	
	if(-2 >= interval)
		return;		/* don't update this feed */
		
	if(-1 == interval)
		interval = getNumericConfValue(DEFAULT_UPDATE_INTERVAL);
	
	if(interval > 0)
		if(fp->lastPoll.tv_sec + interval*60 <= now.tv_sec)
			feed_schedule_update(fp, 0);

	/* And check for favicon updating */
	if(fp->lastFaviconPoll.tv_sec + 30*24*60*60 <= now.tv_sec)
		favicon_download(fp);
}

gboolean feedlist_auto_update(void *data) {

	debug_enter("feedlist_auto_update");
	if(download_is_online()) {
		ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED, (gpointer)feedlist_check_update_counter);
	} else {
		debug0(DEBUG_UPDATE, "no update processing because we are offline!");
	}
	debug_exit("feedlist_auto_update");

	return TRUE;
}

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
static void feed_set_error_description(feedPtr fp, gint httpstatus, gint resultcode, gchar *filterErrors) {
	gchar		*tmp1, *tmp2 = NULL, *buffer = NULL;
	gboolean	errorFound = FALSE;

	g_assert(NULL != fp);
	g_free(fp->errorDescription);
	fp->errorDescription = NULL;
	
	if(((httpstatus >= 200) && (httpstatus < 400)) && /* HTTP codes starting with 2 and 3 mean no error */
	   (NULL == filterErrors) && (NULL == fp->parseErrors))
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
			case NET_ERR_PROTO_INVALID:    tmp2 = g_strdup(_("Unsupported network protocol")); break;
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
	
	/* add filtering error messages */
	if(NULL != filterErrors) {	
		errorFound = TRUE;
		tmp1 = g_markup_printf_escaped(FILTER_ERROR_TEXT2, _("Show Details"), filterErrors);
		addToHTMLBuffer(&buffer, FILTER_ERROR_TEXT);
		addToHTMLBuffer(&buffer, tmp1);
		g_free(tmp1);
	}
	
	/* add parsing error messages */
	if(NULL != fp->parseErrors) {
		errorFound = TRUE;
		tmp1 = g_strdup_printf(PARSE_ERROR_TEXT2, _("Show Details"), fp->parseErrors);
		addToHTMLBuffer(&buffer, PARSE_ERROR_TEXT);
		addToHTMLBuffer(&buffer, tmp1);
		if (feed_get_source(fp) != NULL && (NULL != strstr(feed_get_source(fp), "://"))) {
			xmlChar *escsource;
			addToHTMLBufferFast(&buffer,_("<br>You may want to validate the feed using "
			                              "<a href=\"http://feedvalidator.org/check.cgi?url="));
			escsource = xmlURIEscapeStr(feed_get_source(fp),NULL);
			addToHTMLBufferFast(&buffer,escsource);
			xmlFree(escsource);
			addToHTMLBuffer(&buffer,_("\">FeedValidator</a>."));
		}
		addToHTMLBuffer(&buffer, "</span>");
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
	
	g_assert(NULL != request);

	feedlist_load_feed(fp);

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

		/* parse the new downloaded feed into fp */
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

			if(request->flags & FEED_REQ_RESET_UPDATE_INT)
				feed_set_update_interval(fp, feed_get_default_update_interval(fp));
			else
				feed_set_update_interval(fp, old_update_interval);
				
			g_free(old_title);
			g_free(old_source);

			ui_mainwindow_set_status_bar(_("\"%s\" updated..."), feed_get_title(fp));

			itemlist_reload((nodePtr)fp);
			
			if(request->flags & FEED_REQ_SHOW_PROPDIALOG)
				ui_feed_propdialog_new(GTK_WINDOW(mainwindow),fp);
		}
	} else {	
		ui_mainwindow_set_status_bar(_("\"%s\" is not available"), feed_get_title(fp));
		feed_set_available(fp, FALSE);
	}
	
	feed_set_error_description(fp, request->httpstatus, request->returncode, request->filterErrors);

	fp->request = NULL; 

	if(request->flags & FEED_REQ_DOWNLOAD_FAVICON)
		favicon_download(fp);

	/* update UI presentations */
	ui_notification_update(fp);
	ui_feedlist_update();

	feedlist_unload_feed(fp);
}

/* direct user callbacks */

void feedlist_selection_changed(nodePtr np) {

	if(np != selectedNode) {
		selectedNode = np;
	
		/* when the user selects a feed in the feed list we
		   assume that he got notified of the new items or
		   isn't interested in the event anymore... */
		feedlist_reset_new_item_count();
	}
}

void on_menu_delete(GtkMenuItem *menuitem, gpointer user_data) {

	feedlist_remove_node((nodePtr)ui_feedlist_get_selected());
}

void on_popup_refresh_selected(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	nodePtr ptr = (nodePtr)callback_data;

	if(ptr == NULL) {
		ui_show_error_box(_("You have to select a feed entry"));
		return;
	}

	if(download_is_online()) {
		if(FST_FEED == ptr->type)
			feed_schedule_update((feedPtr)ptr, FEED_REQ_PRIORITY_HIGH);
		else
			ui_feedlist_do_for_all(ptr, ACTION_FILTER_FEED, feedlist_update_feed);
	} else
		ui_mainwindow_set_status_bar(_("Liferea is in offline mode. No update possible."));
}

void on_refreshbtn_clicked(GtkButton *button, gpointer user_data) { 

	ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED, feedlist_update_feed);
}

void on_menu_feed_update(GtkMenuItem *menuitem, gpointer user_data) {

	on_popup_refresh_selected((gpointer)ui_feedlist_get_selected(), 0, NULL);
}

void on_menu_update(GtkMenuItem *menuitem, gpointer user_data) {
	gpointer ptr = (gpointer)ui_feedlist_get_selected();
	
	if(ptr != NULL) {
		on_popup_refresh_selected((gpointer)ptr, 0, NULL);
	} else {
		g_warning("You have found a bug in Liferea. You must select a node in the feedlist to do what you just did.");
	}
}

void on_popup_allunread_selected(void) {
	nodePtr	np;
	
	if(NULL != (np = ui_feedlist_get_selected())) {
		itemlist_mark_all_read(np);
		ui_feedlist_update();
	}
}

void on_popup_allfeedsunread_selected(void) {

	ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED, itemlist_mark_all_read);
}

void on_popup_mark_as_read(gpointer callback_data, guint callback_action, GtkWidget *widget) {

	on_popup_allunread_selected();
}

void on_popup_delete(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	
	feedlist_remove_node((nodePtr)callback_data);
}

/*------------------------------------------------------------------------------*/
/* DBUS support for new subscriptions                                           */
/*------------------------------------------------------------------------------*/

#ifdef USE_DBUS

static DBusHandlerResult
ui_feedlist_dbus_subscribe (DBusConnection *connection, DBusMessage *message)
{
	DBusError error;
	DBusMessage *reply;
	char *s;
	gboolean done = TRUE;
	
	/* Retreive the dbus message arguments (the new feed url) */	
	dbus_error_init (&error);
	if (!dbus_message_get_args (message, &error, DBUS_TYPE_STRING, &s, DBUS_TYPE_INVALID))
	{
		fprintf (stderr, "*** ui_feedlist.c: Error while retreiving message parameter, expecting a string url: %s | %s\n", error.name,  error.message);
		reply = dbus_message_new_error (message, error.name, error.message);
		dbus_connection_send (connection, reply, NULL);
		dbus_message_unref (reply);
		dbus_error_free(&error);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}
	dbus_error_free(&error);


	/* Subscribe the feed */
	ui_feedlist_new_subscription(s, NULL, FEED_REQ_SHOW_PROPDIALOG | FEED_REQ_RESET_TITLE | FEED_REQ_RESET_UPDATE_INT);

	/* Acknowledge the new feed by returning true */
	reply = dbus_message_new_method_return (message);
	if (reply != NULL)
	{
#if (DBUS_VERSION == 1)
		dbus_message_append_args (reply, DBUS_TYPE_BOOLEAN, done,DBUS_TYPE_INVALID);
#elif (DBUS_VERSION == 2)
		dbus_message_append_args (reply, DBUS_TYPE_BOOLEAN, &done,DBUS_TYPE_INVALID);
#endif
		dbus_connection_send (connection, reply, NULL);
		dbus_message_unref (reply);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	else
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


static DBusHandlerResult
ui_feedlist_dbus_message_handler (DBusConnection *connection, DBusMessage *message, void *user_data)
{
	const char  *method;
	
	if (connection == NULL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	if (message == NULL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	
	method = dbus_message_get_member (message);
	if (strcmp (DBUS_RSS_METHOD, method) == 0)
		return ui_feedlist_dbus_subscribe (connection, message);
	else
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void
ui_feedlist_dbus_connect ()
{
	DBusError       error;
	DBusConnection *connection;
	DBusObjectPathVTable feedreader_vtable = { NULL, ui_feedlist_dbus_message_handler, NULL};

	/* Get the Session bus */
	dbus_error_init (&error);
	connection = dbus_bus_get (DBUS_BUS_SESSION, &error);
	if (connection == NULL || dbus_error_is_set (&error))
	{
		fprintf (stderr, "*** ui_feedlist.c: Failed get session dbus: %s | %s\n", error.name,  error.message);
		dbus_error_free (&error);
     	return;
	}
	dbus_error_free (&error);
    
	/* Various inits */
	dbus_connection_set_exit_on_disconnect (connection, FALSE);
	dbus_connection_setup_with_g_main (connection, NULL);
	    
	/* Register for the FeedReader service on the bus, so we get method calls */
#if (DBUS_VERSION == 1)
	dbus_bus_acquire_service (connection, DBUS_RSS_SERVICE, 0, &error);
#elif (DBUS_VERSION == 2)
	dbus_bus_request_name (connection, DBUS_RSS_SERVICE, 0, &error);
#else
#error Unknown DBUS version passed to ui_feedlist
#endif
	if (dbus_error_is_set (&error))
	{
		fprintf (stderr, "*** ui_feedlist.c: Failed to get dbus service: %s | %s\n", error.name, error.message);
		dbus_error_free (&error);
		return;
	}
	dbus_error_free (&error);
	
	/* Register the object path so we can receive method calls for that object */
	if (!dbus_connection_register_object_path (connection, DBUS_RSS_OBJECT, &feedreader_vtable, &error))
 	{
 		fprintf (stderr, "*** ui_feedlist.c:Failed to register dbus object path: %s | %s\n", error.name, error.message);
 		dbus_error_free (&error);
    	return;
    }
    dbus_error_free (&error);
}

#endif /* USE_DBUS */
