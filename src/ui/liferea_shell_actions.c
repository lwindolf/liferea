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
#include <gtk/gtk.h>

#include "browser.h"
#include "common.h"
#include "conf.h"
#include "debug.h"
#include "export.h"
#include "feedlist.h"
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
#include "ui/itemview.h"
#include "ui/item_list_view.h"
#include "ui/liferea_dialog.h"
#include "ui/liferea_shell.h"
#include "ui/preferences_dialog.h"
#include "ui/search_dialog.h"
#include "ui/ui_common.h"
#include "ui/ui_update.h"

/* This file contains all

        - accelerators
        - actions
        - and their callbacks
        
   Everything is registered against the LifereaShell window. */


/* all action callbacks */

void
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
	/* We must apply the zoom either to the item view
	   or to an open tab, depending on the browser tabs
	   GtkNotebook page that is active... */
	if (!browser_tabs_get_active_htmlview ())
		itemview_do_zoom (zoom);
	else
		browser_tabs_do_zoom (zoom);
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

/* sensitivity callbacks */

void
liferea_shell_update_history_actions (void)
{
	ui_common_action_enable (shell->generalActions, "prev-read-item", item_history_has_previous ());
	ui_common_action_enable (shell->generalActions, "next-read-item", item_history_has_next ());
}

static const GActionEntry gaction_entries[] = {
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

	{"node-sort-feeds", ui_popup_sort_feeds, NULL, NULL, NULL}
};

