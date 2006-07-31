/**
 * @file fl_default.c default static feedlist provider
 * 
 * Copyright (C) 2005-2006 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2005-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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
#include "support.h"
#include "common.h"
#include "conf.h"
#include "feed.h"
#include "feedlist.h"
#include "export.h"
#include "debug.h"
#include "update.h"
#include "plugin.h"
#include "fl_providers/fl_default.h"
#include "fl_providers/fl_plugin.h"
#include "ui/ui_feed.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_node.h"
#include "ui/ui_tray.h"

/** lock to prevent feed list saving while loading */
static gboolean feedlistImport = FALSE;

static struct flPlugin fpi;

static void fl_default_dbus_connect ();

static void fl_default_copy_dir(gchar *subdir) {
	gchar *dirname10, *dirname11;
	gchar *srcfile, *destfile;
   	GDir *dir;
		
	dirname10 = g_strdup_printf("%s" G_DIR_SEPARATOR_S ".liferea"     G_DIR_SEPARATOR_S "%s", g_get_home_dir(), subdir);
	dirname11 = g_strdup_printf("%s" G_DIR_SEPARATOR_S ".liferea_1.1" G_DIR_SEPARATOR_S "%s", g_get_home_dir(), subdir);
	dir = g_dir_open(dirname10, 0, NULL);
	while(NULL != (srcfile = (gchar *)g_dir_read_name(dir))) {
		gchar	*content;
		gsize	length;
		destfile = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s", dirname11, srcfile);
		srcfile = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s", dirname10, srcfile);
		g_print("copying %s to %s\n", srcfile, destfile);
		if(g_file_get_contents(srcfile, &content, &length, NULL))
			g_file_set_contents(destfile, content, length, NULL);
		g_free(content);
		g_free(destfile);
		g_free(srcfile);
	}
	g_dir_close(dir);
}

static void fl_default_handler_import(nodePtr node) {
	flNodeHandler	*handler;
	gchar		*filename10, *filename11;

	debug_enter("fl_default_handler_import");

	/* create a new handler structure */
	handler = g_new0(struct flNodeHandler_, 1);
	handler->root = node;
	handler->plugin = &fpi;
	node->handler = handler;

	/* start the import */
	feedlistImport = TRUE;

	/* check for 1.0->1.1 migration */
	filename10 = g_strdup_printf("%s" G_DIR_SEPARATOR_S ".liferea/feedlist.opml", g_get_home_dir()); /* 1.0 path dir */
	filename11 = common_create_cache_filename(NULL, "feedlist", "opml");	
		
	if(!g_file_test(filename11, G_FILE_TEST_EXISTS) &&
	   g_file_test(filename10, G_FILE_TEST_EXISTS)) {
		
		g_print("starting 1.0->1.1 cache migration...\n");
		
		/* Note: because v1.1 uses a new cache format we
		   do a cache migration. v1.1 uses $HOME/.liferea_1.1
		   instead of $HOME/.liferea as it's cache directory */

		/* copy old cache files to new cache dir */
		fl_default_copy_dir("cache" G_DIR_SEPARATOR_S "feeds");
		fl_default_copy_dir("cache" G_DIR_SEPARATOR_S "favicons");
		
		/* point feedlist.opml to the old 1.0 file */
		g_free(filename11);
		filename11 = g_strdup(filename10);
	}
	g_free(filename10);

	/* check for default feed list import */
	if(!g_file_test(filename11, G_FILE_TEST_EXISTS)) {
		/* if there is no feedlist.opml we provide a default feed list */
		g_free(filename11);
		/* "feedlist.opml" is translatable so that translators can provide a localized default feed list */
		filename11 = g_strdup_printf(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "opml" G_DIR_SEPARATOR_S "%s", _("feedlist.opml"));
	}
	import_OPML_feedlist(filename11, node, handler, FALSE, TRUE);
	g_free(filename11);
	feedlistImport = FALSE;

#ifdef USE_DBUS
	if(!getBooleanConfValue(DISABLE_DBUS)) {
		/* Start listening on the dbus for new subscriptions */	
		debug0(DEBUG_GUI, "Registering with DBUS...");
		fl_default_dbus_connect();
	} else {
		g_print("DBUS disabled by user request...");
	}
#else
	debug0(DEBUG_GUI, "Compiled without DBUS support.");
#endif

	debug_exit("fl_default_handler_import");
}

static void fl_default_handler_export(nodePtr node) {
	gchar *filename, *filename_real;
	
	if(feedlistImport)
		return;

	debug_enter("fl_default_handler_export");

	filename = g_strdup_printf("%s" G_DIR_SEPARATOR_S "feedlist.opml~", common_get_cache_path());

	if(0 == export_OPML_feedlist(filename, TRUE)) {
		filename_real = g_strdup_printf("%s" G_DIR_SEPARATOR_S "feedlist.opml", common_get_cache_path());
		if(rename(filename, filename_real) < 0)
			g_warning(_("Error renaming %s to %s\n"), filename, filename_real);
		g_free(filename_real);
	}
	g_free(filename);

	debug_exit("fl_default_handler_export");
}

/* DBUS support for new subscriptions */

#ifdef USE_DBUS

static DBusHandlerResult
ui_feedlist_dbus_set_online (DBusConnection *connection, DBusMessage *message)
{
	DBusError error;
	DBusMessage *reply;
	gboolean b;
	gboolean done = TRUE;
	
	/* Retreive the dbus message arguments (the online status) */	
	dbus_error_init (&error);
	if (!dbus_message_get_args (message, &error, DBUS_TYPE_BOOLEAN, &b, DBUS_TYPE_INVALID))
	{
		fprintf (stderr, "*** ui_feedlist.c: Error while retreiving message parameter, expecting a boolean: %s | %s\n", error.name,  error.message);
		reply = dbus_message_new_error (message, error.name, error.message);
		dbus_connection_send (connection, reply, NULL);
		dbus_message_unref (reply);
		dbus_error_free(&error);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}
	dbus_error_free(&error);

	/* Set online status */
	update_set_online(b);

	/* Acknowledge the new feed by returning true */
	reply = dbus_message_new_method_return (message);
	if (reply != NULL)
	{
		dbus_message_append_args (reply, DBUS_TYPE_BOOLEAN, &done,DBUS_TYPE_INVALID);
		dbus_connection_send (connection, reply, NULL);
		dbus_message_unref (reply);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	else
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult
fl_default_dbus_subscribe (DBusConnection *connection, DBusMessage *message) {
	DBusError error;
	DBusMessage *reply;
	char *s;
	gboolean done = TRUE;
	
	/* Retreive the dbus message arguments (the new feed url) */	
	dbus_error_init (&error);
	if(!dbus_message_get_args (message, &error, DBUS_TYPE_STRING, &s, DBUS_TYPE_INVALID)) {
		g_warning("fl_default_dbus_subscribe(): Error while retreiving message parameter, expecting a string url: %s | %s\n", error.name,  error.message);
		reply = dbus_message_new_error (message, error.name, error.message);
		dbus_connection_send (connection, reply, NULL);
		dbus_message_unref (reply);
		dbus_error_free(&error);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}
	dbus_error_free(&error);

	/* Subscribe the feed */
	node_request_automatic_add(s, NULL, NULL, FEED_REQ_SHOW_PROPDIALOG | FEED_REQ_RESET_TITLE | FEED_REQ_RESET_UPDATE_INT);

	/* Acknowledge the new feed by returning true */
	reply = dbus_message_new_method_return (message);
	if(reply) {
		dbus_message_append_args (reply, DBUS_TYPE_BOOLEAN, &done,DBUS_TYPE_INVALID);
		dbus_connection_send (connection, reply, NULL);
		dbus_message_unref (reply);
		return DBUS_HANDLER_RESULT_HANDLED;
	} else {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}
}


static DBusHandlerResult
fl_default_dbus_message_handler (DBusConnection *connection, DBusMessage *message, void *user_data)
{
	const char  *method;
	
	if (connection == NULL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	if (message == NULL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	
	method = dbus_message_get_member (message);
	if (strcmp (DBUS_RSS_METHOD, method) == 0)
		return fl_default_dbus_subscribe (connection, message);
	if (strcmp (DBUS_RSS_SET_ONLINE_METHOD, method) == 0)
		return ui_feedlist_dbus_set_online (connection, message);
	else
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void
fl_default_dbus_connect ()
{
	DBusError       error;
	DBusConnection *connection;
	DBusObjectPathVTable feedreader_vtable = { NULL, fl_default_dbus_message_handler, NULL};

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
	dbus_bus_request_name (connection, DBUS_RSS_SERVICE, 0, &error);

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

/* root node type definition */

static void fl_default_init(void) {

	debug_enter("fl_default_init");

	debug_exit("fl_default_init");
}

static void fl_default_deinit(void) {
	
	debug_enter("fl_default_deinit");

	debug_exit("fl_default_deinit");
}

/* feed list provider plugin definition */

static struct flPlugin fpi = {
	FL_PLUGIN_API_VERSION,
	"fl_default",
	"Static Feed List",
	FL_PLUGIN_CAPABILITY_IS_ROOT |
	FL_PLUGIN_CAPABILITY_ADD |
	FL_PLUGIN_CAPABILITY_REMOVE |
	FL_PLUGIN_CAPABILITY_SUBFOLDERS |
	FL_PLUGIN_CAPABILITY_REORDER,
	fl_default_init,
	fl_default_deinit,
	NULL,
	NULL,
	fl_default_handler_import,
	fl_default_handler_export
};

static struct plugin pi = {
	PLUGIN_API_VERSION,
	"Static Feed List Plugin",
	PLUGIN_TYPE_FEEDLIST_PROVIDER,
	//"Default feed list provider. Allows users to add/remove/reorder subscriptions.",
	&fpi
};

DECLARE_PLUGIN(pi);
DECLARE_FL_PLUGIN(fpi);
