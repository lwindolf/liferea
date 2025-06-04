/**
 * @file default_source.c  default static feed list source
 * 
 * Copyright (C) 2005-2025 Lars Windolf <lars.windolf@gmx.de>
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
#include "debug.h"
#include "export.h"
#include "node_providers/feed.h"
#include "feedlist.h"
#include "node_providers/folder.h"
#include "migrate.h"
#include "update.h"
#include "node_sources/default_source.h"
#include "node_source.h"

/** lock to prevent feed list saving while loading */
static gboolean feedlistImport = TRUE;

static gchar *
default_source_get_feedlist (Node *node)
{
	return common_create_config_filename ("feedlist.opml");
}

static void
default_source_import (Node *node) 
{
	gchar		*filename, *backupFilename;
	gchar		*content;
	gssize		length;

	g_assert (TRUE == feedlistImport);

	filename = default_source_get_feedlist (node);
	backupFilename = g_strdup_printf("%s.backup", filename);
	
	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		if (!import_OPML_feedlist (filename, node, FALSE, TRUE))
			g_error ("Fatal: Feed list import failed! You might want to try to restore\n"
			         "the feed list file %s from the backup in %s", filename, backupFilename);

		/* upon successful import create a backup copy of the feed list */
		if (g_file_get_contents (filename, &content, (gsize *)&length, NULL)) {
			g_file_set_contents (backupFilename, content, length, NULL);
			g_free (content);
		}
	} else {
		/* If subscriptions could not be loaded try cache migration
		   or provide a default feed list */

		gchar *filename14 = g_strdup_printf ("%s/.liferea_1.4/feedlist.opml", g_get_home_dir ());
		gchar *filename16 = g_strdup_printf ("%s/.liferea_1.6/feedlist.opml", g_get_home_dir ());
		gchar *filename18 = g_strdup_printf ("%s/.liferea_1.8/feedlist.opml", g_get_home_dir ());

		if (g_file_test (filename18, G_FILE_TEST_EXISTS)) {
			migration_execute (MIGRATION_FROM_18, node);
		} else if (g_file_test (filename16, G_FILE_TEST_EXISTS)) {
			migration_execute (MIGRATION_FROM_16, node);
		} else if (g_file_test (filename14, G_FILE_TEST_EXISTS)) {
			migration_execute (MIGRATION_FROM_14, node);
		} else {
			gchar *filename = common_get_localized_filename (PACKAGE_DATA_DIR "/" PACKAGE "/opml/feedlist_%s.opml");
			if (!filename)
				g_error ("Fatal: No migration possible and no default feedlist found!");
	
			if (!import_OPML_feedlist (filename, node, FALSE, TRUE))
				g_error ("Fatal: Feed list import failed!");
			g_free (filename);
		}

		g_free (filename18);
		g_free (filename16);
		g_free (filename14);
	}

	g_free (filename);
	g_free (backupFilename);
	
	feedlistImport = FALSE;

}

static void
default_source_export (Node *node)
{
	gchar	*filename;

	if (feedlistImport)
		return;
	
	g_assert (node->source->root == feedlist_get_root ());

	filename = default_source_get_feedlist (node);
	export_OPML_feedlist (filename, node->source->root, TRUE);
	g_free (filename);

}

static void
default_source_auto_update (Node *node)
{	
	node_foreach_child (node, node_auto_update_subscription);
}

static Node *
default_source_add_subscription (Node *node, subscriptionPtr subscription)
{
	/* For the local feed list source subscriptions are always
	   feed subscriptions implemented by the feed node and 
	   subscription type... */
	Node *child = node_new ("feed");
	node_set_title (child, _("New Subscription"));
	node_set_data (child, feed_new ());
	node_set_subscription (child, subscription);	/* feed subscription type is implicit */
	feedlist_node_added (child);
	
	subscription_update (subscription, UPDATE_REQUEST_RESET_TITLE | UPDATE_REQUEST_PRIORITY_HIGH);
	return child;
}

static Node *
default_source_add_folder (Node *node, const gchar *title)
{
	/* For the local feed list source folders are always 
	   real folders implemented by the folder node type... */
	Node *child = node_new ("folder");
	node_set_title (child, title);
	feedlist_node_added (child);
	
	return child;
}

static void
default_source_remove_node (Node *node, Node *child)
{
	/* The default source can always immediately serve remove requests. */
	feedlist_node_removed (child);
}

/* node source provider definition */

typedef struct {
	GObject parent_instance;
} DefaultSource;

typedef struct {
	GObjectClass parent_class;
} DefaultSourceClass;

static void default_source_init(DefaultSource *self) { }
static void default_source_class_init(DefaultSourceClass *klass) { }
static void default_source_interface_init(NodeSourceProviderInterface *iface) {
	iface->id			= "fl_default";
	iface->name			= "Static Feed List";
	iface->capabilities		= NODE_SOURCE_CAPABILITY_IS_ROOT |
					  NODE_SOURCE_CAPABILITY_HIERARCHIC_FEEDLIST |
					  NODE_SOURCE_CAPABILITY_ADD_FEED |
					  NODE_SOURCE_CAPABILITY_ADD_FOLDER |
					  NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST;
	iface->feedSubscriptionType	= feed_get_subscription_type ();
	iface->source_import		= default_source_import;
	iface->source_export		= default_source_export;
	iface->source_get_feedlist	= default_source_get_feedlist;
	iface->source_auto_update	= default_source_auto_update;
	iface->add_subscription		= default_source_add_subscription;
	iface->add_folder		= default_source_add_folder;
	iface->remove_node		= default_source_remove_node;
}

#define DEFAULT_TYPE_SOURCE (default_source_get_type())

G_DEFINE_TYPE_WITH_CODE(DefaultSource, default_source, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE(NODE_TYPE_SOURCE_PROVIDER, default_source_interface_init))

void
default_source_register (void)
{
	NodeSourceProviderInterface *iface = NODE_SOURCE_PROVIDER_GET_IFACE (g_object_new (DEFAULT_TYPE_SOURCE, NULL));
	node_source_type_register (iface);
}
