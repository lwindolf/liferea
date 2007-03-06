/**
 * @file vfolder.c search folder functionality
 *
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.net>
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

	debug_enter("vfolder_new");

	vfolder = g_new0(struct vfolder, 1);
	vfolder->node = node;
	vfolders = g_slist_append(vfolders, vfolder);

	node_set_title(node, _("New Search Folder"));	/* set default title */
	node_set_type(node, vfolder_get_node_type());
	node_set_data(node, (gpointer)vfolder);

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

static itemSetPtr vfolder_load(nodePtr node) {

	return NULL;
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
	
	debug1(DEBUG_CACHE, "adding vfolder: title=%s", node_get_title(node));

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

/* called when a vfolder is processed by feed_free
   to get rid of the vfolder items */
void vfolder_free(vfolderPtr vp) {
	GSList		*rule;

	debug_enter("vfolder_free");

	// FIXME!
	
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

static void vfolder_save(nodePtr node) { }

static void vfolder_update_unread_count(nodePtr node) {

	node->unreadCount = 5;	// FIXME!
}

static void vfolder_reset_update_counter(nodePtr node) { }
static void vfolder_request_update(nodePtr node, guint flags) { }
static void vfolder_request_auto_update(nodePtr node) { }

static void vfolder_remove(nodePtr node) {

	ui_node_remove_node(node);
	vfolder_free(node->data);
}

static void vfolder_mark_all_read(nodePtr node) {

	itemlist_mark_all_read(node->id);
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
		vfolder_load,
		vfolder_save,
		vfolder_update_unread_count,
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
