/**
 * @file item.c common item handling
 *
 * Copyright (C) 2003-2012 Lars Windolf <lars.windolf@gmx.de>
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
 
#include "item.h"

#include <glib.h>
#include <string.h>

#include "comments.h"
#include "common.h"
#include "date.h"
#include "db.h"
#include "debug.h"
#include "metadata.h"
#include "xml.h"

itemPtr
item_new (void)
{
	itemPtr		item;
	
	item = g_new0 (struct item, 1);
	item->popupStatus = TRUE;
	
	return item;
}

itemPtr
item_load (gulong id)
{
	return db_item_load (id);
}

itemPtr
item_copy (itemPtr item)
{
	itemPtr copy = item_new ();

	item_set_title (copy, item->title);
	item_set_source (copy, item->source);
	item_set_description (copy, item->description);
	item_set_id (copy, item->sourceId);
	
	copy->updateStatus = item->updateStatus;
	copy->readStatus = item->readStatus;
	copy->popupStatus = FALSE;
	copy->flagStatus = item->flagStatus;
	copy->time = item->time;
	copy->validGuid = item->validGuid;
	copy->hasEnclosure = item->hasEnclosure;
	
	/* the following line allows state propagation in item.c */
	copy->nodeId = NULL;
	copy->sourceNr = item->id;

	/* this copies metadata */
	copy->metadata = metadata_list_copy (item->metadata);
	
	/* no deep copy of comments necessary as they are automatically 
	   retrieved when reading the article */

	return copy;
}

void
item_set_title (itemPtr item, const gchar * title)
{
	g_free (item->title);

	if (title)
		item->title = g_strstrip (g_strdelimit (g_strdup (title), "\r\n", ' '));
	else
		item->title = g_strdup ("");
}

void
item_set_description (itemPtr item, const gchar *description)
{
	if (!description)
		return;

	if (item->description)
		if (!(strlen (description) > strlen (item->description)))
			return;

	g_free (item->description);
	item->description = g_strdup (description);
}

void
item_set_source (itemPtr item, const gchar * source)
{
	g_free (item->source);
	if (source) 
		item->source = g_strstrip (g_strdup (source));
	else
		item->source = NULL;
}

void
item_set_id (itemPtr item, const gchar * id)
{
	g_free (item->sourceId);
	item->sourceId = g_strdup (id);
}

const gchar *	item_get_id(itemPtr item) { return item->sourceId; }
const gchar *	item_get_title(itemPtr item) {return item->title; }
const gchar *	item_get_description(itemPtr item) { return item->description; }
const gchar *	item_get_source(itemPtr item) { return item->source; }

gchar *
item_make_link (itemPtr item)
{
	const gchar	*src;
	gchar		*link;

	src = item_get_source (item);
	if (!src)
		return NULL;

	/* check for relative link */
	if (*src == '/') {
		const gchar * base = item_get_base_url (item);
		gchar * pos = (gchar *)base;
		int host_url_size, i;

		/* Check for schema-less (protocol-relative) link */
		if (*(src+1) == '/') {
			/* Find first /, end of protocol part in base url */
			pos = strstr (pos, "/");
		} else {
			/* Find the third /, start of link on
			* site. */
			for (i = 0; pos && i < 3; i++) {
				pos = strstr(pos + 1, "/");
			}
		}

		if (!pos) {
			debug0 (DEBUG_PARSING, "Feed contains relative link and invalid base URL");
			return NULL;
		}

		host_url_size = pos - base + 1;

		link = g_malloc (host_url_size + strlen(src));
		strncpy (link, base, host_url_size - 1);
		pos = link + host_url_size - 1;
		strcpy (pos, src);
	} else {
		link = g_strdup (src);
	}
	
	return link;
}

void
item_unload (itemPtr item) 
{
	g_free (item->title);
	g_free (item->source);
	g_free (item->sourceId);
	g_free (item->description);
	g_free (item->commentFeedId);
	g_free (item->nodeId);
	g_free (item->parentNodeId);
	
	g_assert (NULL == item->tmpdata);	/* should be free after rendering */
	metadata_list_free (item->metadata);

	g_free (item);
}

const gchar *
item_get_base_url (itemPtr item)
{
	/* item->node is always the source node for the item 
	   never a search folder or folder */
	return node_get_base_url (node_from_id (item->nodeId));
}

void
item_to_xml (itemPtr item, gpointer xmlNode)
{
	xmlNodePtr	parentNode = (xmlNodePtr)xmlNode;
	xmlNodePtr	duplicatesNode;		
	xmlNodePtr	itemNode;
	gchar		*tmp;
	gchar		*tmp2;
	
	itemNode = xmlNewChild (parentNode, NULL, "item", NULL);
	g_return_if_fail (itemNode);

	xmlNewTextChild (itemNode, NULL, "title", item_get_title (item)?item_get_title (item):"");

	if (item_get_description (item)) {
		tmp = xhtml_strip_dhtml (item_get_description (item));
		tmp2 = xhtml_strip_unsupported_tags (tmp);
		xmlNewTextChild (itemNode, NULL, "description", tmp2);
		g_free (tmp);
		g_free (tmp2);
	}
	
	if (item_get_source (item))
		xmlNewTextChild (itemNode, NULL, "source", item_get_source (item));

	tmp = g_strdup_printf ("%ld", item->id);
	xmlNewTextChild (itemNode, NULL, "nr", tmp);
	g_free (tmp);

	tmp = g_strdup_printf ("%d", item->readStatus?1:0);
	xmlNewTextChild (itemNode, NULL, "readStatus", tmp);
	g_free (tmp);

	tmp = g_strdup_printf ("%d", item->updateStatus?1:0);
	xmlNewTextChild (itemNode, NULL, "updateStatus", tmp);
	g_free (tmp);

	tmp = g_strdup_printf ("%d", item->flagStatus?1:0);
	xmlNewTextChild (itemNode, NULL, "mark", tmp);
	g_free (tmp);

	tmp = g_strdup_printf ("%ld", item->time);
	xmlNewTextChild (itemNode, NULL, "time", tmp);
	g_free (tmp);

	tmp = date_format (item->time, NULL);
	xmlNewTextChild (itemNode, NULL, "timestr", tmp);
	g_free (tmp);

	if (item->validGuid) {
		GSList	*iter, *duplicates;
		
		duplicatesNode = xmlNewChild(itemNode, NULL, "duplicates", NULL);
		duplicates = iter = db_item_get_duplicates(item->sourceId);
		while (iter) {
			gulong id = GPOINTER_TO_UINT (iter->data);
			itemPtr duplicate = item_load (id);
			if (duplicate) {
				nodePtr duplicateNode = node_from_id (duplicate->nodeId);
				if (duplicateNode && (item->id != duplicate->id))
					xmlNewTextChild (duplicatesNode, NULL, "duplicateNode", 
					                 node_get_title (duplicateNode));
				item_unload (duplicate);
			}
			iter = g_slist_next (iter);
		}
		g_slist_free (duplicates);
	}
		
	xmlNewTextChild (itemNode, NULL, "sourceId", item->nodeId);
		
	tmp = g_strdup_printf ("%ld", item->id);
	xmlNewTextChild (itemNode, NULL, "sourceNr", tmp);
	g_free (tmp);

	metadata_add_xml_nodes (item->metadata, itemNode);

	nodePtr feedNode = node_from_id (item->parentNodeId);
	if (feedNode) {
		feedPtr feed = (feedPtr)feedNode->data;
		if (feed) {
			if (!feed->ignoreComments) {
				if (item->commentFeedId)
					comments_to_xml (itemNode, item->commentFeedId);
			} else {
				xmlNewTextChild (itemNode, NULL, "commentsSuppressed", "true");
			}
		}
	}
}
