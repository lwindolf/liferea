/**
 * @file item.c item handling
 *
 * Copyright (C) 2003-2023 Lars Windolf <lars.windolf@gmx.de>
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
#include "feedlist.h"
#include "metadata.h"
#include "render.h"
#include "xml.h"

G_DEFINE_TYPE (LifereaItem, liferea_item, G_TYPE_OBJECT);

static GObjectClass *parent_class = NULL;

static void
liferea_item_finalize (GObject *object)
{
	LifereaItem *item = LIFEREA_ITEM (object);

	g_free (item->title);
	g_free (item->source);
	g_free (item->sourceId);
	g_free (item->description);
	g_free (item->commentFeedId);
	g_free (item->nodeId);
	g_free (item->parentNodeId);

	g_assert (NULL == item->tmpdata);	/* should be free after rendering */
	metadata_list_free (item->metadata);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
liferea_item_class_init (LifereaItemClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = liferea_item_finalize;
}

static void
liferea_item_init (LifereaItem *item)
{
	item->popupStatus = TRUE;
}

LifereaItem *
item_new (void)
{
	return LIFEREA_ITEM (g_object_new (LIFEREA_ITEM_TYPE, NULL));
}

LifereaItem *
item_load (gulong id)
{
	return db_item_load (id);
}

LifereaItem *
item_copy (LifereaItem *item)
{
	LifereaItem *copy = item_new ();

	item_set_title (copy, item->title);
	item_set_source (copy, item->source);
	item_set_description (copy, item->description);
	item_set_id (copy, item->sourceId);

	copy->updateStatus = item->updateStatus;
	copy->readStatus = item->readStatus;
	copy->popupStatus = FALSE;
	copy->flagStatus = item->flagStatus;
	copy->time = item->time;
	copy->validTime = item->validTime;
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
item_set_title (LifereaItem *item, const gchar * title)
{
	g_free (item->title);

	if (title)
		item->title = g_strstrip (g_strdelimit (g_strdup (title), "\r\n", ' '));
	else
		item->title = g_strdup ("");
}

void
item_set_description (LifereaItem *item, const gchar *description)
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
item_set_source (LifereaItem *item, const gchar * source)
{
	g_free (item->source);

	/* We expect only relative URIs starting with '/' or absolute URIs starting with 'http://' or 'https://' */
	if (source && ('/' == source[0] || 'h' == source[0]))
		item->source = g_strstrip (g_strdup (source));
	else
		item->source = NULL;
}

void
item_set_id (LifereaItem *item, const gchar * id)
{
	g_free (item->sourceId);
	item->sourceId = g_strdup (id);
}

void
item_set_time (LifereaItem *item, gint64 time)
{
	item->time = time;
	if (item->time > 0)
		item->validTime = TRUE;
}

const gchar *	item_get_id(LifereaItem *item) { return item->sourceId; }
const gchar *	item_get_title(LifereaItem *item) {return item->title; }
const gchar *	item_get_description(LifereaItem *item) { return item->description; }
const gchar *	item_get_source(LifereaItem *item) { return item->source; }

static GRegex *whitespace_strip_re = NULL;

gchar *
item_get_teaser (LifereaItem *item)
{
	gchar		*input, *tmpDesc;
	gchar		*teaser = NULL;

	if (!whitespace_strip_re) {
		whitespace_strip_re = g_regex_new ("(\n+|\\s\\s+)", G_REGEX_MULTILINE, 0, NULL);
		g_assert (NULL != whitespace_strip_re);
	}

	input = unxmlize (g_strdup (item->description));
	tmpDesc = g_regex_replace_literal (whitespace_strip_re, input, -1, 0, " ", 0, NULL);

	if (strlen (tmpDesc) > 200) {
		// Truncate hard at pos 200 and search backward for a space
		tmpDesc[200] = 0;
		gchar *last_space = g_strrstr (tmpDesc, " ");
		if (last_space) {
			*last_space = 0;
			teaser = tmpDesc;
		}
	}

	if (!teaser)
		return NULL;

	teaser = g_strstrip (g_markup_escape_text (teaser, -1));

	g_free (input);
	g_free (tmpDesc);

	return teaser;
}

gchar *
item_make_link (LifereaItem *item)
{
	const gchar	*src;
	gchar		*link;

	src = item_get_source (item);
	if (!src)
		return NULL;

	/* check for absolute URL */
	if (strstr (src, "://")) {
		link = g_strdup (src);
	} else {
		const gchar * base = item_get_base_url (item);

		link = (gchar *) common_build_url (src, base);
		if (!link) {
			debug0 (DEBUG_PARSING, "Feed contains relative link and invalid base URL");
			return NULL;
		}
	}

	return link;
}

const gchar *
item_get_author(LifereaItem *item)
{
	gchar *author;

	author = (gchar *)metadata_list_get (item->metadata, "author");
	return author;
}

const gchar *
item_get_base_url (LifereaItem *item)
{
	/* item->node is always the source node for the item
	   never a search folder or folder */
	return node_get_base_url (node_from_id (item->nodeId));
}

void
item_to_xml (LifereaItem *item, gpointer xmlNode)
{
	xmlNodePtr	parentNode = (xmlNodePtr)xmlNode;
	xmlNodePtr	duplicatesNode;
	xmlNodePtr	itemNode;
	gchar		*tmp;

	itemNode = xmlNewChild (parentNode, NULL, BAD_CAST "item", NULL);
	g_return_if_fail (itemNode);

	xmlNewTextChild (itemNode, NULL, BAD_CAST "title", BAD_CAST (item_get_title (item)?item_get_title (item):""));

	if (item_get_description (item)) {
		/* Prefer full article over feed inline content */
		const gchar *content = metadata_list_get (item->metadata, "richContent");
		if (NULL == content)
			content = item_get_description (item);

		tmp = xhtml_strip_dhtml (content);
		xmlNewTextChild (itemNode, NULL, BAD_CAST "description", BAD_CAST tmp);
		g_free (tmp);
	}

	if (item_get_source (item))
		xmlNewTextChild (itemNode, NULL, BAD_CAST "source", BAD_CAST item_get_source (item));

	tmp = g_strdup_printf ("%ld", item->id);
	xmlNewTextChild (itemNode, NULL, BAD_CAST "nr", BAD_CAST tmp);
	g_free (tmp);

	tmp = g_strdup_printf ("%d", item->readStatus?1:0);
	xmlNewTextChild (itemNode, NULL, BAD_CAST "readStatus", BAD_CAST tmp);
	g_free (tmp);

	tmp = g_strdup_printf ("%d", item->updateStatus?1:0);
	xmlNewTextChild (itemNode, NULL, BAD_CAST "updateStatus", BAD_CAST tmp);
	g_free (tmp);

	tmp = g_strdup_printf ("%d", item->flagStatus?1:0);
	xmlNewTextChild (itemNode, NULL, BAD_CAST "mark", BAD_CAST tmp);
	g_free (tmp);

	tmp = g_strdup_printf ("%ld", item->time);
	xmlNewTextChild (itemNode, NULL, BAD_CAST "time", BAD_CAST tmp);
	g_free (tmp);

	tmp = date_format (item->time, NULL);
	xmlNewTextChild (itemNode, NULL, BAD_CAST "timestr", BAD_CAST tmp);
	g_free (tmp);

	if (item->validGuid) {
		GSList	*iter, *duplicates;

		duplicatesNode = xmlNewChild(itemNode, NULL, BAD_CAST "duplicates", NULL);
		duplicates = iter = db_item_get_duplicates(item->sourceId);
		while (iter) {
			gulong id = GPOINTER_TO_UINT (iter->data);
			LifereaItem * duplicate = item_load (id);
			if (duplicate) {
				nodePtr duplicateNode = node_from_id (duplicate->nodeId);
				if (duplicateNode && (item->id != duplicate->id))
					xmlNewTextChild (duplicatesNode, NULL, BAD_CAST "duplicateNode", BAD_CAST node_get_title (duplicateNode));
				item_unload (duplicate);
			}
			iter = g_slist_next (iter);
		}
		g_slist_free (duplicates);
	}

	xmlNewTextChild (itemNode, NULL, BAD_CAST "sourceId", BAD_CAST item->nodeId);

	tmp = g_strdup_printf ("%ld", item->id);
	xmlNewTextChild (itemNode, NULL, BAD_CAST "sourceNr", BAD_CAST tmp);
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
				xmlNewTextChild (itemNode, NULL, BAD_CAST "commentsSuppressed", BAD_CAST "true");
			}
		}
	}
}

static const gchar *
item_get_text_direction (LifereaItem *item)
{
	if (item_get_title (item))
		return (common_get_text_direction (item_get_title (item)));
	if (item_get_description (item))
		return (common_get_text_direction (item_get_description (item)));

	/* what can we do? */
	return ("ltr");
}

gchar *
item_render (LifereaItem *item, guint viewMode)
{
	renderParamPtr	params;
	gchar		*output = NULL, *baseUrl = NULL;
	nodePtr		node;
	xmlDocPtr 	doc;
	xmlNodePtr 	xmlNode;

	debug_enter ("item_render");

	/* don't use node from htmlView_priv as this would be
	wrong for folders and other merged item sets */
	node = node_from_id (item->nodeId);

	/* do the XML serialization */
	doc = xmlNewDoc (BAD_CAST "1.0");
	xmlNode = xmlNewDocNode (doc, NULL, BAD_CAST "itemset", NULL);
	xmlDocSetRootElement (doc, xmlNode);

	item_to_xml(item, xmlDocGetRootElement (doc));

	if (IS_FEED (node)) {
		xmlNodePtr feed;
		feed = xmlNewChild (xmlDocGetRootElement (doc), NULL, BAD_CAST "feed", NULL);
		feed_to_xml (node, feed);
	}

	/* do the XSLT rendering */
	params = render_parameter_new ();

	if (NULL != node_get_base_url (node)) {
		baseUrl = (gchar *) common_uri_escape ( BAD_CAST node_get_base_url (node));
		render_parameter_add (params, "baseUrl='%s'", baseUrl);
	}
	render_parameter_add (params, "showFeedName='%d'", (node != feedlist_get_selected ())?1:0);
	render_parameter_add (params, "txtDirection='%s'", item_get_text_direction (item));
	render_parameter_add (params, "appDirection='%s'", common_get_app_direction ());
	output = render_xml (doc, "item", params);

	/* For debugging use: xmlSaveFormatFile("/tmp/test.xml", doc, 1); */
	xmlFreeDoc (doc);
	g_free (baseUrl);

	debug_exit ("item_render");

	return output;
}
