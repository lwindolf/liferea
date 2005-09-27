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

#include "fl_default.h"
#include "feed.h"
#include "feedlist.h"
#include "itemset.h"
#include "export.h"
#include "debug.h"
#include "ui/ui_feedlist.h"

static gboolean feedlistLoading = FALSE;
static flNodeHandler *handler = NULL;
static nodePtr rootNode = NULL;

void fl_default_handler_save(void) {
	gchar *filename, *filename_real;
	
	if(feedlistLoading)
		return;

	debug_enter("fl_default_save");
	filename = g_strdup_printf("%s" G_DIR_SEPARATOR_S "feedlist.opml~", common_get_cache_path());

	if(0 == export_OPML_feedlist(filename, TRUE)) {
		filename_real = g_strdup_printf("%s" G_DIR_SEPARATOR_S "feedlist.opml", common_get_cache_path());
		if(rename(filename, filename_real) < 0)
			g_warning(_("Error renaming %s to %s\n"), filename, filename_real);
		g_free(filename_real);
	}
	g_free(filename);
	debug_exit("fl_default_save");
}

void fl_node_load(nodePtr np) {
	feedPtr	fp = (feedPtr)np->data;

	feed_load(fp, np->id);
	g_assert(NULL == np->itemSet);
	np->itemSet = (itemSetPtr)g_new0(itemSet, 1);
	np->itemSet->items = fp->items;
	np->itemSet->newCount = fp->newCount;
	np->itemSet->unreadCount = fp->unreadCount;
}

void fl_node_unload(nodePtr np) {

	feed_unload((feedPtr)np->data);
	g_assert(NULL != np->itemSet);
	g_free(np->itemSet);
	np->itemSet = NULL;
}

gchar *fl_node_render(nodePtr np) {

	if(FST_FOLDER != np->type)
		return feed_render((feedPtr)np->data);

	return NULL;
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
	NULL,	/* new instance */
	fl_default_handler_save,
	NULL,	/* delete instance */
	fl_default_node_load,
	fl_default_node_unload,
	fl_default_node_render,
	fl_default_feed_add,
	fl_default_feed_delete,
	fl_default_folder_add,
	fl_default_folder_delete
};

static pluginInfo = {
	PLUGIN_API_VERSION,
	"Static Feed List Plugin",
	PLUGIN_TYPE_FEEDLIST_PROVIDER,
	PLUGIN_ID_DEFAULT_FEEDLIST,
	//"Default feed list provider. Allows users to add/remove/reorder subscriptions.",
	&fpi
}

DECLARE_PLUGIN(pi);

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
	
	feedlist_save();
}
