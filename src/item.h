/**
 * @file item.h common item handling
 * 
 * Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <time.h>

/* ------------------------------------------------------------ */
/* item interface						*/
/* ------------------------------------------------------------ */

#define ITEM_PROP_TITLE			0
#define ITEM_PROP_READSTATUS		1
#define ITEM_PROP_DESCRIPTION		2
#define ITEM_PROP_SOURCE		3
#define ITEM_PROP_ID			4
#define ITEM_PROP_TIME			5
#define ITEM_PROP_TYPE			6

struct feed;

/** An item stores a particular entry in a feed or a search */
typedef struct item {
	/* those fields should not be accessed directly. Accessors are provided. */
	gboolean 	readStatus;		/**< TRUE if the item has been read */
	gboolean	newStatus;		/**< TRUE if the item was downloaded and not yet displayed by notification features */
	gboolean	updateStatus;		/**< TRUE if the item content was updated */
	gboolean 	marked;			/**< TRUE if the item has been marked */
	gboolean	hidden;			/**< TRUE if the item should not be displayed due to filtering */
	gchar		*title;			/**< item title */
	gchar		*source;		/**< URL to the item */
	gchar		*real_source_url;	/**< (optional) URL of the real source */
	gchar		*real_source_title;	/**< (optional) title of the real source */
	gchar		*description;		/**< HTML string containing the item's description */
	gchar		*id;			/**< "Unique" syndication item identifier, for example <guid> in RSS */
	
	GSList		*metadata;		/**< metadata of this item */
	GHashTable	*tmpdata;		/**< tmp data hash used during stateful parsing */
	time_t		time;			/**< Item's modified date */
	
	gulong		nr;			/**< internal unique item number used for vfolder reference counting */
	struct feed	*fp;			/**< Pointer to the feed to which this item belongs */
	struct feed	*sourceFeed;		/**< Pointer to the source feed this item was derived from (used for searches and vfolders) */
} *itemPtr;

void ui_item_update(itemPtr ip); /* This is in itemlist.c */
/* Update all items listed. Useful after a display preference change */
void ui_itemlist_update();

/**
 * Allocates a new item structure.
 * @returns the new structure
 */
itemPtr 	item_new(void);

/**
 * Method to copy the contents of an item to another 
 */
void 		item_copy(itemPtr from, itemPtr to);

/**
 * Adds an item to the htmlview. This is used in 3-pane mode
 * @param ip the item to display
 */
void item_display(itemPtr ip);

/**
 * Returns a HTML string with a representation of the item
 * @param ip the item to render
 */
gchar *item_render(itemPtr ip);

/**
 * Free the memory used by an itempointer. The item needs to be
 * removed from the itemlist before calling this function.
 *
 * @param ip the item to remove
 */
void	item_free(itemPtr ip);

/* methods to access properties */
/** Returns the id of ip. */
const gchar *	item_get_id(itemPtr ip);
/** Returns the title of ip. */
const gchar *	item_get_title(itemPtr ip);
/** Returns the description of ip. */
const gchar *	item_get_description(itemPtr ip);
/** Returns the source of ip. */
const gchar *	item_get_source(itemPtr ip);
/** Returns the real source of ip. */
const gchar *	item_get_real_source_url(itemPtr ip);
/** Returns the real source title of ip. */
const gchar *	item_get_real_source_title(itemPtr ip);
/** Returns the modification time of ip. */
const time_t	item_get_time(itemPtr ip);
/** Returns the flag status of ip */
const gboolean	item_get_flag(itemPtr ip);
/** Returns the read status of ip. */
const gboolean	item_get_read_status(itemPtr ip);
/** Returns the hidden flag of ip. */
const gboolean	item_get_hidden(itemPtr ip);
/** Returns the new flag of ip. */
const gboolean	item_get_new_status(itemPtr ip);
/** Returns the update flag of ip. */
const gboolean	item_get_update_status(itemPtr ip);

/** Sets the ip's title */
void		item_set_title(itemPtr ip, const gchar * title);
/** Sets the ip's description */
void		item_set_description(itemPtr ip, const gchar * description);
/** Sets the ip's source */
void		item_set_source(itemPtr ip, const gchar * source);
/** Sets the ip's real source */
void		item_set_real_source_url(itemPtr ip, const gchar * source);
/** Sets the ip's real source title */
void		item_set_real_source_title(itemPtr ip, const gchar * source);
/** Sets the ip's modification time */
void		item_set_time(itemPtr ip, const time_t time);
/** Sets the ip's id */
void		item_set_id(itemPtr ip, const gchar * id);
/** Sets the ip's read status */
void 		item_set_read_status(itemPtr ip, gboolean newStatus);
/** Sets the ip's hidden flag */
void 		item_set_hidden(itemPtr ip, const gboolean hidden);
/** Sets the ip's new flag */
void 		item_set_new_status(itemPtr ip, const gboolean newStatus);
/** Sets the ip's update flag */
void 		item_set_update_status(itemPtr ip, const gboolean newStatus);

/**
 * Marks ip as marked or unmarked and updates the UI to reflect this
 * change.
 *
 * @param ip item to be marked or unmarked
 * @param newStatus set to TRUE if the item is to be marked, or FALSE to
 * unmark the item
 */
void	item_set_flag(itemPtr ip, const gboolean newStatus);

/**
 * Parse an xml tree and return a new itempointer generated 
 * from the current node's information.
 */
itemPtr item_parse_cache(xmlDocPtr doc, xmlNodePtr cur);

/**
 * Does the opposite of item_parse_cache. Generates a XML node
 * to be saved into the feeds cache document. 
 */
void item_save(itemPtr ip, xmlNodePtr feedNode);

#endif
