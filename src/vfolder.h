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
#include "rule.h"

/* standard feed/item type interface */
feedHandlerPtr	vfolder_init_feed_handler(void);

/* sets up a vfolder feed structure */
feedPtr vfolder_new(void);

/**
 * Method thats adds a rule to a vfolder. To be used
 * on loading time or when creating searching. Does 
 * not process items. Just sets up the vfolder.
 *  
 * @param fp		vfolder the rule belongs to
 * @param ruleId	id string for this rule type
 * @param value		argument string for this rule
 * @param additive	indicates positive or negative logic
 */
void	vfolder_add_rule(feedPtr vp, const gchar *ruleId, const gchar *value, gboolean additive);

/** 
 * Method that removes a rule from a vfolder. To be used
 * when deleting or changing vfolders. Does not process
 * items. 
 *
 * @param vp	vfolder
 * @param rp	rule to remove
 */
void	vfolder_remove_rule(feedPtr vp, rulePtr rp);

/**
 * Method that applies the rules of the given vfolder to 
 * all existing items. To be used for creating search
 * results or new vfolders. Not to be used when loading
 * vfolders from cache. 
 *
 * @param vp	vfolder
 */
void	vfolder_refresh(feedPtr vp);

/**
 * Method to be called when a item was updated. This maybe
 * after user interaction or updated item contents 
 *
 * @param ip	item of a feed to check
 */
void	vfolder_update_item(itemPtr ip);

/**
 * Method to be called when a new item needs to be checked
 * against all vfolder rules. To be used upon feed list loading
 * and when new items are downloaded.
 *
 * This method may also be used to recheck an vfolder item
 * copy again. This may remove the item copy if it does not
 * longer match the vfolder rules.
 *
 * @param ip	item of a feed to check
 * @returns TRUE if item (still) added, FALSE otherwise
 */
gboolean vfolder_check_item(itemPtr ip);

/** 
 * Searches all vfolders for copies of the given item and
 * removes them. Used for item remove propagation. If a
 * vfolder item copy is passed it is removed directly.
 *
 * @param ip	feed item or vfolder item copy to remove
 */
void	vfolder_remove_item(itemPtr ip);

/**
 * Called when a vfolder is processed by feed_free
 * to get rid of the vfolder items.
 *
 * @param vp	vfolder to free
 */
void	vfolder_free(feedPtr vp);

#endif
