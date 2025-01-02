/*
 * @file item_actions.c  item actions
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

#include "item_actions.h"

#include "common.h"
#include "debug.h"
#include "feedlist.h"
#include "itemlist.h"
#include "node.h"
#include "node_providers/feed.h"
#include "node_providers/newsbin.h"
#include "ui/itemview.h"
#include "ui/liferea_shell.h"
#include "ui/ui_common.h"

static void
on_launch_item_in_browser (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemPtr item = NULL;
	if (parameter)
		item = item_load (g_variant_get_uint64 (parameter));
	else
		item = itemlist_get_selected ();

	if (item) {
	      itemview_launch_item (item, ITEMVIEW_LAUNCH_INTERNAL);
	      item_unload (item);
	}
}

static void
on_launch_item_in_tab (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemPtr item = NULL;
	if (parameter)
		item = item_load (g_variant_get_uint64 (parameter));
	else
		item = itemlist_get_selected ();

	if (item) {
	      itemview_launch_item (item, ITEMVIEW_LAUNCH_TAB);
	      item_unload (item);
	}
}

static void
on_launch_item_in_external_browser (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemPtr item = NULL;
	if (parameter)
		item = item_load (g_variant_get_uint64 (parameter));
	else
		item = itemlist_get_selected ();

	if (item) {
	      itemview_launch_item (item, ITEMVIEW_LAUNCH_EXTERNAL);
	      item_unload (item);
	}
}

static void
on_toggle_item_flag (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemPtr item = NULL;
	if (parameter)
		item = item_load (g_variant_get_uint64 (parameter));
	else
		item = itemlist_get_selected ();

	if (item) {
		itemlist_toggle_flag (item);
		item_unload (item);
	}
}

static void
on_toggle_unread_status (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemPtr item = NULL;
	if (parameter)
		item = item_load (g_variant_get_uint64 (parameter));
	else
		item = itemlist_get_selected ();

	if (item) {
		itemlist_toggle_read_status (item);
		item_unload (item);
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

static void
on_remove_item (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemPtr item = NULL;
	if (parameter)
		item = item_load (g_variant_get_uint64 (parameter));
	else
		item = itemlist_get_selected ();

	if (item) {
		itemview_select_item (NULL);
		itemlist_remove_item (item);
	} else {
		liferea_shell_set_important_status_bar (_("No item has been selected"));
	}
}

static void
on_copy_to_newsbin (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemPtr		item = NULL;
	guint32 	newsbin_index;
	guint64 	item_id;
	gboolean 	maybe_item_id;

	g_variant_get (parameter, "(umt)", &newsbin_index, &maybe_item_id, &item_id);
	if (maybe_item_id)
		item = item_load (item_id);
	else
		item = itemlist_get_selected ();

	newsbin_add_item (newsbin_index, item);
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

static const GActionEntry gaction_entries[] = {
	{"toggle-selected-item-read-status", on_toggle_unread_status, NULL, NULL, NULL},
	{"toggle-selected-item-flag", on_toggle_item_flag, NULL, NULL, NULL},
	{"remove-selected-item", on_remove_item, NULL, NULL, NULL},
	{"launch-selected-item-in-tab", on_launch_item_in_tab, NULL, NULL, NULL},
	{"launch-selected-item-in-browser", on_launch_item_in_browser, NULL, NULL, NULL},
	{"launch-selected-item-in-external-browser", on_launch_item_in_external_browser, NULL, NULL, NULL},
	{"copy-item-to-newsbin", on_copy_to_newsbin, "(umt)", NULL, NULL},
	{"toggle-item-read-status", on_toggle_unread_status, "t", NULL, NULL},
	{"toggle-item-flag", on_toggle_item_flag, "t", NULL, NULL},
	// FIXME: duplicate?
	{"remove-item", on_remove_item, "t", NULL, NULL},
	{"open-item-in-tab", on_launch_item_in_tab, "t", NULL, NULL},
	{"open-item-in-browser", on_launch_item_in_browser, "t", NULL, NULL},
	{"open-item-in-external-browser", on_launch_item_in_external_browser, "t", NULL, NULL},
	{"email-the-author", email_the_author, "t", NULL, NULL}
};

static void
item_actions_update (gpointer obj, gchar *unused, gpointer user_data)
{
        GActionGroup *ag = G_ACTION_GROUP (user_data);

        ui_common_action_group_enable (ag, itemlist_get_selected () != NULL);
}

GActionGroup *
item_actions_create (void)
{
        GActionGroup *ag = liferea_shell_add_actions (gaction_entries, G_N_ELEMENTS (gaction_entries));

        g_signal_connect (g_object_get_data (G_OBJECT (liferea_shell_get_instance ()), "feedlist"),
                          "items-updated",
                          G_CALLBACK (item_actions_update), ag);
	g_signal_connect (g_object_get_data (G_OBJECT (liferea_shell_get_instance ()), "itemlist"),
                          "item-updated",
                          G_CALLBACK (item_actions_update), ag);
	g_signal_connect (liferea_shell_lookup ("feedlist"),
                          "selection-changed",
                          G_CALLBACK (item_actions_update), ag);

	ui_common_action_group_enable (ag, FALSE);

	return ag;
}