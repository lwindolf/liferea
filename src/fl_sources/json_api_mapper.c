/**
 * @file json_api_mapper.c   data mapper for JSON APIs
 * 
 * Copyright (C) 2013 Lars Windolf <lars.lindner@gmail.com>
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

#include "json_api_mapper.h"

#include <string.h>

#include "debug.h"
#include "item.h"
#include "metadata.h"
#include "xml.h"

JsonNode *
json_api_get_node (JsonNode *parent, const gchar *mapping)
{
	JsonNode	*node = parent;
	gchar		**step;
	gchar		**steps = g_strsplit (g_strdup (mapping), "/", 0);

	step = steps;
	if (!*step)
		return node;

	while (*(step + 1) && node) {
		node = json_get_node (node, *step);
		step++;
	}

	g_strfreev (steps);

	return node;
}

const gchar *
json_api_get_string (JsonNode *parent, const gchar *mapping)
{
	JsonNode	*node;
	const gchar	*field;

	if (!node || !mapping)
		return NULL;

	node = json_api_get_node (parent, mapping);
	field = strrchr (mapping, '/');
	if (!field)
		field = mapping;
	else
		field++;

	return json_get_string (node, field);
}

gint
json_api_get_int (JsonNode *parent, const gchar *mapping)
{
	JsonNode	*node;
	const gchar	*field;

	if (!node || !mapping)
		return 0;

	node = json_api_get_node (parent, mapping);
	field = strrchr (mapping, '/');
	if (!field)
		field = mapping;
	else
		field++;

	return json_get_int (node, field);
}

gboolean
json_api_get_bool (JsonNode *parent, const gchar *mapping)
{
	JsonNode	*node;
	const gchar	*field;

	if (!node || !mapping)
		return FALSE;

	node = json_api_get_node (parent, mapping);
	field = strrchr (mapping, '/');
	if (!field)
		field = mapping;
	else
		field++;

	return json_get_bool (node, field);
}

GList *
json_api_get_items (const gchar *json, const gchar *root, jsonApiMapping *mapping, jsonApiItemCallbackFunc callback)
{
	GList		*items = NULL;
	JsonParser	*parser = json_parser_new ();

	if (json_parser_load_from_data (parser, json, -1, NULL)) {
		JsonArray	*array = json_node_get_array (json_get_node (json_parser_get_root (parser), root));
		GList		*elements = json_array_get_elements (array);
		GList		*iter = elements;

		debug1 (DEBUG_PARSING, "JSON API: found items root node \"%s\"", root);
                
		while (iter) {
			JsonNode *node = (JsonNode *)iter->data;
			itemPtr item = item_new ();

			/* Parse default feeds */
			item_set_id	(item, json_api_get_string (node, mapping->id));
			item_set_title	(item, json_api_get_string (node, mapping->title));
			item_set_source	(item, json_api_get_string (node, mapping->link));

			item->time       = json_api_get_int (node, mapping->updated);
			item->readStatus = json_api_get_bool (node, mapping->read);
			item->flagStatus = json_api_get_bool (node, mapping->flag);

			if (mapping->negateRead)
				item->readStatus = !item->readStatus;

			/* Handling encoded content */
			const gchar *content; 
			gchar *xhtml;

			content = json_api_get_string (node, mapping->description);
			if (mapping->xhtml) {
				xhtml = xhtml_extract_from_string (content, NULL);
				item_set_description (item, xhtml);
				xmlFree (xhtml);
			} else {
				item_set_description (item, content);
			}

			/* Optional meta data */
			const gchar *tmp = json_api_get_string (node, mapping->author);
			if (tmp)
				item->metadata = metadata_list_append (item->metadata, "author", tmp);
	
			items = g_list_append (items, (gpointer)item);

			/* Allow optional item callback to process stuff */
			if (callback)
				(*callback)(node, item);
				
			iter = g_list_next (iter);
		}

		g_list_free (elements);
		g_object_unref (parser);
	} else {
		debug1 (DEBUG_PARSING, "Could not parse JSON \"%s\"", json);
	}

	return items;
}
