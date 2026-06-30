/**
 * @file theoldreader_source_feed.c  TheOldReader feed subscription routines
 *
 * Copyright (C) 2008  Arnold Noronha <arnstein87@gmail.com>
 * Copyright (C) 2014-2026  Lars Windolf <lars.windolf@gmx.de>
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
#include <string.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "common.h"
#include "debug.h"
#include "xml.h"

#include "feedlist.h"
#include "google_reader_api_edit.h"
#include "theoldreader_source.h"
#include "subscription.h"
#include "node.h"
#include "metadata.h"
#include "db.h"
#include "item_state.h"

static itemPtr
theoldreader_source_load_item_from_sourceid (Node *node, gchar *sourceId, GHashTable *cache)
{
	gpointer    ret = g_hash_table_lookup (cache, sourceId);
	itemSetPtr  itemset;
	int         num = g_hash_table_size (cache);
	GList       *iter;
	itemPtr     item = NULL;

	if (ret)
		return item_load (GPOINTER_TO_UINT (ret));

	/* skip the top 'num' entries */
	itemset = node_get_itemset (node);
	iter = itemset->ids;
	while (num--) iter = g_list_next (iter);

	for (; iter; iter = g_list_next (iter)) {
		item = item_load (GPOINTER_TO_UINT (iter->data));
		if (item) {
			if (item->sourceId) {
				/* save to cache */
				g_hash_table_insert (cache, g_strdup(item->sourceId), (gpointer) item->id);
				if (g_str_equal (item->sourceId, sourceId)) {
					itemset_free (itemset);
					return item;
				}
			}
			item_unload (item);
		}
	}

	g_warning ("TheOldReader: Could not find item for %s!", sourceId);
	itemset_free (itemset);
	return NULL;
}

static void
theoldreader_source_item_retrieve_status (const xmlNodePtr entry, subscriptionPtr subscription, GHashTable *cache)
{
	xmlNodePtr      xml;
	Node		*node = subscription->node;
	xmlChar         *id = NULL;
	gboolean        read = FALSE;

	xml = entry->children;
	g_assert (xml);

	/* Note: at the moment TheOldReader doesn't exposed a "starred" label
	   like Google Reader did. It also doesn't expose the like feature it
	   implements. Therefore we cannot sync the flagged state with
	   TheOldReader. */

	for (xml = entry->children; xml; xml = xml->next) {
		if (g_str_equal (xml->name, "id"))
			id = xmlNodeGetContent (xml);

		if (g_str_equal (xml->name, "category")) {
			xmlChar* label = xmlGetProp (xml, BAD_CAST"label");
			if (!label)
				continue;

			if (g_str_equal (label, "read"))
				read = TRUE;

			xmlFree (label);
		}
	}

	if (!id) {
		g_warning ("TheOldReader: Skipping item without id in theoldreader_source_item_retrieve_status()!");
		return;
	}

	itemPtr item = theoldreader_source_load_item_from_sourceid (node, (gchar *)id, cache);
	if (item && item->sourceId) {
		if (g_str_equal (item->sourceId, id) && !google_reader_api_edit_is_in_queue(node->source, (gchar *)id)) {
			if (item->readStatus != read)
				item_read_state_changed (item, read);
		}
	}
	if (item)
		item_unload (item);
	xmlFree (id);
}

static void
theoldreader_feed_subscription_process_update_result (subscriptionPtr subscription, const UpdateResult* const result, updateFlags flags)
{
	g_autofree gchar *id;

	debug (DEBUG_UPDATE, "theoldreader_source: %s |%s| process feed update result", subscription->node->id, subscription->node->title);

	/* Save old subscription metadata which contains "theoldreader-feed-id"
	   which is mission critical and the feed parser currently drops all
	   previous metadata :-( */
	id = g_strdup (metadata_list_get (subscription->metadata, "theoldreader-feed-id"));

	/* Always do standard feed parsing to get the items... */
	feed_get_subscription_type ()->process_update_result (subscription, result, flags);

	/* Set remote id again */
	metadata_list_set (&subscription->metadata, "theoldreader-feed-id", id);

	if (!result->data || result->httpstatus != 200) {
		debug (DEBUG_UPDATE, "theoldreader_source: %s |%s| ERROR: feed update failed (HTTP status %d)", subscription->node->id, subscription->node->title, result->httpstatus);
		subscription->error = FETCH_ERROR_NET;
		return;
	}

	xmlDocPtr doc = xml_parse (result->data, result->size, NULL);
	if (doc) {
		xmlNodePtr root = xmlDocGetRootElement (doc);
		xmlNodePtr entry = root->children ;
		g_autoptr(GHashTable) cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

		while (entry) {
			if (!g_str_equal (entry->name, "entry")) {
				entry = entry->next;
				continue; /* not an entry */
			}

			theoldreader_source_item_retrieve_status (entry, subscription, cache);
			entry = entry->next;
		}

		xmlFreeDoc (doc);
	} else {
		debug (DEBUG_UPDATE, "theoldreader_source: %s |%s| ERROR: Couldn't parse XML!", subscription->node->id, subscription->node->title);
		subscription->node->available = FALSE;
		subscription->error = FETCH_ERROR_XML;
	}

}

static gboolean
theoldreader_feed_subscription_prepare_update_request (subscriptionPtr subscription,
                                                       UpdateRequest *request)
{
	debug (DEBUG_UPDATE, "theoldreader_source: %s |%s| update feed", subscription->node->id, subscription->node->title);

	if (!metadata_list_get (subscription->metadata, "theoldreader-feed-id")) {
		g_warning ("Skipping TheOldReader feed '%s' (%s) without id!", subscription->origSource, subscription->node->id);
		return FALSE;
	}

	gchar* url = g_strdup_printf ("%s/atom/%s", subscription->origSource, metadata_list_get (subscription->metadata, "theoldreader-feed-id"));
	update_request_set_source (request, url);
	g_free (url);

	update_request_set_auth_value (request, subscription->node->source->authToken);
	return TRUE;
}

struct subscriptionType theOldReaderSourceFeedSubscriptionType = {
	theoldreader_feed_subscription_prepare_update_request,
	theoldreader_feed_subscription_process_update_result
};
