/**
 * @file item.c common item handling
 *
 * Copyright (C) 2003-2006 Lars Lindner <lars.lindner@gmx.net>
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

/* function to create a new feed structure */
itemPtr item_new(void) {
	itemPtr		ip;
	
	ip = g_new0(struct item, 1);
	ip->newStatus = TRUE;
	ip->popupStatus = TRUE;
	
	return ip;
}

itemPtr item_copy(itemPtr ip) {
	itemPtr copy = item_new();

	item_set_title(copy, ip->title);
	item_set_source(copy, ip->source);
	item_set_real_source_url(copy, ip->real_source_url);
	item_set_real_source_title(copy, ip->real_source_title);
	item_set_description(copy, ip->description);
	item_set_id(copy, ip->id);
	
	copy->updateStatus = ip->updateStatus;
	copy->readStatus = ip->readStatus;
	copy->newStatus = FALSE;
	copy->popupStatus = FALSE;
	copy->flagStatus = ip->flagStatus;
	copy->time = ip->time;
	
	/* the following line allows state propagation in item.c */
	copy->sourceNode = ip->itemSet->node;
	copy->sourceNr = ip->nr;

	/* this copies metadata */
	copy->metadata = metadata_list_copy(ip->metadata);

	return copy;
}

void item_set_title(itemPtr ip, const gchar * title) {

	g_free(ip->title);
	ip->title = g_strdup(title);
}

void item_set_description(itemPtr ip, const gchar * description) {

	g_free(ip->description);
	ip->description = g_strdup(description);
}

void item_set_source(itemPtr ip, const gchar * source) { 

	g_free(ip->source);
	if(source) 
		ip->source = g_strchomp(g_strdup(source));
	else
		ip->source = NULL;
}

void item_set_real_source_url(itemPtr ip, const gchar * source) { 

	g_free(ip->real_source_url);
	if(source)
		ip->real_source_url = g_strchomp(g_strdup(source));
	else
		ip->real_source_url = NULL;
}

void item_set_real_source_title(itemPtr ip, const gchar * source) { 

	g_free(ip->real_source_title);
	if(source)
		ip->real_source_title = g_strchomp(g_strdup(source));
	else
		ip->real_source_title = NULL;
}

void item_set_id(itemPtr ip, const gchar * id) {
	g_free(ip->id);
	ip->id = g_strdup(id);
}

const gchar *	item_get_id(itemPtr ip) { return ip->id; }
const gchar *	item_get_title(itemPtr ip) {return ip->title; }
const gchar *	item_get_description(itemPtr ip) { return ip->description; }
const gchar *	item_get_source(itemPtr ip) { return ip->source; }
const gchar *	item_get_real_source_url(itemPtr ip) { return ip->real_source_url; }
const gchar *	item_get_real_source_title(itemPtr ip) { return ip->real_source_title; }

void item_free(itemPtr ip) {

	g_free(ip->title);
	g_free(ip->source);
	g_free(ip->real_source_url);
	g_free(ip->real_source_title);
	g_free(ip->description);
	g_free(ip->id);
	g_assert(NULL == ip->tmpdata);	/* should be free after rendering */
	metadata_list_free(ip->metadata);

	g_free(ip);
}

const gchar * item_get_base_url(itemPtr item) {

	if(item->sourceNode && (NODE_TYPE_FEED == item->sourceNode->type))
		return feed_get_html_url((feedPtr)item->sourceNode->data);
	else
		return itemset_get_base_url(item->itemSet);
}

gchar *item_render(itemPtr item) {
	gchar		**params = NULL, *output = NULL;
	xmlDocPtr	doc;

	doc = feed_to_xml(item->sourceNode, NULL, TRUE);
	item_to_xml(item, xmlDocGetRootElement(doc), TRUE);
	
	params = render_add_parameter(params, "pixmapsDir='file://" PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S "'");
	output = render_xml(doc, "item", params);
	xmlFree(doc);

	return output;
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
		   !(tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1)))) {
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
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"nr"))
			item->nr = item->sourceNr = atol(tmp);

		else if(!xmlStrcmp(cur->name, BAD_CAST"newStatus"))
			item->newStatus = (0 == atoi(tmp))?FALSE:TRUE;
			
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
		cur = cur->next;
	}
	
	if(migrateCache && item->description)
		item_set_description(item, common_text_to_xhtml(item->description));

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

	tmp = g_strdup_printf("%ld", item->nr);
	xmlNewTextChild(itemNode, NULL, "nr", tmp);
	g_free(tmp);

	tmp = g_strdup_printf("%d", item->newStatus?1:0);
	xmlNewTextChild(itemNode, NULL, "newStatus", tmp);
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
		/* @translators: localize this format string to change the 
		   date format in HTML output */
		tmp = common_format_date(item->time, _("%b %d %H:%M"));
		xmlNewTextChild(itemNode, NULL, "timestr", tmp);
		g_free(tmp);
		
		xmlNewTextChild(itemNode, NULL, "sourceId", item->sourceNode->id);
	}		

	metadata_add_xml_nodes(item->metadata, itemNode);
}
