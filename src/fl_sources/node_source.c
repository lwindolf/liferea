/**
 * @file node_source.c  generic node source provider implementation
 * 
 * Copyright (C) 2005-2009 Lars Lindner <lars.lindner@gmail.com>
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

#include "fl_sources/node_source.h"

#include <gmodule.h>
#include <gtk/gtk.h>
#include <string.h>

#include "common.h"
#include "db.h"
#include "debug.h"
#include "feedlist.h"
#include "folder.h"
#include "node.h"
#include "node_type.h"
#include "ui/liferea_dialog.h"
#include "ui/ui_common.h"
#include "ui/ui_node.h"
#include "fl_sources/bloglines_source.h"
#include "fl_sources/default_source.h"
#include "fl_sources/dummy_source.h"
#include "fl_sources/google_source.h"
#include "fl_sources/opml_source.h"
#include "notification/notification.h"

static GSList	*nodeSourceTypes = NULL;

static nodeSourceTypePtr
node_source_type_find (gchar *typeStr, guint capabilities)
{
	GSList *iter = nodeSourceTypes;
	
	while (iter) {
		nodeSourceTypePtr type = (nodeSourceTypePtr)iter->data;
		if (((NULL == typeStr) || !strcmp(type->id, typeStr)) &&
		    ((0 == capabilities) || (type->capabilities & capabilities)))
			return type;
		iter = g_slist_next (iter);
	}
	
	g_warning ("Could not find source type \"%s\"\n!", typeStr);
	return NULL;
}

static gboolean
node_source_type_register (nodeSourceTypePtr type)
{
	/* check feed list provider plugin version */
	if (NODE_SOURCE_TYPE_API_VERSION != type->api_version) {
		debug3 (DEBUG_PLUGINS, "feed list source API version mismatch: \"%s\" has version %d should be %d", type->name, type->api_version, NODE_SOURCE_TYPE_API_VERSION);
		return FALSE;
	} 

	/* check if all mandatory functions are provided */
	if (!(type->source_type_init &&
	      type->source_type_deinit)) {
		debug1 (DEBUG_PLUGINS, "mandatory functions missing: \"%s\"", type->name);
		return FALSE;
	}

	/* allow the plugin to initialize */
	type->source_type_init ();

	nodeSourceTypes = g_slist_append (nodeSourceTypes, type);
	
	return TRUE;
}

nodePtr
node_source_setup_root (void)
{
	nodePtr	rootNode;
	nodeSourceTypePtr type;
	
	debug_enter ("node_source_setup_root");
	
	/* we need to register all source types once before doing anything... */
	node_source_type_register (default_source_get_type ());
	node_source_type_register (dummy_source_get_type ());
	node_source_type_register (opml_source_get_type ());
	node_source_type_register (bloglines_source_get_type ());
	node_source_type_register (google_source_get_type ());

	type = node_source_type_find (NULL, NODE_SOURCE_CAPABILITY_IS_ROOT);
	if (!type) 
		g_error ("No root capable node source found!");
		
	rootNode = node_new (root_get_node_type());
	rootNode->title = g_strdup ("root");
	rootNode->source = g_new0 (struct nodeSource, 1);
	rootNode->source->root = rootNode;
	rootNode->source->type = type;
	type->source_import (rootNode);

	debug_exit ("node_source_setup_root");
	
	return rootNode;
}

static void
node_source_import (nodePtr node, nodePtr parent, xmlNodePtr xml, gboolean trusted)
{
	nodeSourceTypePtr	type;
	xmlChar			*typeStr = NULL;

	debug_enter ("node_source_import");

	typeStr = xmlGetProp (xml, BAD_CAST"sourceType");
	if (!typeStr)
		typeStr = xmlGetProp (xml, BAD_CAST"pluginType"); /* for migration only */

	if (typeStr) {
		debug2 (DEBUG_CACHE, "creating node source instance (type=%s,id=%s)", typeStr, node->id);

		node->available = FALSE;

		/* scan for matching node source and create new instance */
		type = node_source_type_find (typeStr, 0);
		
		if (!type) {
			/* Source type is not available for some reason, but
			   we need a representation to keep the node source
			   in the feed list. So we load a dummy source type
			   instead and save the real source id in the
			   unused node's data field */
			type = node_source_type_find (NODE_SOURCE_TYPE_DUMMY_ID, 0);
			g_assert (NULL != type);
			node->data = g_strdup (typeStr);
		}
		
		node->available = TRUE;
		node->source = NULL;
		node_source_new (node, type);
		node_set_subscription (node, subscription_import (xml, trusted));
	
		type->source_import (node);	// FIXME: pass trusted flag?
	} else {
		g_warning ("No source type given for node \"%s\". Ignoring it.", node_get_title (node));
	}	

	debug_exit ("node_source_import");
}

static void
node_source_export (nodePtr node, xmlNodePtr xml, gboolean trusted)
{
	debug_enter ("node_source_export");

	debug2 (DEBUG_CACHE, "node source export for node %s, id=%s", node->title, NODE_SOURCE_TYPE (node)->id);
	
	/* If the node source type was loaded using the dummy node source
	   type we need to restore the original node source type id from
	   temporarily saved into node->data */
	if (!strcmp (NODE_SOURCE_TYPE (node)->id, NODE_SOURCE_TYPE_DUMMY_ID))
		xmlNewProp (xml, BAD_CAST"sourceType", BAD_CAST (node->data));
	else
		xmlNewProp (xml, BAD_CAST"sourceType", BAD_CAST (NODE_SOURCE_TYPE(node)->id));
		
	if (ui_node_is_expanded (node->id))
		xmlNewProp (xml, BAD_CAST"expanded", BAD_CAST"true");
	else
		xmlNewProp (xml, BAD_CAST"collapsed", BAD_CAST"true");
		
	subscription_export (node->subscription, xml, trusted);

	debug_exit("node_source_export");
}

void
node_source_new (nodePtr node, nodeSourceTypePtr type)
{ 
	g_assert (NULL == node->source);
	node->source = g_new0 (struct nodeSource, 1);
	node->source->root = node;
	node->source->type = type;
}

/* source instance creation dialog */

static void
on_node_source_type_selected (GtkDialog *dialog, gint response_id, gpointer user_data)
{
	GtkTreeSelection	*selection;
	GtkTreeModel		*model;
	GtkTreeIter		iter;
	nodeSourceTypePtr	type;
	
	if (response_id == GTK_RESPONSE_OK) {
		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (liferea_dialog_lookup (GTK_WIDGET (dialog), "type_list")));
		g_assert (NULL != selection);
		gtk_tree_selection_get_selected (selection, &model, &iter);
		gtk_tree_model_get (model, &iter, 1, &type, -1);
		type->source_new ();
	}
	
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
on_node_source_type_dialog_destroy (GtkDialog *dialog, gpointer user_data) 
{
	g_object_unref (user_data);
}

static gboolean
ui_node_source_type_dialog (void)
{
	GSList 			*iter = nodeSourceTypes;
	GtkWidget 		*dialog, *treeview;
	GtkTreeStore		*treestore;
	GtkCellRenderer		*renderer;
	GtkTreeViewColumn 	*column;
	GtkTreeIter		treeiter;
	nodeSourceTypePtr	type;

	if (!nodeSourceTypes) {
		ui_show_error_box (_("No feed list source types found!"));
		return FALSE;
	}		

	/* set up the dialog */
	dialog = liferea_dialog_new ("node_source.glade", "node_source_type_dialog");

	treestore = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
	
	/* add available feed list source to treestore */
	while (iter) {
		type = (nodeSourceTypePtr) iter->data;
		if (type->capabilities & NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION) {

			gtk_tree_store_append (treestore, &treeiter, NULL);
			gtk_tree_store_set (treestore, &treeiter, 
			                               // FIXME: this leaks memory!
			                               0, g_strdup_printf("<b>%s</b>\n<i>%s</i>", type->name, _(type->description)),
			                               1, type,
						       -1);
		}
		iter = g_slist_next (iter);
	}

	treeview = liferea_dialog_lookup (dialog, "type_list");
	g_assert (NULL != treeview);

	column = gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "wrap-width", 400, NULL);
	g_object_set (renderer, "wrap-mode", PANGO_WRAP_WORD, NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview), -1, _("Source Type"), renderer, "markup", 0, NULL);
	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (treestore));
	g_object_unref (treestore);

	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)),
	                             GTK_SELECTION_SINGLE);

	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (on_node_source_type_selected), 
			  NULL);
			  
	return TRUE;
}

void
node_source_update (nodePtr node)
{
	NODE_SOURCE_TYPE (node)->source_update (node);
}

void
node_source_auto_update (nodePtr node)
{
	NODE_SOURCE_TYPE (node)->source_auto_update (node);
}

nodePtr
node_source_add_subscription (nodePtr node, subscriptionPtr subscription)
{
	if (NODE_SOURCE_TYPE (node)->add_subscription)
		return NODE_SOURCE_TYPE (node)->add_subscription (node, subscription);
	else
		g_warning ("node_source_add_subscription(): called on node source type that doesn't implement me!");
		
	return NULL;
}

nodePtr
node_source_add_folder (nodePtr node, const gchar *title)
{
	if (NODE_SOURCE_TYPE (node)->add_folder)
		return NODE_SOURCE_TYPE (node)->add_folder (node, title);
	else
		g_warning ("node_source_add_folder(): called on node source type that doesn't implement me!");

	return NULL;
}

void
node_source_remove_node (nodePtr node, nodePtr child)
{
	if (NODE_SOURCE_TYPE (node)->remove_node)
		NODE_SOURCE_TYPE (node)->remove_node (node, child);
	else
		g_warning ("node_source_remove_node(): called on node source type that doesn't implement me!");
}

void
node_source_item_mark_read (nodePtr node, itemPtr item, gboolean newState)
{
	/* Item read state changes are optional for node source
	   implementations. If they are supported the implementation
	   has to call item_read_state_changed(), otherwise we do
	   call it here. */
	   
	if (NODE_SOURCE_TYPE (node)->item_mark_read)
		NODE_SOURCE_TYPE (node)->item_mark_read (node, item, newState);
	else
		item_read_state_changed (item, newState);
}

void
node_source_item_set_flag (nodePtr node, itemPtr item, gboolean newState)
{
	/* Item flag state changes are optional for node source
	   implementations. If they are supported the implementation
	   has to call item_flag_state_changed(), otherwise we do
	   call it here. */
	   
	if (NODE_SOURCE_TYPE (node)->item_set_flag)
		NODE_SOURCE_TYPE (node)->item_set_flag (node, item, newState);
	else
		item_flag_state_changed (item, newState);
}

/* implementation of the node type interface */

static void
node_source_remove (nodePtr node)
{
	if (NULL != NODE_SOURCE_TYPE (node)->source_delete)
		NODE_SOURCE_TYPE (node)->source_delete (node);
		
	notification_node_removed (node);
	ui_node_remove_node (node);
}

static void
node_source_save (nodePtr node)
{
	node_foreach_child (node, node_save);
}

static void
node_source_free (nodePtr node)
{
	if (NULL != NODE_SOURCE_TYPE (node)->free)
		NODE_SOURCE_TYPE (node)->free (node);
		
	g_free (node->source);
	node->source = NULL;
}

nodeTypePtr
node_source_get_node_type (void)
{
	static nodeTypePtr	nodeType;

	if (!nodeType) {
		/* derive the node source node type from the folder node type */
		nodeType = (nodeTypePtr) g_new0 (struct nodeType, 1);
		nodeType->id			= "source";
		nodeType->icon			= icons[ICON_DEFAULT];
		nodeType->capabilities		= NODE_CAPABILITY_SHOW_UNREAD_COUNT |
						  NODE_CAPABILITY_SHOW_ITEM_FAVICONS |
						  NODE_CAPABILITY_UPDATE_CHILDS |
						  NODE_CAPABILITY_UPDATE |
						  NODE_CAPABILITY_ADD_CHILDS |
						  NODE_CAPABILITY_REMOVE_CHILDS;
		nodeType->import		= node_source_import;
		nodeType->export		= node_source_export;
		nodeType->load			= folder_get_node_type()->load;
		nodeType->save			= node_source_save;
		nodeType->update_counters	= folder_get_node_type()->update_counters;
		nodeType->remove		= node_source_remove;
		nodeType->render		= node_default_render;
		nodeType->request_add		= ui_node_source_type_dialog;
		nodeType->request_properties	= ui_node_rename;
		nodeType->free			= node_source_free;
	}

	return nodeType; 
}
