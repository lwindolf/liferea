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
#include "conf.h"
#include "debug.h"
#include "vfolder.h"

/**
 * The list of all existing vfolders. Used for updating vfolder
 * information upon item changes
 */
static GSList		*vfolders = NULL;

/* sets up a vfolder feed structure */
feedPtr vfolder_new(void) {
	feedPtr		fp;
	
	debug_enter("vfolder_new");

	fp = feed_new();;
	feed_set_type(fp, FST_VFOLDER);
	feed_set_title(fp, "vfolder");
	feed_set_source(fp, "vfolder");
	feed_set_id(fp, conf_new_id());
	feed_set_available(fp, TRUE);
	fp->fhp = feed_type_str_to_fhp("vfolder");
	vfolders = g_slist_append(vfolders, fp);
	
	debug_exit("vfolder_new");
	
	return fp;
}

/* Method thats adds a rule to a vfolder. To be used
   on loading time or when creating searching. Does 
   not process items. Just sets up the vfolder */
void vfolder_add_rule(feedPtr vp, const gchar *ruleId, const gchar *value, gboolean additive) {
	rulePtr		rp;
	
	debug_enter("vfolder_add_rule");

	if(NULL != (rp = rule_new(vp, ruleId, value, additive))) {
		vp->rules = g_slist_append(vp->rules, rp);
	} else {
		g_warning("unknown rule id: \"%s\"", ruleId);
	}
	
	debug_exit("vfolder_add_rule");
}

/* Method that remove a rule from a vfolder. To be used
   when deleting or changing vfolders. Does not process
   items. */
void vfolder_remove_rule(feedPtr vp, rulePtr rp) {

	debug_enter("vfolder_remove_rule");
	vp->rules = g_slist_remove(vp->rules, rp);
	debug_exit("vfolder_remove_rule");
}

/**
 * Adds an item to this VFolder, this method is called
 * when any rule of the vfolder matched 
 */
static void vfolder_add_item(feedPtr vp, itemPtr ip) {
	GSList		*iter;
	itemPtr		tmp;
	
	/* need to check for vfolder item because the
	   rule checking can be called on item copies and
	   may trigger the adding */
	if(FST_VFOLDER == feed_get_type(ip->fp))
		return;

	/* check if the item was already added */
	iter = vp->items;
	while(NULL != iter) {
		tmp = iter->data;
		if((ip->nr == tmp->nr) && (ip->fp == tmp->sourceFeed))
			return;
		iter = g_slist_next(iter);
	}

	/* add an item copy to the vfolder */	
	tmp = item_new();
	item_copy(ip, tmp);
	feed_add_item(vp, tmp);
	ui_itemlist_update_vfolder((nodePtr)vp);		/* update the itemlist if this vfolder is selected */
}

/* used to remove a vfolder item copy from a vfolder */
static void vfolder_remove_item_copy(feedPtr vp, itemPtr ip) {
	GSList		*items;
	gboolean	found = FALSE;

	items = vp->items;
	while(NULL != items) {
		if(items->data == ip)
			found = TRUE;
		items = g_slist_next(items);
	}
	
	if(found) {
		item_free(ip);
		vp->items = g_slist_remove(vp->items, ip);
	} else {
		g_warning("vfolder_remove_item(): item not found...");
	}
}

/** 
 * Searches a given vfolder for a copy of the passed item and
 * removes them. Used for item remove propagation and for 
 * processing of removing vfolder rules.
 */
static void vfolder_remove_matching_item_copy(feedPtr vp, itemPtr ip) {
	GSList		*items;
	itemPtr		tmp;

	items = feed_get_item_list(vp);
	while(NULL != items) {
		tmp = items->data;
		g_assert(NULL != ip->fp);
		g_assert(NULL != tmp->fp);
		if((ip->nr == tmp->nr) &&
		   (ip->fp == tmp->sourceFeed)) {
			vfolder_remove_item_copy(vp, tmp);
			break;
		}
		items = g_slist_next(items);
	}
}

/** 
 * Searches all vfolders for copies of the given item and
 * removes them. Used for item remove propagation. If a
 * vfolder item copy is passed it is removed directly.
 */
void vfolder_remove_item(itemPtr ip) {
	GSList		*iter;
	feedPtr		vp;
	
	debug_enter("vfolder_remove_item");

	/* distinguish between checking vfolder items
	   and feed items ... */
	if(FST_VFOLDER != feed_get_type(ip->fp)) {
		iter = vfolders;
		while(NULL != iter) {
			vp = (feedPtr)iter->data;
			vfolder_remove_matching_item_copy(vp, ip);
			iter = g_slist_next(iter);
		}
	} else {
		vfolder_remove_item_copy(ip->fp, ip);
	}
	
	debug_exit("vfolder_remove_item");
}

/**
 * Check item against rules of a vfolder. When the item matches an 
 * additive rule it is added to the vfolder and when it matches an excluding 
 * rule the item is removed again from the given vfolder.
 */
static gboolean vfolder_apply_rules_for_item(feedPtr vp, itemPtr ip) {
	rulePtr		rp;
	GSList		*iter;
	gboolean	added = FALSE;

	/* check against all rules */
	iter = vp->rules;
	while(NULL != iter) {
		rp = iter->data;
		if(rp->additive) {
			if(!added && rule_check_item(rp, ip)) {
				debug2(DEBUG_UPDATE, "adding matching item (%d): %s\n", ip->nr, item_get_title(ip));
				vfolder_add_item(vp, ip);
				added = TRUE;
			}
		} else {
			if(added && rule_check_item(rp, ip)) {
				debug2(DEBUG_UPDATE, "deleting matching item (%d): %s\n", ip->nr, item_get_title(ip));
				vfolder_remove_matching_item_copy(vp, ip);
				added = FALSE;
			}
		}
		iter = g_slist_next(iter);
	}
	
	return added;
}

/**
 * Checks all items of the given feed list node against a vfolder. 
 * Used by vfolder_refresh().
 */
static void vfolder_apply_rules(nodePtr np, gpointer userdata) {
	feedPtr		vp = (feedPtr)userdata;
	feedPtr		fp = (feedPtr)np;
	GSList		*items;

	/* do not search in vfolders */
	g_return_if_fail(FST_VFOLDER != feed_get_type(fp));

	debug_enter("vfolder_apply_rules");

	debug1(DEBUG_UPDATE, "applying rules for (%s)", feed_get_source(fp));
	feed_load(fp);

	/* check all feed items */
	items = feed_get_item_list(fp);
	while(NULL != items) {
		vfolder_apply_rules_for_item(vp, items->data);
		items = g_slist_next(items);
	}
	
	feed_unload(fp);
	debug_exit("vfolder_apply_rules");
}

/* Method that applies the rules of the given vfolder to 
   all existing items. To be used for creating search
   results or new vfolders. Not to be used when loading
   vfolders from cache. */
void vfolder_refresh(feedPtr vp) {
	GSList		*iter;
	
	debug_enter("vfolder_refresh");
	
	/* free vfolder items */
	iter = vp->items;
	while(NULL != iter) {
		item_free(iter->data);
		iter = g_slist_next(iter);
	}
	g_slist_free(vp->items);
	vp->items = NULL;
	
	ui_feedlist_do_for_all_data(NULL, ACTION_FILTER_FEED | ACTION_FILTER_DIRECTORY, vfolder_apply_rules, vp);
	
	debug_exit("vfolder_refresh");
}

/**
 * Method to be called when a feed item was updated. This maybe
 * after user interaction or updated item contents.
 */
void vfolder_update_item(itemPtr ip) {
	GSList		*iter, *items, *rule;
	itemPtr		tmp;
	feedPtr		vp;
	rulePtr		rp;
	gboolean	keep, remove;

	debug_enter("vfolder_update_item");

	/* never process vfolder items! */
	g_assert(FST_VFOLDER != feed_get_type(ip->fp));
	
	iter = vfolders;
	while(NULL != iter) {
		vp = (feedPtr)iter->data;
		
		/* first step: update item copy if found */
		items = feed_get_item_list(vp);
		while(NULL != items) {
			tmp = items->data;
			g_assert(NULL != ip->fp);
			g_assert(NULL != tmp->fp);
			/* find the item copies */
			if((ip->nr == tmp->nr) &&
			   (ip->fp == tmp->sourceFeed)) {
				/* check if the item still matches, the item won't get added
				   another time so this call effectivly just checks if the
				   item is still to remain added. */

				keep = FALSE; 
				remove = FALSE;

				/* check against all rules */
				rule = vp->rules;
				while(NULL != rule) {
					rp = rule->data;
					if(TRUE == rule_check_item(rp, ip)) {
						/* the rule we checked does apply */
						if(TRUE == rp->additive) {
							keep = TRUE;
						} else {
							remove = TRUE;
							break;
						}
					}
					rule = g_slist_next(rule);
				}

				if((TRUE == keep) && (FALSE == remove)) {
			   		debug2(DEBUG_UPDATE, "item (%s) used in vfolder (%s), updating vfolder copy...", ip->title, vp->title);
					item_copy(ip, tmp);
				} else {
					debug2(DEBUG_UPDATE, "item (%s) used in vfolder (%s) does not match anymore -> removing...",  ip->title, vp->title);

					/* we cannot remove the item copy just now because 
					   there maybe other state changes be done (e.g. 
					   ui_itemlist_selection_changed might set read 
					   and update status */
					tmp->hidden = TRUE;	/* just hide the item copy */
				}
				break;
			}
			items = g_slist_next(items);
		}
		
		/* second step: update vfolder unread count */
		vp->unreadCount = 0;
		items = feed_get_item_list(vp);
		while(NULL != items) {
			tmp = items->data;
			if(FALSE == item_get_read_status(tmp)) 
				feed_increase_unread_counter(vp);

			items = g_slist_next(items);
		}
		iter = g_slist_next(iter);
		
		ui_itemlist_update_vfolder((nodePtr)vp);		/* update the itemlist if this vfolder is selected */
	}
	
	debug_exit("vfolder_update_item");
}

/**
 * Method to be called when a new item needs to be checked
 * against all vfolder rules. To be used upon feed list loading
 * and when new items are downloaded.
 *
 * This method may also be used to recheck an vfolder item
 * copy again. This may remove the item copy if it does not
 * longer match the vfolder rules.
 */
gboolean vfolder_check_item(itemPtr ip) {
	GSList		*iter;
	gboolean	added = FALSE;

	debug_enter("vfolder_check_item");

	/* distinguish between checking vfolder items
	   and feed items ... */
	if(FST_VFOLDER != feed_get_type(ip->fp)) {
		iter = vfolders;
		while(NULL != iter) {
			added |= vfolder_apply_rules_for_item(iter->data, ip);
			iter = g_slist_next(iter);
		}
	} else {
		added = vfolder_apply_rules_for_item(ip->fp, ip);
	}
	
	debug_exit("vfolder_check_item");
	return added;
}

/* called when a vfolder is processed by feed_free
   to get rid of the vfolder items */
void vfolder_free(feedPtr vp) {
	GSList		*iter;

	debug_enter("vfolder_free");

	vfolders = g_slist_remove(vfolders, vp);

	/* free vfolder items */
	iter = vp->items;
	while(NULL != iter) {
		item_free(iter->data);
		iter = g_slist_next(iter);
	}
	g_slist_free(vp->items);
	vp->items = NULL;
	
	/* free vfolder rules */
	iter = vp->rules;
	while(NULL != iter) {
		rule_free(iter->data);
		iter = g_slist_next(iter);
	}
	g_slist_free(vp->rules);
	vp->rules = NULL;
	
	debug_exit("vfolder_free");
}

feedHandlerPtr vfolder_init_feed_handler(void) {
	feedHandlerPtr	fhp;
	
	fhp = g_new0(struct feedHandler, 1);

	/* prepare feed handler structure, we need this for
	   vfolders too to set item and OPML type identifier */
	fhp->typeStr		= "vfolder";
	fhp->icon		= ICON_VFOLDER;
	fhp->directory		= FALSE;
	fhp->feedParser		= NULL;
	fhp->checkFormat	= NULL;
	fhp->merge		= FALSE;
	
	rule_init();
	
	return fhp;
}
