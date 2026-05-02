/*
 * @file node_actions.c  node actions
 *
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2007-2025 Lars Windolf <lars.windolf@gmx.de>
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

#include "node_actions.h"

#include "common.h"
#include "conf.h"
#include "debug.h"
#include "feedlist.h"
#include "itemlist.h"
#include "net_monitor.h"
#include "node.h"
#include "node_provider.h"
#include "node_providers/feed.h"
#include "node_providers/folder.h"
#include "node_providers/newsbin.h"
#include "node_providers/vfolder.h"
#include "node_source.h"
#include "ui/feed_list_view.h"
#include "ui/liferea_dialog.h"
#include "ui/ui_common.h"

/* action callbacks */

static void
do_menu_update (Node *node)
{
	if (network_monitor_is_online ())
		node_update_subscription (node, GUINT_TO_POINTER (UPDATE_REQUEST_PRIORITY_HIGH));
	else
		liferea_shell_set_status_bar (_("Liferea is in offline mode. No update possible."));

}

static void
on_menu_update (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	Node *node = NULL;

	if (user_data)
		node = (Node *) user_data;
	else
		node = feedlist_get_selected ();

	if (node)
		do_menu_update (node);
	else
		g_warning ("on_menu_update: no feedlist selected");
}

static void
on_menu_feed_new (GSimpleAction *menuitem, GVariant *parameter, gpointer user_data)
{
	node_provider_request_add (feed_get_provider ());
}

static void
on_new_plugin_activate (GSimpleAction *menuitem, GVariant *parameter, gpointer user_data)
{
	node_provider_request_add (node_source_get_provider ());
}

static void
on_new_newsbin_activate (GSimpleAction *menuitem, GVariant *parameter, gpointer user_data)
{
	node_provider_request_add (newsbin_get_provider ());
}

static void
on_menu_folder_new (GSimpleAction *menuitem, GVariant *parameter, gpointer user_data)
{
	node_provider_request_add (folder_get_provider ());
}

static void
on_new_vfolder_activate (GSimpleAction *menuitem, GVariant *parameter, gpointer user_data)
{
	node_provider_request_add (vfolder_get_provider ());
}

static void
ui_popup_rebuild_vfolder (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	vfolder_rebuild (feedlist_get_selected ());
}

static void
on_properties (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
        Node *node = feedlist_get_selected ();

        if (node)
                NODE_PROVIDER (node)->request_properties (node);
}

static void
on_delete (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
        feed_list_view_remove (feedlist_get_selected ());
}

static void
ui_popup_add_convert_to_local (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	node_source_convert_to_local ((Node *)user_data);
}

static void
on_menu_export_items_to_file_cb (GtkDialog *dialog, gint res, gpointer user_data)
{
       	Node *node = (Node *) user_data;
       	GError *err = NULL;

	if (res != GTK_RESPONSE_ACCEPT)
                return;

        g_autoptr(GFile) file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
	g_autofree gchar *filename = g_file_get_path (file);
	node_save_items_to_file (node, filename, &err);
	if (err) {
                ui_show_error_box (err->message);
                g_error_free (err);
        }
}

static void
on_menu_export_items_to_file (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	Node		*node = (Node *) user_data;
	GtkWindow	*parent;
	GtkWidget	*dialog;
	GtkFileChooser	*chooser;
	GtkFileFilter	*feed_files_filter, *all_files_filter;
        g_autoptr(GFile) file;
	g_autofree gchar *curname = NULL;
	const gchar	*title;

	parent = GTK_WINDOW (liferea_shell_lookup ("mainwindow"));

	dialog = gtk_file_chooser_dialog_new (_("Save items to file"),
	                                      parent,
	                                      GTK_FILE_CHOOSER_ACTION_SAVE,
	                                      _("_Cancel"),
	                                      GTK_RESPONSE_CANCEL,
	                                      _("_Save"),
	                                      GTK_RESPONSE_ACCEPT,
	                                      NULL);
	chooser = GTK_FILE_CHOOSER (dialog);

	/* Filters are only for improving usability for now, as the code
	 * itself can only save feeds as RSS 2.0.
	 */
	feed_files_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (feed_files_filter, _("RSS 2.0 files"));
	gtk_file_filter_add_pattern (feed_files_filter, "*.rss");
	gtk_file_filter_add_pattern (feed_files_filter, "*.xml");
	gtk_file_chooser_add_filter(chooser, feed_files_filter);

	all_files_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (all_files_filter, _("All files"));
	gtk_file_filter_add_pattern (all_files_filter, "*");
	gtk_file_chooser_add_filter(chooser, all_files_filter);

	title = node_get_title (node);
	curname = g_strdup_printf("%s.rss", title != NULL ? title : _("Untitled"));
        file = g_file_new_for_path (curname);
	gtk_file_chooser_set_file (chooser, file, NULL);
	gtk_file_chooser_set_current_name (chooser, curname);

        g_signal_connect (dialog, "response", G_CALLBACK (on_menu_export_items_to_file_cb), user_data);
}

static void
on_mark_all_read_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
	if (response_id == GTK_RESPONSE_OK)
		feedlist_mark_all_read ((Node *) user_data);
}

static void
on_mark_all_read (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	Node		*node;
	gboolean 	confirm_mark_read;

	if (!g_strcmp0 (g_action_get_name (G_ACTION (action)), "mark-all-feeds-read"))
		node = feedlist_get_root ();
	else if (!g_strcmp0 (g_action_get_name (G_ACTION (action)), "mark-feed-as-read"))
		node = node_from_id (g_variant_get_string (parameter, NULL));
	else
		node = feedlist_get_selected ();

	conf_get_bool_value (CONFIRM_MARK_ALL_READ, &confirm_mark_read);

	if (confirm_mark_read) {
		GtkMessageDialog *confirm_dialog = GTK_MESSAGE_DIALOG (liferea_dialog_new ("mark_read_dialog"));
		GtkWidget *dont_ask_toggle = liferea_dialog_lookup (GTK_WIDGET (confirm_dialog), "dontAskAgainToggle");
		const gchar *feed_title = (feedlist_get_root () == node) ? _("all feeds"):node_get_title (node);
		gchar *primary_message = g_strdup_printf (_("Mark %s as read ?"), feed_title);

		g_object_set (confirm_dialog, "text", primary_message, NULL);
		g_free (primary_message);
		gtk_message_dialog_format_secondary_text (confirm_dialog, _("Are you sure you want to mark all items in %s as read ?"), feed_title);

		conf_bind (CONFIRM_MARK_ALL_READ, dont_ask_toggle, "active", G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_INVERT_BOOLEAN);

		g_signal_connect (G_OBJECT (confirm_dialog), "response",
	                  G_CALLBACK (on_mark_all_read_response), (gpointer)node);
	} else {
		feedlist_mark_all_read (node);
	}
}

static void
on_remove_items_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	Node *node = feedlist_get_selected ();

	// FIXME: use node type capability check
	if (node && (IS_FEED (node) || IS_NEWSBIN (node)))
		itemlist_remove_all_items (node);
	else
		ui_show_error_box (_("You must select a feed to delete its items!"));
}

static const GActionEntry gaction_entries[] = {
	// feedlist menu node actions
	{"update-selected", on_menu_update, NULL, NULL, NULL},
	{"node-mark-all-read", on_mark_all_read, NULL, NULL, NULL},
	{"node-rebuild-vfolder", ui_popup_rebuild_vfolder, NULL, NULL, NULL},
	{"node-properties", on_properties, NULL, NULL, NULL},
	{"node-delete", on_delete, NULL, NULL, NULL},
	{"node-convert-to-local", ui_popup_add_convert_to_local, NULL, NULL, NULL},
	{"node-update", on_menu_update, NULL, NULL, NULL},
	{"node-export-items-to-file", on_menu_export_items_to_file, NULL, NULL, NULL},

        // enable/disable based on node source properties
	{"new-subscription", on_menu_feed_new, NULL, NULL, NULL},
	{"new-folder", on_menu_folder_new, NULL, NULL, NULL},
	{"new-vfolder", on_new_vfolder_activate, NULL, NULL, NULL},
	{"new-source", on_new_plugin_activate, NULL, NULL, NULL},
	{"new-newsbin", on_new_newsbin_activate, NULL, NULL, NULL},

	// headerbar node actions
	{"mark-all-feeds-read", on_mark_all_read, NULL, NULL, NULL},

	{"mark-feed-as-read", on_mark_all_read, "s", NULL, NULL},
	{"remove-selected-feed-items", on_remove_items_activate, NULL, NULL, NULL}
};

static void
node_actions_item_updated (gpointer obj, gint itemId, gpointer user_data)
{
	// FIXME
}

static void
node_actions_node_selected (gpointer obj, gchar *nodeId, gpointer user_data)
{
        GActionGroup *ag = G_ACTION_GROUP (user_data);

	/* We need to use the selected node here, as for search folders
	   if we'd rely on the parent node of the changed item we would
	   enable the wrong menu options */
	Node *node = node_from_id (nodeId);

	if (!node) {
		ui_common_action_group_enable (ag, FALSE);

                // Allow adding stuff, as it would get added to root node, which is always allowed
	        liferea_shell_action_enable ("new-subscription", TRUE);
                liferea_shell_action_enable ("new-folder", TRUE);
                liferea_shell_action_enable ("new-vfolder", TRUE);
                liferea_shell_action_enable ("new-source", TRUE);
                liferea_shell_action_enable ("new-newsbin", TRUE);
	} else {
		liferea_shell_action_enable ("remove-selected-feed-items", TRUE);

		gboolean allowModify = 0 < (NODE_SOURCE_TYPE (node->source->root)->capabilities & NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST);
		gboolean allowUpdate = 0 < ((NODE_PROVIDER (node)->capabilities & NODE_CAPABILITY_UPDATE) ||
				            (NODE_PROVIDER (node)->capabilities & NODE_CAPABILITY_UPDATE_CHILDS));

		liferea_shell_action_enable ("update-selected", allowUpdate);
		liferea_shell_action_enable ("node-mark-all-read", node->unreadCount > 0);
		liferea_shell_action_enable ("node-rebuild-vfolder", IS_VFOLDER(node));
		liferea_shell_action_enable ("node-properties", allowModify);
		liferea_shell_action_enable ("node-delete", allowModify);
		liferea_shell_action_enable ("node-convert-to-local", IS_NODE_SOURCE (node));
		liferea_shell_action_enable ("node-update", allowUpdate);
		liferea_shell_action_enable ("node-export-items-to-file", TRUE);

		liferea_shell_action_enable ("new-subscription", allowModify && node_can_add_child_feed (node));
		liferea_shell_action_enable ("new-folder", allowModify && node_can_add_child_folder (node));
		liferea_shell_action_enable ("new-vfolder", allowModify && NODE_SOURCE_TYPE (node->source->root)->capabilities & NODE_SOURCE_CAPABILITY_IS_ROOT);
		liferea_shell_action_enable ("new-source",  allowModify && NODE_SOURCE_TYPE (node->source->root)->capabilities & NODE_SOURCE_CAPABILITY_IS_ROOT);
		liferea_shell_action_enable ("new-newsbin", allowModify && NODE_SOURCE_TYPE (node->source->root)->capabilities & NODE_SOURCE_CAPABILITY_IS_ROOT);
	}

	liferea_shell_action_enable ("mark-all-feeds-read", feedlist_get_root ()->unreadCount > 0);
	liferea_shell_action_enable ("mark-feed-as-read", TRUE); // always true because selection-less
}

GActionGroup *
node_actions_create (LifereaShell *shell)
{
	GObject *feedlist, *itemlist;
	gboolean toggle;

        GActionGroup *ag = liferea_shell_add_actions (gaction_entries, G_N_ELEMENTS (gaction_entries));
	
	g_object_get (G_OBJECT (shell),
	              "feedlist", &feedlist,
	              "itemlist", &itemlist, NULL);

	g_signal_connect (itemlist, "item-updated", G_CALLBACK (node_actions_item_updated), ag);
	g_signal_connect (feedlist, "items-updated", G_CALLBACK (node_actions_node_selected), ag);
	g_signal_connect (feedlist, "node-selected", G_CALLBACK (node_actions_node_selected), ag);

	node_actions_item_updated (NULL, 0, ag);
	node_actions_node_selected (NULL, NULL, ag);

        return ag;
}