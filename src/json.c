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

#include "json.h"

JsonNode *
json_get_node (JsonNode *node, const gchar *keyName)
{
	JsonObject *obj;
	JsonNode *key;
	
	obj = json_node_get_object (node);
	if (!obj)
		return NULL;
		
	key = json_object_get_member (obj, keyName);
	if (!key)
		return NULL;
	
	return key;
}

const gchar *
json_get_string (JsonNode *node, const gchar *keyName)
{
	JsonObject *obj;
	JsonNode *key;
	
	obj = json_node_get_object (node);
	if (!obj)
		return NULL;
		
	key = json_object_get_member (obj, keyName);
	if (!key)
		return NULL;
	
	return json_node_get_string (key);
}

gint64
json_get_int (JsonNode *node, const gchar *keyName)
{
	JsonObject *obj;
	JsonNode *key;
	
	obj = json_node_get_object (node);
	if (!obj)
		return 0;
		
	key = json_object_get_member (obj, keyName);
	if (!key)
		return 0;
	
	return json_node_get_int (key);
}

gboolean
json_get_bool (JsonNode *node, const gchar *keyName)
{
	JsonObject *obj;
	JsonNode *key;
	
	obj = json_node_get_object (node);
	if (!obj)
		return FALSE;
		
	key = json_object_get_member (obj, keyName);
	if (!key)
		return FALSE;
	
	return json_node_get_boolean (key);
}
