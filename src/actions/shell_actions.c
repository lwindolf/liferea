/*
 * @file shell_actions.c  shell scope actions
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

#include "shell_actions.h"

#include <glib.h>
#include <gtk/gtk.h>

#include "browser.h"
#include "common.h"
#include "conf.h"
#include "debug.h"
#include "export.h"
#include "feedlist.h"
#include "item.h"
#include "item_history.h"
#include "itemlist.h"
#include "liferea_application.h"
#include "net_monitor.h"
#include "social.h"
#include "node_source.h"
#include "node_providers/newsbin.h"
#include "node_providers/vfolder.h"
#include "plugins/plugins_engine.h"
#include "ui/browser_tabs.h"
#include "ui/feed_list_view.h"
#include "ui/icons.h"
#include "ui/item_list_view.h"
#include "ui/liferea_dialog.h"
#include "ui/liferea_shell.h"
#include "ui/preferences_dialog.h"
#include "ui/search_dialog.h"
#include "ui/ui_common.h"
#include "ui/ui_update.h"
#include "webkit/liferea_web_view.h"

static void
do_menu_update (Node *node)
{
	if (network_monitor_is_online ())
		node_update_subscription (node, GUINT_TO_POINTER (UPDATE_REQUEST_PRIORITY_HIGH));
	else
		liferea_shell_set_status_bar (_("Liferea is in offline mode. No update possible."));

}

/*static void
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
}*/

static void
on_menu_update_all(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	do_menu_update (feedlist_get_root ());
}

static void
on_mark_all_read_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
	if (response_id == GTK_RESPONSE_OK) {
		feedlist_mark_all_read ((Node *) user_data);
	}
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
	}
}

static void
on_remove_items_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	Node		*node;

	node = feedlist_get_selected ();
	// FIXME: use node type capability check
	if (node && (IS_FEED (node) || IS_NEWSBIN (node)))
		itemlist_remove_all_items (node);
	else
		ui_show_error_box (_("You must select a feed to delete its items!"));
}

// FIXME replace this with a bind!
static void
on_feedlist_reduced_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GVariant *state = g_action_get_state (G_ACTION (action));
	gboolean val = !g_variant_get_boolean (state);
	feed_list_view_set_reduce_mode (val);
	g_simple_action_set_state (action, g_variant_new_boolean (val));
	g_object_unref (state);
}

static void
on_prefbtn_clicked (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	(void) g_object_new (PREFERENCES_DIALOG_TYPE, NULL);
}

static void
on_searchbtn_clicked (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	simple_search_dialog_open ();
}

static void
on_manage_plugins_clicked (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	liferea_plugins_manage_dialog (GTK_WINDOW (liferea_shell_get_window ()));
}

static void
on_about_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GtkWidget *dialog;

	dialog = liferea_dialog_new ("about");
	gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (dialog), VERSION);
	// FIXME GTK4 g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_set_visible), NULL);
}

static gchar *
create_help_url (const gchar *file)
{
	gchar *filepattern = g_strdup_printf (PACKAGE_DATA_DIR "/doc/html/%s", file);
	gchar *filename = common_get_localized_filename (filepattern);
	gchar *fileuri = g_strdup_printf ("file://%s", filename);

	g_free (filepattern);
	g_free (filename);
	return fileuri;
}

static void
on_discover_feeds_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)     
{
        LifereaBrowser *browser = browser_tabs_add_new ("https://lwindolf.github.io/rss-finder/?target=_self&show-title=false", _("Discover Feeds"), TRUE);
	g_object_set (G_OBJECT (browser), "hidden-urlbar", TRUE, NULL);
}

static void
on_topics_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	g_autofree gchar *url = create_help_url("topics_%s.html");
	LifereaBrowser *browser = browser_tabs_add_new (url, _("Help Topics"), TRUE);
	g_object_set (G_OBJECT (browser), "hidden-urlbar", TRUE, NULL);
}

static void
on_shortcuts_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	gtk_window_present (GTK_WINDOW (liferea_dialog_new ("shortcuts")));
}

static void
on_menu_quit (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	liferea_application_shutdown ();
}

static void
on_menu_fullscreen_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GtkWindow *window = GTK_WINDOW (liferea_shell_get_window ());
	gboolean fullscreen = gtk_window_is_fullscreen(window);
	if (fullscreen)
		gtk_window_unfullscreen (window);
	else
		gtk_window_fullscreen(window);
	g_simple_action_set_state (action, g_variant_new_boolean (fullscreen));
}

/* For zoom in : zoom = 1, for zoom out : zoom= -1, for reset : zoom = 0 */
static void
liferea_shell_actions_do_zoom (gint zoom)
{
	LifereaBrowser *htmlview = browser_tabs_get_active_htmlview ();
	liferea_browser_do_zoom (htmlview, zoom);
}

static void
on_zoomin_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	liferea_shell_actions_do_zoom (1);
}

static void
on_zoomout_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	liferea_shell_actions_do_zoom (-1);
}

static void
on_zoomreset_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	liferea_shell_actions_do_zoom (0);
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
ui_popup_sort_feeds (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	feed_list_view_sort_folder ((Node *)user_data);
}

static void
on_next_unread_item_activate (GSimpleAction *menuitem, GVariant*parameter, gpointer user_data)
{
	itemlist_set_selected (liferea_shell_find_next_unread (0));
}

static void
on_print_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	LifereaBrowser *browser;
	GtkWidget *webview;
	
	// First see if a browser tab is active
	browser = browser_tabs_get_active_htmlview ();
	g_return_if_fail (browser != NULL);

	g_object_get (G_OBJECT (browser), "renderwidget", &webview, NULL);
	liferea_web_view_print (LIFEREA_WEB_VIEW (webview));
}

static const GActionEntry gaction_entries[] = {
	{"update-all", on_menu_update_all, NULL, NULL, NULL},
	{"mark-feed-as-read", on_mark_all_read, "s", NULL, NULL},
	{"mark-selected-feed-as-read", on_mark_all_read, NULL, NULL, NULL},
	{"mark-all-feeds-read", on_mark_all_read, NULL, NULL, NULL},
	{"import-feed-list", on_menu_import, NULL, NULL, NULL},
	{"export-feed-list", on_menu_export, NULL, NULL, NULL},
	{"quit", on_menu_quit, NULL, NULL, NULL},
	// FIXME: this looks wrong here
	{"remove-selected-feed-items", on_remove_items_activate, NULL, NULL, NULL},
	{"prev-read-item", on_prev_read_item_activate, NULL, NULL, NULL},
	{"next-read-item", on_next_read_item_activate, NULL, NULL, NULL},
	{"next-unread-item", on_next_unread_item_activate, NULL, NULL, NULL},
	{"zoom-in", on_zoomin_activate, NULL, NULL, NULL},
	{"zoom-out", on_zoomout_activate, NULL, NULL, NULL},
	{"zoom-reset", on_zoomreset_activate, NULL, NULL, NULL},
	{"show-update-monitor", on_menu_show_update_monitor, NULL, NULL, NULL},
	{"manage-plugins", on_manage_plugins_clicked, NULL, NULL, NULL},
	{"show-preferences", on_prefbtn_clicked, NULL, NULL, NULL},
	{"search-feeds", on_searchbtn_clicked, NULL, NULL, NULL},
	{"discover-feeds", on_discover_feeds_activate, NULL, NULL, NULL},
	{"show-help-contents", on_topics_activate, NULL, NULL, NULL},
	{"show-shortcuts", on_shortcuts_activate, NULL, NULL, NULL},
	{"show-about", on_about_activate, NULL, NULL, NULL},
	{"print", on_print_activate, NULL, NULL, NULL},

	/* Parameter type must be NULL for toggle. */
	{"fullscreen", NULL, NULL, "@b false", on_menu_fullscreen_activate},
	{"reduced-feed-list", NULL, NULL, "@b false", on_feedlist_reduced_activate},

	{"node-sort-feeds", ui_popup_sort_feeds, NULL, NULL, NULL}
};

static void
shell_actions_update_history (gpointer obj, gchar *unused, gpointer user_data)
{
	GActionGroup *ag = G_ACTION_GROUP (user_data);
	
	ui_common_action_enable (ag, "prev-read-item", item_history_has_previous ());
	ui_common_action_enable (ag, "next-read-item", item_history_has_next ());
}

GActionGroup *
shell_actions_create (LifereaShell *shell)
{
	GActionGroup *ag = liferea_shell_add_actions (gaction_entries, G_N_ELEMENTS (gaction_entries));

	// FIXME: action group state init

	g_signal_connect (G_OBJECT (item_history_get_instance ()), "changed", G_CALLBACK (shell_actions_update_history), ag);

	return ag;
}