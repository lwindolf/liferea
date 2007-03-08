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

#include "db.h"
#include "debug.h"
#include "common.h"
#include "item.h"
#include "itemview.h"
#include "metadata.h"
#include "support.h"

/* Item duplicate handling */

GSList * item_guid_list_get_duplicates_for_id(itemPtr item) {
	// FIXME!
	return NULL;
}

/* Item comments handling */

static void item_comments_process_update_result(struct request *request) {
	feedParserCtxtPtr	ctxt;
	itemPtr			item = (itemPtr)request->user_data;

	debug_enter("item_comments_process_update_result");
	
	/* How to check this? -> g_assert(0 < item->sourceNode->loaded); */

	/* note this is to update the feed URL on permanent redirects */
	if(!strcmp(request->source, metadata_list_get(item->metadata, "commentFeedUri"))) {
	
		debug2(DEBUG_UPDATE, "updating comment feed URL from \"%s\" to \"%s\"", 
		                     metadata_list_get(item->metadata, "commentFeedUri"), 
				     request->source);
				     
		metadata_list_set(&(item->metadata), "commentFeedUri", request->source);
	}
	
	if(401 == request->httpstatus) { /* unauthorized */
		item->commentsError = g_strdup("");
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
		feed_parse(ctxt);

		if(ctxt->failed) {
			debug0(DEBUG_UPDATE, "parsing comment feed failed!");
		} else {
			itemSetPtr comments;
			
			debug1(DEBUG_UPDATE, "parsing comment feed successful (%d comments downloaded)", g_list_length(ctxt->items));
			
			if(!item->commentFeedId)
				item->commentFeedId = node_new_id();
			
			comments = db_itemset_load(item->commentFeedId);
			itemset_merge_items(comments, ctxt->items);
			itemset_free(comments);
		}
				
		feed_free_parser_ctxt(ctxt);
		node_free(ctxt->node);
	}
	
	/* update error message */
	g_free(item->commentsError);
	item->commentsError = NULL;
	
	if(!(request->httpstatus >= 200) && (request->httpstatus < 400)) {
		const gchar * tmp;
		
		/* first specific codes (guarantees tmp to be set) */
		tmp = common_http_error_to_str(request->httpstatus);

		/* second netio errors */
		if(common_netio_error_to_str(request->returncode))
			tmp = common_netio_error_to_str(request->returncode);
			
		item->commentsError = g_strdup(tmp);
	}	

	/* clean up request */
	g_free(request->options);
	item->updateRequest = NULL; 

	/* rerender item */
	itemview_update_item(item); 
	itemview_update();
	
	db_item_update(item);
	
	debug_exit("item_comments_process_update_result");
}

void item_comments_refresh(itemPtr item) { 
	struct request	*request;
	const gchar	*url;
	
	url = metadata_list_get(item->metadata, "commentFeedUri");
	
	if(url) {
		debug2(DEBUG_UPDATE, "Updating comments for item \"%s\" (comment URL: %s)", item->title, url);

		request = update_request_new(item);
		request->user_data = item;
		request->options = g_new0(struct updateOptions, 1);
		request->callback = item_comments_process_update_result;
		request->source = g_strdup(url);
		request->priority = 1;
		item->updateRequest = request;
		update_execute_request(request);

		itemview_update_item(item); 
		itemview_update();
	}
}

/* Item handling */

itemPtr item_new(void) {
	itemPtr		item;
	
	item = g_new0(struct item, 1);
	
	item->updateState = update_state_new();
	
	item->newStatus = TRUE;
	item->popupStatus = TRUE;
	
	return item;
}

itemPtr item_load(gulong id) {

	return db_item_load(id);
}

itemPtr item_copy(itemPtr item) {
	itemPtr copy = item_new();

	item_set_title(copy, item->title);
	item_set_source(copy, item->source);
	item_set_real_source_url(copy, item->real_source_url);
	item_set_real_source_title(copy, item->real_source_title);
	item_set_description(copy, item->description);
	item_set_id(copy, item->sourceId);
	
	copy->updateStatus = item->updateStatus;
	copy->readStatus = item->readStatus;
	copy->newStatus = FALSE;
	copy->popupStatus = FALSE;
	copy->flagStatus = item->flagStatus;
	copy->time = item->time;
	copy->validGuid = item->validGuid;
	
	/* the following line allows state propagation in item.c */
	copy->nodeId = NULL;
	copy->sourceNr = item->id;

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
	g_free(item->sourceId);
	item->sourceId = g_strdup(id);
}

const gchar *	item_get_id(itemPtr item) { return item->sourceId; }
const gchar *	item_get_title(itemPtr item) {return item->title; }
const gchar *	item_get_description(itemPtr item) { return item->description; }
const gchar *	item_get_source(itemPtr item) { return item->source; }
const gchar *	item_get_real_source_url(itemPtr item) { return item->real_source_url; }
const gchar *	item_get_real_source_title(itemPtr item) { return item->real_source_title; }

void item_unload(itemPtr item) {

	g_free(item->title);
	g_free(item->source);
	g_free(item->sourceId);
	g_free(item->real_source_url);
	g_free(item->real_source_title);
	g_free(item->description);
	g_assert(NULL == item->tmpdata);	/* should be free after rendering */
	metadata_list_free(item->metadata);
	
	if(item->updateRequest)
		update_cancel_requests((gpointer)item);
	
	if(item->updateState)
		update_state_free(item->updateState);
		
	g_free(item->commentFeedId);
	g_free(item->commentsError);
	g_free(item);
}

const gchar * item_get_base_url(itemPtr item) {

	/* item->node is always the source node for the item 
	   never a search folder or folder */
	return node_get_base_url(node_from_id(item->nodeId));
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
		
		g_free(tmp);	
		tmp = NULL;
		cur = cur->next;
	}
	
	item->hasEnclosure = (NULL != metadata_list_get(item->metadata, "enclosure"));
	
	if(migrateCache && item->description)
		item_set_description(item, common_text_to_xhtml(item->description));

	return item;
}

void item_to_xml(itemPtr item, xmlNodePtr parentNode) {
	xmlNodePtr	duplicatesNode;		
	GSList		*duplicates;
	xmlNodePtr	itemNode;
	gchar		*tmp;
	
	itemNode = xmlNewChild(parentNode, NULL, "item", NULL);
	g_return_if_fail(itemNode);

	xmlNewTextChild(itemNode, NULL, "title", item_get_title(item)?item_get_title(item):"");

	if(item_get_description(item)) {
		tmp = common_strip_dhtml(item_get_description(item));
		xmlNewTextChild(itemNode, NULL, "description", tmp);
		g_free(tmp);
	}
	
	if(item_get_source(item))
		xmlNewTextChild(itemNode, NULL, "source", item_get_source(item));

	if(item_get_real_source_title(item))
		xmlNewTextChild(itemNode, NULL, "real_source_title", item_get_real_source_title(item));

	if(item_get_real_source_url(item))
		xmlNewTextChild(itemNode, NULL, "real_source_url", item_get_real_source_url(item));

	tmp = g_strdup_printf("%ld", item->id);
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

	duplicatesNode = xmlNewChild(itemNode, NULL, "duplicates", NULL);
	duplicates = item_guid_list_get_duplicates_for_id(item);
	while(duplicates) {
		nodePtr duplicateNode = (nodePtr)duplicates->data;
		if(!strcmp(duplicateNode->id, item->nodeId)) {
			xmlNewTextChild(duplicatesNode, NULL, "duplicateNode", 
			                node_get_title(duplicateNode));
		}
		duplicates = g_slist_next(duplicates);
	}
		
	xmlNewTextChild(itemNode, NULL, "sourceId", item->nodeId);
		
	tmp = g_strdup_printf("%ld", item->id);
	xmlNewTextChild(itemNode, NULL, "sourceNr", tmp);
	g_free(tmp);

	metadata_add_xml_nodes(item->metadata, itemNode);
		
	if(item->commentFeedId) {
		itemSetPtr	comments = db_itemset_load(item->commentFeedId);
		xmlNodePtr	commentsNode = xmlNewChild(itemNode, NULL, "comments", NULL);
		GList		*iter = comments->ids;

// FIXME: move update states to DB
//		update_state_export(commentsNode, item->updateState);

 		while(iter) {
			itemPtr comment = item_load(GPOINTER_TO_UINT(iter->data));
			item_to_xml(comment, commentsNode);
			item_unload(comment);
			iter = g_list_next(iter);
		}
		
		xmlNewTextChild(commentsNode, NULL, "updateState", 
		                (item->updateRequest)?"updating":"ok");
		
		if(item->commentsError)
			xmlNewTextChild(commentsNode, NULL, "updateError", item->commentsError);
			
		itemset_free(comments);
	}
}
