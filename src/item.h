/**
 * @file feed.h common item handling
 * 
 * Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>
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
	gchar		*title;		/**< item title */
	gboolean 	readStatus;	/**< TRUE if the item has been read */
	gboolean	newStatus;	/**< TRUE if the item was downloaded and not yet displayed by notification features */
	gboolean 	marked;		/**< TRUE if the item has been marked */
	gboolean	hidden;		/**< TRUE if the item should not be displayed due to filtering */
	gchar		*description;	/**< HTML string containing the item's description */
	gchar		*source;	/**< URL to the item */
	gchar		*id;		/**< Unique item identifier, for example <guid> in RSS */
	GSList		*metadata;	/**< metadata of this item */
	GHashTable	*tmpdata;	/**< tmp data hash used during stateful parsing */
	time_t		time;		/**< Item's modified date */
	
	struct feed	*fp;		/**< Pointer to the feed to which this item belongs */
	GSList		*vfolders;	/**< List of vfolders in which this item appears */
	void		*ui_data;	/**< UI specific data such as in which row an item is displayed */
} *itemPtr;

void ui_free_item_ui_data(itemPtr ip); /* This is in itemlist.c */
void ui_update_item(itemPtr ip); /* This is in itemlist.c */
/* Update all items listed. Useful after a display preference change */
void ui_update_itemlist();

/**
 * Allocates a new item structure.
 * @returns the new structure
 */
itemPtr 	item_new(void);

void 		addVFolderToItem(itemPtr ip, gpointer fp);
void		removeVFolderFromItem(itemPtr ip, gpointer fp);


/**
 * Adds an item to the htmlview. This is used in 3-pane mode
 * @param ip the item to display
 */
void item_display(itemPtr ip);

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
/** Returns the modification time of ip. */
const time_t	item_get_time(itemPtr ip);
/** Returns the mark status of ip */
const gboolean	item_get_mark(itemPtr ip);
/** Returns the read status of ip. */
const gboolean	item_get_read_status(itemPtr ip);
/** Returns the hidden flag of ip. */
const gboolean	item_get_hidden(itemPtr ip);
/** Returns the new flag of ip. */
const gboolean	item_get_new_status(itemPtr ip);

/** Marks ip as read and updates the UI to reflect this change */
void 		item_set_read(itemPtr ip);
/** Marks ip as unread and updates the UI to reflect this change */
void 		item_set_unread(itemPtr ip);
/** Sets the ip's title */
void		item_set_title(itemPtr ip, const gchar * title);
/** Sets the ip's description */
void		item_set_description(itemPtr ip, const gchar * description);
/** Sets the ip's source */
void		item_set_source(itemPtr ip, const gchar * source);
/** Sets the ip's modification time */
void		item_set_time(itemPtr ip, const time_t time);
/** Sets the ip's read status */
void		item_set_read_status(itemPtr ip, const gboolean readStatus);
/** Sets the ip's id */
void		item_set_id(itemPtr ip, const gchar * id);
/** Sets the ip's hidden flag */
void 		item_set_hidden(itemPtr ip, const gboolean hidden);
/** Sets the ip's new flag */
void 		item_set_new_status(itemPtr ip, const gboolean newStatus);

/**
 * Marks ip as marked or unmarked and updates the UI to reflect this
 * change.
 *
 * @param ip item to be marked or unmarked
 * @param flag set to TRUE if the item is to be marked, or FALSE to
 * unmark the item
 */
void	item_set_mark(itemPtr ip, const gboolean flag);

/** Parse an xml tree and return a new itempointer generated from the
    current node's information */
itemPtr item_parse_cache(xmlDocPtr doc, xmlNodePtr cur);

#endif
