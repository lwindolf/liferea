/*
 * @file node_source.c  generic node source provider implementation
 *
 * Copyright (C) 2005-2026 Lars Windolf <lars.windolf@gmx.de>
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

#include "node_source.h"

#include <gmodule.h>
#include <gtk/gtk.h>
#include <libadwaita-1/adwaita.h>
#include <string.h>

#include "common.h"
#include "db.h"
#include "debug.h"
#include "export.h"
#include "feedlist.h"
#include "item_state.h"
#include "metadata.h"
#include "node.h"
#include "node_provider.h"
#include "plugins/plugins_engine.h"
#include "ui/auth_dialog.h"
#include "ui/icons.h"
#include "ui/liferea_dialog.h"
#include "ui/ui_common.h"
#include "ui/feed_list_view.h"
#include "node_providers/feed.h"
#include "node_providers/folder.h"
#include "node_sources/default_source.h"
#include "node_sources/dummy_source.h"
#include "node_sources/google_source.h"
#include "node_sources/google_reader_api.h"
#include "node_sources/opml_source.h"
#include "node_sources/reedah_source.h"
#include "node_sources/webdav_source.h"
#include "node_sources/theoldreader_source.h"
#include "node_sources/ttrss_source.h"

static GSList	*nodeSourceTypes = NULL;

/** lock to prevent feed list saving while loading */
static gboolean feedlistImport = TRUE;

static void
node_source_cleanup (void) {
	g_slist_free (nodeSourceTypes);
	nodeSourceTypes = NULL;
}

static void
node_source_import_feedlist (Node *node) 
{
	g_autofree gchar *filename, *backupFilename, *content = NULL;
	gssize	length;

	g_assert (TRUE == feedlistImport);

	filename = common_create_config_filename ("feedlist.opml");
	backupFilename = g_strdup_printf("%s.backup", filename);
	
	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		if (!import_OPML_feedlist (filename, node, FALSE, TRUE))
			g_error ("Fatal: Feed list import failed! You might want to try to restore\n"
			         "the feed list file %s from the backup in %s", filename, backupFilename);

		/* upon successful import create a backup copy of the feed list */
		if (g_file_get_contents (filename, &content, (gsize *)&length, NULL))
			g_file_set_contents (backupFilename, content, length, NULL);
	} else {
		/* If subscriptions could not be loaded provide a default feed list */
		g_autofree gchar *defaultFilename = common_get_localized_filename (PACKAGE_DATA_DIR "/opml/feedlist_%s.opml");
		if (!defaultFilename)
			g_error ("Fatal: No migration possible and no default feedlist found!");

		if (!import_OPML_feedlist (defaultFilename, node, FALSE, TRUE))
			g_error ("Fatal: Feed list import failed!");
	}

	feedlistImport = FALSE;

}

void
node_source_export_feedlist (void)
{
	g_autofree gchar *filename = NULL;
	
	if (feedlistImport)
		return;
	
	filename = common_create_config_filename ("feedlist.opml");
	export_OPML_feedlist (filename, feedlist_get_root (), TRUE);
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
	if (type->source_type_init)
		type->source_type_init ();

	nodeSourceTypes = g_slist_append (nodeSourceTypes, type);

	return TRUE;
}

Node *
node_source_setup_root (void)
{	
	/* register a generic type for storing feed-id strings of remote services */
	metadata_type_register ("feed-id", METADATA_TYPE_TEXT);

	/* 1. we need to register all source types once before doing anything... */
	node_source_type_register (default_source_get_type ());
	node_source_type_register (dummy_source_get_type ());
	node_source_type_register (opml_source_get_type ());
	node_source_type_register (google_source_get_type ());
	node_source_type_register (reedah_source_get_type ());
	node_source_type_register (ttrss_source_get_type ());
	node_source_type_register (theoldreader_source_get_type ());
	node_source_type_register (webdav_source_get_type ());
	atexit (node_source_cleanup);

	/* 2. Register all source types that are google like */
	nodeSourceTypePtr type = g_new0 (struct nodeSourceType, 1);
	memcpy (type, google_source_get_type (), sizeof(struct nodeSourceType));
	type->name = N_("Miniflux");
	type->id = "fl_miniflux";
	node_source_type_register (type);

	/* 3. Create a root node */
	Node *root = node_new ("root");
	root->title = g_strdup ("root");	// not visible anywhere...
	root->source = g_new0 (struct nodeSource, 1);
	root->source->root = root;
	root->source->type = default_source_get_type ();
	node_source_import_feedlist (root);

	/* 4. Start updating*/
	default_source_start_updating (root);

	/* 5. Purge old nodes from the database 

	   This relies on node_source_import_feedlist (root) above to be
	   synchronous about loading the previous state of all node sources 
	   from OPML cache files.
	 */
	db_node_source_cleanup (root);

	return root;
}

static void
node_source_set_feed_subscription_type (Node *node, gpointer type)
{
	if (IS_FEED(node) && node->subscription)
		node->subscription->type = type;

	node_foreach_child_data (node, node_source_set_feed_subscription_type, type);
}

static void
node_source_set_initial_import_flag (Node *node)
{
	node->syncState = NODE_SYNC_STATE_INITIAL_IMPORT;

	node_foreach_child (node, node_source_set_initial_import_flag);
}

static void
node_source_import (Node *node, Node *parent, xmlNodePtr xml, gboolean trusted)
{
	nodeSourceTypePtr	type;
	xmlChar			*typeStr = NULL;

	typeStr = xmlGetProp (xml, BAD_CAST"sourceType");
	if (typeStr) {
		debug (DEBUG_CACHE, "node_source: %s |%s| import as type=%s", node->id, node->title, typeStr);

		node->available = FALSE;

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
		node_set_subscription (node, subscription_import (xml, trusted));
		node_source_new (node, type, NULL);
	} else {
		g_warning ("Feed list import: No source type given for node \"%s\". Ignoring it.", node_get_title (node));
	}
}

static void
node_source_export (Node *node, xmlNodePtr xml, gboolean trusted)
{
	debug (DEBUG_CACHE, "node_source: %s |%s| exporting", node->id, node->title);

	/* If the node source type was loaded using the dummy node source
	   type we need to restore the original node source type id from
	   temporarily saved into node->data */
	if (!strcmp (NODE_SOURCE_TYPE (node)->id, NODE_SOURCE_TYPE_DUMMY_ID)) {
		xmlNewProp (xml, BAD_CAST"sourceType", BAD_CAST (node->data));
		// no OPML dump for dummy source!
	} else {
		xmlNewProp (xml, BAD_CAST"sourceType", BAD_CAST (NODE_SOURCE_TYPE(node)->id));
		opml_source_export (node);
	}

	subscription_export (node->subscription, xml, trusted);
}

/* called both when importing and when creating new sources */
void
node_source_new (Node *node, nodeSourceTypePtr type, const gchar *url)
{
	g_assert (NULL == node->source);

	node->source = g_new0 (struct nodeSource, 1);
	node->source->root = node;
	node->source->type = type;
	node->source->loginState = NODE_SOURCE_STATE_NONE;
	node->source->actionQueue = g_queue_new ();

	if (url) {
		/* This is a bit complicated, when subscribing we get a URL,
		   when importing the subscription is already assigned. */
		g_assert (!node->subscription);

		subscriptionPtr subscription = subscription_new (url, NULL, NULL);
		node_set_subscription (node, subscription);
	}

	/* Ensure all imported children have proper type and state */
	opml_source_import_tree_from_file (node);
	node->subscription->type = type->sourceSubscriptionType;
	node_foreach_child_data (node, node_source_set_feed_subscription_type, type->feedSubscriptionType);
	node_foreach_child (node, node_source_set_initial_import_flag);

	/* Only then setup the source */
	if (type->source_new)
		type->source_new (node);

	/* And check if Google Reader like sources set up all the necessary API endpoints */
	if(type->capabilities & NODE_SOURCE_CAPABILITY_GOOGLE_READER_API)
		google_reader_api_check (&(node->source->api));
}

static void
node_source_set_state (Node *node, gint newState)
{
	if ((nodeSourceState)newState == node->source->loginState)
		return;

	debug (DEBUG_UPDATE, "node_source: %s |%s| now in state %d (was %d)", node->id, node->title, newState, node->source->loginState);

	/* State transition actions */
	if (newState == NODE_SOURCE_STATE_ACTIVE)
		node->available = TRUE;

	if (newState == NODE_SOURCE_STATE_AUTH_FAILED)
		node->available = FALSE;

	if (newState == NODE_SOURCE_STATE_AUTH_CHALLENGE)
		auth_dialog_new (node->subscription, UPDATE_REQUEST_PRIORITY_HIGH);

	/* State change */
	node->source->loginState = newState;

	feedlist_node_was_updated (node);
}

void
node_source_set_auth_failed (Node *root, gboolean challenge)
{
	if (challenge)
		node_source_set_state (root, NODE_SOURCE_STATE_AUTH_CHALLENGE);
	else
		node_source_set_state (root, NODE_SOURCE_STATE_AUTH_FAILED);
}

void
node_source_set_active (Node *node)
{
	node_source_set_state (node, NODE_SOURCE_STATE_ACTIVE);
}

void
node_source_set_auth_token (Node *node, gchar *token)
{
	g_assert (!node->source->authToken);

	debug (DEBUG_UPDATE, "node_source: %s |%s| Auth token found: %s", node->id, node->title, token);
	node->source->authToken = token;

	node_source_set_state (node, NODE_SOURCE_STATE_ACTIVE);
}

/* source instance creation dialog */

static void
on_node_source_type_selected (GtkListBox *list, GtkListBoxRow *row, gpointer userdata)
{
	GtkWidget	*button = liferea_dialog_lookup (GTK_WIDGET (userdata), "applyBtn");

	gtk_widget_set_sensitive (button, NULL != row);
}

static void
on_new_node_source_create (GtkButton *button, gpointer user_data)
{
        GtkWidget *dialog = GTK_WIDGET (user_data);
	struct nodeSourceType *type = g_object_get_data (G_OBJECT (dialog), "type");

        const gchar *username = liferea_dialog_entryrow_get (dialog, "usernameEntry");
        const gchar *password = liferea_dialog_entryrow_get (dialog, "passwordEntry");
        const gchar *url = liferea_dialog_entryrow_get (dialog, "serverEntry");
	const gchar *name = liferea_dialog_entryrow_get (dialog, "nameEntry");

	g_assert (username);
	g_assert (password);
	g_assert (url);
	g_assert (name);

        Node *node = node_new ("source");
        node_source_new (node, type, url);
        node_set_title (node, name);
        subscription_set_auth_info (node->subscription, username, password);

        feedlist_node_added (node);
        node_source_update (node);

        adw_dialog_close (ADW_DIALOG (dialog));
}

static void
on_node_source_type_response (GtkButton *btn, gpointer user_data)
{
	GtkDialog		*dialog = GTK_DIALOG (user_data);
	GtkListBoxRow		*row;
	nodeSourceTypePtr	type;

	row = gtk_list_box_get_selected_row (GTK_LIST_BOX (liferea_dialog_lookup (GTK_WIDGET (dialog), "type_list")));
	type = row ? g_object_get_data (G_OBJECT (row), "type") : NULL;

	if (type) {
		/* Two cases to handle:
		1.) special dialogs with callback defined using an interface function
		2.) standard generic node source creation dialog */

		if (type->source_create) {
			type->source_create ();
		} else {
			GtkWidget *dialog = liferea_dialog_new ("new_node_source");

			g_object_set_data (G_OBJECT (dialog), "type", type);
			liferea_dialog_entryrow_set (dialog, "nameEntry", type->name);
			gtk_label_set_markup (GTK_LABEL (liferea_dialog_lookup (dialog, "label")), type->addInfo);

			/* If the type has a hard-coded URL set it to pass it to the callback,
			   otherwise show the URL entry for the user to enter it */
			if (type->url)
				liferea_dialog_entryrow_set (dialog, "serverEntry", type->url);
			else
				gtk_widget_set_visible (liferea_dialog_lookup (dialog, "serverEntry"), TRUE);

			g_signal_connect (liferea_dialog_lookup (dialog, "applyBtn"), "clicked",
				G_CALLBACK (on_new_node_source_create), dialog);
		}
	}

	adw_dialog_close (ADW_DIALOG (dialog));
}

static gboolean
feed_list_node_source_type_dialog_idle (gpointer unused)
{
	GSList 			*iter = nodeSourceTypes;
	GtkWidget 		*dialog;
	GtkListBox		*listbox;
	nodeSourceTypePtr	type;

	g_assert (nodeSourceTypes);

	/* set up the dialog */
	dialog = liferea_dialog_new ("node_source");
	listbox = GTK_LIST_BOX (liferea_dialog_lookup (dialog, "type_list"));
	g_assert (NULL != listbox);

	/* add available feed list source rows */
	while (iter) {
		type = (nodeSourceTypePtr) iter->data;
		if (type->capabilities & NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION) {
			GtkWidget *row = gtk_list_box_row_new ();
			GtkWidget *label = gtk_label_new (NULL);

			gtk_label_set_markup (GTK_LABEL (label), type->name);
			gtk_label_set_wrap (GTK_LABEL (label), TRUE);
			gtk_label_set_wrap_mode (GTK_LABEL (label), PANGO_WRAP_WORD);
			gtk_label_set_xalign (GTK_LABEL (label), 0.0);
			gtk_widget_set_margin_top (label, 6);
			gtk_widget_set_margin_bottom (label, 6);
			gtk_widget_set_margin_start (label, 12);
			gtk_widget_set_margin_end (label, 12);

			gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), label);
			g_object_set_data (G_OBJECT (row), "type", type);
			gtk_list_box_append (listbox, row);
		}
		iter = g_slist_next (iter);
	}

	g_signal_connect (liferea_dialog_lookup (dialog, "applyBtn"), "clicked",
			  G_CALLBACK (on_node_source_type_response),
			  dialog);
	g_signal_connect_swapped (liferea_dialog_lookup (dialog, "cancelBtn"), "clicked",
			  G_CALLBACK (adw_dialog_close),
			  dialog);
	g_signal_connect (G_OBJECT (listbox), "row-selected",
	                  G_CALLBACK (on_node_source_type_selected),
	                  dialog);

	adw_dialog_present (ADW_DIALOG (dialog), liferea_shell_get_window ());

	return FALSE;
}

static gboolean
feed_list_node_source_type_dialog (void)
{
	g_idle_add (feed_list_node_source_type_dialog_idle, NULL);
	return TRUE;
}

void
node_source_update (Node *node)
{
	if (node->subscription) {
		subscription_update (node->subscription, 0);

		/* Note that node sources are required to auto-update child
		   nodes themselves once login and feed list update is fine. */
	} else {
		/* only for default source we update all childs here */
		node_foreach_child_data (node, node_update_subscription, GUINT_TO_POINTER (0));
	}
}

void
node_source_auto_update (Node *node, updateFlags flags)
{
	if (node->source->loginState == NODE_SOURCE_STATE_NONE) {
		debug (DEBUG_UPDATE, "node_source_auto_update: %s |%s| start login", node->id, node->id);
		node_source_set_state (node, NODE_SOURCE_STATE_IN_PROGRESS);
		g_assert (node->source->type->source_login);
		(node->source->type->source_login) (node, flags);
		return;
	}

	if (node->source->loginState == NODE_SOURCE_STATE_IN_PROGRESS) {
		debug (DEBUG_UPDATE, "node_source_auto_update: %s |%s| skipped as login in progress", node->id, node->title);
		return;
	}

	// FIXME: does it make sense to use autoupdate for the subscription?
	// there is no fetching besides login anyway
	subscription_auto_update (node->subscription, 0 /* flags */);
}

static gboolean
node_source_is_logged_in (Node *node)
{
	if (FALSE == (NODE_SOURCE_TYPE (node)->capabilities & NODE_SOURCE_CAPABILITY_CAN_LOGIN))
		return TRUE;

	if (node->source->loginState != NODE_SOURCE_STATE_ACTIVE)
		ui_show_error_box (_("Login for '%s' has not yet completed! Please wait until login is done."), node->title);

	return node->source->loginState == NODE_SOURCE_STATE_ACTIVE;
}

Node *
node_source_add_subscription (Node *node, subscriptionPtr subscription)
{
	if (!node_source_is_logged_in (node))
		return NULL;

	if (NODE_SOURCE_TYPE (node)->add_subscription)
		return NODE_SOURCE_TYPE (node)->add_subscription (node, subscription);
	else
		g_print ("node_source_add_subscription(): called on node source type that doesn't implement me!");

	return NULL;
}

Node *
node_source_add_folder (Node *node, const gchar *title)
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
node_source_update_folder (Node *node, Node *folder)
{
	if (!node_source_is_logged_in (node))
		return;

	if (!folder)
		folder = node->source->root;

	if (node->parent != folder) {
		debug (DEBUG_UPDATE, "Moving node \"%s\" to folder \"%s\"", node->title, folder->title);
		feedlist_node_was_moved (node, folder, -1, FALSE);
	}
}

Node *
node_source_find_or_create_folder (Node *parent, const gchar *id, const gchar *name)
{
	Node	*folder = NULL;
	gchar	*folderNodeId;

	if (!id)
		return parent->source->root;	/* No id means folder is root node */

	folderNodeId = g_strdup_printf ("%s-folder-%s", NODE_SOURCE_TYPE (parent->source->root)->id, id);
	folder = node_from_id (folderNodeId);
	if (!folder) {
		folder = node_new ("folder");
		node_set_id (folder, folderNodeId);
		node_set_title (folder, name);
		node_set_parent (folder, parent, -1);
		feedlist_node_imported (folder);
		subscription_update (folder->subscription, UPDATE_REQUEST_RESET_TITLE | UPDATE_REQUEST_PRIORITY_HIGH);
	}

	return folder;
}

void
node_source_remove_node (Node *node, Node *child)
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
node_source_item_mark_read (Node *node, itemPtr item, gboolean newState)
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
node_source_item_set_flag (Node *node, itemPtr item, gboolean newState)
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
node_source_convert_to_local_child_node (Node *node)
{
	node_source_set_state (node, NODE_SOURCE_STATE_MIGRATE);

	/* Ensure to remove special subscription types and cancel updates
	   Note: we expect that all feeds already have the subscription URL
	   set. This might need to be done by the node type specific
	   convert_to_local() method! */
	if (node->subscription) {
		update_job_cancel_by_owner ((gpointer)node);
		update_job_cancel_by_owner ((gpointer)node->subscription);

		debug (DEBUG_UPDATE, "Converting feed: %s = %s", node->title, node->subscription->source);

		node->subscription->type = feed_get_subscription_type ();
	}

	if (IS_FOLDER (node))
		node_foreach_child (node, node_source_convert_to_local_child_node);

	node->source = feedlist_get_root ()->source;
}

void
node_source_convert_to_local (Node *node)
{
	g_assert (node == node->source->root);

	/* Preparation */

	update_job_cancel_by_owner ((gpointer)node);
	update_job_cancel_by_owner ((gpointer)node->subscription);
	update_job_cancel_by_owner ((gpointer)node->source);

	/* Perform conversion */

	debug (DEBUG_UPDATE, "Converting root node to folder...");
	node->source = feedlist_get_root ()->source;
	node->provider = folder_get_provider ();
	node->subscription = NULL;	/* leaking subscription is ok */
	node->data = NULL;		/* leaking data is ok */

	node_foreach_child (node, node_source_convert_to_local_child_node);

	feedlist_schedule_save ();

	/* FIXME: something is not perfect, because if you immediately
	   remove the subscription tree afterwards there is a double free */

	ui_show_info_box (_("The '%s' subscription was successfully converted to local feeds!"), node->title);
}

void
node_source_to_json (Node *node, JsonBuilder *b)
{
	json_builder_set_member_name (b, "nodeSource");
	json_builder_begin_object (b);
	json_builder_set_member_name (b, "title");
	json_builder_add_string_value (b, node->source->root->title);
	json_builder_set_member_name (b, "type");
	json_builder_add_string_value (b, node->source->type->id);

	if (NODE_SOURCE_TYPE (node)->capabilities & NODE_SOURCE_CAPABILITY_CAN_LOGIN) {
		json_builder_set_member_name (b, "loginState");
		json_builder_add_int_value (b, node->source->loginState);
	}

	if (node->source->actionQueue) {
		json_builder_set_member_name (b, "actionQueueLength");
		json_builder_add_int_value (b, g_queue_get_length (node->source->actionQueue));
	}
	json_builder_end_object (b);
}

/* implementation of the node type interface */

static void
node_source_remove_from_db (Node *node, gpointer userdata)
{
	Node *root = (Node *)userdata;

	if (node->source->root != root)
		return;

	node_foreach_child_data (node, node_source_remove_from_db, root);
	db_node_remove (node->id);
}

static void
node_source_remove (Node *node)
{
	g_assert (node == node->source->root);

	/* Note for online accounts we never delete nodes in the account
	   we just disconnect the account, so the user does not unintentionally
	   loose data.
	   
	   Note: an exception is the OPML source, as it is not an online account
	   and defines a "source_remove" method */
	
	if (NODE_SOURCE_TYPE (node)->source_delete)
		NODE_SOURCE_TYPE (node)->source_delete (node);

	// Always remove OPML file in cache dir
	opml_export_remove (node);

	// FIXME: remove all nodes from DB, do not use node_source_remove_node()
	// as we do not want to trigger any remote removal
	node_source_remove_from_db (node, node);
}

static void
node_source_free (Node *node)
{
	g_assert (node == node->source->root);

	update_job_cancel_by_owner (node);
	update_job_cancel_by_owner (node->subscription);
	update_job_cancel_by_owner (node->source);

	if (NODE_SOURCE_TYPE (node)->source_free)
		NODE_SOURCE_TYPE (node)->source_free (node);

	google_reader_api_free (&(node->source->api));

	g_free (node->source->authToken);
	g_free (node->source);
	node->source = NULL;
}

nodeProviderPtr
node_source_get_provider (void)
{
	static nodeProviderPtr	provider;

	if (!provider) {
		/* derive the node source node type from the folder node type */
		provider = (nodeProviderPtr) g_new0 (struct nodeProvider, 1);
		provider->id			= "source";
		provider->icon			= ICON_DEFAULT;
		provider->capabilities		= NODE_CAPABILITY_SHOW_UNREAD_COUNT |
						  NODE_CAPABILITY_SHOW_ITEM_FAVICONS |
						  NODE_CAPABILITY_UPDATE_CHILDS |
						  NODE_CAPABILITY_UPDATE |
						  NODE_CAPABILITY_UPDATE_FAVICON |
						  NODE_CAPABILITY_ADD_CHILDS |
						  NODE_CAPABILITY_REMOVE_CHILDS;
		provider->import		= node_source_import;
		provider->export		= node_source_export;
		provider->load			= folder_get_provider ()->load;
		provider->update_counters	= folder_get_provider ()->update_counters;
		provider->remove		= node_source_remove;
		provider->request_add		= feed_list_node_source_type_dialog;
		provider->request_properties	= feed_list_view_rename_node;
		provider->free			= node_source_free;
	}

	return provider;
}
