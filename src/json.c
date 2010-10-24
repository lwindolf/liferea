/**
 * @file json.h	 simplification wrappers for libjson-glib
 * 
 * Copyright (C) 2010  Lars Lindner <lars.lindner@gmail.com>
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

const gchar *
json_get_string (JsonNode *node, const gchar *keyName)
{
	JsonObject *obj;
	JsonNode *key;
	
	obj = json_node_get_object(node);
	if (!obj)
		return NULL;
		
	key = json_object_get_member (obj, keyName);
	if (!key)
		return NULL;
	
	return json_node_get_string (key);
	
}
