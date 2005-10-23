/**
 * @file fl_default.c default static feedlist provider
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

#include <common.h>
#include <string.h>
#include <libxml/uri.h>
#include "fl_default.h"
#include "support.h"
#include "conf.h"
#include "favicon.h"
#include "feed.h"
#include "feedlist.h"
#include "itemset.h"
#include "export.h"
#include "debug.h"
#include "update.h"
#include "plugin.h"
#include "fl_providers/fl_plugin.h"
#include "ui/ui_feed.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_htmlview.h"
#include "ui/ui_notification.h"

extern GtkWindow *mainwindow;

static gboolean feedlistLoading = FALSE;
static flNodeHandler *handler = NULL;
static nodePtr rootNode = NULL;

static void fl_default_handler_new(nodePtr np) {
	g_warning("Implement me!");
}

static void fl_default_handler_delete(nodePtr np) {
	g_warning("Implement me!");
}

static void fl_default_save_root(void) {
	gchar *filename, *filename_real;
	
	if(feedlistLoading)
		return;

	debug_enter("fl_default_save_root");

	filename = g_strdup_printf("%s" G_DIR_SEPARATOR_S "feedlist.opml~", common_get_cache_path());

	if(0 == export_OPML_feedlist(filename, TRUE)) {
		filename_real = g_strdup_printf("%s" G_DIR_SEPARATOR_S "feedlist.opml", common_get_cache_path());
		if(rename(filename, filename_real) < 0)
			g_warning(_("Error renaming %s to %s\n"), filename, filename_real);
		g_free(filename_real);
	}
	g_free(filename);

	debug_exit("fl_default_save_root");
}

/* used by fl_default_node_add but also from ui_search.c! */
void fl_default_feed_add(const gchar *source, gchar *filter, gint flags) {
	feedPtr			fp;
	int			pos;
	nodePtr			np, parent;
	
	debug_enter("fl_default_feed_add");	
	
	fp = feed_new();
	feed_set_source(fp, source);
	feed_set_title(fp, _("New subscription"));
	feed_set_filter(fp, filter);

	feed_schedule_update(fp, flags | FEED_REQ_PRIORITY_HIGH | FEED_REQ_DOWNLOAD_FAVICON | FEED_REQ_AUTH_DIALOG);

	np = node_new();
	node_set_title(np, feed_get_title(fp));
	node_add_data(np, FST_FEED, (gpointer)fp);
	parent = ui_feedlist_get_target_folder(&pos);
	feedlist_add_node(parent, np, pos);

	debug_exit("fl_default_feed_add");	
}

static void fl_default_node_add(nodePtr np) {
	GtkWidget	*dialog;

	switch(np->type) {
		case FST_FEED:
			dialog = ui_feed_newdialog_new(mainwindow);
			gtk_widget_show(dialog);
			break;
		case FST_FOLDER:
		case FST_VFOLDER:
			break;
		default:
			g_warning("adding unsupported type node!");
			break;
	}
}

static void fl_default_node_remove(nodePtr np) {

	g_warning("FIXME: fl_default_node_remove() implement me!");
}

static void fl_default_node_load(nodePtr np) {
	feedPtr	fp = (feedPtr)np->data;

	debug_enter("fl_default_node_load");
	
	g_assert(NULL == np->itemSet);
	np->itemSet = feed_load_from_cache(fp, np->id);

	debug_exit("fl_default_node_load");
}

static void fl_default_node_unload(nodePtr np) {
	feedPtr	fp = (feedPtr)np->data;

	debug_enter("fl_default_node_unload");

	if(CACHE_DISABLE == fp->cacheLimit) {
		debug1(DEBUG_CACHE, "not unloading node (%s) because cache is disabled", node_get_title(np));
	} else {
		debug1(DEBUG_CACHE, "unloading node (%s)", node_get_title(np));
		g_assert(NULL != np->itemSet);
		g_list_free(np->itemSet->items);
		np->itemSet->items = NULL;
		g_free(np->itemSet);
		np->itemSet = NULL;	
	} 

	debug_exit("fl_default_node_unload");
}

static gchar *fl_default_node_render(nodePtr np) {

	switch(np->type) {
		case FST_FEED:
			return feed_render((feedPtr)np->data);
			break;
		case FST_FOLDER:
			//return folder_render(np);
			break;
		case FST_VFOLDER:
			//return vfolder_render(np);
			break;
		case FST_PLUGIN:
			/* should never happen as we are root plugin! */
			break;
	}

	return NULL;
}

static void fl_default_node_save(nodePtr np) {

	if(NULL == np) {
		/* Saving the root node means saving the feed list... */
		fl_default_save_root();
		return;
	}

	switch(np->type) {
		case FST_FEED:
			feed_save_to_cache((feedPtr)np->data, np->itemSet, node_get_id(np));
			break;
		case FST_FOLDER:
		case FST_VFOLDER:
		case FST_PLUGIN:
			/* nothing to do */
			break;
	}
}

/* update handling */

static void fl_default_node_auto_update(nodePtr np) {
	feedPtr		fp = (feedPtr)np;
	GTimeVal	now;
	gint		interval;

	if(FST_FEED != np->type)	/* don't process folders and vfolders */
		return;

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
		favicon_download(np);
}

static void fl_default_node_update(nodePtr np, guint flags) {

	if(FST_FEED == np->type)	/* don't process folders and vfolders */
		feed_schedule_update((feedPtr)np->data, flags | FEED_REQ_PRIORITY_HIGH);
}

// FIXME: move method to feed specific code
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
	nodePtr			np = (nodePtr)request->user_data;
	feedPtr			fp = (feedPtr)np->data;
	feedHandlerPtr		fhp;
	itemSetPtr		sp;
	gchar			*old_title, *old_source;
	gint			old_update_interval;
	
	feedlist_load_node(np);

	/* no matter what the result of the update is we need to save update
	   status and the last update time to cache */
	np->needsCacheSave = TRUE;
	
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

		/* parse the new downloaded feed into fp and sp */
		sp = (itemSetPtr)g_new0(struct itemSet, 1);
		fhp = feed_parse(fp, sp, request->data, request->size, request->flags & FEED_REQ_AUTO_DISCOVER);

		// FIXME: item set merging?
		// do something with sp!
		
		if(fhp == NULL) {
			feed_set_available(fp, FALSE);
			fp->parseErrors = g_strdup_printf(_("<p>Could not detect the type of this feed! Please check if the source really points to a resource provided in one of the supported syndication formats!</p>%s"), fp->parseErrors);
		} else {
			fp->fhp = fhp;
			
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

			itemlist_reload(np);
			
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
		favicon_download(np);

	/* update UI presentations */
	ui_notification_update(np);
	ui_feedlist_update();

	feedlist_unload_node(np);
}

/* DBUS support for new subscriptions */

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
	fl_default_feed_add(s, NULL, FEED_REQ_SHOW_PROPDIALOG | FEED_REQ_RESET_TITLE | FEED_REQ_RESET_UPDATE_INT);

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

static flPluginInfo fpi;

void fl_default_init(void) {
	gchar	*filename;

	feedlistLoading = TRUE;
	filename = g_strdup_printf("%s" G_DIR_SEPARATOR_S ".liferea" G_DIR_SEPARATOR_S "feedlist.opml", g_get_home_dir());
	if(!g_file_test(filename, G_FILE_TEST_EXISTS)) {
		/* if there is no feedlist.opml we provide a default feed list */
		g_free(filename);
		/* "feedlist.opml" is translatable so that translators can provide a localized default feed list */
		filename = g_strdup_printf(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "opml" G_DIR_SEPARATOR_S "%s", _("feedlist.opml"));
	}
	import_OPML_feedlist(filename, NULL, FALSE, TRUE);
	g_free(filename);
	feedlistLoading = FALSE;

#ifdef USE_DBUS
	/* Start listening on the dbus for new subscriptions */
	debug0(DEBUG_GUI, "Registering with DBUS...");
	ui_feedlist_dbus_connect();
#else
	debug0(DEBUG_GUI, "No DBUS support active.");
#endif

	handler = g_new0(flNodeHandler, 1);
	handler->plugin = &fpi;
}

void fl_default_deinit(void) {
	
	// FIXME
}

/* feed list provider plugin definition */

static flPluginInfo fpi = {
	FL_PLUGIN_API_VERSION,
	"Static Feed List",
	FL_PLUGIN_CAPABILITY_IS_ROOT |
	FL_PLUGIN_CAPABILITY_ADD |
	FL_PLUGIN_CAPABILITY_REMOVE |
	FL_PLUGIN_CAPABILITY_ADD_FOLDER |
	FL_PLUGIN_CAPABILITY_REMOVE_FOLDER |
	FL_PLUGIN_CAPABILITY_REORDER,
	fl_default_init,
	fl_default_deinit,
	fl_default_handler_new,
	fl_default_handler_delete,
	fl_default_node_load,
	fl_default_node_unload,
	fl_default_node_save,
	fl_default_node_render,
	fl_default_node_auto_update,
	fl_default_node_update,
	fl_default_node_add,
	fl_default_node_remove
};

static pluginInfo pi = {
	PLUGIN_API_VERSION,
	"Static Feed List Plugin",
	PLUGIN_TYPE_FEEDLIST_PROVIDER,
	PLUGIN_ID_DEFAULT_FEEDLIST,
	//"Default feed list provider. Allows users to add/remove/reorder subscriptions.",
	&fpi
};

DECLARE_PLUGIN(pi);
DECLARE_FL_PLUGIN(fpi);
