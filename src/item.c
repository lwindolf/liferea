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

#include "comments.h"
#include "db.h"
#include "debug.h"
#include "item.h"
#include "itemview.h"
#include "metadata.h"
#include "xml.h"

itemPtr item_new(void) {
	itemPtr		item;
	
	item = g_new0(struct item, 1);
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

void
item_unload (itemPtr item) 
{

	g_free (item->title);
	g_free (item->source);
	g_free (item->sourceId);
	g_free (item->real_source_url);
	g_free (item->real_source_title);
	g_free (item->description);
	g_free (item->commentFeedId);
	g_free (item->nodeId);
	
	g_assert (NULL == item->tmpdata);	/* should be free after rendering */
	metadata_list_free (item->metadata);

	g_free (item);
}

const gchar * item_get_base_url(itemPtr item) {

	/* item->node is always the source node for the item 
	   never a search folder or folder */
	return node_get_base_url(node_from_id(item->nodeId));
}

void item_to_xml(itemPtr item, xmlNodePtr parentNode) {
	xmlNodePtr	duplicatesNode;		
	xmlNodePtr	itemNode;
	gchar		*tmp;
	
	itemNode = xmlNewChild(parentNode, NULL, "item", NULL);
	g_return_if_fail(itemNode);

	xmlNewTextChild(itemNode, NULL, "title", item_get_title(item)?item_get_title(item):"");

	if (item_get_description (item)) {
		tmp = xhtml_strip_dhtml (item_get_description (item));
		xmlNewTextChild (itemNode, NULL, "description", tmp);
		g_free (tmp);
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

	tmp = itemview_format_date(item->time);
	xmlNewTextChild(itemNode, NULL, "timestr", tmp);
	g_free(tmp);

	if (item->validGuid) 
	{
		GSList	*iter, *duplicates;
		
		duplicatesNode = xmlNewChild(itemNode, NULL, "duplicates", NULL);
		duplicates = iter = db_item_get_duplicates(item->sourceId);
		while(iter) 
		{
			gulong id = GPOINTER_TO_UINT (iter->data);
			itemPtr duplicate = item_load (id);
			if (duplicate)
			{
				nodePtr duplicateNode = node_from_id (duplicate->nodeId);
				if (duplicateNode && (item->id != duplicate->id))
					xmlNewTextChild(duplicatesNode, NULL, "duplicateNode", 
					                node_get_title(duplicateNode));
				item_unload (duplicate);
			}
			iter = g_slist_next (iter);
		}
		g_slist_free (duplicates);
	}
		
	xmlNewTextChild(itemNode, NULL, "sourceId", item->nodeId);
		
	tmp = g_strdup_printf("%ld", item->id);
	xmlNewTextChild(itemNode, NULL, "sourceNr", tmp);
	g_free(tmp);

	metadata_add_xml_nodes(item->metadata, itemNode);
	
	if (item->commentFeedId)
		comments_to_xml (itemNode, item->commentFeedId);
}
