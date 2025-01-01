/*
 * @file liferea_shell.c  UI action handling
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

#include "ui/liferea_shell_actions.h"

#include <glib.h>

#include "conf.h"
#include "ui/liferea_shell.h"

/* This file contains all

        - accelerators
        - actions
        - and their callbacks
        
   Everything is registered against the LifereaShell window. */


static GActionGroup	*generalActions = NULL;
static GActionGroup	*addActions = NULL;		/*<< all types of "New" options */
static GActionGroup	*feedActions = NULL;		/*<< update and mark read */
static GActionGroup	*readWriteActions = NULL;	/*<< node remove and properties, node itemset items remove */
static GActionGroup	*itemActions = NULL;		/*<< item state toggline, single item remove */

static const GActionEntry liferea_shell_gaction_entries[] = {
	{"update-all", on_menu_update_all, NULL, NULL, NULL},
	{"mark-all-feeds-read", on_mark_all_read, NULL, NULL, NULL},
	{"import-feed-list", on_menu_import, NULL, NULL, NULL},
	{"export-feed-list", on_menu_export, NULL, NULL, NULL},
	{"quit", on_menu_quit, NULL, NULL, NULL},
	{"remove-selected-feed-items", on_remove_items_activate, NULL, NULL, NULL},
	{"prev-read-item", on_prev_read_item_activate, NULL, NULL, NULL},
	{"next-read-item", on_next_read_item_activate, NULL, NULL, NULL},
	{"next-unread-item", on_next_unread_item_activate, NULL, NULL, NULL},
	{"zoom-in", on_zoomin_activate, NULL, NULL, NULL},
	{"zoom-out", on_zoomout_activate, NULL, NULL, NULL},
	{"zoom-reset", on_zoomreset_activate, NULL, NULL, NULL},
	{"show-update-monitor", on_menu_show_update_monitor, NULL, NULL, NULL},
	{"show-preferences", on_prefbtn_clicked, NULL, NULL, NULL},
	{"search-feeds", on_searchbtn_clicked, NULL, NULL, NULL},
	{"show-help-contents", on_topics_activate, NULL, NULL, NULL},
	{"show-help-quick-reference", on_quick_reference_activate, NULL, NULL, NULL},
	{"show-help-faq", on_faq_activate, NULL, NULL, NULL},
	{"show-about", on_about_activate, NULL, NULL, NULL},

	/* Parameter type must be NULL for toggle. */
	{"fullscreen", NULL, NULL, "@b false", on_menu_fullscreen_activate},
	{"reduced-feed-list", NULL, NULL, "@b false", on_feedlist_reduced_activate},

	{"toggle-item-read-status", on_toggle_unread_status, "t", NULL, NULL},
	{"toggle-item-flag", on_toggle_item_flag, "t", NULL, NULL},
	{"remove-item", on_remove_item, "t", NULL, NULL},
	{"launch-item-in-tab", on_launch_item_in_tab, "t", NULL, NULL},
	{"launch-item-in-browser", on_launch_item_in_browser, "t", NULL, NULL},
	{"launch-item-in-external-browser", on_launch_item_in_external_browser, "t", NULL, NULL}
};

static const GActionEntry liferea_shell_add_gaction_entries[] = {
	{"new-subscription", on_menu_feed_new, NULL, NULL, NULL},
	{"new-folder", on_menu_folder_new, NULL, NULL, NULL},
	{"new-vfolder", on_new_vfolder_activate, NULL, NULL, NULL},
	{"new-source", on_new_plugin_activate, NULL, NULL, NULL},
	{"new-newsbin", on_new_newsbin_activate, NULL, NULL, NULL}
};

static const GActionEntry liferea_shell_feed_gaction_entries[] = {
	{"mark-selected-feed-as-read", on_mark_all_read, NULL, NULL, NULL},
	{"update-selected", on_menu_update, NULL, NULL, NULL}
	// from ui_popup.clayout
	{"node-mark-all-read", on_mark_all_read, NULL, NULL, NULL},
	{"node-rebuild-vfolder", ui_popup_rebuild_vfolder, NULL, NULL, NULL},
	{"node-properties", ui_popup_properties, NULL, NULL, NULL},
	{"node-delete", ui_popup_delete, NULL, NULL, NULL},
	{"node-sort-feeds", ui_popup_sort_feeds, NULL, NULL, NULL},
	{"node-convert-to-local", ui_popup_add_convert_to_local, NULL, NULL, NULL},
	{"node-update", on_menu_update, NULL, NULL, NULL},
	{"node-export-items-to-file", on_menu_export_items_to_file, NULL, NULL, NULL}
};

static const GActionEntry liferea_shell_read_write_gaction_entries[] = {
	{"selected-node-properties", on_menu_properties, NULL, NULL, NULL},
	{"delete-selected", on_menu_delete, NULL, NULL, NULL}
};

static const GActionEntry liferea_shell_item_gaction_entries[] = {
	{"toggle-selected-item-read-status", on_toggle_unread_status, NULL, NULL, NULL},
	{"toggle-selected-item-flag", on_toggle_item_flag, NULL, NULL, NULL},
	{"remove-selected-item", on_remove_item, NULL, NULL, NULL},
	{"launch-selected-item-in-tab", on_launch_item_in_tab, NULL, NULL, NULL},
	{"launch-selected-item-in-browser", on_launch_item_in_browser, NULL, NULL, NULL},
	{"launch-selected-item-in-external-browser", on_launch_item_in_external_browser, NULL, NULL, NULL},
	// from ui_popup.c
	{"copy-item-to-newsbin", on_copy_to_newsbin, "(umt)", NULL, NULL},
	{"toggle-item-read-status", on_toggle_unread_status, "t", NULL, NULL},
	{"toggle-item-flag", on_toggle_item_flag, "t", NULL, NULL},
	{"remove-item", on_remove_item, "t", NULL, NULL},
	{"open-item-in-tab", on_launch_item_in_tab, "t", NULL, NULL},
	{"open-item-in-browser", on_launch_item_in_browser, "t", NULL, NULL},
	{"open-item-in-external-browser", on_launch_item_in_external_browser, "t", NULL, NULL}
};

static const GActionEntry liferea_shell_link_gaction_entries[] = {
	{"open-link-in-tab", on_open_link_in_tab, "s", NULL, NULL},
	{"open-link-in-browser", on_open_link_in_browser, "s", NULL, NULL},
	{"open-link-in-external-browser", on_open_link_in_external_browser, "s", NULL, NULL},
	/* The parameters are link, then title. */
	{"social-bookmark-link", on_social_bookmark_link, "(ss)", NULL, NULL},
	{"copy-link-to-clipboard", on_copy_link_to_clipboard, "s", NULL, NULL},
	{"email-the-author", email_the_author, "t", NULL, NULL}
};

/* all action callbacks */

static void
on_prefbtn_clicked (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	preferences_dialog_open ();
}

static void
on_searchbtn_clicked (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	simple_search_dialog_open ();
}

static void
on_about_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GtkWidget *dialog;

	dialog = liferea_dialog_new ("about");
	gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (dialog), VERSION);
	// FIXME GTK4 g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_set_visible), NULL);
}

static void
liferea_shell_add_html_tab (const gchar *file, const gchar *name)
{
	gchar *filepattern = g_strdup_printf (PACKAGE_DATA_DIR "/" PACKAGE "/doc/html/%s", file);
	gchar *filename = common_get_localized_filename (filepattern);
	gchar *fileuri = g_strdup_printf ("file://%s", filename);

	browser_tabs_add_new (fileuri, name, TRUE);

	g_free (filepattern);
	g_free (filename);
	g_free (fileuri);
}

static void
on_topics_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	liferea_shell_add_html_tab ("topics_%s.html", _("Help Topics"));
}

static void
on_quick_reference_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	liferea_shell_add_html_tab ("reference_%s.html", _("Quick Reference"));
}

static void
on_faq_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	liferea_shell_add_html_tab ("faq_%s.html", _("FAQ"));
}

static void
on_menu_quit (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	liferea_application_shutdown ();
}

static void
on_menu_fullscreen_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	gboolean fullscreen = gtk_window_is_fullscreen(shell->window);
	if (fullscreen)
		gtk_window_unfullscreen (shell->window);
	else
		gtk_window_fullscreen(shell->window);
	g_simple_action_set_state (action, g_variant_new_boolean (fullscreen));
}

static void
on_zoomin_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	liferea_shell_do_zoom (1);
}

static void
on_zoomout_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	liferea_shell_do_zoom (-1);
}

static void
on_zoomreset_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	liferea_shell_do_zoom (0);
}

static void
on_menu_import (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	import_OPML_file ();
}

static void
on_menu_export (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	export_OPML_file ();
}

static void
on_open_link_in_browser (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemview_launch_URL (g_variant_get_string (parameter, NULL), TRUE /* use internal browser */);
}

static void
on_open_link_in_external_browser (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	browser_launch_URL_external (g_variant_get_string (parameter, NULL));
}

static void
on_open_link_in_tab (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	browser_tabs_add_new (g_variant_get_string (parameter, NULL), g_variant_get_string (parameter, NULL), FALSE);
}

static void
on_social_bookmark_link (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	gchar *social_url, *link, *title;

	g_variant_get (parameter, "(ss)", &link, &title);
	social_url = social_get_bookmark_url (link, title);
	(void)browser_tabs_add_new (social_url, social_url, TRUE);
	g_free (social_url);
}

static void
on_copy_link_to_clipboard (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	g_autofree gchar *link = (gchar *) common_uri_sanitize (BAD_CAST g_variant_get_string (parameter, NULL));

	liferea_shell_copy_to_clipboard (link);
}

static void
email_the_author(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemPtr item = NULL;

	if (parameter)
		item = item_load (g_variant_get_uint64 (parameter));
	else
		item = itemlist_get_selected ();

	if(item) {
		const gchar *author, *subject;
		GError		*error = NULL;
		gchar 		*argv[5];

		author = item_get_author(item);
		subject = item_get_title (item);

		g_assert (author != NULL);

		argv[0] = g_strdup("xdg-email");
		argv[1] = g_strdup_printf ("mailto:%s", author);
		argv[2] = g_strdup("--subject");
		argv[3] = g_strdup_printf ("%s", subject);
		argv[4] = NULL;

		g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error);

		if (error && (0 != error->code)) {
			debug (DEBUG_GUI, "Email command failed: %s : %s", argv[0], error->message);
			liferea_shell_set_important_status_bar (_("Email command failed: %s"), error->message);
			g_error_free (error);
		} else {
			liferea_shell_set_status_bar (_("Starting: \"%s\""), argv[0]);
		}

		g_free(argv[0]);
		g_free(argv[1]);
		g_free(argv[2]);
		g_free(argv[3]);
		item_unload(item);
	}
}

static void
ui_popup_rebuild_vfolder (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	vfolder_rebuild ((Node *)user_data);
}

static void
ui_popup_properties (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	Node *node = (Node *) user_data;

	NODE_PROVIDER (node)->request_properties (node);
}

static void
ui_popup_delete (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	feed_list_view_remove ((Node *)user_data);
}

static void
ui_popup_sort_feeds (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	feed_list_view_sort_folder ((Node *)user_data);
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

	res = gtk_dialog_run (dialog);
	if (res == GTK_RESPONSE_ACCEPT) {
		gchar *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		node_save_items_to_file (node, filename, &err);
		g_free (filename);
	}

	if (err) {
		GtkWidget *errdlg;
		GtkDialogFlags flags = GTK_DIALOG_DESTROY_WITH_PARENT;
		errdlg = gtk_message_dialog_new (GTK_WINDOW (dialog),
		                                flags,
		                                GTK_MESSAGE_ERROR,
		                                GTK_BUTTONS_CLOSE,
		                                "%s",
		                                err->message);
		gtk_dialog_run (GTK_DIALOG (errdlg));
		gtk_widget_destroy (errdlg);
		g_error_free(err);
		err = NULL;
	}

	gtk_widget_destroy (dialog);
}

static void
on_menu_export_items_to_file (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	Node *node = (Node *) user_data;
	GtkWindow *parent;
	GtkWidget *dialog;
	GtkFileChooser *chooser;
	GtkFileFilter *feed_files_filter, *all_files_filter;
	gint res;
	gchar *curname;
	const gchar *title;
	GError *err = NULL;

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
	gtk_file_chooser_set_filename (chooser, curname);
	gtk_file_chooser_set_current_name (chooser, curname);
	gtk_file_chooser_set_do_overwrite_confirmation (chooser, TRUE);
	g_free(curname);

        g_signal_connect (dialog, "response", G_CALLBACK (on_menu_export_items_to_file_cb), user_data);
}

/* sensitivity callbacks */

static void
liferea_shell_actions_update_update_menu (gboolean enabled)
{
	g_simple_action_set_enabled (G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (shell->feedActions), "update-selected")), enabled);
}

static void
liferea_shell_actions_update_feed_menu (gboolean add, gboolean enabled, gboolean readWrite)
{
	liferea_shell_simple_action_group_set_enabled (shell->addActions, add);
	liferea_shell_simple_action_group_set_enabled (shell->feedActions, enabled);
	liferea_shell_simple_action_group_set_enabled (shell->readWriteActions, readWrite);
}

void
liferea_shell_actions_update_item_menu (gboolean enabled)
{
	liferea_shell_simple_action_group_set_enabled (shell->itemActions, enabled);
}

static void
liferea_shell_actions_update_allitems_actions (gboolean isNotEmpty, gboolean isRead)
{
	g_simple_action_set_enabled (G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (shell->generalActions), "remove-selected-feed-items")), isNotEmpty);
	g_simple_action_set_enabled (G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (shell->feedActions), "mark-selected-feed-as-read")), isRead);
}

void
liferea_shell_actions_update_history_actions (void)
{
	g_simple_action_set_enabled (G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (shell->generalActions), "prev-read-item")), item_history_has_previous ());
	g_simple_action_set_enabled (G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (shell->generalActions), "next-read-item")), item_history_has_next ());
}

static void
liferea_shell_actions_update_node_actions (gpointer obj, gchar *unusedNodeId, gpointer data)
{
	/* We need to use the selected node here, as for search folders
	   if we'd rely on the parent node of the changed item we would
	   enable the wrong menu options */
	Node	*node = feedlist_get_selected ();

	if (!node) {
		liferea_shell_update_feed_menu (TRUE, FALSE, FALSE);
		liferea_shell_update_allitems_actions (FALSE, FALSE);
		liferea_shell_update_update_menu (FALSE);
		return;
	}

	gboolean allowModify = (NODE_SOURCE_TYPE (node->source->root)->capabilities & NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST);
	liferea_shell_update_feed_menu (allowModify, TRUE, allowModify);
	liferea_shell_update_update_menu ((NODE_PROVIDER (node)->capabilities & NODE_CAPABILITY_UPDATE) ||
	                                  (NODE_PROVIDER (node)->capabilities & NODE_CAPABILITY_UPDATE_CHILDS));

	// Needs to be last as liferea_shell_update_update_menu() default enables actions
	if (IS_FEED (node))
		liferea_shell_update_allitems_actions (0 != node->itemCount, 0 != node->unreadCount);
	else
		liferea_shell_update_allitems_actions (FALSE, 0 != node->unreadCount);
}

static void
liferea_shell_simple_action_group_set_enabled (GActionGroup *group, gboolean enabled)
{
	gchar **actions_list = g_action_group_list_actions (group);
	gint i;
	for (i=0;actions_list[i] != NULL;i++) {
		g_simple_action_set_enabled (G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (group), actions_list [i])), enabled);
	}
	g_strfreev (actions_list);
}

static void
liferea_shell_add_action_group_to_map (GActionGroup *group, GActionMap *map)
{
	gchar **actions_list = g_action_group_list_actions (group);
	gint i;
	for (i=0;actions_list[i] != NULL;i++) {
		g_action_map_add_action (map, g_action_map_lookup_action (G_ACTION_MAP (group), actions_list [i]));
	}
	g_strfreev (actions_list);

}

void
liferea_shell_actions_register (void)
{
        LifereaShell *shell = liferea_shell_get ();
        GtkWindow *window = liferea_shell_get_window ();

	/* Add GActions to application */
	generalActions = G_ACTION_GROUP (g_simple_action_group_new ());
	g_action_map_add_action_entries (G_ACTION_MAP(generalActions), liferea_shell_gaction_entries, G_N_ELEMENTS (liferea_shell_gaction_entries), NULL);
	liferea_shell_add_action_group_to_map (generalActions, G_ACTION_MAP (app));

	addActions = G_ACTION_GROUP (g_simple_action_group_new ());
	g_action_map_add_action_entries (G_ACTION_MAP(addActions), liferea_shell_add_gaction_entries, G_N_ELEMENTS (liferea_shell_add_gaction_entries), NULL);
	liferea_shell_add_action_group_to_map (addActions, G_ACTION_MAP (app));

	feedActions = G_ACTION_GROUP (g_simple_action_group_new ());
	g_action_map_add_action_entries (G_ACTION_MAP(feedActions), liferea_shell_feed_gaction_entries, G_N_ELEMENTS (liferea_shell_feed_gaction_entries), NULL);
	liferea_shell_add_action_group_to_map (feedActions, G_ACTION_MAP (app));

	itemActions = G_ACTION_GROUP (g_simple_action_group_new ());
	g_action_map_add_action_entries (G_ACTION_MAP(itemActions), liferea_shell_item_gaction_entries, G_N_ELEMENTS (liferea_shell_item_gaction_entries), shell);
	liferea_shell_add_action_group_to_map (itemActions, G_ACTION_MAP (app));

	readWriteActions = G_ACTION_GROUP (g_simple_action_group_new ());
	g_action_map_add_action_entries (G_ACTION_MAP(readWriteActions), liferea_shell_read_write_gaction_entries, G_N_ELEMENTS (liferea_shell_read_write_gaction_entries), NULL);
	liferea_shell_add_action_group_to_map (readWriteActions, G_ACTION_MAP (app));

	g_action_map_add_action_entries (G_ACTION_MAP(app), liferea_shell_link_gaction_entries, G_N_ELEMENTS (liferea_shell_link_gaction_entries), NULL);

	g_signal_connect (g_object_get_data (shell, "feedlist"), "new-items",
	                  G_CALLBACK (liferea_shell_update_unread_stats), shell->feedlist);
	g_signal_connect (g_object_get_data (shell, "feedlist"), "items-updated",
	                  G_CALLBACK (liferea_shell_update_node_actions), NULL);
	g_signal_connect (g_object_get_data (shell, "itemlist"), "item-updated",
	                  G_CALLBACK (liferea_shell_update_node_actions), NULL);
	g_signal_connect (liferea_shell_lookup ("feedlist"),
	                  G_CALLBACK (liferea_shell_update_node_actions), NULL);

	/* Add accelerators for shell */
	gtk_application_set_accels_for_action (app, "app.update-all", {"<Control>u", NULL});
	gtk_application_set_accels_for_action (app, "app.quit", {"<Control>q", NULL});
	gtk_application_set_accels_for_action (app, "app.mark-selected-feed-as-read", {"<Control>r", NULL});
	gtk_application_set_accels_for_action (app, "app.next-unread-item", {"<Control>n", NULL});
	gtk_application_set_accels_for_action (app, "app.prev-read-item", {"<Control><Shift>n", NULL});
	gtk_application_set_accels_for_action (app, "app.toggle-selected-item-read-status", {"<Control>m", NULL});
	gtk_application_set_accels_for_action (app, "app.toggle-selected-item-flag", {"<Control>t", NULL});
	gtk_application_set_accels_for_action (app, "app.fullscreen", {"F11", NULL});
	gtk_application_set_accels_for_action (app, "app.zoom-in", {"<Control>plus", "<Control>equal", NULL});
	gtk_application_set_accels_for_action (app, "app.zoom-out", {"<Control>minus", NULL});
	gtk_application_set_accels_for_action (app, "app.zoom-reset", {"<Control>0", NULL});
	gtk_application_set_accels_for_action (app, "app.search-feeds", {"<Control>f", NULL});
	gtk_application_set_accels_for_action (app, "app.show-help-contents", {"F1", NULL});
	gtk_application_set_accels_for_action (app, "app.launch-item-in-external-browser", {"<Control>d", NULL});

	/* Prepare some toggle button states */
	conf_get_bool_value (REDUCED_FEEDLIST, &toggle);
	g_simple_action_set_state ( G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (app), "reduced-feed-list")), g_variant_new_boolean (toggle));

	liferea_shell_update_item_menu (FALSE);
        liferea_shell_update_history_actions ();
}