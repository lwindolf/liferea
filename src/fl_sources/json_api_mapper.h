/**
 * @file json_api_mapper.h   data mapper for JSON APIs
 * 
 * Copyright (C) 2013 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _JSON_API_MAPPER_H
#define _JSON_API_MAPPER_H

#include "item.h"
#include "json.h"

typedef struct jsonApiMapping {
	const gchar	*id;		/**< list of location steps to id field */
	const gchar	*title;		/**< list of location steps to title field */
	const gchar	*link;		/**< list of location steps to link field */
	const gchar	*description;	/**< list of location steps to description field */
	const gchar	*updated;	/**< list of location steps to updated field */
	const gchar	*author;	/**< list of location steps to author field */
	const gchar	*read;		/**< list of location steps to read field */
	const gchar	*flag;		/**< list of location steps to flagged/marked/starred field */
	gboolean	negateRead;	/**< TRUE if read status is to be negated (i.e. is "unread flag") */
	gboolean	xhtml;		/**< TRUE if description field is XHTML */
} jsonApiMapping;

typedef void (*jsonApiItemCallbackFunc)(JsonNode *node, itemPtr item);

/**
 * Extracts all items from a JSON document.
 *
 * @param json		the JSON document to parse
 * @param root		the name of the root node (e.g. "items")
 * @param mapping	hash table defining location steps and logic
 * @param callback	optional callback function to process item node
 *			for everything that cannot be easily mapped
 *
 * @returns a list of items (all to be freed with item_free()) or NULL
 */
GList * json_api_get_items (const gchar *json, const gchar *root, jsonApiMapping *mapping, jsonApiItemCallbackFunc callback);

#endif
