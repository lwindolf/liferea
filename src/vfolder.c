/**
 * @file vfolder.c  search folder node type
 *
 * Copyright (C) 2003-2020 Lars Windolf <lars.windolf@gmx.de>
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

#include "vfolder.h"

#include "common.h"
#include "db.h"
#include "debug.h"
#include "feedlist.h"
#include "itemset.h"
#include "itemlist.h"
#include "node.h"
#include "rule.h"
#include "vfolder_loader.h"
#include "ui/icons.h"
#include "ui/search_folder_dialog.h"

/** The list of all existing vfolders. Used for updating vfolder information upon item changes */
static GSList		*vfolders = NULL;

vfolderPtr
vfolder_new (nodePtr node)
{
	vfolderPtr	vfolder;

	debug_enter ("vfolder_new");

	vfolder = g_new0 (struct vfolder, 1);
	vfolder->itemset = g_new0 (struct itemSet, 1);
	vfolder->itemset->nodeId = node->id;
	vfolder->itemset->ids = NULL;
	vfolder->itemset->anyMatch = TRUE;
	vfolder->node = node;
	vfolders = g_slist_append (vfolders, vfolder);

	if (!node->title)
		node_set_title (node, _("New Search Folder"));	/* set default title */
	node_set_data (node, (gpointer) vfolder);

	debug_exit ("vfolder_new");

	return vfolder;
}

static void
vfolder_import_rules (xmlNodePtr cur,
                      vfolderPtr vfolder)
{
	xmlChar		*tmp, *type, *ruleId, *value, *additive;

	tmp = xmlGetProp (cur, BAD_CAST"matchType");
	if (tmp)
		/* currently we only OR or AND the rules,
		   "any" is the value for OR'ing, "all" for AND'ing */
		vfolder->itemset->anyMatch = (0 != xmlStrcmp (tmp, BAD_CAST"all"));
	else
		vfolder->itemset->anyMatch = TRUE;
	xmlFree (tmp);

	tmp = xmlGetProp (cur, BAD_CAST"unreadOnly");
	if (tmp)
		vfolder->unreadOnly = (0 == xmlStrcmp (tmp, BAD_CAST"true"));
	else
		vfolder->unreadOnly = FALSE;
	xmlFree (tmp);

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
						itemset_add_rule (vfolder->itemset, (gchar *)ruleId, (gchar *)value, TRUE);
					else
						itemset_add_rule (vfolder->itemset, (gchar *)ruleId, (gchar *)value, FALSE);
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
	return db_search_folder_load (node->id);
}

void
vfolder_foreach (nodeActionFunc func)
{
	GSList	*iter = vfolders;

	g_assert (NULL != func);
	while (iter) {
		vfolderPtr vfolder = (vfolderPtr)iter->data;
		(*func)(vfolder->node);
		iter = g_slist_next (iter);
	}
}

GSList *
vfolder_get_all_with_item_id (itemPtr item)
{
	GSList	*result = NULL;
	GSList	*iter = vfolders;

	while (iter) {
		vfolderPtr vfolder = (vfolderPtr)iter->data;
		if (itemset_check_item (vfolder->itemset, item))
			result = g_slist_append (result, vfolder);
		iter = g_slist_next (iter);
	}

	return result;
}

GSList *
vfolder_get_all_without_item_id (itemPtr item)
{
	GSList	*result = NULL;
	GSList	*iter = vfolders;

	while (iter) {
		vfolderPtr vfolder = (vfolderPtr)iter->data;
		if (!itemset_check_item (vfolder->itemset, item))
			result = g_slist_append (result, vfolder);
		iter = g_slist_next (iter);
	}

	return result;
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

	/* We use the itemset only to keep itemset rules, not to
	   have the items in memory! Maybe the itemset<->filtering
	   dependency is not a good idea... */

	vfolder_import_rules (cur, vfolder);
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

	xmlNewProp (cur, BAD_CAST"matchType", BAD_CAST (vfolder->itemset->anyMatch?"any":"all"));
	xmlNewProp (cur, BAD_CAST"unreadOnly", BAD_CAST (vfolder->unreadOnly?"true":"false"));

	iter = vfolder->itemset->rules;
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

void
vfolder_reset (vfolderPtr vfolder)
{
	itemlist_unload ();

	if (vfolder->loader) {
		g_object_unref (vfolder->loader);
		vfolder->loader = NULL;
	}

	g_list_free (vfolder->itemset->ids);
	vfolder->itemset->ids = NULL;
	db_search_folder_reset (vfolder->node->id);
}

void
vfolder_rebuild (nodePtr node)
{
	vfolderPtr	vfolder = (vfolderPtr)node->data;

	vfolder_reset (vfolder);
	vfolder->loader = vfolder_loader_new (node);
	itemlist_add_search_result (vfolder->loader);
}

static void
vfolder_free (nodePtr node)
{
	vfolderPtr	vfolder = (vfolderPtr) node->data;

	debug_enter ("vfolder_free");

	if (vfolder->loader) {
		g_object_unref (vfolder->loader);
		vfolder->loader = NULL;
	}

	vfolders = g_slist_remove (vfolders, vfolder);
	itemset_free (vfolder->itemset);

	debug_exit ("vfolder_free");
}

/* implementation of the node type interface */

static void vfolder_save (nodePtr node) { }

static void
vfolder_update_counters (nodePtr node)
{
	node->needsUpdate = TRUE;
	node->unreadCount = db_search_folder_get_unread_count (node->id);
	node->itemCount = db_search_folder_get_item_count (node->id);
}

static void
vfolder_remove (nodePtr node)
{
	vfolder_reset (node->data);
}

static void
vfolder_properties (nodePtr node)
{
	search_folder_dialog_new (node);
}

static gboolean
vfolder_add (void)
{
	nodePtr	node;

	node = node_new (vfolder_get_node_type ());
	vfolder_new (node);
	vfolder_properties (node);

	return TRUE;
}

nodeTypePtr
vfolder_get_node_type (void)
{
	static struct nodeType nti = {
		NODE_CAPABILITY_SHOW_ITEM_FAVICONS |
		NODE_CAPABILITY_SHOW_UNREAD_COUNT |
		NODE_CAPABILITY_EXPORT_ITEMS,
		"vfolder",
		ICON_VFOLDER,
		vfolder_import,
		vfolder_export,
		vfolder_load,
		vfolder_save,
		vfolder_update_counters,
		vfolder_remove,
		node_default_render,
		vfolder_add,
		vfolder_properties,
		vfolder_free
	};

	return &nti;
}
