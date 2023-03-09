/*
 * @file node.c  hierarchic feed list node handling
 *
 * Copyright (C) 2003-2022 Lars Windolf <lars.windolf@gmx.de>
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
#include "render.h"
#include "subscription_icon.h"
#include "update.h"
#include "vfolder.h"
#include "date.h"
#include "fl_sources/node_source.h"
#include "ui/feed_list_view.h"
#include "ui/icons.h"
#include "ui/liferea_shell.h"

static GHashTable *nodes = NULL;	/*<< node id -> node lookup table */

#define NODE_ID_LEN	7

nodePtr
node_is_used_id (const gchar *id)
{
	if (!id || !nodes)
		return NULL;

	return (nodePtr)g_hash_table_lookup (nodes, id);
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

nodePtr
node_from_id (const gchar *id)
{
	nodePtr node;

	node = node_is_used_id (id);
	if (!node)
		debug1 (DEBUG_GUI, "Fatal: no node with id \"%s\" found!", id);

	return node;
}

nodePtr
node_new (nodeTypePtr type)
{
	nodePtr	node;
	gchar	*id;

	g_assert (NULL != type);

	node = g_new0 (struct node, 1);
	node->type = type;
	node->sortColumn = NODE_VIEW_SORT_BY_TIME;
	node->sortReversed = TRUE;	/* default sorting is newest date at top */
	node->available = TRUE;

	id = node_new_id ();
	node_set_id (node, id);
	g_free (id);

	return node;
}

void
node_set_data (nodePtr node, gpointer data)
{
	g_assert (NULL == node->data);
	g_assert (NULL != node->type);

	node->data = data;
}

void
node_set_subscription (nodePtr node, subscriptionPtr subscription)
{
	g_assert (NULL == node->subscription);
	g_assert (NULL != node->type);

	node->subscription = subscription;
	subscription->node = node;

	/* Besides the favicon age we have no persistent
	   update state field, so everything else goes NULL */
	if (node->iconFile && !strstr(node->iconFile, "default.svg")) {
		subscription->updateState->lastFaviconPoll = (guint64)(common_get_mod_time (node->iconFile)) * G_USEC_PER_SEC;
		debug2 (DEBUG_UPDATE, "Setting last favicon poll time for %s to %lu", node->id, subscription->updateState->lastFaviconPoll / G_USEC_PER_SEC);
	}
}

void
node_update_subscription (nodePtr node, gpointer user_data)
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
node_auto_update_subscription (nodePtr node)
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
node_reset_update_counter (nodePtr node, guint64 *now)
{
	subscription_reset_update_counter (node->subscription, now);

	node_foreach_child_data (node, node_reset_update_counter, now);
}

gboolean
node_is_ancestor (nodePtr node1, nodePtr node2)
{
	nodePtr	tmp;

	tmp = node2->parent;
	while (tmp) {
		if (node1 == tmp)
			return TRUE;
		tmp = tmp->parent;
	}
	return FALSE;
}

void
node_free (nodePtr node)
{
	if (node->data && NODE_TYPE (node)->free)
		NODE_TYPE (node)->free (node);

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
	g_free (node);
}

static void
node_calc_counters (nodePtr node)
{
	/* Order is important! First update all children
	   so that hierarchical nodes (folders and feed
	   list sources) can determine their own unread
	   count as the sum of all childs afterwards */
	node_foreach_child (node, node_calc_counters);

	NODE_TYPE (node)->update_counters (node);
}

static void
node_update_parent_counters (nodePtr node)
{
	guint old;

	if (!node)
		return;

	old = node->unreadCount;

	NODE_TYPE (node)->update_counters (node);

	if (old != node->unreadCount) {
		feed_list_view_update_node (node->id);
		feedlist_new_items (0);	/* add 0 new items, as 'new-items' signal updates unread items also */
	}

	if (node->parent)
		node_update_parent_counters (node->parent);
}

void
node_update_counters (nodePtr node)
{
	guint oldUnreadCount = node->unreadCount;
	guint oldItemCount = node->itemCount;

	/* Update the node itself and its children */
	node_calc_counters (node);

	if ((oldUnreadCount != node->unreadCount) ||
	    (oldItemCount != node->itemCount))
		feed_list_view_update_node (node->id);

	/* Update the unread count of the parent nodes,
	   usually they just add all child unread counters */
	if (!IS_VFOLDER (node))
		node_update_parent_counters (node->parent);
}

void
node_update_favicon (nodePtr node)
{
	if (NODE_TYPE (node)->capabilities & NODE_CAPABILITY_UPDATE_FAVICON) {
		debug1 (DEBUG_UPDATE, "favicon of node %s needs to be updated...", node->title);
		subscription_icon_update (node->subscription);
	}

	/* Recursion */
	if (node->children)
		node_foreach_child (node, node_update_favicon);
}

itemSetPtr
node_get_itemset (nodePtr node)
{
	return NODE_TYPE (node)->load (node);
}

void
node_mark_all_read (nodePtr node)
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

gchar *
node_render(nodePtr node)
{
	return NODE_TYPE (node)->render (node);
}

/* import callbacks and helper functions */

void
node_set_parent (nodePtr node, nodePtr parent, gint position)
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
node_reparent (nodePtr node, nodePtr new_parent)
{
	nodePtr old_parent;

	g_assert (NULL != new_parent);
	g_assert (NULL != node);

	debug2 (DEBUG_GUI, "Reparenting node '%s' to a parent '%s'", node_get_title(node), node_get_title(new_parent));

	old_parent = node->parent;
	if (NULL != old_parent)
		old_parent->children = g_slist_remove (old_parent->children, node);

	new_parent->children = g_slist_insert (new_parent->children, node, -1);
	node->parent = new_parent;

	feed_list_view_remove_node (node);
	feed_list_view_add_node (node);
}

void
node_remove (nodePtr node)
{
	/* using itemlist_remove_all_items() ensures correct unread
	   and item counters for all parent folders and matching
	   search folders */
	itemlist_remove_all_items (node);

	NODE_TYPE (node)->remove (node);
}

static xmlDocPtr
node_to_xml (nodePtr node)
{
	xmlDocPtr	doc;
	xmlNodePtr	rootNode;
	gchar		*tmp;

	doc = xmlNewDoc (BAD_CAST"1.0");
	rootNode = xmlNewDocNode (doc, NULL, BAD_CAST"node", NULL);
	xmlDocSetRootElement (doc, rootNode);

	xmlNewTextChild (rootNode, NULL, BAD_CAST"title", BAD_CAST node_get_title (node));

	tmp = g_strdup_printf ("%u", node->unreadCount);
	xmlNewTextChild (rootNode, NULL, BAD_CAST"unreadCount", BAD_CAST tmp);
	g_free (tmp);

	tmp = g_strdup_printf ("%u", g_slist_length (node->children));
	xmlNewTextChild (rootNode, NULL, BAD_CAST"children", BAD_CAST tmp);
	g_free (tmp);

	return doc;
}

gchar *
node_default_render (nodePtr node)
{
	gchar		*result;
	xmlDocPtr	doc;

	doc = node_to_xml (node);
	result = render_xml (doc, NODE_TYPE(node)->id, NULL);
	xmlFreeDoc (doc);

	return result;
}

/* helper functions to be used with node_foreach* */

void
node_save(nodePtr node)
{
	NODE_TYPE(node)->save(node);
}

/* node attributes encapsulation */

void
node_set_title (nodePtr node, const gchar *title)
{
	g_free (node->title);
	node->title = g_strstrip (g_strdelimit (g_strdup (title), "\r\n", ' '));
}

const gchar *
node_get_title (nodePtr node)
{
	return node->title;
}

void
node_load_icon (nodePtr node)
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
		node->iconFile = g_build_filename (PACKAGE_DATA_DIR, PACKAGE, "pixmaps", "default.svg", NULL);
}

/* determines the nodes favicon or default icon */
gpointer
node_get_icon (nodePtr node)
{
	if (!node->icon)
		return (gpointer) icon_get (NODE_TYPE(node)->icon);

	return node->icon;
}

const gchar *
node_get_favicon_file (nodePtr node)
{
	return node->iconFile;
}

void
node_set_id (nodePtr node, const gchar *id)
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
node_get_id (nodePtr node)
{
	return node->id;
}

gboolean
node_set_sort_column (nodePtr node, nodeViewSortType sortColumn, gboolean reversed)
{
	if (node->sortColumn == sortColumn &&
	    node->sortReversed == reversed)
	    	return FALSE;

	node->sortColumn = sortColumn;
	node->sortReversed = reversed;

	return TRUE;
}

const gchar *
node_get_base_url(nodePtr node)
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
node_can_add_child_feed (nodePtr node)
{
	g_assert (node->source->root);

	if (!(NODE_TYPE (node->source->root)->capabilities & NODE_CAPABILITY_ADD_CHILDS))
		return FALSE;

	return (NODE_SOURCE_TYPE (node)->capabilities & NODE_SOURCE_CAPABILITY_ADD_FEED);
}

gboolean
node_can_add_child_folder (nodePtr node)
{
	g_assert (node->source->root);

	if (!(NODE_TYPE (node->source->root)->capabilities & NODE_CAPABILITY_ADD_CHILDS))
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
node_save_items_to_file (nodePtr node, const gchar *filename, GError **error)
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
node_foreach_child_full (nodePtr node, gpointer func, gint params, gpointer user_data)
{
	GSList		*children, *iter;

	g_assert (NULL != node);

	/* We need to copy because func might modify the list */
	iter = children = g_slist_copy (node->children);
	while (iter) {
		nodePtr childNode = (nodePtr)iter->data;

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
