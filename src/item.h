/*
 * @file item.h item handling
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

#ifndef _ITEM_H
#define _ITEM_H

#include <glib.h>
#include <glib-object.h>

/* Each feed/subscription type provider must provide it's data using `Item` */

G_BEGIN_DECLS

#define LIFEREA_ITEM_TYPE	(liferea_item_get_type ())
G_DECLARE_FINAL_TYPE (LifereaItem, liferea_item, LIFEREA, ITEM, GObject)

/*
 * An item stores a particular entry in a feed or a search.
 *
 *  Each item belongs to an item set. An itemset is a collection
 *  of items. There are different item set types (e.g. feed,
 *  folder, search folder or plugin). Each item has a source node.
 *  The item set node and the item source node is different
 *  for folders and search folders.
 */
struct _LifereaItem {
	GObject	parent_instance;

	gulong		id;			/*<< internally unique item id */

	/* those fields should not be accessed directly. Accessors are provided. */
	gboolean 	readStatus;		/*<< TRUE if the item has been read */
	gboolean	popupStatus;		/*<< TRUE if the item was downloaded and is yet to be displayed by the popup notification feature */
	gboolean	updateStatus;		/*<< TRUE if the item content was updated */
	gboolean 	flagStatus;		/*<< TRUE if the item has been flagged */
	gboolean	hasEnclosure;		/*<< TRUE if this item has at least one enclosure */
        gboolean        isHidden;               /*<< TRUE if this item has been 'hidden' in search folder */
	gchar		*title;			/*<< Title */
	gchar		*source;		/*<< URL to the post online */
	gchar		*sourceId;		/*<< "Unique" syndication item identifier, for example <guid> in RSS */
	gboolean	validGuid;		/*<< TRUE if id of this item is a GUID and can be used for duplicate detection */
	gboolean	validTime;		/*<< TRUE if time of this item is valid (from feed and not just timestamp of download) */
	gchar		*description;		/*<< XHTML string containing the item's description */

	GSList		*metadata;		/*<< Metadata of this item */
	GHashTable	*tmpdata;		/*<< Temporary data hash used during stateful parsing */
	gint64		time;			/*<< Last modified date of the headline */

	gchar		*commentFeedId;		/*<< Id of the comment feed of this item (or NULL if there is no comment feed) */

	/* comment item properties */
	gulong		parentItemId;		/*<< Id of the parent item the item belongs to(or 0 if no comment item) */
	gboolean	isComment;		/*<< TRUE if item is from a comment feed */

	/* item source properties */
	gchar		*nodeId;		/*<< Node id the containing node. Might be a comment feed id. */
	gchar		*parentNodeId;		/*<< Real parent node id. Always a feed list node id. */
	gulong 		sourceNr;		/*<< Either equal to nr or the number of the item this one is a copy of */

	/* remote states used during sync of remote accounts */
	gboolean	remoteReadStatus;	/*<< TRUE if the remote copy of the item has been read */
	gboolean	remoteFlagStatus;	/*<< TRUE if the remote copy of the item has been flagged */
};

typedef struct _LifereaItem *itemPtr;

/**
 * item_new: (skip)
 * Allocates a new item structure.
 *
 * Returns: (transfer full): the new structure
 */
LifereaItem *	item_new(void);

/**
 * item_load: (skip)
 * @id:	item id to load
 *
 * Returns the item structure for the given item id or
 * NULL if no such item does exist. The caller has to free
 * the item with item_unload() once it is not used anymore.
 *
 * Returns: (transfer full) (nullable): item structure
 */
LifereaItem *	item_load(gulong id);

// For legacy code let's keep item_unload()
#define item_unload(a) g_object_unref(a)

/**
 * item_copy: (skip)
 * @item: the item to copy
 *
 * Method to create a copy of an item. The copy will be
 * linked to the original item to allow state update
 * propagation (to be used with vfolders).
 *
 * Returns: (transfer full): copy of the item.
 */
LifereaItem *	item_copy(LifereaItem * item);

/**
 * item_get_base_url: (skip)
 * @item:	the item
 *
 * Returns the base URL for the given item.
 *
 * Returns: base URL
 */
const gchar * item_get_base_url(LifereaItem *item);

/* methods to access properties */
/* Returns the id of item. */
const gchar *	item_get_id(LifereaItem *item);
/* Returns the title of item. */
const gchar *	item_get_title(LifereaItem *item);
/* Returns the description of item. */
const gchar *	item_get_description(LifereaItem *item);
/* Returns the source of item. */
const gchar *	item_get_source(LifereaItem *item);

/**
 * item_get_teaser: (skip)
 * @item:	the item
 *
 * Create a plain text teaser from the item description
 *
 * Returns: (transfer full): newly allocated string to be free'd using g_free() (or NULL)
 */
gchar * item_get_teaser(LifereaItem *item);

/**
 * item_make_link: (skip)
 * @item:	the item
 *
 * Returns the resolved link for the item.
 *
 * Returns: (transfer full): newly allocated URI to be free'd using g_free()
 */
gchar *	item_make_link(LifereaItem *item);

/**
 * item_get_author: (skip)
 * @item:	the item
 *
 * Returns the resolved author for the item
 *
 * Returns: pointer to string in GSList meta data
 */
const gchar * item_get_author(LifereaItem *item);

/**
 * item_set_title: (skip)
 * @item:		the item
 * @title:		the title
 *
 * Sets the item title
 */
void item_set_title(LifereaItem *item, const gchar * title);

/**
 * item_set_description: (skip)
 * @item:		the item
 * @description:	the content
 *
 * Sets the item description. If called more than once it
 * will merge the new description against the old one deciding
 * on the best to keep.
 */
void item_set_description (LifereaItem *item, const gchar *description);

/**
 * item_set_source: (skip)
 * @item:		the item
 * @source:		the source
 *
 * Sets the item source 
 */
void item_set_source(LifereaItem *item, const gchar * source);

/**
 * item_set_id: (skip)
 * @item:		the item
 * @id:			the id
 *
 * Sets the item id 
 */
void item_set_id (LifereaItem *item, const gchar * id);

/**
 * item_set_time: (skip)
 * @item:		the item
 * @time:		the time
 *
 * Sets the item time. Always use this when a valid date was 
 * supplied for the item!
 */
void item_set_time (LifereaItem *item, gint64 time);

/**
 * item_to_xml: (skip)
 * @item:		the item to serialize
 * @parentNode:	the xmlNodePtr to add to
 *
 * Adds an XML node to the given item.
 *
 */
void item_to_xml (LifereaItem *item, gpointer parentNode);

/**
 * item_render: (skip)
 * @item:		the item to serialize
 * @viewMode:		the item view mode
 *
 * Uses item_xml() and adds additional feed info to the item info for rendering
 *
 * Returns XML string (to be free'd using g_free())
 */
gchar * item_render (LifereaItem *item, guint viewMode);

G_END_DECLS

#endif