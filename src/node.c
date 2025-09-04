/*
 * @file node.c  hierarchic feed list node handling
 *
 * Copyright (C) 2003-2025 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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
#include <errno.h>
#include <libxml/xmlwriter.h>

#include "common.h"
#include "conf.h"
#include "date.h"
#include "db.h"
#include "debug.h"
#include "favicon.h"
#include "feedlist.h"
#include "itemlist.h"
#include "itemset.h"
#include "item_state.h"
#include "metadata.h"
#include "enclosure.h"
#include "node.h"
#include "node_view.h"
#include "subscription_icon.h"
#include "update.h"
#include "node_provider.h"
#include "node_providers/vfolder.h"
#include "node_source.h"
#include "ui/feed_list_view.h"
#include "ui/icons.h"
#include "ui/liferea_shell.h"

static GHashTable *nodes = NULL;	/*<< node id -> node lookup table */

#define NODE_ID_LEN	7

G_DEFINE_TYPE (Node, node, G_TYPE_OBJECT);

static void
node_finalize (GObject *obj)
{
	Node *node = LIFEREA_NODE (obj);

	if (node->data && NODE_PROVIDER (node)->free)
		NODE_PROVIDER (node)->free (node);

	g_assert (NULL == node->children);

	g_hash_table_remove (nodes, node->id);

	update_job_cancel_by_owner (node);

	if (node->subscription)
		subscription_free (node->subscription);

	if (node->icon)
		g_object_unref (node->icon);
	g_free (node->iconFile);
	g_free (node->title);
	g_free (node->id);

	G_OBJECT_CLASS (node_parent_class)->finalize (obj);
}

static void
node_class_init (NodeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = node_finalize;
}

static void
node_init (Node *node)
{
}

Node *
node_is_used_id (const gchar *id)
{
	if (!id || !nodes)
		return NULL;

	return LIFEREA_NODE (g_hash_table_lookup (nodes, id));
}

gchar *
node_new_id (void)
{
	gchar *id;

	id = g_new0 (gchar, NODE_ID_LEN + 1);
	do {
		int i;
		for (i = 0; i < NODE_ID_LEN; i++)
			id[i] = (gchar)g_random_int_range ('a', 'z');
	} while (NULL != node_is_used_id (id));

	return id;
}

Node *
node_from_id (const gchar *id)
{
	Node *node = node_is_used_id (id);
	if (!node)
		debug (DEBUG_GUI, "Fatal: no node with id \"%s\" found!", id);

	return node;
}

Node *
node_new (const gchar *name)
{
	nodeProviderPtr	provider = node_provider_by_name (name);
	Node		*node;
	gchar		*id;

	node = LIFEREA_NODE (g_object_new (NODE_TYPE, NULL));
	node->provider = provider;
	node->sortColumn = NODE_VIEW_SORT_BY_TIME;
	node->sortReversed = TRUE;	/* default sorting is newest date at top */
	node->available = TRUE;

	id = node_new_id ();
	node_set_id (node, id);
	g_free (id);

	return node;
}

void
node_set_data (Node *node, gpointer data)
{
	g_assert (NULL == node->data);
	g_assert (NULL != node->provider);

	node->data = data;
}

void
node_set_subscription (Node *node, subscriptionPtr subscription)
{
	g_assert (NULL == node->subscription);
	g_assert (NULL != node->provider);

	node->subscription = subscription;
	subscription->node = node;

	/* Besides the favicon age we have no persistent
	   update state field, so everything else goes NULL */
	if (node->iconFile && !strstr(node->iconFile, "default.svg")) {
		subscription->updateState->lastFaviconPoll = (guint64)(common_get_mod_time (node->iconFile)) * G_USEC_PER_SEC;
		debug (DEBUG_UPDATE, "Setting last favicon poll time for %s to %lu", node->id, subscription->updateState->lastFaviconPoll / G_USEC_PER_SEC);
	}
}

void
node_update_subscription (Node *node, gpointer user_data)
{
	if (node->source->root == node) {
		node_source_update (node);
		return;
	}

	if (node->subscription)
		subscription_update (node->subscription, GPOINTER_TO_UINT (user_data));

	node_foreach_child_data (node, node_update_subscription, user_data);
}

void
node_auto_update_subscription (Node *node)
{
	if (node->source->root == node) {
		node_source_auto_update (node);
		return;
	}

	if (node->subscription)
		subscription_auto_update (node->subscription);

	node_foreach_child (node, node_auto_update_subscription);
}

void
node_reset_update_counter (Node *node, guint64 *now)
{
	subscription_reset_update_counter (node->subscription, now);

	node_foreach_child_data (node, node_reset_update_counter, now);
}

gboolean
node_is_ancestor (Node *node1, Node *node2)
{
	Node	*tmp;

	tmp = LIFEREA_NODE (node2->parent);
	while (tmp) {
		if (node1 == tmp)
			return TRUE;
		tmp = tmp->parent;
	}
	return FALSE;
}

static void
node_calc_counters (Node *node)
{
	/* Order is important! First update all children
	   so that hierarchical nodes (folders and feed
	   list sources) can determine their own unread
	   count as the sum of all childs afterwards */
	node_foreach_child (node, node_calc_counters);

	NODE_PROVIDER (node)->update_counters (node);
}

static void
node_update_parent_counters (Node *node)
{
	guint old;

	if (!node)
		return;

	old = node->unreadCount;

	NODE_PROVIDER (node)->update_counters (node);

	if (old != node->unreadCount) {
		feedlist_new_items (0);	/* add 0 new items, as 'new-items' signal updates unread items also */
		feedlist_node_was_updated (node);
	}

	if (node->parent)
		node_update_parent_counters (node->parent);
}

void
node_update_counters (Node *node)
{
	guint oldUnreadCount = node->unreadCount;
	guint oldItemCount = node->itemCount;

	/* Update the node itself and its children */
	node_calc_counters (node);

	if ((oldUnreadCount != node->unreadCount) ||
	    (oldItemCount != node->itemCount))
		feedlist_node_was_updated (node);

	/* Update the unread count of the parent nodes,
	   usually they just add all child unread counters */
	if (!IS_VFOLDER (node))
		node_update_parent_counters (node->parent);
}

void
node_update_favicon (Node *node)
{
	if (NODE_PROVIDER (node)->capabilities & NODE_CAPABILITY_UPDATE_FAVICON) {
		debug (DEBUG_UPDATE, "favicon of node %s needs to be updated...", node->title);
		subscription_icon_update (node->subscription);
	}

	/* Recursion */
	if (node->children)
		node_foreach_child (node, node_update_favicon);
}

itemSetPtr
node_get_itemset (Node *node)
{
	return NODE_PROVIDER (node)->load (node);
}

void
node_mark_all_read (Node *node)
{
	if (!node)
		return;

	if ((node->unreadCount > 0) || (IS_VFOLDER (node))) {
		itemset_mark_read (node);
		node->unreadCount = 0;
		node->needsUpdate = TRUE;
	}

	if (node->children)
		node_foreach_child (node, node_mark_all_read);
}

/* import callbacks and helper functions */

void
node_set_parent (Node *node, Node *parent, gint position)
{
	g_assert (NULL != parent);

	parent->children = g_slist_insert (parent->children, node, position);
	node->parent = parent;

	/* new nodes may be provided by another node source, if
	   not they are handled by the parents node source */
	if (!node->source)
		node->source = parent->source;
}

void
node_reparent (Node *node, Node *new_parent)
{
	Node *old_parent;

	g_assert (NULL != new_parent);
	g_assert (NULL != node);

	debug (DEBUG_GUI, "Reparenting node '%s' to a parent '%s'", node_get_title(node), node_get_title(new_parent));

	old_parent = node->parent;
	if (NULL != old_parent)
		old_parent->children = g_slist_remove (old_parent->children, node);

	new_parent->children = g_slist_insert (new_parent->children, node, -1);
	node->parent = new_parent;

	feed_list_view_reparent (node);
}

void
node_remove (Node *node)
{
	/* using itemlist_remove_all_items() ensures correct unread
	   and item counters for all parent folders and matching
	   search folders */
	itemlist_remove_all_items (node);

	NODE_PROVIDER (node)->remove (node);
}

gchar *
node_to_json (Node *node)
{
	g_autoptr(JsonBuilder) b = json_builder_new ();

	json_builder_begin_object (b);
	json_builder_set_member_name (b, "type");
	json_builder_add_string_value (b, NODE_PROVIDER (node)->id);
	json_builder_set_member_name (b, "id");
	json_builder_add_string_value (b, node->id);
	json_builder_set_member_name (b, "title");
	json_builder_add_string_value (b, node_get_title (node));
	json_builder_set_member_name (b, "unreadCount");
	json_builder_add_int_value (b, node->unreadCount);
	json_builder_set_member_name (b, "children");
	json_builder_add_int_value (b, g_slist_length (node->children));

	if (node->subscription) {
		json_builder_set_member_name (b, "source");
		json_builder_add_string_value (b, subscription_get_source (node->subscription));
		json_builder_set_member_name (b, "origSource");
		json_builder_add_string_value (b, node->subscription->origSource);

		json_builder_set_member_name (b, "error");
		json_builder_add_int_value (b, node->subscription->error);
		json_builder_set_member_name (b, "updateError");
		json_builder_add_string_value (b, node->subscription->updateError);
		json_builder_set_member_name (b, "httpError");
		json_builder_add_string_value (b, node->subscription->httpError);
		json_builder_set_member_name (b, "httpErrorCode");
		json_builder_add_int_value (b, node->subscription->httpErrorCode);
		json_builder_set_member_name (b, "filterError");
		json_builder_add_string_value (b, node->subscription->filterError);

		metadata_list_to_json (node->subscription->metadata, b);
	}

	if(node->subscription && node->subscription->parseErrors && (strlen(node->subscription->parseErrors->str) > 0)) {
		json_builder_set_member_name (b, "parseError");
		json_builder_add_string_value (b, node->subscription->parseErrors->str);
	}

	json_builder_end_object (b);

	return json_dump (b);
}

/* helper functions to be used with node_foreach* */

void
node_save(Node *node)
{
	NODE_PROVIDER(node)->save(node);
}

/* node attributes encapsulation */

void
node_set_title (Node *node, const gchar *title)
{
	g_free (node->title);
	node->title = g_strstrip (g_strdelimit (g_strdup (title), "\r\n", ' '));
}

const gchar *
node_get_title (Node *node)
{
	return node->title;
}

void
node_load_icon (Node *node)
{
	/* Load pixbuf for all widget based rendering */
	if (node->icon)
		g_object_unref (node->icon);

	// FIXME: don't use constant size, but size corresponding to GTK icon
	// size used in wide view
	node->icon = favicon_load_from_cache (node->id, 128);

	/* Create filename for HTML rendering */
	g_free (node->iconFile);

	if (node->icon)
		node->iconFile = common_create_cache_filename ("favicons", node->id, "png");
	else
		node->iconFile = g_build_filename (PACKAGE_DATA_DIR, "pixmaps", "default.svg", NULL);
}

/* determines the nodes favicon or default icon */
gpointer
node_get_icon (Node *node)
{
	if (!node->icon)
		return (gpointer) icon_get (NODE_PROVIDER(node)->icon);

	return node->icon;
}

const gchar *
node_get_favicon_file (Node *node)
{
	return node->iconFile;
}

void
node_set_id (Node *node, const gchar *id)
{
	if (!nodes)
		nodes = g_hash_table_new(g_str_hash, g_str_equal);

	if (node->id) {
		g_hash_table_remove (nodes, node->id);
		g_free (node->id);
	}
	node->id = g_strdup (id);

	g_hash_table_insert (nodes, node->id, node);
}

const gchar *
node_get_id (Node *node)
{
	return node->id;
}

gboolean
node_set_sort_column (Node *node, nodeViewSortType sortColumn, gboolean reversed)
{
	if (node->sortColumn == sortColumn &&
	    node->sortReversed == reversed)
	    	return FALSE;

	node->sortColumn = sortColumn;
	node->sortReversed = reversed;

	return TRUE;
}

const gchar *
node_get_base_url(Node *node)
{
	const gchar 	*baseUrl = NULL;

	if (node->subscription) {
		baseUrl = subscription_get_homepage (node->subscription);
		if (!baseUrl)
			baseUrl = subscription_get_source (node->subscription);
	}


	/* prevent feed scraping commands to end up as base URI */
	if (!((baseUrl != NULL) &&
	      (baseUrl[0] != '|') &&
	      (strstr(baseUrl, "://") != NULL)))
	   	baseUrl = NULL;

	return baseUrl;
}

gboolean
node_can_add_child_feed (Node *node)
{
	g_assert (node->source->root);

	if (!(NODE_PROVIDER (node->source->root)->capabilities & NODE_CAPABILITY_ADD_CHILDS))
		return FALSE;

	return (NODE_SOURCE_TYPE (node)->capabilities & NODE_SOURCE_CAPABILITY_ADD_FEED);
}

gboolean
node_can_add_child_folder (Node *node)
{
	g_assert (node->source->root);

	if (!(NODE_PROVIDER (node->source->root)->capabilities & NODE_CAPABILITY_ADD_CHILDS))
		return FALSE;

	return (NODE_SOURCE_TYPE (node)->capabilities & NODE_SOURCE_CAPABILITY_ADD_FOLDER);
}


static void
save_item_to_file_metadata_callback (const gchar *key, const gchar *value, guint index, gpointer user_data)
{
	xmlTextWriterPtr writer = (xmlTextWriterPtr) user_data;
	if (value == NULL) {
		return;	/* No value, nothing to do anyway. */
	}

	if (g_strcmp0(key, "commentsUri") == 0) {
		xmlTextWriterWriteElement (writer, BAD_CAST "comments", BAD_CAST value);
	}
	else if (g_strcmp0(key, "category") == 0) {
		xmlTextWriterWriteElement (writer, BAD_CAST "category", BAD_CAST value);
	}
	else if (g_strcmp0(key, "enclosure") == 0) {
		enclosurePtr encl = enclosure_from_string (value);
		if (encl != NULL) {
			/* There is no reason to save an enclosure with no URL. */
			if (encl->url) {
				xmlTextWriterStartElement(writer, BAD_CAST "enclosure");
				xmlTextWriterWriteAttribute(writer, BAD_CAST "url", BAD_CAST encl->url);

				/* Spec says both size and type are required but not all feeds respect this. */
				if (encl->mime) {
					xmlTextWriterWriteAttribute(writer, BAD_CAST "type", BAD_CAST encl->mime);
				}
				if (encl->size > 0) {
					gchar buf[32];
					g_snprintf(buf, sizeof(buf), "%ld", encl->size);
					buf[sizeof(buf)-1] = '\0';
					xmlTextWriterWriteAttribute(writer, BAD_CAST "length", BAD_CAST buf);
				}
				xmlTextWriterEndElement (writer);
			}
			enclosure_free (encl);
		}
	}
}

static void
save_item_to_file_callback (itemPtr item, gpointer userdata)
{
	xmlTextWriterPtr writer = (xmlTextWriterPtr) userdata;
	xmlTextWriterStartElement (writer, BAD_CAST "item");

	if (item->title) {
		xmlTextWriterWriteElement (writer, BAD_CAST "title", BAD_CAST item->title);
	}

	gchar *link = item_make_link (item);
	if (link) {
		xmlTextWriterWriteElement (writer, BAD_CAST "link", BAD_CAST link);
		g_free (link);
	}

	if (item->sourceId && item->validGuid) {
		xmlTextWriterWriteElement (writer, BAD_CAST "guid", BAD_CAST item->sourceId);
	}

	if (item->time > 0) {
		gchar *datestr = date_format_rfc822_en_gmt (item->time);
		if (datestr) {
			xmlTextWriterWriteElement (writer, BAD_CAST "pubDate", BAD_CAST datestr);
			g_free (datestr);
		}
	}

	const gchar *author = item_get_author (item);
	if (author) {
		xmlTextWriterWriteElement (writer, BAD_CAST "author", BAD_CAST author);
	}

	if (item->metadata) {
		metadata_list_foreach(item->metadata,
		                      save_item_to_file_metadata_callback,
		                      (gpointer) writer);

	}

	if (item->description) {
		xmlTextWriterStartElement (writer, BAD_CAST "description");
		xmlTextWriterWriteCDATA (writer, BAD_CAST item->description);
		xmlTextWriterEndElement (writer);	/* </description> */
	}

	xmlTextWriterEndElement (writer);	/* </item> */
}


void
node_save_items_to_file (Node *node, const gchar *filename, GError **error)
{
	itemSetPtr items;

	xmlTextWriterPtr writer = xmlNewTextWriterFilename (filename, 0);
	if (writer == NULL) {
		int errno_tmp = errno;
		if (error) {
			g_set_error (error,
			             G_IO_ERROR,
			             G_IO_ERROR_FAILED,
			             _("Failed to create feed file: %s"),
			             g_strerror (errno_tmp));
		}
		return;
	}

	xmlTextWriterStartDocument (writer, NULL, "UTF-8", NULL);
	xmlTextWriterStartElement (writer, BAD_CAST "rss");
	xmlTextWriterWriteAttribute (writer, BAD_CAST "version", BAD_CAST "2.0");
	xmlTextWriterStartElement (writer, BAD_CAST "channel");

	xmlTextWriterWriteElement (writer, BAD_CAST "title", BAD_CAST node_get_title(node));
	const gchar *baseurl = node_get_base_url (node);
	if (baseurl) {
		xmlTextWriterWriteElement (writer, BAD_CAST "link", BAD_CAST baseurl);
	}

	/* RSS 2.0 spec requires a description */
	xmlTextWriterWriteElement (writer, BAD_CAST "description", BAD_CAST
		_("Feed file exported from Liferea"));
	xmlTextWriterWriteElement (writer, BAD_CAST "generator", BAD_CAST "Liferea");

	items = node_get_itemset (node);
	itemset_foreach (items, save_item_to_file_callback, (gpointer) writer);
	itemset_free (items);

	xmlTextWriterEndElement (writer);	/* </channel> */
	xmlTextWriterEndElement (writer);	/* </rss> */

	if (xmlTextWriterEndDocument (writer) == -1) {
		if (error) {
			g_set_error (error,
			             G_IO_ERROR,
			             G_IO_ERROR_FAILED,
			             _("Error while saving feed file"));
		}
	}

	xmlFreeTextWriter (writer);
	xmlCleanupParser ();
}


/* node children iterating interface */

void
node_foreach_child_full (Node *node, gpointer func, gint params, gpointer user_data)
{
	GSList		*children, *iter;

	g_assert (NULL != node);

	/* We need to copy because func might modify the list */
	iter = children = g_slist_copy (node->children);
	while (iter) {
		Node *childNode = (Node *)iter->data;

		/* Apply the method to the child */
		if (0 == params)
			((nodeActionFunc)func) (childNode);
		else
			((nodeActionDataFunc)func) (childNode, user_data);

		/* Never descend! */

		iter = g_slist_next (iter);
	}

	g_slist_free (children);
}
