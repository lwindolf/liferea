/**
 * @file vfolder.c search folder node type
 *
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.com>
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

#include "common.h"
#include "conf.h"
#include "db.h"
#include "debug.h"
#include "feedlist.h"
#include "itemset.h"
#include "itemlist.h"
#include "node.h"
#include "vfolder.h"
#include "ui/ui_node.h"
#include "ui/ui_vfolder.h"

/** The list of all existing vfolders. Used for updating vfolder information upon item changes */
static GSList		*vfolders = NULL;

vfolderPtr
vfolder_new (nodePtr node) 
{
	vfolderPtr	vfolder;

	debug_enter ("vfolder_new");

	vfolder = g_new0 (struct vfolder, 1);
	vfolder->node = node;
	vfolders = g_slist_append (vfolders, vfolder);

	if (!node->title)
		node_set_title (node, _("New Search Folder"));	/* set default title */
	node_set_type (node, vfolder_get_node_type());
	node_set_data (node, (gpointer) vfolder);

	debug_exit ("vfolder_new");
	
	return vfolder;
}

static void
vfolder_import_rules (xmlNodePtr cur,
                      vfolderPtr vfolder)
{
	xmlChar		*type, *ruleId, *value, *additive;
	
	/* process any children */
	cur = cur->xmlChildrenNode;
	while (cur) {
		if (!xmlStrcmp (cur->name, BAD_CAST"outline")) {
			type = xmlGetProp (cur, BAD_CAST"type");
			if (type && !xmlStrcmp (type, BAD_CAST"rule")) {

				ruleId = xmlGetProp (cur, BAD_CAST"rule");
				value = xmlGetProp (cur, BAD_CAST"value");
				additive = xmlGetProp (cur, BAD_CAST"additive");

				if (ruleId && value) {			
					debug2 (DEBUG_CACHE, "loading rule \"%s\" \"%s\"", ruleId, value);

					if (additive && !xmlStrcmp (additive, BAD_CAST"true"))
						vfolder_add_rule (vfolder, ruleId, value, TRUE);
					else
						vfolder_add_rule (vfolder, ruleId, value, FALSE);
				} else {
					g_warning ("ignoring invalid rule entry for vfolder \"%s\"...\n", node_get_title (vfolder->node));
				}
				
				xmlFree (ruleId);
				xmlFree (value);
				xmlFree (additive);
			}
			xmlFree (type);
		}
		cur = cur->next;
	}
}

static itemSetPtr
vfolder_load (nodePtr node) 
{
	return db_view_load (node->id);
}

void
vfolder_update_counters (nodePtr node) 
{
	node->unreadCount = db_view_get_unread_count (node->id);
	node->itemCount = db_view_get_item_count (node->id);
	ui_node_update (node->id);
}

void
vfolder_refresh (vfolderPtr vfolder)
{
	g_return_if_fail (NULL != vfolder->node);

	if (0 != g_slist_length (vfolder->rules))
		rules_to_view (vfolder->rules, vfolder->node->id);
	
	vfolder_update_counters (vfolder->node);
}

static gboolean
vfolder_is_affected (vfolderPtr vfolder, const gchar *ruleName)
{
	GSList *iter = vfolder->rules;
	while (iter) {
		rulePtr rule = (rulePtr)iter->data;
		if (g_str_equal (rule->ruleInfo->ruleId, ruleName))
			return TRUE;
		iter = g_slist_next(iter);
	}
	return FALSE;
}

void
vfolder_foreach (nodeActionFunc func)
{
	vfolder_foreach_with_rule (NULL, func);
}

void
vfolder_foreach_with_rule (const gchar *ruleName, nodeActionFunc func) 
{
	GSList	*iter = vfolders;
	
	g_assert (NULL != func);
	while (iter) {
		vfolderPtr vfolder = (vfolderPtr)iter->data;
		if (!ruleName || vfolder_is_affected (vfolder, ruleName))
			(*func) (vfolder->node);
		iter = g_slist_next (iter);
	}
}

void
vfolder_foreach_with_item (gulong itemId, nodeActionFunc func)
{
	GSList	*iter = vfolders;
	
	while (iter) {
		vfolderPtr vfolder = (vfolderPtr)iter->data;
		if (db_view_contains_item (vfolder->node->id, itemId))
			(*func) (vfolder->node);	
		iter = g_slist_next (iter);
	}
}

static void
vfolder_import (nodePtr node,
                nodePtr parent,
                xmlNodePtr cur,
                gboolean trusted) 
{
	vfolderPtr vfolder;

	debug1 (DEBUG_CACHE, "import vfolder: title=%s", node_get_title (node));

	vfolder = vfolder_new (node);
	vfolder_import_rules (cur, vfolder);
	node_add_child (parent, node, -1);
	vfolder_refresh (vfolder);
}

static void
vfolder_export (nodePtr node,
                xmlNodePtr cur,
                gboolean trusted)
{
	vfolderPtr	vfolder = (vfolderPtr) node->data;
	xmlNodePtr	ruleNode;
	rulePtr		rule;
	GSList		*iter;

	debug_enter ("vfolder_export");
	
	g_assert (TRUE == trusted);

	iter = vfolder->rules;
	while (iter) {
		rule = iter->data;
		ruleNode = xmlNewChild (cur, NULL, BAD_CAST"outline", NULL);
		xmlNewProp (ruleNode, BAD_CAST"type", BAD_CAST "rule");
		xmlNewProp (ruleNode, BAD_CAST"text", BAD_CAST rule->ruleInfo->title);
		xmlNewProp (ruleNode, BAD_CAST"rule", BAD_CAST rule->ruleInfo->ruleId);
		xmlNewProp (ruleNode, BAD_CAST"value", BAD_CAST rule->value);
		if (rule->additive)
			xmlNewProp (ruleNode, BAD_CAST"additive", BAD_CAST "true");
		else
			xmlNewProp (ruleNode, BAD_CAST"additive", BAD_CAST "false");

		iter = g_slist_next (iter);
	}
	
	debug1 (DEBUG_CACHE, "adding vfolder: title=%s", node_get_title (node));

	debug_exit ("vfolder_export");
}

/* Method thats adds a rule to a vfolder. To be used
   on loading time or when creating searching. Does 
   not process items. Just sets up the vfolder */
void
vfolder_add_rule (vfolderPtr vfolder,
                  const gchar *ruleId,
                  const gchar *value,
                  gboolean additive)
{
	rulePtr		rule;
	
	rule = rule_new (vfolder, ruleId, value, additive);
	if(rule)
		vfolder->rules = g_slist_append(vfolder->rules, rule);
	else
		g_warning("unknown search folder rule id: \"%s\"", ruleId);
}

/* Method that remove a rule from a vfolder. To be used
   when deleting or changing vfolders. Does not process
   items. */
void
vfolder_remove_rule (vfolderPtr vfolder, rulePtr rule) 
{
	vfolder->rules = g_slist_remove (vfolder->rules, rule);
}

static void
vfolder_free (nodePtr node) 
{
	vfolderPtr	vfolder = (vfolderPtr) node->data;
	GSList		*rule;

	debug_enter ("vfolder_free");
	
	vfolders = g_slist_remove (vfolders, vfolder);
	
	/* free vfolder rules */
	rule = vfolder->rules;
	while (rule) {
		rule_free (rule->data);
		rule = g_slist_next (rule);
	}
	g_slist_free (vfolder->rules);
	vfolder->rules = NULL;
	
	debug_exit ("vfolder_free");
}

/* implementation of the node type interface */

static void vfolder_save (nodePtr node) { }

static void
vfolder_update_unread_count (nodePtr node) 
{
	g_warning("Should never be called!");
}

static void
vfolder_remove (nodePtr node) 
{
	ui_node_remove_node (node);
	vfolder_free (node);
}

nodeTypePtr
vfolder_get_node_type (void)
{ 
	static struct nodeType nti = {
		NODE_CAPABILITY_SHOW_UNREAD_COUNT |
		NODE_CAPABILITY_SHOW_ITEM_COUNT,
		"vfolder",
		NULL,
		vfolder_import,
		vfolder_export,
		vfolder_load,
		vfolder_save,
		vfolder_update_unread_count,
		NULL,			/* process_update_result() */
		vfolder_remove,
		node_default_render,
		ui_vfolder_add,
		ui_vfolder_properties,
		vfolder_free
	};
	nti.icon = icons[ICON_VFOLDER];

	return &nti; 
}
