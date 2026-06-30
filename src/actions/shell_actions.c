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
#include "liferea_application.h"
#include "net_monitor.h"
#include "social.h"
#include "node_source.h"
#include "node_providers/newsbin.h"
#include "node_providers/vfolder.h"
#include "ui/browser_tabs.h"
#include "ui/feed_list_view.h"
#include "ui/icons.h"
#include "ui/item_list_view.h"
#include "ui/liferea_browser.h"
#include "ui/liferea_dialog.h"
#include "ui/liferea_shell.h"
#include "ui/preferences_dialog.h"
#include "ui/search_dialog.h"
#include "ui/ui_common.h"
#include "webkit/liferea_web_view.h"

static void
do_menu_update (Node *node)
{
	if (network_monitor_is_online ())
		node_auto_update_subscription (node, GINT_TO_POINTER (UPDATE_REQUEST_PRIORITY_HIGH));
	else
		liferea_shell_set_status_bar (_("Liferea is in offline mode. No update possible."));

}

static void
on_menu_update_all(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	do_menu_update (feedlist_get_root ());
}

static void
on_feedlist_reduced_activate (GAction *action, GParamSpec *pspec, gpointer user_data)
{
	GVariant *state = g_action_get_state (G_ACTION (action));
	feed_list_view_set_reduce_mode (g_variant_get_boolean (state));
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
on_about_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GtkWidget *dialog;

	dialog = liferea_dialog_new ("about");
	gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (dialog), VERSION);
	gtk_window_present (GTK_WINDOW (dialog));
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
        browser_launch_URL_external ("https://lwindolf.github.io/rss-finder/");
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

static gboolean
update_monitor_refresh_cb (gpointer user_data)
{
	LifereaBrowser *b = browser_tabs_get_tab ("Update Monitor");
	if (!b)
		return FALSE; 	// probably closed by user, stop callback

	g_autofree gchar *json = feedlist_to_json ();
        liferea_browser_update_view (b, json);
	return TRUE;
}

static void
on_menu_show_update_monitor (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	LifereaBrowser *b = browser_tabs_focus_tab ("Update Monitor");
	if (b)
		return;
	
	b = browser_tabs_add_new (NULL, _("Update Monitor"), TRUE);
	browser_tabs_set_tab_name (b, "Update Monitor");

	g_autofree gchar *json = feedlist_to_json ();
        liferea_browser_set_view (b, "update_monitor", json, "file://", NULL);

	g_timeout_add (1000, update_monitor_refresh_cb, NULL);
}

static void
ui_popup_sort_feeds (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	feed_list_view_sort_folder ((Node *)user_data);
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
	{"import-feed-list", on_menu_import, NULL, NULL, NULL},
	{"export-feed-list", on_menu_export, NULL, NULL, NULL},
	{"quit", on_menu_quit, NULL, NULL, NULL},
	{"prev-read-item", on_prev_read_item_activate, NULL, NULL, NULL},
	{"next-read-item", on_next_read_item_activate, NULL, NULL, NULL},
	{"zoom-in", on_zoomin_activate, NULL, NULL, NULL},
	{"zoom-out", on_zoomout_activate, NULL, NULL, NULL},
	{"zoom-reset", on_zoomreset_activate, NULL, NULL, NULL},
	{"show-update-monitor", on_menu_show_update_monitor, NULL, NULL, NULL},
	{"show-preferences", on_prefbtn_clicked, NULL, NULL, NULL},
	{"search-feeds", on_searchbtn_clicked, NULL, NULL, NULL},
	{"discover-feeds", on_discover_feeds_activate, NULL, NULL, NULL},
	{"show-help-contents", on_topics_activate, NULL, NULL, NULL},
	{"show-shortcuts", on_shortcuts_activate, NULL, NULL, NULL},
	{"show-about", on_about_activate, NULL, NULL, NULL},
	{"print", on_print_activate, NULL, NULL, NULL},

	/* Parameter type must be NULL for toggle. */
	{"fullscreen", NULL, NULL, "@b false", on_menu_fullscreen_activate},

	{"node-sort-feeds", ui_popup_sort_feeds, NULL, NULL, NULL}
};

static void
shell_actions_update_history (gpointer obj, gpointer user_data)
{
	liferea_shell_action_enable ("prev-read-item", item_history_has_previous ());
	liferea_shell_action_enable ("next-read-item", item_history_has_next ());
}

GActionGroup *
shell_actions_create (LifereaShell *shell)
{
	GActionGroup *ag = liferea_shell_add_actions (gaction_entries, G_N_ELEMENTS (gaction_entries));

	g_signal_connect (G_OBJECT (item_history_get_instance ()), "changed", G_CALLBACK (shell_actions_update_history), NULL);

	shell_actions_update_history (NULL, NULL);

	/* Prepare some toggle button states */
	GtkApplication *app = gtk_window_get_application (GTK_WINDOW (liferea_shell_get_window ()));
	GAction *action = g_settings_create_action (conf_get_settings (), "reduced-feedlist");
	g_action_map_add_action (G_ACTION_MAP (app), action);
	g_signal_connect (action, "notify::state", G_CALLBACK (on_feedlist_reduced_activate), action);
	on_feedlist_reduced_activate (G_ACTION (action), NULL, NULL);

	return ag;
}