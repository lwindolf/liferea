/**
 * @file default_source.c  default static feedlist provider
 * 
 * Copyright (C) 2005-2008 Lars Lindner <lars.lindner@gmail.com>
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
#include "debug.h"
#include "export.h"
#include "feed.h"
#include "feedlist.h"
#include "folder.h"
#include "migrate.h"
#include "update.h"
#include "fl_sources/default_source.h"
#include "fl_sources/node_source.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_node.h"
#include "ui/ui_subscription.h"
#include "ui/ui_tray.h"

/** lock to prevent feed list saving while loading */
static gboolean feedlistImport = FALSE;

static gchar *
default_source_source_get_feedlist (nodePtr node)
{
	return common_create_cache_filename (NULL, "feedlist", "opml");
}

static void
default_source_import (nodePtr node) 
{
	gchar		*filename10;
	gchar		*filename12;
	gchar		*filename13;
	gchar		*filename14;
	gchar		*filename, *backupFilename;
	gchar		*content;
	gssize		length;
	migrationMode	migration = 0;

	debug_enter ("default_source_source_import");

	/* start the import */
	feedlistImport = TRUE;

	/* build test file names */
	filename10 = g_strdup_printf ("%s/.liferea/feedlist.opml", g_get_home_dir ());
	filename12 = g_strdup_printf ("%s/.liferea_1.2/feedlist.opml", g_get_home_dir ());
	filename13 = g_strdup_printf ("%s/.liferea_1.3/feedlist.opml", g_get_home_dir ());
	filename14 = g_strdup_printf ("%s/.liferea_1.4/feedlist.opml", g_get_home_dir ());
	filename = default_source_source_get_feedlist (node);
	backupFilename = g_strdup_printf("%s.backup", filename);
	
	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		/* if feed list is missing, try migration */
		
		if (g_file_test (filename13, G_FILE_TEST_EXISTS)) {
			/* migration needs to be done before feed list import... */
			migration_execute (MIGRATION_FROM_13);
		} else if (g_file_test (filename14, G_FILE_TEST_EXISTS)) {
			/* migration needs to be done before feed list import... */
			migration_execute (MIGRATION_FROM_14);
		} else if (g_file_test (filename12, G_FILE_TEST_EXISTS)) {
			/* migration needs to be done after feed list import
			   so we redirect the feed list OPML file name and
			   import later */
			g_free (filename);
		     	filename = g_strdup (filename12);
			migration = MIGRATION_FROM_12;
		} else if (g_file_test (filename10, G_FILE_TEST_EXISTS)) {
			/* same as 1.2->1.4: delayed migration... */
			g_free (filename);
			filename = g_strdup (filename10);
			migration = MIGRATION_FROM_10;
		}
	}

	g_free (filename14);
	g_free (filename13);
	g_free (filename12);
	g_free (filename10);
	
	/* check for default feed list import */
	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		/* if there is no feedlist.opml we provide a default feed list */
		g_free (filename);
		
		/* "feedlist.opml" is translatable so that translators can provide a localized default feed list */
		filename = g_build_filename (PACKAGE_DATA_DIR, PACKAGE, "opml", _("feedlist.opml"), NULL);
		
		/* sanity check to catch wrong filenames supplied in translations */
		if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
			g_warning ("Configured localized feed list \"%s\" does not exist!", filename);
			g_free (filename);
			filename = g_build_filename (PACKAGE_DATA_DIR, PACKAGE, "opml", "feedlist.opml", NULL);
		}
	}

	if (!import_OPML_feedlist (filename, node, FALSE, TRUE))
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
	
	feedlistImport = FALSE;

	debug_exit ("default_source_source_import");
}

static void
default_source_export (nodePtr node)
{
	gchar	*filename;
	
	if (feedlistImport)
		return;

	debug_enter ("default_source_source_export");
	
	g_assert (node->source->root == feedlist_get_root ());

	filename = default_source_source_get_feedlist (node);
	export_OPML_feedlist (filename, node->source->root, TRUE);
	g_free (filename);

	debug_exit ("default_source_source_export");
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

static nodePtr
default_source_add_subscription (nodePtr node, subscriptionPtr subscription)
{
	/* For the local feed list source subscriptions are always
	   feed subscriptions implemented by the feed node and 
	   subscription type... */
	nodePtr child = node_new (feed_get_node_type ());
	node_set_title (child, _("New Subscription"));
	node_set_data (child, feed_new ());
	node_set_subscription (child, subscription);	/* feed subscription type is implicit */
	feedlist_node_added (child);
	
	subscription_update (subscription, FEED_REQ_RESET_TITLE | FEED_REQ_PRIORITY_HIGH);
	return child;
}

static nodePtr
default_source_add_folder (nodePtr node, const gchar *title)
{
	/* For the local feed list source folders are always 
	   real folders implemented by the folder node type... */
	nodePtr child = node_new (folder_get_node_type());
	node_set_title (child, title);
	feedlist_node_added (child);
	
	return child;
}

static void
default_source_remove_node (nodePtr node, nodePtr child)
{
	/* The default source can always immediately serve remove requests. */
	feedlist_node_removed (child);
}

static void default_source_init (void) { }
static void default_source_deinit (void) { }

/* node source type definition */

static struct nodeSourceType nst = {
	.api_version		= NODE_SOURCE_TYPE_API_VERSION,
	.id			= "fl_default",
	.name			= "Static Feed List",
	.description		= "The default feed list source. Should never be added manually. If you see this then something went wrong!",
	.capabilities		= NODE_SOURCE_CAPABILITY_IS_ROOT |
				  NODE_SOURCE_CAPABILITY_HIERARCHIC_FEEDLIST |
				  NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST,
	.source_type_init	= default_source_init,
	.source_type_deinit	= default_source_deinit,
	.source_new		= NULL,
	.source_delete		= NULL,
	.source_import		= default_source_import,
	.source_export		= default_source_export,
	.source_get_feedlist	= default_source_source_get_feedlist,
	.source_update		= default_source_update,
	.source_auto_update	= default_source_auto_update,
	.free 			= NULL,
	.add_subscription	= default_source_add_subscription,
	.add_folder		= default_source_add_folder,
	.remove_node		= default_source_remove_node
};

nodeSourceTypePtr default_source_get_type (void) { return &nst; }
