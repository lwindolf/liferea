/**
 * @file feed.c  feed node and subscription type
 * 
 * Copyright (C) 2003-2013 Lars Windolf <lars.windolf@gmx.de>
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

#include "feed.h"

#include <string.h>

#include "conf.h"
#include "common.h"
#include "db.h"
#include "debug.h"
#include "favicon.h"
#include "feedlist.h"
#include "itemlist.h"
#include "metadata.h"
#include "node.h"
#include "render.h"
#include "update.h"
#include "xml.h"
#include "ui/icons.h"
#include "ui/liferea_shell.h"
#include "ui/subscription_dialog.h"
#include "ui/feed_list_node.h"

feedPtr
feed_new (void)
{
	feedPtr		feed;
	
	feed = g_new0 (struct feed, 1);

	feed->cacheLimit = CACHE_DEFAULT;
	feed->valid = TRUE;

	return feed;
}

static void
feed_import (nodePtr node, nodePtr parent, xmlNodePtr xml, gboolean trusted)
{
	gchar		*cacheLimitStr, *title; 
	gchar		*tmp; 
	feedPtr		feed = NULL;
		
	xmlChar	*typeStr = xmlGetProp (xml, BAD_CAST"type");
		
	feed = feed_new ();
	feed->fhp = feed_type_str_to_fhp (typeStr);
	xmlFree (typeStr);
		
	node_set_data (node, feed);
	node_set_subscription (node, subscription_import (xml, trusted));

	/* Set the feed cache limit */
	cacheLimitStr = xmlGetProp (xml, BAD_CAST "cacheLimit");
	if (cacheLimitStr && !xmlStrcmp (cacheLimitStr, "unlimited"))
		feed->cacheLimit = CACHE_UNLIMITED;
	else
		feed->cacheLimit = common_parse_long (cacheLimitStr, CACHE_DEFAULT);
	xmlFree (cacheLimitStr);
	
	/* enclosure auto download flag */
	tmp = xmlGetProp (xml, BAD_CAST"encAutoDownload");
	if (tmp && !xmlStrcmp (tmp, BAD_CAST"true"))
		feed->encAutoDownload = TRUE;
	xmlFree (tmp);
			
	/* comment feed handling flag */
	tmp = xmlGetProp (xml, BAD_CAST"ignoreComments");
	if (tmp && !xmlStrcmp (tmp, BAD_CAST"true"))
		feed->ignoreComments = TRUE;
	xmlFree (tmp);

	tmp = xmlGetProp (xml, BAD_CAST"markAsRead");
	if (tmp && !xmlStrcmp (tmp, BAD_CAST"true"))
		feed->markAsRead = TRUE;
	xmlFree (tmp);
							
	title = xmlGetProp (xml, BAD_CAST"title");
	if (!title || !xmlStrcmp (title, BAD_CAST"")) {
		if (title)
			xmlFree (title);
		title = xmlGetProp (xml, BAD_CAST"text");
	}

	node_set_title (node, title);
	xmlFree (title);
	
	if (node->subscription)
		debug4 (DEBUG_CACHE, "import feed: title=%s source=%s typeStr=%s interval=%d", 
		        node_get_title (node), 
	        	subscription_get_source (node->subscription), 
		        typeStr, 
		        subscription_get_update_interval (node->subscription));
}

static void
feed_export (nodePtr node, xmlNodePtr xml, gboolean trusted)
{
	feedPtr feed = (feedPtr) node->data;
	gchar *cacheLimit = NULL;

	if (node->subscription)
		subscription_export (node->subscription, xml, trusted);

	if (trusted) {
		if (feed->cacheLimit >= 0)
			cacheLimit = g_strdup_printf ("%d", feed->cacheLimit);
		if (feed->cacheLimit == CACHE_UNLIMITED)
			cacheLimit = g_strdup ("unlimited");
		if (cacheLimit)
			xmlNewProp (xml, BAD_CAST"cacheLimit", BAD_CAST cacheLimit);

		if (feed->encAutoDownload)
			xmlNewProp (xml, BAD_CAST"encAutoDownload", BAD_CAST"true");
			
		if (feed->ignoreComments)
			xmlNewProp (xml, BAD_CAST"ignoreComments", BAD_CAST"true");
			
		if (feed->markAsRead)
			xmlNewProp (xml, BAD_CAST"markAsRead", BAD_CAST"true");
	}

	if (node->subscription)
		debug3 (DEBUG_CACHE, "adding feed: source=%s interval=%d cacheLimit=%s",
		        subscription_get_source (node->subscription), 
			subscription_get_update_interval (node->subscription),
		        (cacheLimit != NULL ? cacheLimit : ""));
	g_free (cacheLimit);
}

static void
feed_add_xml_attributes (nodePtr node, xmlNodePtr feedNode)
{
	feedPtr	feed = (feedPtr)node->data;
	gchar	*tmp;
	
	xmlNewTextChild (feedNode, NULL, "feedId", node_get_id (node));
	xmlNewTextChild (feedNode, NULL, "feedTitle", node_get_title (node));

	if (node->subscription)
		subscription_to_xml (node->subscription, feedNode);

	tmp = g_strdup_printf("%d", node->available?1:0);
	xmlNewTextChild(feedNode, NULL, "feedStatus", tmp);
	g_free(tmp);

	tmp = g_strdup_printf("file://%s", node_get_favicon_file (node));
	xmlNewTextChild(feedNode, NULL, "favicon", tmp);
	g_free(tmp);

	if(feed->parseErrors && (strlen(feed->parseErrors->str) > 0))
		xmlNewTextChild(feedNode, NULL, "parseError", feed->parseErrors->str);
}

xmlDocPtr
feed_to_xml (nodePtr node, xmlNodePtr feedNode)
{
	xmlDocPtr	doc = NULL;
	
	if (!feedNode) {
		doc = xmlNewDoc ("1.0");
		feedNode = xmlNewDocNode (doc, NULL, "feed", NULL);
		xmlDocSetRootElement (doc, feedNode);
	}
	feed_add_xml_attributes (node, feedNode);
	
	return doc;
}

guint
feed_get_max_item_count (nodePtr node)
{
	gint	default_max_items;
	feedPtr	feed = (feedPtr)node->data;
	
	switch (feed->cacheLimit) {
		case CACHE_DEFAULT:
			conf_get_int_value (DEFAULT_MAX_ITEMS, &default_max_items);
			return default_max_items;
			break;
		case CACHE_DISABLE:
		case CACHE_UNLIMITED:
			return G_MAXUINT;
			break;
		default:
			return feed->cacheLimit;
			break;
	}
}

/* implementation of subscription type interface */

static void
feed_process_update_result (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	feedParserCtxtPtr	ctxt;
	nodePtr			node = subscription->node;
	feedPtr			feed = (feedPtr)node->data;

	debug_enter ("feed_process_update_result");
	
	if (result->data) {
		/* parse the new downloaded feed into feed and itemSet */
		ctxt = feed_create_parser_ctxt ();
		ctxt->feed = feed;
		ctxt->data = result->data;
		ctxt->dataLength = result->size;
		ctxt->subscription = subscription;

		/* try to parse the feed */
		feed_parse (ctxt);
		
		if (ctxt->failed) {
			/* No feed found, display an error */
			node->available = FALSE;

			g_string_prepend (feed->parseErrors, _("<p>Could not detect the type of this feed! Please check if the source really points to a resource provided in one of the supported syndication formats!</p>"
			                                       "XML Parser Output:<br /><div class='xmlparseroutput'>"));
			g_string_append (feed->parseErrors, "</div>");
		} else if (!ctxt->failed && !ctxt->feed->fhp) {
			/* There's a feed but no Handler. This means autodiscovery
			 * found a feed, but we still need to download it.
			 * An update should be in progress that will process it */
		} else {
			/* Feed found, process it */
			itemSetPtr	itemSet;
			
			node->available = TRUE;
			
			/* merge the resulting items into the node's item set */
			itemSet = node_get_itemset (node);
			node->newCount = itemset_merge_items (itemSet, ctxt->items, ctxt->feed->valid, ctxt->feed->markAsRead);
			itemlist_merge_itemset (itemSet);
			itemset_free (itemSet);
		
			/* restore user defined properties if necessary */
			if ((flags & FEED_REQ_RESET_TITLE) && ctxt->title)
				node_set_title (node, ctxt->title);

			if (flags > 0)
				db_subscription_update (subscription);

			liferea_shell_set_status_bar (_("\"%s\" updated..."), node_get_title (node));
		}

		feed_free_parser_ctxt (ctxt);
	} else {
		node->available = FALSE;

		liferea_shell_set_status_bar (_("\"%s\" is not available"), node_get_title (node));
	}

	debug_exit ("feed_process_update_result");
}

static gboolean
feed_prepare_update_request (subscriptionPtr subscription, struct updateRequest *request)
{
	/* Nothing to do. Feeds require no subscription extra handling. */
	
	return TRUE;
}

/* implementation of the node type interface */

static itemSetPtr
feed_load (nodePtr node)
{
	return db_itemset_load(node->id);
}

static void
feed_save (nodePtr node)
{
	/* Nothing to do. Feeds do not have any UI states */
}

static void
feed_update_counters (nodePtr node)
{
	node->itemCount = db_itemset_get_item_count (node->id);
	node->unreadCount = db_itemset_get_unread_count (node->id);
}

static void
feed_remove (nodePtr node)
{
	feed_list_node_remove_node (node);
	
	favicon_remove_from_cache (node->id);
	db_subscription_remove (node->id);
}

static const gchar *
feed_get_direction(nodePtr feed)
{
	if (node_get_title (feed))
		return (common_get_text_direction (node_get_title (feed)));
	else
		return ("ltr");
}

static gchar *
feed_render (nodePtr node)
{
	gchar		*output = NULL;
	xmlDocPtr	doc;
	renderParamPtr	params;
	const gchar     *text_direction = NULL;

	text_direction = feed_get_direction (node);
	params = render_parameter_new ();
	render_parameter_add (params, "appDirection='%s'", common_get_app_direction ());
	render_parameter_add (params, "txtDirection='%s'", text_direction);

	doc = feed_to_xml (node, NULL);
	output = render_xml (doc, "feed", params);
	xmlFreeDoc (doc);

	return output;
}

static gboolean
feed_add (void)
{
	subscription_dialog_new ();
	return TRUE;
}

static void
feed_properties (nodePtr node)
{
	subscription_prop_dialog_new (node->subscription);
}

static void
feed_free (nodePtr node)
{
	feedPtr	feed = (feedPtr)node->data;

	if (feed->parseErrors)
		g_string_free (feed->parseErrors, TRUE);
	g_free (feed);
}

subscriptionTypePtr
feed_get_subscription_type (void)
{
	static struct subscriptionType sti = {
		feed_prepare_update_request,
		feed_process_update_result
	};
	
	return &sti;
}

nodeTypePtr
feed_get_node_type (void)
{ 
	static struct nodeType nti = {
		NODE_CAPABILITY_SHOW_UNREAD_COUNT |
		NODE_CAPABILITY_UPDATE |
		NODE_CAPABILITY_UPDATE_FAVICON |
		NODE_CAPABILITY_EXPORT,
		"feed",		/* not used, feed format ids are used instead */
		NULL,
		feed_import,
		feed_export,
		feed_load,
		feed_save,
		feed_update_counters,
		feed_remove,
		feed_render,
		feed_add,
		feed_properties,
		feed_free
	};
	nti.icon = icon_get (ICON_DEFAULT);
	
	return &nti; 
}
