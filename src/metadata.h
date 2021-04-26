/**
 * @file metadata.h  handling of typed item and feed metadata
 *
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2008-2020 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _METADATA_H
#define _METADATA_H

#include <glib.h>
#include <libxml/tree.h>

#include "feed_parser.h"

/* -------------------------------------------------------- */
/* interface definitions for namespace parsing handler      */
/* -------------------------------------------------------- */
struct NsHandler;

/** definition of various namespace tag handler */
typedef void	(*registerNsFunc) (struct NsHandler *nsh, GHashTable *prefixhash, GHashTable *urihash);
typedef void	(*parseChannelTagFunc)	(feedParserCtxtPtr ctxt, xmlNodePtr cur);
typedef void	(*parseItemTagFunc)	(feedParserCtxtPtr ctxt, xmlNodePtr cur);

/** struct used to register namespace handler */
typedef struct NsHandler {
	const gchar		*prefix;		/**< namespace prefix */
	registerNsFunc		registerNs;

	parseItemTagFunc	parseItemTag;		/**< item tag parsing method */
	parseChannelTagFunc	parseChannelTag;	/**< channel tag parsing method */
} NsHandler;

/** Metadata value types */
enum {
	METADATA_TYPE_TEXT = 1,	/**< metadata can be any character data and needs escaping */
	METADATA_TYPE_URL = 2,	/**< metadata is an URL and guaranteed to be valid for use in XML */
	METADATA_TYPE_HTML = 3,	/**< metadata is XHTML content and valid to be embedded in output with Readability.js */
	METADATA_TYPE_HTML5 = 4	/**< metadata is trusted HTML5 content and can be embedded in output with Readability.js */
};

/**
 * Register a metadata type. This allows type specific
 * sanity handling and detecting invalid metadata.
 *
 * @param name	key name
 * @param type	key type
 */
void metadata_type_register (const gchar *name, gint);

/**
 * Checks whether a metadata type is registered
 *
 * @param strid		the metadata type identifier
 *
 * @returns TRUE if the metadata type is registered, otherwise FALSE
 */
gboolean metadata_is_type_registered (const gchar *strid);

/**
 * Appends a value to the value list of a specific metadata type
 * Don't mix this function with metadata_list_set() !
 *
 * @param metadata	the metadata list
 * @param strid		the metadata type identifier
 * @param data		data to add
 *
 * @returns the changed meta data list
 */
GSList * metadata_list_append(GSList *metadata, const gchar *strid, const gchar *data);

/**
 * Sets (and overwrites if necessary) the value of a specific metadata type.
 * Don't mix this function with metadata_list_append() !
 *
 * @param metadata	the metadata list
 * @param strid		the metadata type identifier
 * @param data		data to add
 */
void metadata_list_set(GSList **metadata, const gchar *strid, const gchar *data);

/**
 * Returns the first value of a given type from a specified metadata list.
 * Do use this function only for single instance types.
 *
 * @param metadata	the metadata list
 * @param strid		the metadata type identifier
 *
 * @returns the first value (or NULL)
 */
const gchar * metadata_list_get(GSList *metadata, const gchar *strid);

/**
 * Definition of metadata foreach function
 *
 * @param key		metadata type id
 * @param value		metadata value
 * @param index		metadata list ordering index
 * @param user_data	user data
 */
typedef void	(*metadataForeachFunc)	(const gchar *key, const gchar *value, guint index, gpointer user_data);

/**
 * Can be used to iterate over all key/value pairs of the given metadata list.
 *
 * @param metadata	the metadata list
 * @param func		callback function
 * @param user_data	data to be passed to func
 */
void metadata_list_foreach(GSList *metadata, metadataForeachFunc func, gpointer user_data);

/**
 * Returns a list of all values of a given type from a specified metadata list.
 *
 * @param metadata	the metadata list
 * @param strid		the metadata type identifier
 *
 * @returns a list of values (or NULL)
 */
GSList * metadata_list_get_values(GSList *metadata, const gchar *strid);

/**
 * Creates a copy of a given metadata list.
 *
 * @param metadata	the metadata list
 *
 * @returns the new list
 */
GSList * metadata_list_copy(GSList *list);

/**
 * Frees all memory allocated by the given metadata list.
 *
 * @param metadata	the metadata list
 */
void metadata_list_free(GSList *metadata);

/**
 * Adds the given metadata list to a given XML document node.
 * To be used for saving feed metadata to cache.
 *
 * @param metadata	the metadata list
 * @param parentNode	the XML node
 */
void metadata_add_xml_nodes(GSList *metadata, xmlNodePtr parentNode);

#endif
