/**
 * @file feed.c  feed node and subscription type
 * 
 * Copyright (C) 2003-2009 Lars Lindner <lars.lindner@gmail.com>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include "conf.h"
#include "common.h"
#include "db.h"
#include "debug.h"
#include "favicon.h"
#include "feed.h"
#include "feedlist.h"
#include "itemlist.h"
#include "metadata.h"
#include "node.h"
#include "render.h"
#include "script.h"
#include "update.h"
#include "xml.h"
#include "ui/liferea_shell.h"
#include "ui/auth_dialog.h"
#include "ui/ui_subscription.h"
#include "ui/ui_node.h"
#include "notification/notification.h"

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
	gchar		*cacheLimitStr, *filter, *intervalStr, *title; 
	gchar		*htmlUrlStr, *source, *tmp; 
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
	
	tmp = xmlGetProp (xml, BAD_CAST"noIncremental");
	if (tmp && !xmlStrcmp (tmp, BAD_CAST"true"))
		feed->noIncremental = TRUE;
	xmlFree (tmp);
	
	/* enclosure auto download flag */
	tmp = xmlGetProp (xml, BAD_CAST"encAutoDownload");
	if (tmp && !xmlStrcmp (tmp, BAD_CAST"true"))
		feed->encAutoDownload = TRUE;
	xmlFree (tmp);
			
	/* auto item link loading flag */
	tmp = xmlGetProp (xml, BAD_CAST"loadItemLink");
	if (tmp && !xmlStrcmp (tmp, BAD_CAST"true"))
		feed->loadItemLink = TRUE;
	xmlFree (tmp);

	/* comment feed handling flag */
	tmp = xmlGetProp (xml, BAD_CAST"ignoreComments");
	if (tmp && !xmlStrcmp (tmp, BAD_CAST"true"))
		feed->ignoreComments = TRUE;
	xmlFree (tmp);

	/* popup enforcement/prevention flags */
	tmp = xmlGetProp (xml, BAD_CAST"enforcePopup");
	if (tmp && !xmlStrcmp (tmp, BAD_CAST"true"))
		feed->enforcePopup = TRUE;
	xmlFree (tmp);

	tmp = xmlGetProp (xml, BAD_CAST"preventPopup");
	if (tmp && !xmlStrcmp (tmp, BAD_CAST"true"))
		feed->preventPopup = TRUE;
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
	
	node_set_icon (node, favicon_load_from_cache (node->id));
	
	if (node->subscription)
		debug4 (DEBUG_CACHE, "import feed: title=%s source=%s typeStr=%s interval=%d", 
		        node_get_title (node), 
	        	subscription_get_source (node->subscription), 
		        typeStr, 
		        subscription_get_update_interval (node->subscription));

	node_set_parent (node, parent, -1);
	feedlist_node_imported (node);
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

		if (feed->noIncremental)
			xmlNewProp (xml, BAD_CAST"noIncremental", BAD_CAST"true");
			
		if (feed->encAutoDownload)
			xmlNewProp (xml, BAD_CAST"encAutoDownload", BAD_CAST"true");
			
		if (feed->loadItemLink)
			xmlNewProp (xml, BAD_CAST"loadItemLink", BAD_CAST"true");
			
		if (feed->ignoreComments)
			xmlNewProp (xml, BAD_CAST"ignoreComments", BAD_CAST"true");
			
		if (feed->enforcePopup)
			xmlNewProp (xml, BAD_CAST"enforcePopup", BAD_CAST"true");
			
		if (feed->preventPopup)
			xmlNewProp (xml, BAD_CAST"preventPopup", BAD_CAST"true");
			
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

	// FIXME: move subscription stuff to subscription.c
	if (node->subscription) {
		xmlNewTextChild (feedNode, NULL, "feedSource", subscription_get_source (node->subscription));
		xmlNewTextChild (feedNode, NULL, "feedOrigSource", subscription_get_orig_source (node->subscription));

		tmp = g_strdup_printf ("%d", subscription_get_default_update_interval (node->subscription));
		xmlNewTextChild (feedNode, NULL, "feedUpdateInterval", tmp);
		g_free (tmp);
	
		tmp = g_strdup_printf ("%d", node->subscription->discontinued?1:0);
		xmlNewTextChild (feedNode, NULL, "feedDiscontinued", tmp);
		g_free (tmp);

		if (node->subscription->updateError)
			xmlNewTextChild (feedNode, NULL, "updateError", node->subscription->updateError);
		if (node->subscription->httpError) {
			xmlNewTextChild (feedNode, NULL, "httpError", node->subscription->httpError);

			tmp = g_strdup_printf ("%d", node->subscription->httpErrorCode);
			xmlNewTextChild (feedNode, NULL, "httpErrorCode", tmp);
			g_free (tmp);
		}
		if (node->subscription->filterError)
			xmlNewTextChild (feedNode, NULL, "filterError", node->subscription->filterError);
	
		metadata_add_xml_nodes (node->subscription->metadata, feedNode);
	}

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
		feedNode = xmlDocGetRootElement (doc);
		feedNode = xmlNewDocNode (doc, NULL, "feed", NULL);
		xmlDocSetRootElement (doc, feedNode);
	}
	feed_add_xml_attributes (node, feedNode);
	
	return doc;
}

guint
feed_get_max_item_count (nodePtr node)
{
	feedPtr	feed = (feedPtr)node->data;
	
	switch (feed->cacheLimit) {
		case CACHE_DEFAULT:
			return conf_get_int_value (DEFAULT_MAX_ITEMS);
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
	gchar			*old_source;

	debug_enter ("feed_process_update_result");
	
	if (result->data) {
		/* we save all properties that should not be overwritten in all cases */
		old_source = g_strdup (subscription_get_source (subscription));	// FIXME: stupid concept?

		/* parse the new downloaded feed into feed and itemSet */
		ctxt = feed_create_parser_ctxt ();
		ctxt->feed = feed;
		ctxt->data = result->data;
		ctxt->dataLength = result->size;
		ctxt->subscription = subscription;

		/* try to parse the feed */
		if (!feed_parse (ctxt)) {
			/* if it doesn't work and it is a new subscription
			   start feed auto discovery */
			if (flags & FEED_REQ_AUTO_DISCOVER)
				feed_parser_auto_discover (ctxt);
		}
		
		if (ctxt->failed) {
			node->available = FALSE;

			g_string_prepend (feed->parseErrors, _("<p>Could not detect the type of this feed! Please check if the source really points to a resource provided in one of the supported syndication formats!</p>"
			                                       "XML Parser Output:<br /><div class='xmlparseroutput'>"));
			g_string_append (feed->parseErrors, "</div>");
		}
		
		if (!ctxt->failed && !(flags & FEED_REQ_AUTO_DISCOVER)) {
			itemSetPtr	itemSet;
			guint		newCount;
			
			node->available = TRUE;
			
			/* merge the resulting items into the node's item set */
			itemSet = node_get_itemset (node);
			newCount = itemset_merge_items (itemSet, ctxt->items, ctxt->feed->valid, ctxt->feed->markAsRead);
			itemlist_merge_itemset (itemSet);
			itemset_free (itemSet);

			feedlist_node_was_updated (node, newCount);
			
			/* restore user defined properties if necessary */
			if (flags & FEED_REQ_RESET_TITLE)
				node_set_title (node, ctxt->title);
				
			if (!(flags & FEED_REQ_AUTO_DISCOVER))
				subscription_set_source (subscription, old_source);

			if (flags & FEED_REQ_RESET_UPDATE_INT) {
				/* The following check is to prevent the rare case
				   that a high-frequency/volume feed provides a feed-
				   specific update interval that is lower than the
				   users preferred update interval. This e.g. 1min
				   updates might be bad for laptop users... */
				if (subscription_get_default_update_interval (subscription) < conf_get_int_value (DEFAULT_UPDATE_INTERVAL))
					subscription_set_update_interval (subscription, conf_get_int_value (DEFAULT_UPDATE_INTERVAL));
				else
					subscription_set_update_interval (subscription, subscription_get_default_update_interval(subscription));
			}
			
			if (flags > 0)
				db_subscription_update (subscription);

			liferea_shell_set_status_bar (_("\"%s\" updated..."), node_get_title (node));
	
			if (!feed->preventPopup)				
				notification_node_has_new_items (node, feed->enforcePopup);
		}
				
		g_free (old_source);
		feed_free_parser_ctxt (ctxt);
	} else {
		node->available = FALSE;
		
		liferea_shell_set_status_bar (_("\"%s\" is not available"), node_get_title (node));
	}

	script_run_for_hook (SCRIPT_HOOK_FEED_UPDATED);

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
feed_update_unread_count (nodePtr node)
{
	node->itemCount = db_itemset_get_item_count (node->id);
	node->unreadCount = db_itemset_get_unread_count (node->id);
}

static void
feed_remove (nodePtr node)
{
	notification_node_removed (node);
	ui_node_remove_node (node);
	
	favicon_remove_from_cache (node->id);
	db_subscription_remove (node->id);
}

static gchar *
feed_render (nodePtr node)
{
	renderParamPtr	params;
	gchar		*output = NULL;
	xmlDocPtr	doc;

	doc = feed_to_xml (node, NULL);
	params = render_parameter_new ();
	render_parameter_add (params, "pixmapsDir='file://" PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S "'");
	output = render_xml (doc, "feed", params);
	xmlFreeDoc (doc);

	return output;
}

static void
feed_add (nodePtr parentNode)
{
	ui_subscription_dialog_new (parentNode);
}

static void
feed_properties (nodePtr node)
{
	ui_subscription_prop_dialog_new (node->subscription);
}

static void
feed_free (nodePtr node)
{
	feedPtr	feed = (feedPtr)node->data;

	if (feed->parseErrors)
		g_string_free(feed->parseErrors, TRUE);
	g_free (feed->htmlUrl);
	g_free (feed);
}

subscriptionTypePtr
feed_get_subscription_type (void)
{
	static struct subscriptionType sti = {
		feed_prepare_update_request,
		feed_process_update_result,
		NULL  // FIXME
	};
	
	return &sti;
}

nodeTypePtr
feed_get_node_type (void)
{ 
	static struct nodeType nti = {
		NODE_CAPABILITY_SHOW_UNREAD_COUNT |
		NODE_CAPABILITY_UPDATE,
		"feed",		/* not used, feed format ids are used instead */
		NULL,
		feed_import,
		feed_export,
		feed_load,
		feed_save,
		feed_update_unread_count,
		feed_remove,
		feed_render,
		feed_add,
		feed_properties,
		feed_free
	};
	nti.icon = icons[ICON_DEFAULT];
	
	return &nti; 
}
