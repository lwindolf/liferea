/**
 * @file default_source.c default static feedlist provider
 * 
 * Copyright (C) 2005-2007 Lars Lindner <lars.lindner@gmail.com>
 * Copyright (C) 2005-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>

#include "common.h"
#include "conf.h"
#include "db.h"
#include "debug.h"
#include "export.h"
#include "feed.h"
#include "feedlist.h"
#include "migrate.h"
#include "plugin.h"
#include "update.h"
#include "fl_sources/default_source.h"
#include "fl_sources/node_source.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_node.h"
#include "ui/ui_subscription.h"
#include "ui/ui_tray.h"

/** lock to prevent feed list saving while loading */
static gboolean feedlistImport = FALSE;

extern gboolean cacheMigrated;	/* feedlist.c */

static gchar * default_source_source_get_feedlist(nodePtr node) {

	return common_create_cache_filename(NULL, "feedlist", "opml");
}

static void
default_source_source_import (nodePtr node) 
{
	gchar		*filename10;
	gchar		*filename12;
	gchar		*filename13;
	gchar		*filename, *backupFilename;
	gchar		*content;
	gssize		length;
	GSList		*iter, *subscriptions;
	migrationMode	migration = 0;

	debug_enter ("default_source_source_import");

	/* start the import */
	feedlistImport = TRUE;

	/* build test file names */
	filename10 = g_strdup_printf ("%s/.liferea/feedlist.opml", g_get_home_dir ());
	filename12 = g_strdup_printf ("%s/.liferea_1.2/feedlist.opml", g_get_home_dir ());
	filename13 = g_strdup_printf ("%s/.liferea_1.3/feedlist.opml", g_get_home_dir ());
	filename = default_source_source_get_feedlist (node);
	backupFilename = g_strdup_printf("%s.backup", filename);
	
	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		/* if feed list is missing, try migration */
		
		if (g_file_test (filename13, G_FILE_TEST_EXISTS)) {
			/* migration needs to be done before feed list import... */
			migration_execute (MIGRATION_MODE_13_TO_14);
		} else if (g_file_test (filename12, G_FILE_TEST_EXISTS)) {
			/* migration needs to be done after feed list import
			   so we redirect the feed list OPML file name and
			   import later */
			g_free (filename);
		     	filename = g_strdup (filename12);
			migration = MIGRATION_MODE_12_TO_14;
		} else if (g_file_test (filename10, G_FILE_TEST_EXISTS)) {
			/* same as 1.2->1.4: delayed migration... */
			g_free (filename);
			filename = g_strdup (filename10);
			migration = MIGRATION_MODE_10_TO_14;
		}
	}

	g_free (filename13);
	g_free (filename12);
	g_free (filename10);
	
	/* check for default feed list import */
	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		/* if there is no feedlist.opml we provide a default feed list */
		g_free (filename);
		
		/* "feedlist.opml" is translatable so that translators can provide a localized default feed list */
		filename = g_strdup_printf (PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "opml" G_DIR_SEPARATOR_S "%s", _("feedlist.opml"));
		
		/* sanity check to catch wrong filenames supplied in translations */
		if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
			g_warning ("Configured localized feed list \"%s\" does not exist!", filename);
			g_free (filename);
			filename = g_strdup_printf(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "opml" G_DIR_SEPARATOR_S "%s", "feedlist.opml");
		}
	}

	if (!import_OPML_feedlist (filename, node, node->source, FALSE, TRUE))
		g_error ("Fatal: Feed list import failed! You might want to try to restore\n"
		         "the feed list file %s from the backup in %s", filename, backupFilename);

	/* upon successful import create a backup copy of the feed list */
	if (g_file_get_contents (filename, &content, &length, NULL)) {
		g_file_set_contents (backupFilename, content, length, NULL);
		g_free (content);
	}

	g_free (filename);
	g_free (backupFilename);
			
	if (migration)
		migration_execute (migration);
	
	/* DB cleanup, ensure that there are no subscriptions in the DB
	   that have no representation in the OPML feed list. */
	   debug0 (DEBUG_DB, "checking for lost subscriptions...");
	subscriptions = db_subscription_list_load ();
	for (iter = subscriptions; iter != NULL; iter = iter->next) {
		gchar *id = (gchar *)iter->data;
		if (NULL == node_from_id (id)) {
			debug1 (DEBUG_DB, "found lost subscription (id=%s), dropping it...", id);
			db_subscription_remove (id);
		}
		g_free (id);
	}
	g_slist_free (subscriptions);
	
	feedlistImport = FALSE;

	debug_exit ("default_source_source_import");
}

static void default_source_source_export(nodePtr node) {
	gchar	*filename;
	
	if(feedlistImport)
		return;

	debug_enter("default_source_source_export");
	
	g_assert(node->source->root == feedlist_get_root());

	filename = default_source_source_get_feedlist(node);
	export_OPML_feedlist(filename, node->source->root, TRUE);
	g_free(filename);

	debug_exit("default_source_source_export");
}

static void
default_source_update (nodePtr node)
{	
	node_foreach_child_data (node, node_update_subscription, GUINT_TO_POINTER (0));
}

static void
default_source_auto_update (nodePtr node)
{	
	node_foreach_child (node, node_auto_update_subscription);
}

static void default_source_init (void) { }
static void default_source_deinit (void) { }

/* feed list provider plugin definition */

static struct nodeSourceType nst = {
	NODE_SOURCE_TYPE_API_VERSION,
	"fl_default",
	"Static Feed List",
	"The default feed list source. Should never be added manually. If you see this then something went wrong!",
	NODE_SOURCE_CAPABILITY_IS_ROOT |
	NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST,
	default_source_init,
	default_source_deinit,
	NULL,	/* ui_add */
	NULL,	/* remove */
	default_source_source_import,
	default_source_source_export,
	default_source_source_get_feedlist,
	default_source_update,
	default_source_auto_update,
	NULL	/* free */
};

nodeSourceTypePtr default_source_get_type(void) { return &nst; }
