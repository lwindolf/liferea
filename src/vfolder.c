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
 * The list of the rules of all vfolders, used to check new or
 * updated items against these rules.
 */
static GSList		*vfolder_rules = NULL;

/**
 * This pseudo feed is used to keep all items in memory that belong
 * to a vfolder. This is necessary because the source feed may not
 * not be kept in memory.
 */
static feedPtr		vfolder_item_pool = NULL;

/* sets up a vfolder feed structure */
feedPtr vfolder_new(void) {
	feedPtr		fp;

	/* .... hmmm... this is not yet correct */
	fp = g_new0(struct feed, 1);
	fp->type = FST_VFOLDER;
	fp->title = g_strdup("vfolder");
	fp->source = g_strdup("vfolder");
	fp->id = conf_new_id();
	
	return fp;
}

/* Method thats adds a rule to a vfolder. To be used
   on loading time or when creating searching. Does 
   not process items. Just sets up the vfolder */
void vfolder_add_rule(feedPtr vp, gchar *ruleId, gchar *value) {
	rulePtr		rp;

	if(NULL != (rp = rule_new(vp, ruleId, value))) {
		vfolder_rules = g_slist_append(vfolder_rules, rp);
	} else {
		g_warning("unknown rule id: \"%s\"", ruleId);
	}
}

/* Method that remove a rule from a vfolder. To be used
   when deleting or changing vfolders. Does not process
   items. */
void vfolder_remove_rule(feedPtr vp, rulePtr rp) {

	vfolder_rules = g_slist_remove(vfolder_rules, rp);
}

/* Adds an item to this VFolder, this method is called
   when any rule of the vfolder matched */
static void vfolder_add_item(feedPtr vp, itemPtr ip) {
	itemPtr		tmp;

	/* We internally create a item copy which is added
	   to the vfolder item pool and referenced by the 
	   vfolder. When the item is already in the pool
	   we only reference it. */	
	tmp = feed_lookup_item(vfolder_item_pool, ip->id);
	if(-1 == (int)tmp) {
		/* we need to avoid NULL id's somehow */
	} else if(NULL == tmp) {
		tmp = item_new();
		item_copy(ip, tmp);
		tmp->references = 1;
		feed_add_item(vfolder_item_pool, tmp);
		feed_add_item(vp, tmp);
	} else {
		/* do we need reference counting? */
		if(tmp->sourceFeed != ip->fp) {
// does not work as long id's are not unique
//			tmp->references++;
//			feed_add_item(vp, tmp);
		} else 
			g_warning("a search feed contains non-unique id's, one matching item was dropped...");
	}
}

static void vfolder_remove_item(feedPtr vp, itemPtr ip) {
	GSList		*items;
	gboolean	found = FALSE;

	items = vp->items;
	while(NULL != items) {
		if(items->data == ip) {
			found = TRUE;
			ip->references--;
			if(0 == ip->references) {
				vfolder_item_pool->items = g_slist_remove(vfolder_item_pool->items, ip);
				item_free(ip);
			}
		}
		items = g_slist_next(items);
	}
	
	if(found) {
		vp->items = g_slist_remove(vp->items, ip);
	} else {
		g_warning("vfolder_remove_item(): item not found...");
	}
}

/* Checks a new item against all additive rules of all feeds
   except the addition rules of the parent feed. In the second
   step the function checks wether there are parent feed rules,
   which do exclude this item. If there is such a rule the 
   function returns FALSE, otherwise TRUE to signalize if 
   this new item should be added. */
static void vfolder_apply_rules(nodePtr np, gpointer userdata) {
	feedPtr		vp = (feedPtr)userdata;
	feedPtr		fp = (feedPtr)np;
	GSList		*iter, *items;
	rulePtr		rp;
	itemPtr		ip;

	debug_enter("vfolder_apply_rules");
	debug1(DEBUG_UPDATE, "applying rules for %s", feed_get_source(fp));
	feed_load(fp);
	
	/* do not recursivly search ourselves!!! */
	if(vp == fp)
		return;

	/* check against all rules */
	iter = vfolder_rules;
	while(NULL != iter) {
		rp = iter->data;
		/* if rule is from this vfolder process it */
		if(vp == rp->fp)  {
			/* check all feed items */
			items = feed_get_item_list(fp);
			while(NULL != items) {
				ip = items->data;			
				if(rule_check_item(rp, ip)) {
					if(rp->additive) {
						debug1(DEBUG_UPDATE, "adding matching item: %s\n", item_get_title(ip));
						vfolder_add_item(vp, ip);
					} else {
						debug1(DEBUG_UPDATE, "deleting matching item: %s\n", item_get_title(ip));
						vfolder_remove_item(vp, ip);
					}
				}
				items = g_slist_next(items);
			}
		}
		iter = g_slist_next(iter);
	}
	
	feed_unload(fp);
	debug_exit("vfolder_apply_rules");
}

/* Method that applies the rules of the given vfolder to 
   all existing items. To be used for creating search
   results or new vfolders. Not to be used when loading
   vfolders from cache. */
void vfolder_refresh(feedPtr vp) {

	debug_enter("vfolder_refresh");
	ui_feedlist_do_for_all_data(NULL, ACTION_FILTER_FEED | ACTION_FILTER_DIRECTORY, vfolder_apply_rules, vp);
	debug_exit("vfolder_refresh");
}

/* Method to be called when a feed item was updated. This maybe
   after user interaction or updated item contents */
void vfolder_update_item(itemPtr ip) {
	GSList		*items = vfolder_item_pool->items;
	itemPtr		tmp;

	debug_enter("vfolder_update_item");

	/* never process vfolder items! */
	if(FST_VFOLDER == feed_get_type(ip->fp))
		return;
	
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
	
	debug_exit("vfolder_update_item");
}

/* called when a vfolder is processed by feed_free
   to get rid of the vfolder items */
void vfolder_free(feedPtr vp) {
	GSList		*items;
	itemPtr		ip;

	items = vp->items;
	while(NULL != items) {
		ip = items->data;
		ip->references--;
		if(0 == ip->references) {
			vfolder_item_pool->items = g_slist_remove(vfolder_item_pool->items, ip);
			item_free(ip);
		}
		items = g_slist_next(items);
	}
	g_slist_free(vp->items);
	vp->items = NULL;
}

feedHandlerPtr vfolder_init_feed_handler(void) {
	feedHandlerPtr	fhp;
	
	vfolder_item_pool = feed_new();
	vfolder_item_pool->type = FST_VFOLDER;
	
	fhp = g_new0(struct feedHandler, 1);

	/* prepare feed handler structure, we need this for
	   vfolders too to set item and OPML type identifier */
	fhp->typeStr		= "vfolder";
	fhp->icon		= ICON_AVAILABLE;
	fhp->directory		= FALSE;
	fhp->feedParser		= NULL;
	fhp->checkFormat	= NULL;
	fhp->merge		= FALSE;
	
	rule_init();
	
	return fhp;
}
