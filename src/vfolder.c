/**
 * @file vfolder.c VFolder functionality
 *
 * Copyright (C) 2003-2006 Lars Lindner <lars.lindner@gmx.net>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "support.h"
#include "callbacks.h"
#include "common.h"
#include "conf.h"
#include "debug.h"
#include "itemset.h"
#include "itemlist.h"
#include "node.h"
#include "vfolder.h"
#include "ui/ui_vfolder.h"

/**
 * The list of all existing vfolders. Used for updating vfolder
 * information upon item changes
 */
static GSList		*vfolders = NULL;

void vfolder_init(void) {

	rule_init();
}

/* sets up a vfolder feed structure */
vfolderPtr vfolder_new(nodePtr node) {
	vfolderPtr	vfolder;
	itemSetPtr	itemSet;

	debug_enter("vfolder_new");

	vfolder = g_new0(struct vfolder, 1);
	vfolder->node = node;
	vfolders = g_slist_append(vfolders, vfolder);

	node_set_title(node, _("New Search Folder"));	/* set default title */
	node_set_type(node, vfolder_get_node_type());
	node_set_data(node, (gpointer)vfolder);

	itemSet = (itemSetPtr)g_new0(struct itemSet, 1); /* create empty itemset */
	itemSet->type = ITEMSET_TYPE_VFOLDER;
	node_set_itemset(node, itemSet);
	
	debug_exit("vfolder_new");
	
	return vfolder;
}

static void vfolder_import_rules(xmlNodePtr cur, vfolderPtr vp) {
	xmlChar		*type, *ruleId, *value, *additive;
	
	/* process any children */
	cur = cur->xmlChildrenNode;
	while(cur != NULL) {
		if(!xmlStrcmp(cur->name, BAD_CAST"outline")) {
			type = xmlGetProp(cur, BAD_CAST"type");
			if(type != NULL && !xmlStrcmp(type, BAD_CAST"rule")) {

				ruleId = xmlGetProp(cur, BAD_CAST"rule");
				value = xmlGetProp(cur, BAD_CAST"value");
				additive = xmlGetProp(cur, BAD_CAST"additive");

				if((NULL != ruleId) && (NULL != value)) {			
					debug2(DEBUG_CACHE, "loading rule \"%s\" \"%s\"\n", ruleId, value);

					if(additive != NULL && !xmlStrcmp(additive, BAD_CAST"true"))
						vfolder_add_rule(vp, ruleId, value, TRUE);
					else
						vfolder_add_rule(vp, ruleId, value, FALSE);
				} else {
					g_warning("ignoring invalid rule entry in feed list...\n");
				}
				
				xmlFree(ruleId);
				xmlFree(value);
				xmlFree(additive);
			}
			xmlFree(type);
		}
		cur = cur->next;
	}
}

static void vfolder_import(nodePtr node, nodePtr parent, xmlNodePtr cur, gboolean trusted) {
	vfolderPtr vfolder;

	debug_enter("vfolder_import");

	vfolder = g_new0(struct vfolder, 1);
	vfolder->node = node;
	vfolder_import_rules(cur, vfolder);
	vfolders = g_slist_append(vfolders, vfolder);

	debug1(DEBUG_CACHE, "import vfolder: title=%s", node_get_title(node));
	
	node_set_type(node, vfolder_get_node_type());
	node_set_data(node, (gpointer)vfolder);
	node_add_child(parent, node, -1);

	debug_exit("vfolder_import");
}

static void vfolder_export(nodePtr node, xmlNodePtr cur, gboolean trusted) {
	vfolderPtr	vfolder = (vfolderPtr)node->data;
	xmlNodePtr	ruleNode;
	rulePtr		rule;
	GSList		*iter;

	debug_enter("vfolder_export");
	
	g_assert(TRUE == trusted);

	iter = vfolder->rules;
	while(iter) {
		rule = iter->data;
		ruleNode = xmlNewChild(cur, NULL, BAD_CAST"outline", NULL);
		xmlNewProp(ruleNode, BAD_CAST"type", BAD_CAST "rule");
		xmlNewProp(ruleNode, BAD_CAST"text", BAD_CAST rule->ruleInfo->title);
		xmlNewProp(ruleNode, BAD_CAST"rule", BAD_CAST rule->ruleInfo->ruleId);
		xmlNewProp(ruleNode, BAD_CAST"value", BAD_CAST rule->value);
		if(TRUE == rule->additive)
			xmlNewProp(ruleNode, BAD_CAST"additive", BAD_CAST "true");
		else
			xmlNewProp(ruleNode, BAD_CAST"additive", BAD_CAST "false");

		iter = g_slist_next(iter);
	}

	debug_exit("vfolder_export");
}

/* Method thats adds a rule to a vfolder. To be used
   on loading time or when creating searching. Does 
   not process items. Just sets up the vfolder */
void vfolder_add_rule(vfolderPtr vp, const gchar *ruleId, const gchar *value, gboolean additive) {
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
void vfolder_remove_rule(vfolderPtr vp, rulePtr rp) {

	debug_enter("vfolder_remove_rule");
	vp->rules = g_slist_remove(vp->rules, rp);
	debug_exit("vfolder_remove_rule");
}

/**
 * Adds an item to this VFolder, this method is called
 * when any rule of the vfolder matched 
 */
static void vfolder_add_item(vfolderPtr vp, itemPtr item) {
	GList		*iter;
	itemPtr		tmp;
	
	/* need to check for vfolder items because the
	   rule checking can be called on item copies of
	   the same vfolder, */
	if(item->itemSet->type == ITEMSET_TYPE_VFOLDER)
		return;
	
	/* check if the item was already added */
	g_assert(vp->node->itemSet);
	iter = vp->node->itemSet->items;
	while(iter) {
		tmp = iter->data;
		if((item->sourceNr == tmp->sourceNr) && 
		   (item->sourceNode == tmp->sourceNode))
			return;
		iter = g_list_next(iter);
	}

	/* add an item copy to the vfolder */	
	
	if(!item->readStatus)
		vp->node->unreadCount++;
	
	tmp = item_copy(item);
	itemset_prepend_item(vp->node->itemSet, tmp);
	itemlist_update_vfolder(vp);		/* update the itemlist if this vfolder is selected */
}

/** 
 * Searches a given vfolder for a copy of the passed item and
 * removes them. Used for item remove propagation and for 
 * processing of removing vfolder rules.
 */
static void vfolder_remove_matching_item_copy(vfolderPtr vp, itemPtr ip) {
	gboolean	found = FALSE;
	GList		*items;
	itemPtr		tmp;

	items = vp->node->itemSet->items;
	while(NULL != items) {
		tmp = items->data;
		g_assert(NULL != ip->itemSet);

		if((ip->nr == tmp->sourceNr) &&
		   (ip->itemSet->node == tmp->sourceNode)) {
			found = TRUE;
			break;
		}

		items = g_list_next(items);
	}

	if(found) {
		/*g_print("  removing item copy %d from vfolder %d\n", ip, vp);*/

		/* because itemlist_request_remove_item might delay the removal
		   the original item may not exist anymore when the 
		   removal is executed, so we need to remove the
		   pointer to the original item */
		tmp->sourceNode = tmp->itemSet->node;
		tmp->sourceNr = -1;
		
		if(!tmp->readStatus)
			vp->node->unreadCount--;
		
		/* we call itemlist_request_remove_item to prevent removing
		   an item copy selected in the GUI... */
		itemlist_request_remove_item(tmp);
	}
}

/** 
 * Searches all vfolders for copies of the given item and
 * removes them. Used for item remove propagation. 
 */
void vfolder_remove_item(itemPtr ip) {
	GSList		*iter;

	g_assert(ip->itemSet->type != ITEMSET_TYPE_VFOLDER);

	debug_enter("vfolder_remove_item");

	iter = vfolders;
	while(iter) {
		vfolder_remove_matching_item_copy((vfolderPtr)iter->data, ip);
		iter = g_slist_next(iter);
	}

	debug_exit("vfolder_remove_item");
}

/**
 * Check item against rules of a vfolder. When the item matches an 
 * additive rule it is added to the vfolder and when it matches an excluding 
 * rule the item is removed again from the given vfolder.
 */
static gboolean vfolder_apply_rules_for_item(vfolderPtr vp, itemPtr ip) {
	rulePtr		rp;
	GSList		*iter;
	gboolean	added = FALSE;

	/* check against all rules */
	/* debug2(DEBUG_UPDATE, "applying rules of (%s) to item #%d", feed_get_title(vp), ip->nr); */
	iter = vp->rules;
	while(NULL != iter) {
		rp = iter->data;
		if(rp->additive) {
			if(!added && rule_check_item(rp, ip)) {
				debug3(DEBUG_UPDATE, "adding matching item #%d (%s) to (%s)", ip->nr, item_get_title(ip), node_get_title(vp->node));
				vfolder_add_item(vp, ip);
				added = TRUE;
			}
		} else {
			if(added && rule_check_item(rp, ip)) {
				debug3(DEBUG_UPDATE, "deleting matching item #%d (%s) to (%s)", ip->nr, item_get_title(ip), node_get_title(vp->node));
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
static void vfolder_apply_rules(nodePtr node, gpointer userdata) {
	vfolderPtr	vp = (vfolderPtr)userdata;
	GList		*items;

	/* do not search in vfolders */
	if(NODE_TYPE_VFOLDER == node->type)
		return;
		
	if(NODE_TYPE_FOLDER != node->type) {
		debug_enter("vfolder_apply_rules");

		debug1(DEBUG_UPDATE, "applying rules for (%s)", node_get_title(node));
		node_load(node);

		/* check all node items */
		items = node->itemSet->items;
		while(NULL != items) {
			vfolder_apply_rules_for_item(vp, items->data);
			items = g_list_next(items);
		}
	
		node_unload(node);
	
		debug_exit("vfolder_apply_rules");
	} else  {
		/* Recursion */
		node_foreach_child_data(node, vfolder_apply_rules, vp);
	}
}

/* Method that applies the rules of the given vfolder to 
   all existing items. To be used for creating search
   results or new vfolders. Not to be used when loading
   vfolders from cache! */
void vfolder_refresh(vfolderPtr vfolder) {
	
	debug_enter("vfolder_refresh");
	
	itemset_remove_items(vfolder->node->itemSet);
	feedlist_foreach_data(vfolder_apply_rules, vfolder);
	
	debug_exit("vfolder_refresh");
}

/**
 * Method to be called when an item was updated. Maybe called
 * after user interaction or updated item contents.
 */
void vfolder_update_item(itemPtr ip) {
	GList		*items;
	GSList		*iter, *rule;
	itemPtr		tmp;
	vfolderPtr	vp;
	rulePtr		rp;
	gboolean	keep, remove;

	debug_enter("vfolder_update_item");

	/* never process vfolder items! */
	g_assert(ip->itemSet->type != ITEMSET_TYPE_VFOLDER);
	
	iter = vfolders;
	while(iter) {
		vp = (vfolderPtr)iter->data;
		
		/* first step: update item copy if found */
		items = vp->node->itemSet->items;
		while(NULL != items) {
			tmp = items->data;
			g_assert(NULL != ip->itemSet);
			
			/* avoid processing items that are in deletion state */
			if(-1 == tmp->sourceNr) {
				items = g_list_next(items);
				continue;
			}
			
			/* find the item copies */
			if((ip->nr == tmp->sourceNr) &&
			   (ip->itemSet->node == tmp->sourceNode)) {
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
				
				/* update vfolder unread count */
				if(tmp->readStatus != ip->readStatus) {
					if(ip->readStatus)
						vp->node->unreadCount--;
					else
						vp->node->unreadCount++;
				}
				
				/* always update the item... funny? Maybe, but necessary so that with 
				   deferred removal items have correct state until really removed */
				tmp->readStatus = ip->readStatus;
				tmp->updateStatus = ip->updateStatus;
				tmp->flagStatus = ip->flagStatus;

				if((TRUE == keep) && (FALSE == remove)) {
			   		debug2(DEBUG_UPDATE, "item (%s) used in vfolder (%s), updating vfolder copy...", item_get_title(ip), node_get_title(vp->node));
					itemlist_update_item(tmp);
				} else {
					debug2(DEBUG_UPDATE, "item (%s) used in vfolder (%s) does not match anymore -> removing...", item_get_title(ip), node_get_title(vp->node));
					itemlist_request_remove_item(tmp);
				}
				break;
			}
			items = g_list_next(items);
		}
		
		ui_node_update(vp->node);	/* update the feedlist */

		iter = g_slist_next(iter);
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
gboolean vfolder_check_item(itemPtr item) {
	GSList		*iter;
	gboolean	added = FALSE;

	debug_enter("vfolder_check_item");
	
	g_assert(item->itemSet->type == ITEMSET_TYPE_FEED);
	g_assert(item->itemSet->node == item->sourceNode);
	g_assert(item->nr == item->sourceNr);

	iter = vfolders;
	while(iter) {
		added |= vfolder_apply_rules_for_item(iter->data, item);
		iter = g_slist_next(iter);
	}
	
	debug_exit("vfolder_check_item");
	return added;
}

void vfolder_check_node(nodePtr node) {
	GList		*iter;

	iter = node->itemSet->items;
	while(iter) {
		vfolder_check_item((itemPtr)iter->data);
		iter = g_list_next(iter);
	}
}

/* called when a vfolder is processed by feed_free
   to get rid of the vfolder items */
void vfolder_free(vfolderPtr vp) {
	GList		*iter;
	GSList		*rule;

	debug_enter("vfolder_free");

	vfolders = g_slist_remove(vfolders, vp);

	/* free vfolder items */
	g_assert(NULL != vp->node->itemSet);
	iter = vp->node->itemSet->items;
	while(NULL != iter) {
		item_free(iter->data);
		iter = g_list_next(iter);
	}
	g_list_free(vp->node->itemSet->items);
	vp->node->itemSet->items = NULL;
	
	/* free vfolder rules */
	rule = vp->rules;
	while(NULL != rule) {
		rule_free(rule->data);
		rule = g_slist_next(rule);
	}
	g_slist_free(vp->rules);
	vp->rules = NULL;
	
	debug_exit("vfolder_free");
}

/* implementation of the node type interface */

static void vfolder_initial_load(nodePtr node) { }
static void vfolder_load(nodePtr node) { }
static void vfolder_save(nodePtr node) { }
static void vfolder_unload(nodePtr node) { }
static void vfolder_reset_update_counter(nodePtr node) { }
static void vfolder_request_update(nodePtr node, guint flags) { }
static void vfolder_request_auto_update(nodePtr node) { }

static void vfolder_remove(nodePtr node) {

	ui_node_remove_node(node);
	vfolder_free(node->data);
}

static void vfolder_mark_all_read(nodePtr node) {

	itemlist_mark_all_read(node->itemSet);
}

nodeTypePtr vfolder_get_node_type(void) { 

	static struct nodeType nti = {
		NODE_CAPABILITY_SHOW_UNREAD_COUNT |
		NODE_CAPABILITY_SHOW_ITEM_COUNT,
		"vfolder",
		NULL,
		NODE_TYPE_VFOLDER,
		vfolder_import,
		vfolder_export,
		vfolder_initial_load,
		vfolder_load,
		vfolder_save,
		vfolder_unload,
		vfolder_reset_update_counter,
		vfolder_request_update,
		vfolder_request_auto_update,
		vfolder_remove,
		vfolder_mark_all_read,
		node_default_render,
		ui_vfolder_add,
		ui_vfolder_properties
	};
	nti.icon = icons[ICON_VFOLDER];

	return &nti; 
}
