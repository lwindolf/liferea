/*
 * @file node_source.c  generic node source provider implementation
 *
 * Copyright (C) 2005-2023 Lars Windolf <lars.windolf@gmx.de>
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
#include "feed.h"
#include "feedlist.h"
#include "folder.h"
#include "item_state.h"
#include "metadata.h"
#include "node.h"
#include "node_type.h"
#include "plugins_engine.h"
#include "ui/icons.h"
#include "ui/liferea_dialog.h"
#include "ui/ui_common.h"
#include "ui/feed_list_view.h"
#include "fl_sources/default_source.h"
#include "fl_sources/dummy_source.h"
#include "fl_sources/google_source.h"
#include "fl_sources/google_reader_api.h"
#include "fl_sources/opml_source.h"
#include "fl_sources/reedah_source.h"
#include "fl_sources/theoldreader_source.h"
#include "fl_sources/ttrss_source.h"
#include "fl_sources/node_source_activatable.h"

static GSList		*nodeSourceTypes = NULL;
static PeasExtensionSet	*extensions = NULL;

nodePtr
node_source_root_from_node (nodePtr node)
{
	while (node->parent->source == node->source)
		node = node->parent;

	return node;
}

static nodeSourceTypePtr
node_source_type_find (const gchar *typeStr, guint capabilities)
{
	GSList *iter = nodeSourceTypes;

	while (iter) {
		nodeSourceTypePtr type = (nodeSourceTypePtr)iter->data;
		if (((NULL == typeStr) || !strcmp(type->id, typeStr)) &&
		    ((0 == capabilities) || (type->capabilities & capabilities)))
			return type;
		iter = g_slist_next (iter);
	}

	g_print ("Could not find source type \"%s\"\n!", typeStr);
	return NULL;
}

gboolean
node_source_type_register (nodeSourceTypePtr type)
{
	debug1 (DEBUG_PARSING, "Registering node source type %s", type->name);

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
	
	/* register a generic type for storing feed-id strings of remote services */
	metadata_type_register ("feed-id", METADATA_TYPE_TEXT);

	/* we need to register all source types once before doing anything... */
	node_source_type_register (default_source_get_type ());
	node_source_type_register (dummy_source_get_type ());
	node_source_type_register (opml_source_get_type ());
	node_source_type_register (google_source_get_type ());
	node_source_type_register (reedah_source_get_type ());
	node_source_type_register (ttrss_source_get_type ());
	node_source_type_register (theoldreader_source_get_type ());

	/* register all source types that are google like */
	type = g_new0 (struct nodeSourceType, 1);
	memcpy (type, google_source_get_type (), sizeof(struct nodeSourceType));
	type->name = N_("Miniflux");
	type->id = "fl_miniflux";
	node_source_type_register (type);

	extensions = peas_extension_set_new (PEAS_ENGINE (liferea_plugins_engine_get_default ()),
		                             LIFEREA_NODE_SOURCE_ACTIVATABLE_TYPE, NULL);
	liferea_plugins_engine_set_default_signals (extensions, NULL);

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
node_source_set_feed_subscription_type (nodePtr folder, subscriptionTypePtr type)
{
	GSList *iter;

	for (iter = folder->children; iter; iter = g_slist_next(iter)) {
		nodePtr node = (nodePtr) iter->data;

		if (node->subscription)
			node->subscription->type = type;

		/* Recurse for hierarchic nodes... */
		node_source_set_feed_subscription_type (node, type);
	}
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
		type = node_source_type_find ((const gchar *)typeStr, 0);

		if (!type) {
			/* Source type is not available for some reason, but
			   we need a representation to keep the node source
			   in the feed list. So we load a dummy source type
			   instead and save the real source id in the
			   unused node's data field */
			type = node_source_type_find (NODE_SOURCE_TYPE_DUMMY_ID, 0);
			g_assert (NULL != type);
			node->data = g_strdup ((gchar *)typeStr);
		}

		node->available = TRUE;
		node->source = NULL;
		node_source_new (node, type, NULL);
		node_set_subscription (node, subscription_import (xml, trusted));

		type->source_import (node);

		/* Set subscription type for all child nodes imported */
		node_source_set_feed_subscription_type (node, type->feedSubscriptionType);

		if (!strcmp ((gchar *)typeStr, "fl_bloglines")) {
			g_print ("Removing obsolete Bloglines subscription.");
			feedlist_node_removed (node);
		}
		
		if(type->capabilities & NODE_SOURCE_CAPABILITY_GOOGLE_READER_API)
			google_reader_api_check (&(node->source->api));
	} else {
		g_print ("No source type given for node \"%s\". Ignoring it.", node_get_title (node));
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

	subscription_export (node->subscription, xml, trusted);

	NODE_SOURCE_TYPE (node)->source_export (node);

	debug_exit("node_source_export");
}

void
node_source_new (nodePtr node, nodeSourceTypePtr type, const gchar *url)
{
	subscriptionPtr	subscription;

	g_assert (NULL == node->source);

	node->source = g_new0 (struct nodeSource, 1);
	node->source->root = node;
	node->source->type = type;
	node->source->loginState = NODE_SOURCE_STATE_NONE;
	node->source->actionQueue = g_queue_new ();

	if (url) {
		subscription = subscription_new (url, NULL, NULL);
		node_set_subscription (node, subscription);

		subscription->type = node->source->type->sourceSubscriptionType;
	}
}

void
node_source_set_state (nodePtr node, gint newState)
{
	debug3 (DEBUG_UPDATE, "node source '%s' now in state %d (was %d)", node->id, newState, node->source->loginState);

	/* State transition actions below... */
	if (newState == NODE_SOURCE_STATE_ACTIVE)
		node->source->authFailures = 0;

	if (newState == NODE_SOURCE_STATE_NONE) {
		node->source->authFailures++;
		node->available = FALSE;
	}

	if (node->source->authFailures >= NODE_SOURCE_MAX_AUTH_FAILURES)
		newState = NODE_SOURCE_STATE_NO_AUTH;

	node->source->loginState = newState;
}

void
node_source_set_auth_token (nodePtr node, gchar *token)
{
	g_assert (!node->source->authToken);

	debug2 (DEBUG_UPDATE, "node source \"%s\" Auth token found: %s", node->id, token);
	node->source->authToken = token;

	node_source_set_state (node, NODE_SOURCE_STATE_ACTIVE);
}

/* source instance creation dialog */

static void
on_node_source_type_selected (GtkTreeSelection *selection, gpointer user_data)
{
	GtkTreeIter	iter;
	GtkTreeModel	*model;

	if (gtk_tree_selection_get_selected (selection, &model, &iter))
		gtk_widget_set_sensitive (GTK_WIDGET (user_data), TRUE);
	else
		gtk_widget_set_sensitive (GTK_WIDGET (user_data), FALSE);
}

static void
on_node_source_type_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
	GtkTreeSelection	*selection;
	GtkTreeModel		*model;
	GtkTreeIter		iter;
	nodeSourceTypePtr	type;

	if (response_id == GTK_RESPONSE_OK) {
		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (liferea_dialog_lookup (GTK_WIDGET (dialog), "type_list")));
		g_assert (NULL != selection);
		if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
			gtk_tree_model_get (model, &iter, 1, &type, -1);
			if (type)
				type->source_new ();
		}
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static gboolean
feed_list_node_source_type_dialog (void)
{
	GSList 			*iter = nodeSourceTypes;
	GtkWidget 		*dialog, *treeview;
	GtkTreeStore		*treestore;
	GtkCellRenderer		*renderer;
	GtkTreeIter		treeiter;
	nodeSourceTypePtr	type;

	if (!nodeSourceTypes) {
		ui_show_error_box (_("No feed list source types found!"));
		return FALSE;
	}

	/* set up the dialog */
	dialog = liferea_dialog_new ("node_source");

	treestore = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);

	/* add available feed list source to treestore */
	while (iter) {
		type = (nodeSourceTypePtr) iter->data;
		if (type->capabilities & NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION) {

			gtk_tree_store_append (treestore, &treeiter, NULL);
			gtk_tree_store_set (treestore, &treeiter,
			                               0, type->name,
			                               1, type,
						       -1);
		}
		iter = g_slist_next (iter);
	}

	treeview = liferea_dialog_lookup (dialog, "type_list");
	g_assert (NULL != treeview);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "wrap-width", 400, NULL);
	g_object_set (renderer, "wrap-mode", PANGO_WRAP_WORD, NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview), -1, _("Source Type"), renderer, "markup", 0, NULL);
	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (treestore));
	g_object_unref (treestore);

	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)),
	                             GTK_SELECTION_SINGLE);

	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (on_node_source_type_response),
			  NULL);
	g_signal_connect (G_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview))), "changed",
	                  G_CALLBACK (on_node_source_type_selected),
	                  liferea_dialog_lookup (dialog, "ok_button"));


	return TRUE;
}

void
node_source_update (nodePtr node)
{
	if (node->subscription) {
		/* Reset NODE_SOURCE_STATE_NO_AUTH as this is a manual
		   user interaction and no auto-update so we can query
		   for credentials again. */
		if (node->source->loginState == NODE_SOURCE_STATE_NO_AUTH)
			node_source_set_state (node, NODE_SOURCE_STATE_NONE);

		subscription_update (node->subscription, 0);

		/* Note that node sources are required to auto-update child
		   nodes themselves once login and feed list update is fine. */
	} else {
		/* for default source */
		node_foreach_child_data (node, node_update_subscription, GUINT_TO_POINTER (0));
	}
}

void
node_source_auto_update (nodePtr node)
{
	NODE_SOURCE_TYPE (node)->source_auto_update (node);
}

static gboolean
node_source_is_logged_in (nodePtr node)
{
	if (FALSE == (NODE_SOURCE_TYPE (node)->capabilities & NODE_SOURCE_CAPABILITY_CAN_LOGIN))
		return TRUE;

	if (node->source->loginState != NODE_SOURCE_STATE_ACTIVE)
		ui_show_error_box (_("Login for '%s' has not yet completed! Please wait until login is done."), node->title);

	return node->source->loginState == NODE_SOURCE_STATE_ACTIVE;
}

nodePtr
node_source_add_subscription (nodePtr node, subscriptionPtr subscription)
{
	if (!node_source_is_logged_in (node))
		return NULL;

	if (NODE_SOURCE_TYPE (node)->add_subscription)
		return NODE_SOURCE_TYPE (node)->add_subscription (node, subscription);
	else
		g_print ("node_source_add_subscription(): called on node source type that doesn't implement me!");

	return NULL;
}

nodePtr
node_source_add_folder (nodePtr node, const gchar *title)
{
	if (!node_source_is_logged_in (node))
		return NULL;

	if (NODE_SOURCE_TYPE (node)->add_folder)
		return NODE_SOURCE_TYPE (node)->add_folder (node, title);
	else
		g_print ("node_source_add_folder(): called on node source type that doesn't implement me!");

	return NULL;
}

void
node_source_update_folder (nodePtr node, nodePtr folder)
{
	if (!node_source_is_logged_in (node))
		return;

	if (!folder)
		folder = node->source->root;

	if (node->parent != folder) {
		debug2 (DEBUG_UPDATE, "Moving node \"%s\" to folder \"%s\"", node->title, folder->title);
		node_reparent (node, folder);
	}
}

nodePtr
node_source_find_or_create_folder (nodePtr parent, const gchar *id, const gchar *name)
{
	nodePtr		folder = NULL;
	gchar		*folderNodeId;

	if (!id)
		return parent->source->root;	/* No id means folder is root node */

	folderNodeId = g_strdup_printf ("%s-folder-%s", NODE_SOURCE_TYPE (parent->source->root)->id, id);
	folder = node_from_id (folderNodeId);
	if (!folder) {
		folder = node_new (folder_get_node_type ());
		node_set_id (folder, folderNodeId);
		node_set_title (folder, name);
		node_set_parent (folder, parent, -1);
		feedlist_node_imported (folder);
		subscription_update (folder->subscription, FEED_REQ_RESET_TITLE | FEED_REQ_PRIORITY_HIGH);
	}

	return folder;
}

void
node_source_remove_node (nodePtr node, nodePtr child)
{
	if (!node_source_is_logged_in (node))
		return;

	g_assert (child != node);
	g_assert (child != child->source->root);

	if (NODE_SOURCE_TYPE (node)->remove_node)
		NODE_SOURCE_TYPE (node)->remove_node (node, child);
	else
		g_print ("node_source_remove_node(): called on node source type that doesn't implement me!");
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

static void
node_source_convert_to_local_child_node (nodePtr node)
{
	/* Ensure to remove special subscription types and cancel updates
	   Note: we expect that all feeds already have the subscription URL
	   set. This might need to be done by the node type specific
	   convert_to_local() method! */
	if (node->subscription) {
		update_job_cancel_by_owner ((gpointer)node);
		update_job_cancel_by_owner ((gpointer)node->subscription);

		debug2 (DEBUG_UPDATE, "Converting feed: %s = %s\n", node->title, node->subscription->source);

		node->subscription->type = feed_get_subscription_type ();
	}

	if (IS_FOLDER (node))
		node_foreach_child (node, node_source_convert_to_local_child_node);

	node->source = ((nodePtr)feedlist_get_root ())->source;
}

void
node_source_convert_to_local (nodePtr node)
{
	g_assert (node == node->source->root);

	/* Preparation */

	update_job_cancel_by_owner ((gpointer)node);
	update_job_cancel_by_owner ((gpointer)node->subscription);
	update_job_cancel_by_owner ((gpointer)node->source);

	/* Give the node source type the chance to do things ... */
	if (NULL != NODE_SOURCE_TYPE (node)->convert_to_local)
		NODE_SOURCE_TYPE (node)->convert_to_local (node);

	/* Perform conversion */

	debug0 (DEBUG_UPDATE, "Converting root node to folder...");
	node->source = ((nodePtr)feedlist_get_root ())->source;
	node->type = folder_get_node_type ();
	node->subscription = NULL;	/* leaking subscription is ok */
	node->data = NULL;		/* leaking data is ok */

	node_foreach_child (node, node_source_convert_to_local_child_node);

	feedlist_schedule_save ();

	/* FIXME: something is not perfect, because if you immediately
	   remove the subscription tree afterwards there is a double free */

	ui_show_info_box (_("The '%s' subscription was successfully converted to local feeds!"), node->title);
}

/* implementation of the node type interface */

static void
node_source_remove (nodePtr node)
{
	if (!node_source_is_logged_in (node))
		return;

	g_assert (node == node->source->root);

	if (NULL != NODE_SOURCE_TYPE (node)->source_delete)
		NODE_SOURCE_TYPE (node)->source_delete (node);

	feed_list_view_remove_node (node);
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

	google_reader_api_free (&(node->source->api));

	g_free (node->source->authToken);
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
		nodeType->icon			= ICON_DEFAULT;
		nodeType->capabilities		= NODE_CAPABILITY_SHOW_UNREAD_COUNT |
						  NODE_CAPABILITY_SHOW_ITEM_FAVICONS |
						  NODE_CAPABILITY_UPDATE_CHILDS |
						  NODE_CAPABILITY_UPDATE |
						  NODE_CAPABILITY_UPDATE_FAVICON |
						  NODE_CAPABILITY_ADD_CHILDS |
						  NODE_CAPABILITY_REMOVE_CHILDS;
		nodeType->import		= node_source_import;
		nodeType->export		= node_source_export;
		nodeType->load			= folder_get_node_type()->load;
		nodeType->save			= node_source_save;
		nodeType->update_counters	= folder_get_node_type()->update_counters;
		nodeType->remove		= node_source_remove;
		nodeType->render		= node_default_render;
		nodeType->request_add		= feed_list_node_source_type_dialog;
		nodeType->request_properties	= feed_list_view_rename_node;
		nodeType->free			= node_source_free;
	}

	return nodeType;
}
