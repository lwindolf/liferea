/**
 * @file google_source.c  Google reader feed list source support
 * 
 * Copyright (C) 2007-2013 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2008 Arnold Noronha <arnstein87@gmail.com>
 * Copyright (C) 2011 Peter Oliver
 * Copyright (C) 2011 Sergey Snitsaruk <narren96c@gmail.com>
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

#include "fl_sources/google_source.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <libxml/xpath.h>

#include "common.h"
#include "debug.h"
#include "feedlist.h"
#include "item_state.h"
#include "metadata.h"
#include "node.h"
#include "subscription.h"
#include "update.h"
#include "xml.h"
#include "ui/liferea_dialog.h"
#include "fl_sources/node_source.h"
#include "fl_sources/opml_source.h"

/** default Google reader subscription list update interval = once a day */
#define GOOGLE_SOURCE_UPDATE_INTERVAL 60*60*24

/** create a google source with given node as root */ 
static GoogleSourcePtr
google_source_new (nodePtr node) 
{
	GoogleSourcePtr source = g_new0 (struct GoogleSource, 1) ;
	
	return source;
}

static void
google_source_free (GoogleSourcePtr gsource) 
{
	update_job_cancel_by_owner (gsource);
	g_free (gsource);
}

/* node source type implementation */

static void
google_source_auto_update (nodePtr node)
{
}

static void
google_source_init (void)
{
	metadata_type_register ("GoogleBroadcastOrigFeed", METADATA_TYPE_URL);
	metadata_type_register ("sharedby", METADATA_TYPE_TEXT);
}

static void google_source_deinit (void) { }

static void
google_source_import (nodePtr node)
{
	opml_source_import (node);
	
	if (!node->data)
		node->data = (gpointer) google_source_new (node);
}

static void
google_source_cleanup (nodePtr node)
{
	GoogleSourcePtr reader = (GoogleSourcePtr) node->data;
	google_source_free(reader);
	node->data = NULL ;
}

/**
 * Convert all subscriptions of a google source to local feeds
 *
 * @param node The node to migrate (not the nodeSource!)
 */
static void
google_source_convert_to_local (nodePtr node)
{
	/* Nothing to do when migrating */
}

/* node source type definition */

static struct nodeSourceType nst = {
	.id                  = "fl_google",
	.name                = N_("Google Reader"),
	.capabilities        = NODE_SOURCE_CAPABILITY_CONVERT_TO_LOCAL,
	.source_type_init    = google_source_init,
	.source_type_deinit  = google_source_deinit,
	.source_new          = NULL,
	.source_delete       = opml_source_remove,
	.source_import       = google_source_import,
	.source_export       = opml_source_export,
	.source_get_feedlist = opml_source_get_feedlist,
	.source_auto_update  = google_source_auto_update,
	.free                = google_source_cleanup,
	.item_set_flag       = NULL,
	.item_mark_read      = NULL,
	.add_folder          = NULL, 
	.add_subscription    = NULL,
	.remove_node         = NULL,
	.convert_to_local    = google_source_convert_to_local
};

nodeSourceTypePtr
google_source_get_type (void)
{
	nst.feedSubscriptionType = feed_get_subscription_type ();

	return &nst;
}
