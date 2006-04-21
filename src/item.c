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

#include "vfolder.h"
#include "item.h"
#include "support.h"
#include "common.h"
#include "ui/ui_htmlview.h"
#include "callbacks.h"
#include "ui/ui_tray.h"
#include "metadata.h"
#include "debug.h"

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
	if(NULL != source) 
		ip->source = g_strchomp(g_strdup(source));
	else
		ip->source = NULL;
}

void item_set_real_source_url(itemPtr ip, const gchar * source) { 

	g_free(ip->real_source_url);
	if(NULL != source)
		ip->real_source_url = g_strchomp(g_strdup(source));
	else
		ip->real_source_url = NULL;
}

void item_set_real_source_title(itemPtr ip, const gchar * source) { 

	g_free(ip->real_source_title);
	if(NULL != source)
		ip->real_source_title = g_strchomp(g_strdup(source));
	else
		ip->real_source_title = NULL;
}

void item_set_time(itemPtr ip, const time_t t) { ip->time = t; }

void item_set_id(itemPtr ip, const gchar * id) {
	g_free(ip->id);
	ip->id = g_strdup(id);
}

const gchar *	item_get_id(itemPtr ip) { return (ip != NULL ? ip->id : NULL); }
const gchar *	item_get_title(itemPtr ip) {return (ip != NULL ? ip->title : NULL); }
const gchar *	item_get_description(itemPtr ip) { return (ip != NULL ? ip->description : NULL); }
const gchar *	item_get_source(itemPtr ip) { return (ip != NULL ? ip->source : NULL); }
const gchar *	item_get_real_source_url(itemPtr ip) { return (ip != NULL ? ip->real_source_url : NULL); }
const gchar *	item_get_real_source_title(itemPtr ip) { return (ip != NULL ? ip->real_source_title : NULL); }
time_t	item_get_time(itemPtr ip) { return (ip != NULL ? ip->time : 0); }

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

	if(item->sourceNode && (FST_FEED == item->sourceNode->type))
		return feed_get_html_url((feedPtr)item->sourceNode->data);
	else
		return itemset_get_base_url(item->itemSet);
}

gchar *item_render(itemPtr ip) {
	struct displayset	displayset;
	gchar			*escapedSrc;
	gchar			*buffer = NULL;
	const gchar		*htmlurl = NULL;
	gchar			*tmp, *tmp2;
	xmlChar			*tmp3;
	nodePtr			np;
	
	displayset.headtable = NULL;
	displayset.head = NULL;
	displayset.body = g_strdup(item_get_description(ip));
	displayset.foot = NULL;
	displayset.foottable = NULL;
	
	metadata_list_render(ip->metadata, &displayset);	
	
	escapedSrc = g_markup_escape_text(item_get_source(ip), -1);

	/* Head table */
	addToHTMLBufferFast(&buffer, HEAD_START);

	/* the following ensure to use the real parent mode */
	if(NULL == ip->sourceNode)
		np = ip->itemSet->node;
	else
		np = ip->sourceNode;
	
	if(FST_FEED == np->type)
		htmlurl = feed_get_html_url((feedPtr)np->data);

	/*  -- Feed line */
	if(htmlurl)
		tmp = g_markup_printf_escaped("<span class=\"feedtitle\"><a href=\"%s\">%s</a></span>",
		                              htmlurl, node_get_title(np));
	else
		tmp = g_markup_printf_escaped("<span class=\"feedtitle\">%s</span>",
		                              node_get_title(np));

	tmp2 = g_strdup_printf(HEAD_LINE, _("Feed:"), tmp);
	g_free(tmp);
	addToHTMLBufferFast(&buffer, tmp2);
	g_free(tmp2);

	/*  -- Item line */
	if(np->icon) {
		tmp2 = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "favicons", np->id, "png");
		tmp = g_markup_printf_escaped("<a class=\"favicon\" href=\"%s\"><img src=\"file://%s\" alt=\"\" /></a>", htmlurl, tmp2);
		g_free(tmp2);
	} else {
		tmp2 = g_strdup(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S "available.png");
		tmp = g_markup_printf_escaped("<a class=\"favicon\" href=\"%s\"><img src=\"file://%s\" alt=\"\" /></a>", htmlurl, tmp2);
		g_free(tmp2);
	}
	tmp3 = g_markup_escape_text(item_get_title(ip)?item_get_title(ip):_("[No title]"), -1);
	if(item_get_source(ip))
		tmp2 = g_strdup_printf("<span class=\"itemtitle\">%s<a href=\"%s\">%s</a></span>",
		                       tmp, escapedSrc, tmp3);
	else
		tmp2 = g_strdup_printf("<span class=\"itemtitle\">%s%s</span>", tmp, tmp3);
	g_free(tmp);
	g_free(tmp3);
	
	tmp = g_strdup_printf(HEAD_LINE, _("Item:"), tmp2);
	g_free(tmp2);
	addToHTMLBufferFast(&buffer, tmp);
	g_free(tmp);

	/*  -- real source line */
	tmp = NULL;
	if(item_get_real_source_url(ip))
		tmp = g_markup_printf_escaped("<span class=\"itemsource\"><a href=\"%s\">%s</a></span>",
			              item_get_real_source_url(ip),
			              item_get_real_source_title(ip)? item_get_real_source_title(ip) : _("[No title]"));
	else if(item_get_real_source_title(ip) != NULL)
		tmp = g_markup_printf_escaped("<span class=\"itemsource\">%s</span>",
			              item_get_real_source_title(ip));

	if(tmp) {
		tmp2 = g_strdup_printf(HEAD_LINE, _("Source:"), tmp);
		g_free(tmp);
		addToHTMLBufferFast(&buffer, tmp2);
		g_free(tmp2);	
	}

	addToHTMLBufferFast(&buffer, displayset.headtable);
	g_free(displayset.headtable);
	addToHTMLBufferFast(&buffer, HEAD_END);

	/* Head */
	if(displayset.head) {
		addToHTMLBufferFast(&buffer, displayset.head);
		g_free(displayset.head);
	}

	if(displayset.body) {
		addToHTMLBufferFast(&buffer, "<div class='content'>");
		addToHTMLBufferFast(&buffer, displayset.body);
		addToHTMLBufferFast(&buffer, "</div>");
		g_free(displayset.body);
	}

	if(displayset.foot) {
		addToHTMLBufferFast(&buffer, displayset.foot);
		g_free(displayset.foot);
	}

	/* add technorati link */
	tmp3 = common_uri_escape(escapedSrc);
	tmp2 = g_strdup("file://" PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S "technorati.png");
	tmp = g_strdup_printf(TECHNORATI_LINK, tmp3, tmp2);
	addToHTMLBufferFast(&buffer, tmp);
	xmlFree(tmp3);
	g_free(tmp2);
	g_free(tmp);

	addToHTMLBufferFast(&buffer, FEED_FOOT_TABLE_START);
		
	/* add the date before the other meta-data */
	addToHTMLBufferFast(&buffer, FEED_FOOT_FIRSTTD);
	addToHTMLBufferFast(&buffer, _("date"));
	addToHTMLBufferFast(&buffer, FEED_FOOT_NEXTTD);
	tmp = ui_itemlist_format_date(ip->time);
	addToHTMLBufferFast(&buffer, tmp);
	g_free(tmp);
	addToHTMLBufferFast(&buffer, FEED_FOOT_LASTTD);
		
	addToHTMLBufferFast(&buffer, displayset.foottable);
	addToHTMLBufferFast(&buffer, FEED_FOOT_TABLE_END);
	g_free(displayset.foottable);

	g_free(escapedSrc);

	return buffer;
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
		   NULL == (tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1)))) {
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
			item->nr = atol(tmp);

		else if(!xmlStrcmp(cur->name, BAD_CAST"newStatus"))
			item->newStatus = (0 == atoi(tmp))?FALSE:TRUE;
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"readStatus"))
			item->readStatus = (0 == atoi(tmp))?FALSE:TRUE;
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"updateStatus"))
			item->updateStatus = (0 == atoi(tmp))?FALSE:TRUE;

		else if(!xmlStrcmp(cur->name, BAD_CAST"mark")) 
			item->flagStatus = (1 == atoi(tmp))?TRUE:FALSE;
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"time"))
			item_set_time(item, atol(tmp));
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"attributes"))
			item->metadata = metadata_parse_xml_nodes(cur);
		
		g_free(tmp);	
		cur = cur->next;
	}
	
	if(migrateCache && item->description)
		item_set_description(item, common_text_to_xhtml(item->description));

	return item;
}

void item_save(itemPtr ip, xmlNodePtr feedNode) {
	xmlNodePtr	itemNode;
	gchar		*tmp;
	
	if(NULL != (itemNode = xmlNewChild(feedNode, NULL, "item", NULL))) {

		/* should never happen... */
		if(NULL == item_get_title(ip))
			item_set_title(ip, "");
		xmlNewTextChild(itemNode, NULL, "title", item_get_title(ip));

		if(NULL != item_get_description(ip))
			xmlNewTextChild(itemNode, NULL, "description", item_get_description(ip));

		if(NULL != item_get_source(ip))
			xmlNewTextChild(itemNode, NULL, "source", item_get_source(ip));

		if(NULL != item_get_real_source_title(ip))
			xmlNewTextChild(itemNode, NULL, "real_source_title", item_get_real_source_title(ip));

		if(NULL != item_get_real_source_url(ip))
			xmlNewTextChild(itemNode, NULL, "real_source_url", item_get_real_source_url(ip));

		if(NULL != item_get_id(ip))
			xmlNewTextChild(itemNode, NULL, "id", item_get_id(ip));

		tmp = g_strdup_printf("%ld", ip->nr);
		xmlNewTextChild(itemNode, NULL, "nr", tmp);
		g_free(tmp);

		tmp = g_strdup_printf("%d", (TRUE == ip->newStatus)?1:0);
		xmlNewTextChild(itemNode, NULL, "newStatus", tmp);
		g_free(tmp);

		tmp = g_strdup_printf("%d", (TRUE == ip->readStatus)?1:0);
		xmlNewTextChild(itemNode, NULL, "readStatus", tmp);
		g_free(tmp);
		
		tmp = g_strdup_printf("%d", (TRUE == ip->updateStatus)?1:0);
		xmlNewTextChild(itemNode, NULL, "updateStatus", tmp);
		g_free(tmp);

		tmp = g_strdup_printf("%d", (TRUE == ip->flagStatus)?1:0);
		xmlNewTextChild(itemNode, NULL, "mark", tmp);
		g_free(tmp);

		tmp = g_strdup_printf("%ld", item_get_time(ip));
		xmlNewTextChild(itemNode, NULL, "time", tmp);
		g_free(tmp);

		metadata_add_xml_nodes(ip->metadata, itemNode);

	} else {
		g_warning("could not write XML item node!\n");
	}
}
