/**
 * @file vfolder.h VFolder functionality
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
 
#ifndef _VFOLDER_H
#define _VFOLDER_H

#include "feed.h"
#include "item.h"

/* standard feed/item type interface */
feedHandlerPtr	vfolder_init_feed_handler(void);

/* sets up a vfolder feed structure */
feedPtr vfolder_new(void);

/* Method thats adds a rule to a vfolder. To be used
   on loading time or when creating searching. Does 
   not process items. Just sets up the vfolder */
void	vfolder_add_rule(feedPtr vp, gchar *ruleId, gchar *value);

/* Method that applies the rules of the given vfolder to 
   all existing items. To be used for creating search
   results or new vfolders. Not to be used when loading
   vfolders from cache. */
void vfolder_refresh(feedPtr vp);

/* Method to be called when a item was updated. This maybe
   after user interaction or updated item contents */
void	vfolder_update_item(itemPtr ip);

/* called when a vfolder is processed by feed_free
   to get rid of the vfolder items */
void	vfolder_free(feedPtr vp);

#endif
