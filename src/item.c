/**
 * @file item.c common item handling
 *
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.com>
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

#include <glib.h>
#include <time.h>
#include <string.h> /* For memset() */
#include <stdlib.h>
#include <libxml/uri.h>

#include "callbacks.h"
#include "common.h"
#include "debug.h"
#include "item.h"
#include "metadata.h"
#include "render.h"
#include "support.h"
#include "social.h"
#include "vfolder.h"

/* Item duplicate handling */

/*
 * To manage duplicate items we keep a hash of all item GUIDs
 * (note: non-globally unique ids are not managed in this list!) 
 * and for each GUID we keep a list of the parent feeds containing
 * items with this GUID. Precondition is of course that GUIDs
 * are unique per feed which is ensured in the parsing/merging
 * code. 
 */
static GHashTable *itemGuids = NULL;

void item_guid_list_add_id(itemPtr item) {
	GSList	*iter;
	
	g_assert(TRUE == item->validGuid);
	
	if(!itemGuids)
		itemGuids = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	iter = (GSList *)g_hash_table_lookup(itemGuids, item->id);
	iter = g_slist_append(iter, item->sourceNode);
	g_hash_table_insert(itemGuids, g_strdup(item->id), iter);
}

gboolean item_guid_list_check_id(itemPtr item) {

	if(!itemGuids)
		return FALSE;

	return (NULL != g_hash_table_lookup(itemGuids, item->id));
}

void item_guid_list_remove_id(itemPtr item) {
	GSList	*iter;
	
	if(!itemGuids)
		return;

	iter = (GSList *)g_hash_table_lookup(itemGuids, item->id);
	iter = g_slist_remove(iter, item->sourceNode);
	g_hash_table_insert(itemGuids, g_strdup(item->id), iter);
}

/* Item comments handling */

static void item_comments_process_update_result(struct request *request) {
	feedParserCtxtPtr	ctxt;
	itemPtr			item = (itemPtr)request->user_data;

	debug_enter("item_comments_process_update_result");
	
	g_assert(0 < item->sourceNode->loaded);

	/* note this is to update the feed URL on permanent redirects */
	if(!strcmp(request->source, metadata_list_get(item->metadata, "commentFeedUri"))) {
	
		debug2(DEBUG_UPDATE, "updating comment feed URL from \"%s\" to \"%s\"", 
		                     metadata_list_get(item->metadata, "commentFeedUri"), 
				     request->source);
				     
		metadata_list_set(&(item->metadata), "commentFeedUri", request->source);
		item->sourceNode->needsCacheSave = TRUE;
	}
	
	if(401 == request->httpstatus) { /* unauthorized */
		// FIXME: argh... give some hint in GUI!
	} else if(410 == request->httpstatus) { /* gone */
		// FIXME: how to prevent further updates?
	} else if(304 == request->httpstatus) {
		debug1(DEBUG_UPDATE, "comment feed \"%s\" did not change", request->source);
	} else if(request->data) {
		debug1(DEBUG_UPDATE, "received update result for comment feed \"%s\"", request->source);

		/* parse the new downloaded feed into feed and itemSet */
		ctxt = feed_create_parser_ctxt();
		ctxt->feed = feed_new(request->source, NULL, NULL);
		ctxt->node = node_new();
		node_set_type(ctxt->node, feed_get_node_type());
		node_set_data(ctxt->node, ctxt->feed);		
		ctxt->data = request->data;
		ctxt->dataLength = request->size;
		feed_parse(ctxt, FALSE);

		if(ctxt->failed) {
			debug0(DEBUG_UPDATE, "parsing comment feed failed!");
		} else {
			/* drop old comment items and take new ones instead */
			GList *iter = item->comments->items;
			while(iter) {
				item_free((itemPtr)iter->data);
				iter = g_list_next(iter);
			}
			g_list_free(item->comments->items);
			g_free(item->comments);
			
			item->comments = ctxt->itemSet;
			ctxt->itemSet = NULL;
			
			iter = item->comments->items;
			while(iter) {
				debug1(DEBUG_UPDATE, " -> %s\n", ((itemPtr)iter->data)->title);
				iter = g_list_next(iter);
			}
			
			item->sourceNode->needsCacheSave = TRUE;
		}
				
		feed_free_parser_ctxt(ctxt);
		node_free(ctxt->node);
	}

	g_free(request->options);
	item->updateRequest = NULL; 

	itemview_update_item(item); 
	itemview_update();
	
	node_unload(item->sourceNode);	/* release item from memory */
	
	debug_exit("item_comments_process_update_result");
}

void item_comments_refresh(itemPtr item) { 
	struct request	*request;
	const gchar	*url;
	
	url = metadata_list_get(item->metadata, "commentFeedUri");
	
	if(url) {
		debug2(DEBUG_UPDATE, "Updating comments for item \"%s\" (comment URL: %s)", item->title, url);

		node_load(item->sourceNode);	/* force item to stay in memory */

		request = update_request_new(item);
		request->user_data = item;
		request->options = g_new0(struct updateOptions, 1);
		request->callback = item_comments_process_update_result;
		request->source = g_strdup(url);
		item->updateRequest = request;
		update_execute_request(request);

		itemview_update_item(item); 
		itemview_update();
	}
}

void item_comments_monitor(itemPtr item) { item->monitorComments = TRUE; }

void item_comments_unmonitor(itemPtr item) { item->monitorComments = FALSE; }

/* Item handling */

itemPtr item_new(void) {
	itemPtr		item;
	
	item = g_new0(struct item, 1);
	
	item->comments = g_new0(struct itemSet, 1);
	item->comments->type = ITEMSET_TYPE_FEED;
	item->updateState = update_state_new();
	
	item->newStatus = TRUE;
	item->popupStatus = TRUE;
	
	return item;
}

itemPtr item_copy(itemPtr item) {
	itemPtr copy = item_new();

	item_set_title(copy, item->title);
	item_set_source(copy, item->source);
	item_set_real_source_url(copy, item->real_source_url);
	item_set_real_source_title(copy, item->real_source_title);
	item_set_description(copy, item->description);
	item_set_id(copy, item->id);
	
	copy->updateStatus = item->updateStatus;
	copy->readStatus = item->readStatus;
	copy->newStatus = FALSE;
	copy->popupStatus = FALSE;
	copy->flagStatus = item->flagStatus;
	copy->time = item->time;
	copy->validGuid = item->validGuid;
	
	/* the following line allows state propagation in item.c */
	copy->sourceNode = item->itemSet->node;
	copy->sourceNr = item->nr;

	/* this copies metadata */
	copy->metadata = metadata_list_copy(item->metadata);
	
	// FIXME: deep copy of comments

	return copy;
}

void item_set_title(itemPtr item, const gchar * title) {

	g_free(item->title);
	item->title = g_strdup(title);
}

void item_set_description(itemPtr item, const gchar * description) {

	g_free(item->description);
	item->description = g_strdup(description);
}

void item_set_source(itemPtr item, const gchar * source) { 

	g_free(item->source);
	if(source) 
		item->source = g_strchomp(g_strdup(source));
	else
		item->source = NULL;
}

void item_set_real_source_url(itemPtr item, const gchar * source) { 

	g_free(item->real_source_url);
	if(source)
		item->real_source_url = g_strchomp(g_strdup(source));
	else
		item->real_source_url = NULL;
}

void item_set_real_source_title(itemPtr item, const gchar * source) { 

	g_free(item->real_source_title);
	if(source)
		item->real_source_title = g_strchomp(g_strdup(source));
	else
		item->real_source_title = NULL;
}

void item_set_id(itemPtr item, const gchar * id) {
	g_free(item->id);
	item->id = g_strdup(id);
}

const gchar *	item_get_id(itemPtr item) { return item->id; }
const gchar *	item_get_title(itemPtr item) {return item->title; }
const gchar *	item_get_description(itemPtr item) { return item->description; }
const gchar *	item_get_source(itemPtr item) { return item->source; }
const gchar *	item_get_real_source_url(itemPtr item) { return item->real_source_url; }
const gchar *	item_get_real_source_title(itemPtr item) { return item->real_source_title; }

void item_free(itemPtr item) {

	/* Explicitely no removal from GUID list here! As item_free is used for unloading too. */
	
	if(item->comments) {
		GList	*iter = item->comments->items;
		while(iter) {
			item_free((itemPtr)iter->data);
			iter = g_list_next(iter);
		}
		g_free(item->comments);
	}

	g_free(item->title);
	g_free(item->source);
	g_free(item->real_source_url);
	g_free(item->real_source_title);
	g_free(item->description);
	g_free(item->id);
	g_assert(NULL == item->tmpdata);	/* should be free after rendering */
	metadata_list_free(item->metadata);
	
	if(item->updateRequest)
		update_cancel_requests((gpointer)item);
	
	if(item->updateState)
		update_state_free(item->updateState);
		
	g_free(item->commentsError);
	g_free(item);
}

const gchar * item_get_base_url(itemPtr item) {

	if(item->sourceNode && (NODE_TYPE_FEED == item->sourceNode->type))
		return feed_get_html_url((feedPtr)item->sourceNode->data);
	else
		return itemset_get_base_url(item->itemSet);
}

static void item_parse_comments(xmlNodePtr cur, itemPtr item) {
	gchar		*tmp;

	update_state_import(cur, item->updateState);
	
	cur = cur->xmlChildrenNode;
	while(cur) {
		if(cur->type != XML_ELEMENT_NODE ||
		   !(tmp = common_utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1)))) {
			cur = cur->next;
			continue;
		}

		if(!xmlStrcmp(cur->name, BAD_CAST"item")) 
			itemset_append_item(item->comments, item_parse_cache(cur, FALSE));

		g_free(tmp);
		tmp = NULL;
		cur = cur->next;
	}
}

itemPtr item_parse_cache(xmlNodePtr cur, gboolean migrateCache) {
	itemPtr 	item;
	gchar		*tmp;
	
	g_assert(NULL != cur);
	
	item = item_new();
	item->popupStatus = FALSE;
	item->newStatus = FALSE;
	
	cur = cur->xmlChildrenNode;
	while(cur) {
		if(cur->type != XML_ELEMENT_NODE ||
		   !(tmp = common_utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1)))) {
			cur = cur->next;
			continue;
		}
		
		if(!xmlStrcmp(cur->name, BAD_CAST"title"))
			item_set_title(item, tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"description"))
			item_set_description(item, tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"source"))
			item_set_source(item, tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"real_source_url"))
			item_set_real_source_url(item, tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"real_source_title"))
			item_set_real_source_title(item, tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"id"))
			item_set_id(item, tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"validGuid"))
			item->validGuid = TRUE;
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"nr"))
			item->nr = item->sourceNr = atol(tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"readStatus"))
			item->readStatus = (0 == atoi(tmp))?FALSE:TRUE;
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"updateStatus"))
			item->updateStatus = (0 == atoi(tmp))?FALSE:TRUE;

		else if(!xmlStrcmp(cur->name, BAD_CAST"mark")) 
			item->flagStatus = (1 == atoi(tmp))?TRUE:FALSE;
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"time"))
			item->time = atol(tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"attributes"))
			item->metadata = metadata_parse_xml_nodes(cur);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"monitorComments"))
			item->monitorComments = TRUE;
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"comments"))
			item_parse_comments(cur, item);
		
		g_free(tmp);	
		tmp = NULL;
		cur = cur->next;
	}
	
	item->hasEnclosure = (NULL != metadata_list_get(item->metadata, "enclosure"));
	
	if(migrateCache && item->description)
		item_set_description(item, common_text_to_xhtml(item->description));
		
	if(item->validGuid)
		item_guid_list_add_id(item);

	return item;
}

void item_to_xml(itemPtr item, xmlNodePtr feedNode, gboolean rendering) {
	xmlNodePtr	itemNode;
	gchar		*tmp;
	
	itemNode = xmlNewChild(feedNode, NULL, "item", NULL);
	g_return_if_fail(itemNode);

	if(NULL == item_get_title(item))
		item_set_title(item, "");
	xmlNewTextChild(itemNode, NULL, "title", item_get_title(item));

	if(item_get_description(item)) {
		if(rendering) {
			tmp = common_strip_dhtml(item_get_description(item));
			xmlNewTextChild(itemNode, NULL, "description", tmp);
			g_free(tmp);
		} else {
			xmlNewTextChild(itemNode, NULL, "description", item_get_description(item));
		}
	}
	
	if(item_get_source(item))
		xmlNewTextChild(itemNode, NULL, "source", item_get_source(item));

	if(item_get_real_source_title(item))
		xmlNewTextChild(itemNode, NULL, "real_source_title", item_get_real_source_title(item));

	if(item_get_real_source_url(item))
		xmlNewTextChild(itemNode, NULL, "real_source_url", item_get_real_source_url(item));

	if(item_get_id(item))
		xmlNewTextChild(itemNode, NULL, "id", item_get_id(item));
		
	if(item->validGuid)
		xmlNewTextChild(itemNode, NULL, "validGuid", BAD_CAST "true");		

	tmp = g_strdup_printf("%ld", item->nr);
	xmlNewTextChild(itemNode, NULL, "nr", tmp);
	g_free(tmp);

	tmp = g_strdup_printf("%d", item->readStatus?1:0);
	xmlNewTextChild(itemNode, NULL, "readStatus", tmp);
	g_free(tmp);

	tmp = g_strdup_printf("%d", item->updateStatus?1:0);
	xmlNewTextChild(itemNode, NULL, "updateStatus", tmp);
	g_free(tmp);

	tmp = g_strdup_printf("%d", item->flagStatus?1:0);
	xmlNewTextChild(itemNode, NULL, "mark", tmp);
	g_free(tmp);

	tmp = g_strdup_printf("%ld", item->time);
	xmlNewTextChild(itemNode, NULL, "time", tmp);
	g_free(tmp);

	if(rendering) {
		tmp = itemview_format_date(item->time);
		xmlNewTextChild(itemNode, NULL, "timestr", tmp);
		g_free(tmp);
		
		xmlNewTextChild(itemNode, NULL, "sourceId", item->sourceNode->id);
		
		tmp = g_strdup_printf("%ld", item->sourceNr);
		xmlNewTextChild(itemNode, NULL, "sourceNr", tmp);
		g_free(tmp);
	}		

	metadata_add_xml_nodes(item->metadata, itemNode);
	
	if(item->monitorComments)
		xmlNewTextChild(itemNode, NULL, "monitorComments", BAD_CAST "true");
		
	if(item->comments) {
		GList		*iter = item->comments->items;
		xmlNodePtr	commentsNode = xmlNewChild(itemNode, NULL, "comments", NULL);

		update_state_export(commentsNode, item->updateState);

 		while(iter) {
			item_to_xml((itemPtr)iter->data, commentsNode, FALSE);
			iter = g_list_next(iter);
		}
		
		if(rendering) {
			xmlNewTextChild(commentsNode, NULL, "commentState", 
			                (item->updateRequest)?"updating":"ok");
		}
	}
}
