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

#include <string.h>
#include <libxml/uri.h>
#include "fl_default.h"
#include "support.h"
#include "common.h"
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

/** lock to prevent feed list saving while loading */
static gboolean feedlistImport = FALSE;

static flNodeHandler *handler = NULL;
static nodePtr rootNode = NULL;

static void fl_default_handler_new(nodePtr np) {
	gchar	*filename;

	debug_enter("fl_default_handler_new");

	/* We only expect to be called to create an plugin instance 
	   serving as the root node (indicated by NULL). */
	g_assert(NULL == np);

	feedlistImport = TRUE;
	filename = common_create_cache_filename(NULL, "feedlist", "opml");
	if(!g_file_test(filename, G_FILE_TEST_EXISTS)) {
		/* if there is no feedlist.opml we provide a default feed list */
		g_free(filename);
		/* "feedlist.opml" is translatable so that translators can provide a localized default feed list */
		filename = g_strdup_printf(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "opml" G_DIR_SEPARATOR_S "%s", _("feedlist.opml"));
	}
	import_OPML_feedlist(filename, NULL, handler, FALSE, TRUE);
	g_free(filename);
	feedlistImport = FALSE;

#ifdef USE_DBUS
	/* Start listening on the dbus for new subscriptions */
	debug0(DEBUG_GUI, "Registering with DBUS...");
	ui_feedlist_dbus_connect();
#else
	debug0(DEBUG_GUI, "No DBUS support active.");
#endif

	debug_exit("fl_default_handler_new");
}

static void fl_default_handler_delete(nodePtr np) {
	g_warning("fl_handler_delete(): Implement me!");
}

static void fl_default_save_root(void) {
	gchar *filename, *filename_real;
	
	if(feedlistImport)
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

static void fl_default_node_add(nodePtr np) {
	GtkWidget	*dialog;

	switch(np->type) {
		case FST_FEED:
			dialog = ui_feed_newdialog_new(mainwindow);
			gtk_widget_show(dialog);
			break;
		case FST_FOLDER:
		case FST_VFOLDER:
			g_warning("adding folder/vfolder: implement me!");
			break;
		default:
			g_warning("adding unsupported type node!");
			break;
	}
}

static void fl_default_node_remove(nodePtr np) {

	switch(np->type) {
		case FST_FEED:
		case FST_VFOLDER:
			feed_remove_from_cache((feedPtr)np->data, np->id);
			break;
		case FST_FOLDER:
			/* nothing to do */
			break;
		default:
			g_warning("removing unsupported type node!");
			break;
	}
}

static void fl_default_node_load(nodePtr np) {
	feedPtr	fp = (feedPtr)np->data;

	debug_enter("fl_default_node_load");
	
	g_assert(NULL == np->itemSet);
	g_assert(FST_FEED == np->type);
	node_set_itemset(np, feed_load_from_cache(fp, np->id));

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
			feed_save_to_cache((feedPtr)np->data, node_get_itemset(np), node_get_id(np));
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
	feedPtr		fp = (feedPtr)np->data;
	GTimeVal	now;
	gint		interval;

	debug_enter("fl_default_node_auto_update");

	if(FST_FEED == np->type)	/* don't process folders and vfolders */
		return;

	g_get_current_time(&now);
	interval = feed_get_update_interval(fp);
	
	if(-2 >= interval)
		return;		/* don't update this feed */
		
	if(-1 == interval)
		interval = getNumericConfValue(DEFAULT_UPDATE_INTERVAL);
	
	if(interval > 0)
		if(fp->lastPoll.tv_sec + interval*60 <= now.tv_sec)
			node_schedule_update(np, ui_feed_process_update_result, 0);

	/* And check for favicon updating */
	if(fp->lastFaviconPoll.tv_sec + 30*24*60*60 <= now.tv_sec)
		favicon_download(np);

	debug_exit("fl_default_node_auto_update");
}

static void fl_default_node_update(nodePtr np, guint flags) {

	if(FST_FEED == np->type)	/* don't process folders and vfolders */
		node_schedule_update(np, ui_feed_process_update_result, flags | FEED_REQ_PRIORITY_HIGH);
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
	ui_feed_add(s, NULL, FEED_REQ_SHOW_PROPDIALOG | FEED_REQ_RESET_TITLE | FEED_REQ_RESET_UPDATE_INT);

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

	debug_enter("fl_default_init");

	handler = g_new0(flNodeHandler, 1);
	handler->plugin = &fpi;

	debug_exit("fl_default_init");
}

void fl_default_deinit(void) {
	
	g_warning("fl_default_deinit(): Implement me!");
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
