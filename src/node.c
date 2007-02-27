/**
 * @file node.c common feed list node handling
 * 
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.com>
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
 
#include <string.h>

#include "node.h"
#include "db.h"
#include "common.h"
#include "conf.h"
#include "callbacks.h"
#include "debug.h"
#include "favicon.h"
#include "feed.h"
#include "feedlist.h"
#include "folder.h"
#include "itemset.h"
#include "render.h"
#include "support.h"
#include "update.h"
#include "vfolder.h"
#include "fl_sources/node_source.h"

static GHashTable *nodes = NULL;
static GSList *nodeTypes = NULL;

void node_type_register(nodeTypePtr nodeType) {

	/* all attributes and methods are mandatory! */
	g_assert(nodeType->id);
	g_assert(0 != nodeType->type);
	g_assert(nodeType->import);
	g_assert(nodeType->export);
	g_assert(nodeType->save);
	g_assert(nodeType->reset_update_counter);
	g_assert(nodeType->request_update);
	g_assert(nodeType->request_auto_update);
	g_assert(nodeType->remove);
	g_assert(nodeType->mark_all_read);
	g_assert(nodeType->render);
	g_assert(nodeType->request_add);
	g_assert(nodeType->request_properties);
	
	nodeTypes = g_slist_append(nodeTypes, (gpointer)nodeType);
}

/* returns a unique node id */
gchar * node_new_id() {
	int		i;
	gchar		*id, *filename;
	gboolean	already_used;
	
	id = g_new0(gchar, 10);
	do {
		for(i=0;i<7;i++)
			id[i] = (char)g_random_int_range('a', 'z');
		id[7] = '\0';
		
		filename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "feeds", id, NULL);
		already_used = g_file_test(filename, G_FILE_TEST_EXISTS);
		g_free(filename);
	} while(already_used);
	
	return id;
}

nodePtr node_from_id(const gchar *id) {

	g_assert(NULL != nodes);
	if(!g_hash_table_lookup(nodes, id))
		g_warning("Fatal: no node with id %s found!", id);
	return (nodePtr)g_hash_table_lookup(nodes, id);
}

nodePtr node_new(void) {
	nodePtr	node;
	gchar	*id;

	node = (nodePtr)g_new0(struct node, 1);
	node->sortColumn = IS_TIME;
	node->sortReversed = TRUE;	/* default sorting is newest date at top */
	node->available = TRUE;
	node->type = NODE_TYPE_INVALID;
	node_set_icon(node, NULL);	/* initialize favicon file name */

	id = node_new_id();
	node_set_id(node, id);
	g_free(id);
	
	return node;
}

static nodeTypePtr node_get_type_impl(guint type) {
	GSList	*iter = nodeTypes;
	
	while(iter) {
		if(type == ((nodeTypePtr)iter->data)->type)
			return iter->data;		
		iter = g_slist_next(iter);
	}
	
	g_error("node_get_type_impl(): fatal unknown node type %u", type);
	return NULL;
}

void node_set_type(nodePtr node, nodeTypePtr type) {

	node->nodeType = type;
	node->type = type?type->type:NODE_TYPE_INVALID;
}

void node_set_data(nodePtr node, gpointer data) {

	g_assert(NULL == node->data);
	g_assert(NULL != node->nodeType);

	node->data = data;
}

gboolean node_is_ancestor(nodePtr node1, nodePtr node2) {
	nodePtr	tmp;

	tmp = node2->parent;
	while(tmp) {
		if(node1 == tmp)
			return TRUE;
		tmp = tmp->parent;
	}
	return FALSE;
}

void node_free(nodePtr node) {

	g_assert(NULL == node->children);
	
	g_hash_table_remove(nodes, node);
	
	update_cancel_requests((gpointer)node);

	db_itemset_remove_all(node->id);

	if(node->icon)
		g_object_unref(node->icon);
	g_free(node->iconFile);
	g_free(node->title);
	g_free(node->id);
	g_free(node);
}

void node_update_new_count(nodePtr node, gint diff) {

	node->newCount += diff;

	/* vfolder new counts are not interesting
	   in the following propagation handling */
	if(NODE_TYPE_VFOLDER == node->type)
		return;

	/* no parent node propagation necessary */

	/* update global feed list statistic */
	if(NODE_TYPE_FEED == node->type)
		feedlist_update_counters(0, diff);	
}

guint node_get_unread_count(nodePtr node) {

	// FIXME: what about folders? recursion!
	return db_itemset_get_unread_count(node->id);
}

/* generic node item set merging functions */

void node_update_favicon(nodePtr node) {

	if(NODE_TYPE_FEED == node->type) {
		debug1(DEBUG_UPDATE, "favicon of node %s needs to be updated...", node->title);
		feed_update_favicon(node);
	}
	
	/* Recursion */
	if(node->children)
		node_foreach_child(node, node_update_favicon);
}

/* plugin and import callbacks and helper functions */

const gchar *node_type_to_str(nodePtr node) {

	switch(node->type) {
		case NODE_TYPE_FEED:
			g_assert(NULL != node->data);
			return feed_type_fhp_to_str(((feedPtr)(node->data))->fhp);
			break;
		default:
			return NODE_TYPE(node)->id;
			break;
	}

	return NULL;
}

nodeTypePtr node_str_to_type(const gchar *str) {
	GSList	*iter = nodeTypes;

	g_assert(NULL != str);

	if(g_str_equal(str, ""))	/* type maybe "" if initial download is not yet done */
		return feed_get_node_type();

	if(NULL != feed_type_str_to_fhp(str))
		return feed_get_node_type();
		
	/* check against all node types */
	while(iter) {
		if(g_str_equal(str, ((nodeTypePtr)iter->data)->id))
			return (nodeTypePtr)iter->data;
		iter = g_slist_next(iter);
	}

	return NULL;
}

void node_add_child(nodePtr parent, nodePtr node, gint position) {

	if(!parent)
		parent = ui_feedlist_get_target_folder(&position);	

	parent->children = g_slist_insert(parent->children, node, position);
	node->parent = parent;
	
	/* new node may be provided by another feed list handler, if 
	   not they are handled by the parents handler */
	if(!node->source)
		node->source = parent->source;
	
	ui_node_add(parent, node, position);	
	ui_node_update(node);
}

/* To be called by node type implementations to add nodes */
void node_add(nodePtr node, nodePtr parent, gint pos, guint flags) {

	debug1(DEBUG_GUI, "new node will be added to folder \"%s\"", node_get_title(parent));

	ui_feedlist_get_target_folder(&pos);

	node_add_child(parent, node, pos);
	node_request_update(node, flags);
}

/* Interactive node adding (e.g. feed menu->new subscription) */
void node_request_interactive_add(guint type) {
	nodeTypePtr	nodeType;
	nodePtr		parent;

	parent = feedlist_get_insertion_point();

	if(0 == (NODE_TYPE(parent->source->root)->capabilities & NODE_CAPABILITY_ADD_CHILDS))
		return;

	nodeType = node_get_type_impl(type);
	nodeType->request_add(parent);
}

/* Automatic subscription adding (e.g. URL DnD), creates a new feed node
   without any user interaction. */
void node_request_automatic_add(const gchar *source, const gchar *title, const gchar *filter, updateOptionsPtr options, gint flags) {
	nodePtr		node, parent;
	gint		pos;

	g_assert(NULL != source);

	parent = feedlist_get_insertion_point();

	if(0 == (NODE_SOURCE_TYPE(parent->source->root)->capabilities & NODE_CAPABILITY_ADD_CHILDS))
		return;

	node = node_new();
	node_set_type(node,feed_get_node_type());
	node_set_title(node, title?title:_("New Subscription"));
	node_set_data(node, feed_new(source, filter, options));

	ui_feedlist_get_target_folder(&pos);
	node_add_child(parent, node, pos);
	node_request_update(node, flags);
	feedlist_schedule_save();
	ui_feedlist_select(node);
}

void node_request_remove(nodePtr node) {

	node_remove(node);

	node->parent->children = g_slist_remove(node->parent->children, node);

	node_free(node);
}

static xmlDocPtr node_to_xml(nodePtr node) {
	xmlDocPtr	doc;
	xmlNodePtr	rootNode;
	gchar		*tmp;
	
	doc = xmlNewDoc("1.0");
	rootNode = xmlDocGetRootElement(doc);
	rootNode = xmlNewDocNode(doc, NULL, "node", NULL);
	xmlDocSetRootElement(doc, rootNode);

	xmlNewTextChild(rootNode, NULL, "title", node_get_title(node));
	
	tmp = g_strdup_printf("%u", node_get_unread_count(node));
	xmlNewTextChild(rootNode, NULL, "unreadCount", tmp);
	g_free(tmp);
	
	tmp = g_strdup_printf("%u", g_slist_length(node->children));
	xmlNewTextChild(rootNode, NULL, "children", tmp);
	g_free(tmp);
	
	return doc;
}

gchar * node_default_render(nodePtr node) {
	gchar		*result;
	xmlDocPtr	doc;

	doc = node_to_xml(node);
	result = render_xml(doc, NODE_TYPE(node)->id, NULL);	
	xmlFreeDoc(doc);
		
	return result;
}

/* wrapper for node type interface */

void node_import(nodePtr node, nodePtr parent, xmlNodePtr cur, gboolean trusted) {
	NODE_TYPE(node)->import(node, parent, cur, trusted);
}

void node_export(nodePtr node, xmlNodePtr cur, gboolean trusted) {
	NODE_TYPE(node)->export(node, cur, trusted);
}

itemSetPtr node_get_itemset(nodePtr node) {
	
	return NODE_TYPE(node)->load(node);
}

void node_save(nodePtr node) {
	NODE_TYPE(node)->save(node);
}

void node_remove(nodePtr node) {
	NODE_TYPE(node)->remove(node);
}

void node_mark_all_read(nodePtr node) {

	if(0 != node_get_unread_count(node))
		NODE_TYPE(node)->mark_all_read(node);
}

gchar * node_render(nodePtr node) {
	return NODE_TYPE(node)->render(node);
}

void node_reset_update_counter(nodePtr node) {
	NODE_TYPE(node)->reset_update_counter(node);
}

void node_request_auto_update(nodePtr node) {
	NODE_TYPE(node)->request_auto_update(node);
}

void node_request_update(nodePtr node, guint flags) {
	NODE_TYPE(node)->request_update(node, flags);
}

void node_request_properties(nodePtr node) {
	NODE_TYPE(node)->request_properties(node);
}

/* node attributes encapsulation */

void node_set_title(nodePtr node, const gchar *title) {

	g_free(node->title);
	node->title = g_strdup(title);
}

const gchar * node_get_title(nodePtr node) { return node->title; }

void node_set_icon(nodePtr node, gpointer icon) {

	if(node->icon) 
		g_object_unref(node->icon);
	node->icon = icon;

	if(node->icon)
		node->iconFile = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "favicons", node->id, "png");
	else
		node->iconFile = g_strdup(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S "default.png");
}

/** determines the nodes favicon or default icon */
gpointer node_get_icon(nodePtr node) { 
	gpointer icon;

	icon = node->icon;

	if(!icon)
		icon = NODE_TYPE(node)->icon;

	if(!node->available)
		icon = icons[ICON_UNAVAILABLE];

	return icon;
}

const gchar * node_get_favicon_file(nodePtr node) { return node->iconFile; }

void node_set_id(nodePtr node, const gchar *id) {

	if(!nodes)
		nodes = g_hash_table_new(g_str_hash, g_str_equal);

	if(node->id) {
		g_hash_table_remove(nodes, node->id);
		g_free(node->id);
	}
	node->id = g_strdup(id);
	
	g_hash_table_insert(nodes, node->id, node);
}

const gchar *node_get_id(nodePtr node) { return node->id; }

void node_set_sort_column(nodePtr node, gint sortColumn, gboolean reversed) {

	node->sortColumn = sortColumn;
	node->sortReversed = reversed;
	feedlist_schedule_save();
}

void node_set_view_mode(nodePtr node, guint newMode) { node->viewMode = newMode; }

gboolean node_get_view_mode(nodePtr node) { return node->viewMode; }

gboolean node_load_link_preferred(nodePtr node) {

	switch(node->type) {
		case ITEMSET_TYPE_FEED:
			return ((feedPtr)node->data)->loadItemLink;
			break;
		default:
			return FALSE;
			break;
	}
}

const gchar * node_get_base_url(nodePtr node) {
	const gchar 	*baseUrl = NULL;

	switch(node->type) {
		case NODE_TYPE_FEED:
			baseUrl = feed_get_html_url((feedPtr)node->data);
			break;
		default:
			break;
	}

	/* prevent feed scraping commands to end up as base URI */
	if(!((baseUrl != NULL) &&
	     (baseUrl[0] != '|') &&
	     (strstr(baseUrl, "://") != NULL)))
	   	baseUrl = NULL;

	return baseUrl;
}

/* node children iterating interface */

void node_foreach_child_full(nodePtr node, gpointer func, gint params, gpointer user_data) {
	GSList		*children, *iter;
	
	g_assert(NULL != node);

	/* We need to copy because func might modify the list */
	iter = children = g_slist_copy(node->children);
	while(iter) {
		nodePtr childNode = (nodePtr)iter->data;
		
		/* Apply the method to the child */
		if(0 == params)
			((nodeActionFunc)func)(childNode);
		else 
			((nodeActionDataFunc)func)(childNode, user_data);
			
		/* Never descend! */

		iter = g_slist_next(iter);
	}
	
	g_slist_free(children);
}

