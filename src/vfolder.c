/**
 * @file vfolder.c VFolder functionality
 *
 * Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>
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

#include "support.h"
#include "callbacks.h"
#include "common.h"
#include "debug.h"
#include "vfolder.h"

/**
 * This pseudo feed is used to keep all items in memory that belong
 * to a vfolder. This is necessary because the source feed may not
 * not be kept in memory.
 */
static feedPtr		vfolder_item_pool = NULL;

/* though VFolders are treated like feeds, there 'll be a read() call
   when creating a new VFolder, we just do nothing but initializing
   the vfolder structure */
/*static feedPtr readVFolder(gchar *url) {
	feedPtr	vp;
	
	initialize channel structure 
	vp = g_new0(struct feed, 1);
	vp->type = FST_VFOLDER;
	vp->title = url;
	
	return vp;
}
*/

feedPtr vfolder_new(void) {
	feedPtr		fp;

	/* .... hmmm... this is not yet correct */
	fp = g_new0(struct feed, 1);
	fp->type = FST_VFOLDER;
	fp->title = g_strdup("vfolder");
	
	return fp;
}

/* Adds an item to this VFolder, this method is called
   when a VFolder scan method of a feed found a matching item */
void vfolder_add_item(feedPtr vp, itemPtr ip) {
	itemPtr		tmp;

	/* We internally create a item copy which is added
	   to the vfolder item pool and referenced by the 
	   vfolder. When the item is already in the pool
	   we only reference it. */	
	tmp = feed_lookup_item(vfolder_item_pool, ip->id);
	if(NULL == tmp) {
		tmp = item_new();	
		item_copy(ip, tmp);
		feed_add_item(vfolder_item_pool, tmp);
		feed_add_item(vp, tmp);
	} else {
		/* do we need reference counting? */
		feed_add_item(vp, tmp);
	}
}

/* Method to be called when a item was updated. This maybe
   after user interaction or updated item contents */
void vfolder_update_item(itemPtr ip) {
	GSList		*items = vfolder_item_pool->items;
	itemPtr		tmp;
	
	while(NULL != items) {
		tmp = items->data;
		g_assert(NULL != ip->fp);
		g_assert(NULL != tmp->fp);
		if((0 == strcmp(ip->id, tmp->id)) &&
		   (0 == strcmp((ip->fp)->id, (tmp->fp)->id))) {
		   	debug0(DEBUG_UPDATE, "item used in vfolder, updating vfolder copy...");
			item_copy(ip, tmp);
			return;
		}
		items = g_slist_next(items);
	}
}

feedHandlerPtr vfolder_init_feed_handler(void) {
	feedHandlerPtr	fhp;
	
	vfolder_item_pool = feed_new();
	
	fhp = g_new0(struct feedHandler, 1);

	/* prepare feed handler structure, we need this for
	   vfolders too to set item and OPML type identifier */
	fhp->typeStr		= "vfolder";
	fhp->icon		= ICON_AVAILABLE;
	fhp->directory		= FALSE;
	fhp->feedParser		= NULL;
	fhp->checkFormat	= NULL;
	fhp->merge		= FALSE;
	
	return fhp;
}

