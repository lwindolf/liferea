/**
 * @file item.h common item handling
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

#ifndef _ITEM_H
#define _ITEM_H

#include <glib.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <time.h>

/* Currently Liferea knows only a single type of items used
   for the itemset types feed, folder and search folder. So each 
   feed list type provider must provide it's data using the
   item interface. */
 
/* ------------------------------------------------------------ */
/* item interface						*/
/* ------------------------------------------------------------ */

/**
 * An item stores a particular entry in a feed or a search.
 *  Each item belongs to an item set. An itemset is a collection
 *  of items. There are different item set types (e.g. feed,
 *  folder,vfolder or plugin). Each item has a source node.
 *  The item set node and the item source node is different
 *  for folders and vfolders. */
typedef struct item {
	gulong		id;			/**< internally unique item id */

	/* those fields should not be accessed directly. Accessors are provided. */
	gboolean 	readStatus;		/**< TRUE if the item has been read */
	gboolean	newStatus;		/**< TRUE if the item was downloaded and is to be counted by the tray icon */
	gboolean	popupStatus;		/**< TRUE if the item was downloaded and is yet to be displayed by the popup notification feature */
	gboolean	updateStatus;		/**< TRUE if the item content was updated */
	gboolean 	flagStatus;		/**< TRUE if the item has been flagged */
	gboolean	hasEnclosure;		/**< TRUE if this item has at least one enclosure */
	gchar		*title;			/**< Title */
	gchar		*source;		/**< URL to the post online */
	gchar		*sourceId;		/**< "Unique" syndication item identifier, for example <guid> in RSS */
	gboolean	validGuid;		/**< TRUE if id of this item is a GUID and can be used for duplicate detection */
	gchar		*real_source_url;	/**< (optional) URL of the real source (e.g. if listed in search engine result) */
	gchar		*real_source_title;	/**< (optional) title of the real source */
	gchar		*description;		/**< XHTML string containing the item's description */
	
	GSList		*metadata;		/**< Metadata of this item */
	GHashTable	*tmpdata;		/**< Temporary data hash used during stateful parsing */
	time_t		time;			/**< Last modified date of the headline */

	/* comment item properties */
	gchar		*commentFeedId;		/**< Id of the comment feed the item belongs to (or NULL if no comment item)*/
	gulong		parentItemId;		/**< Id of the parent item the item belongs to(or 0 if no comment item) */
	gboolean	isComment;		/**< TRUE if item is from a comment feed */

	/* item source properties */
	gchar		*nodeId;		/**< Node id the containing node */
	gulong 		sourceNr;		/**< Either equal to nr or the number of the item this one is a copy of */
} *itemPtr;

/**
 * Allocates a new item structure.
 *
 * @returns the new structure
 */
itemPtr 	item_new(void);

/**
 * Returns the item structure for the given item id or
 * NULL if no such item does exist. The caller has to free
 * the item with item_unload() once it is not used anymore.
 *
 * @param id	item id to load
 *
 * @returns item structure
 */
itemPtr		item_load(gulong id);

/**
 * Method to create a copy of an item. The copy will be
 * linked to the original item to allow state update
 * propagation (to be used with vfolders).
 */
itemPtr		item_copy(itemPtr item);

/**
 * Returns the base URL for the given item.
 *
 * @param item	the item
 *
 * @returns base URL
 */
const gchar * item_get_base_url(itemPtr item);

/**
 * Free the memory used by an itempointer. The item needs to be
 * removed from the itemlist before calling this function.
 *
 * @param item	the item to unload
 */
void	item_unload(itemPtr item);

/* methods to access properties */
/** Returns the id of item. */
const gchar *	item_get_id(itemPtr item);
/** Returns the title of item. */
const gchar *	item_get_title(itemPtr item);
/** Returns the description of item. */
const gchar *	item_get_description(itemPtr item);
/** Returns the source of item. */
const gchar *	item_get_source(itemPtr item);
/** Returns the real source of item. */
const gchar *	item_get_real_source_url(itemPtr item);
/** Returns the real source title of item. */
const gchar *	item_get_real_source_title(itemPtr item);

/** Sets the item title */
void		item_set_title(itemPtr item, const gchar * title);
/** Sets the item description */
void		item_set_description(itemPtr item, const gchar * description);
/** Sets the item source */
void		item_set_source(itemPtr item, const gchar * source);
/** Sets the item real source */
void		item_set_real_source_url(itemPtr item, const gchar * source);
/** Sets the item real source title */
void		item_set_real_source_title(itemPtr item, const gchar * source);
/** Sets the item id */
void		item_set_id(itemPtr item, const gchar * id);

/**
 * Does the opposite of item_parse_cache. Adds an XML node
 * to the given feed item list node. 
 *
 * @param item		the item to save to cache
 * @param parentNode	the XML node to add to
 */
void item_to_xml(itemPtr item, xmlNodePtr parentNode);

#endif
