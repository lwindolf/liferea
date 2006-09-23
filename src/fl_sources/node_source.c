/**
 * @file node_source.c generic feedlist provider implementation
 * 
 * Copyright (C) 2005-2006 Lars Lindner <lars.lindner@gmx.net>
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

#include <gmodule.h>
#include <gtk/gtk.h>
#include <string.h>
#include "callbacks.h"
#include "common.h"
#include "debug.h"
#include "node.h"
#include "folder.h"
#include "plugin.h"
#include "render.h"
#include "support.h"
#include "fl_sources/node_source.h"
#include "fl_sources/node_source-ui.h"
#include "notification/notif_plugin.h"

static GSList	*nodeSourceTypes = NULL;

static nodeSourceTypePtr node_source_type_find(gchar *typeStr, guint capabilities) {
	GSList *iter = nodeSourceTypes;
	
	while(iter) {
		nodeSourceTypePtr type = (nodeSourceTypePtr)iter->data;
		if(((NULL == typeStr) || !strcmp(type->id, typeStr)) &&
		   ((0 == capabilities) || (type->capabilities & capabilities)))
			return type;
		iter = g_slist_next(iter);
	}
	
	g_warning("Could not find source type \"%s\"\n!", typeStr);
	return NULL;
}

nodePtr node_source_setup_root(void) {
	nodePtr	rootNode;
	nodeSourceTypePtr type;
	
	debug_enter("node_source_setup_root");

	type = node_source_type_find(NULL, NODE_SOURCE_CAPABILITY_IS_ROOT);
	if(!type) 
		g_error("No root capable node source found!");
		
	rootNode = node_new();
	node_set_type(rootNode, root_get_node_type());
	rootNode->title = g_strdup("root");
	rootNode->source = g_new0(struct nodeSource, 1);
	rootNode->source->root = rootNode;
	rootNode->source->type = type;
	type->source_import(rootNode);
	
	debug_exit("node_source_setup_root");
	
	return rootNode;
}

gboolean node_source_type_register(nodeSourceTypePtr type) {

	/* check feed list provider plugin version */
	if(NODE_SOURCE_TYPE_API_VERSION != type->api_version) {
		debug3(DEBUG_PLUGINS, "feed list source API version mismatch: \"%s\" has version %d should be %d\n", type->name, type->api_version, NODE_SOURCE_TYPE_API_VERSION);
		return FALSE;
	} 

	/* check if all mandatory functions are provided */
	if(!(type->source_type_init &&
	     type->source_type_deinit)) {
		debug1(DEBUG_PLUGINS, "mandatory functions missing: \"%s\"\n", type->name);
		return FALSE;
	}

	/* allow the plugin to initialize */
	type->source_type_init();

	nodeSourceTypes = g_slist_append(nodeSourceTypes, type);
	
	return TRUE;
}

static void node_source_import(nodePtr node, nodePtr parent, xmlNodePtr cur, gboolean trusted) {
	nodeSourceTypePtr	type;
	xmlChar			*typeStr = NULL;

	debug_enter("node_source_import");

	typeStr = xmlGetProp(cur, BAD_CAST"sourceType");
	if(!typeStr)
		typeStr = xmlGetProp(cur, BAD_CAST"pluginType"); /* for migration only */

	if(typeStr) {
		debug2(DEBUG_CACHE, "creating feed list plugin instance (type=%s,id=%s)\n", typeStr, node->id);

		node_add_child(parent, node, -1);
		
		node->available = FALSE;

		/* scan for matching plugin and create new instance */
		type = node_source_type_find(typeStr, 0);
		
		if(NULL == type) {
			/* Source type is not available for some reason, but
			   we need a representation to keep the node source
			   in the feed list. So we load a dummy source type
			   instead and save the real source id in the
			   unused node's data field */
			type = node_source_type_find(NODE_SOURCE_TYPE_DUMMY_ID, 0);
			g_assert(NULL != type);
			node->data = g_strdup(typeStr);
		}
		
		node->type = NODE_TYPE_SOURCE;
		node->available = TRUE;
		node->source = g_new0(struct nodeSource, 1);
		node->source->updateOptions = g_new0(struct updateOptions, 1);
		node->source->updateState = g_new0(struct updateState, 1);
		node->source->root = node;
		node->source->type = type;
		node->source->url = xmlGetProp(cur, BAD_CAST"xmlUrl");
		
		update_state_import(cur, node->source->updateState);
				
		type->source_import(node);	// FIXME: pass trusted flag?
	} else {
		g_warning("No source type given for node \"%s\"", node_get_title(node));
	}	

	debug_exit("node_source_import");
}

static void node_source_export(nodePtr node, xmlNodePtr cur, gboolean trusted) {

	debug_enter("node_source_export");

	debug2(DEBUG_CACHE, "node source export for node %s, id=%s\n", node->title, NODE_SOURCE_TYPE(node)->id);
	if(!strcmp(NODE_SOURCE_TYPE(node)->id, NODE_SOURCE_TYPE_DUMMY_ID))
		xmlNewProp(cur, BAD_CAST"sourceType", BAD_CAST(node->data));
	else
		xmlNewProp(cur, BAD_CAST"sourceType", BAD_CAST(NODE_SOURCE_TYPE(node)->id));
		
	if(node->source->url)
		xmlNewProp(cur, BAD_CAST"xmlUrl", node->source->url);
		
	update_state_export(cur, node->source->updateState);

	debug_exit("node_source_export");
}

void node_source_new(nodePtr node, nodeSourceTypePtr type, const gchar *sourceUrl) {

	g_assert(NULL == node->source);
	node->source = g_new0(struct nodeSource, 1);
	node->source->root = node;
	node->source->type = type;
	node->source->url = g_strdup(sourceUrl);
	node->source->updateOptions = g_new0(struct updateOptions, 1);
	node->source->updateState = g_new0(struct updateState, 1);
}

/* source instance creation dialog */

static void on_node_source_type_selected(GtkDialog *dialog, gint response_id, gpointer user_data) {
	GtkTreeSelection	*selection;
	GtkTreeModel		*model;
	GtkTreeIter		iter;
	nodePtr 		parent = (nodePtr)user_data;
	nodeSourceTypePtr	type;
	
	if(response_id == GTK_RESPONSE_OK) {
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(lookup_widget(GTK_WIDGET(dialog), "type_list")));
		g_assert(NULL != selection);
		gtk_tree_selection_get_selected(selection, &model, &iter);
		gtk_tree_model_get(model, &iter, 1, &type, -1);
		type->source_new(parent);
	}
	
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

void ui_node_source_type_dialog(nodePtr parent) {
	GSList 			*iter = nodeSourceTypes;
	GtkWidget 		*dialog, *treeview;
	GtkTreeStore		*treestore;
	GtkCellRenderer		*renderer;
	GtkTreeViewColumn 	*column;
	GtkTreeIter		treeiter;
	nodeSourceTypePtr	type;

	if(!nodeSourceTypes) {
		ui_show_error_box(_("No feed list source types found!"));
		return;
	}		

	/* set up the dialog */
	dialog = create_node_source_type_dialog();

	treestore = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_POINTER);
	
	/* add available feed list source to treestore */
	while(iter) {
		type = (nodeSourceTypePtr)iter->data;
		if(type->capabilities & NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION) {

			gtk_tree_store_append(treestore, &treeiter, NULL);
			gtk_tree_store_set(treestore, &treeiter, 
			                              0, type->name, 
			                              1, type,
						      -1);
		}
		iter = g_slist_next(iter);
	}

	treeview = lookup_widget(dialog, "type_list");
	g_assert(NULL != treeview);

	column = gtk_tree_view_column_new();
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), -1, _("Source Type"), renderer, "text", 0, NULL);
	gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(treestore));
	g_object_unref(treestore);

	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)),
	                            GTK_SELECTION_SINGLE);

	g_signal_connect(G_OBJECT(dialog), "response",
			 G_CALLBACK(on_node_source_type_selected), 
			 (gpointer)parent);

	gtk_widget_show_all(dialog);
}

/* implementation of the node type interface */

static void node_source_request_update(nodePtr node, guint flags) {

	if(NULL != NODE_SOURCE_TYPE(node)->source_update)
		NODE_SOURCE_TYPE(node)->source_update(node);
}

static void node_source_request_auto_update(nodePtr node) {

	if(NULL != NODE_SOURCE_TYPE(node)->source_auto_update)
		NODE_SOURCE_TYPE(node)->source_auto_update(node);
}

static void node_source_remove(nodePtr node) {

	if(NULL != NODE_SOURCE_TYPE(node)->source_delete)
		NODE_SOURCE_TYPE(node)->source_delete(node);
		
	notification_node_removed(node);
	ui_node_remove_node(node);
}

static gchar * node_source_render(nodePtr node) {
	gchar	*result, *filename, **params = NULL;

	params = render_add_parameter(params, "headlineCount='%d'", g_list_length(node->itemSet->items));
	filename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "plugins", node->id, "opml");
	result = render_file(filename, "fl_plugin", params);
	g_free(filename);
	
	return result;
}

static void ui_node_source_dummy_properties(nodePtr node) {

	g_warning("Not supported!");
}

static void node_source_save(nodePtr node) {

	node_foreach_child(node, node_save);
}

nodeTypePtr node_source_get_node_type(void) {
	static nodeTypePtr	nodeType;

	if(!nodeType) {
		/* derive the plugin node type from the folder node type */
		nodeType = (nodeTypePtr)g_new0(struct nodeType, 1);
		nodeType->id			= "source";
		nodeType->icon			= icons[ICON_DEFAULT];
		nodeType->type			= NODE_TYPE_SOURCE;
		nodeType->import		= node_source_import;
		nodeType->export		= node_source_export;
		nodeType->initial_load		= folder_get_node_type()->initial_load;
		nodeType->load			= folder_get_node_type()->load;
		nodeType->save			= node_source_save;
		nodeType->unload		= folder_get_node_type()->unload;
		nodeType->reset_update_counter	= folder_get_node_type()->reset_update_counter;
		nodeType->request_update	= node_source_request_update;
		nodeType->request_auto_update	= node_source_request_auto_update;
		nodeType->remove		= node_source_remove;
		nodeType->mark_all_read		= folder_get_node_type()->mark_all_read;
		nodeType->render		= node_source_render;
		nodeType->request_add		= ui_node_source_type_dialog;
		nodeType->request_properties	= ui_node_source_dummy_properties;
	}

	return nodeType; 
}
