/**
 * @file json.h	 simplification wrappers for libjson-glib
 * 
 * Copyright (C) 2010  Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _JSON_H
#define _JSON_H

#include <glib-object.h>
#include <json-glib/json-glib.h>

/**
 * Query a simple json object node for a given subnode.
 *
 * @param node   the Json node to check
 * @param keyName   the name of the subnode
 *
 * @returns subnode (or NULL)
 */
JsonNode *json_get_node (JsonNode *node, const gchar *keyName);

/**
 * Query a simple json object node for a given string key.
 *
 * @param obj   the Json node to check
 * @param key   the key to look up
 *
 * @returns value of the key (or NULL)
 */
const gchar * json_get_string (JsonNode *node, const gchar *key);

/**
 * Query a simple json object node for a given numeric key.
 *
 * @param obj   the Json node to check
 * @param key   the key to look up
 *
 * @returns value of the key (no error handling!)
 */
gint64 json_get_int (JsonNode *node, const gchar *key);

/**
 * Query a simple json object node for a given boolean key.
 *
 * @param obj   the Json node to check
 * @param key   the key to look up
 *
 * @returns value of the key (no error handling!)
 */
gboolean json_get_bool (JsonNode *node, const gchar *key);

#endif
