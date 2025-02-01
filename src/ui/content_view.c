/*
 * @file content_view.c  presenting items and feeds in HTML
 *
 * Copyright (C) 2006-2025 Lars Windolf <lars.windolf@gmx.de>
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

#include "content_view.h"

#include "common.h"
#include "ui/liferea_browser.h"

/*
 The content view renders either node or item content and updates
 automatically when the user selects a new item or node. The content
 view is a singleton instance and is created by the layout module.

 For simplicity we do not update presented content even if it is
 updated in background.
 */

struct _ContentView {
        LifereaBrowser parentInstance;
};

G_DEFINE_TYPE (ContentView, content_view, LIFEREA_BROWSER_TYPE)

static void
content_view_class_init (ContentViewClass *klass)
{
}

static void
content_view_init (ContentView *contentView)
{
}

static void
content_view_item_selected (GObject *obj, gint itemId, gpointer user_data)
{
        ContentView             *cv = CONTENT_VIEW (user_data);
        itemPtr                 item = item_load (itemId);
        Node			*node = NULL;
	g_autofree gchar	*baseURL = NULL;
	g_autofree gchar 	*json = NULL;
	const gchar		*direction = NULL;

        if (!item)
                return;

        node = node_from_id (item->nodeId);

        direction = item_get_text_direction (item);
        json = item_to_json (item);

        if (node_get_base_url (node))
                baseURL = g_markup_escape_text ((gchar *)node_get_base_url (node), -1);

        liferea_browser_set_view (LIFEREA_BROWSER (cv), "item", json, baseURL, direction);
}

static void
content_view_node_selected (GObject *obj, gchar *nodeId, gpointer user_data)
{
        ContentView             *cv = CONTENT_VIEW (user_data);
        Node			*node = node_from_id (nodeId);
        g_autofree gchar	*baseURL = NULL;
	g_autofree gchar 	*json = NULL;
	const gchar		*direction = NULL;

        if (!node)
                return;

        direction = common_get_app_direction ();
        json = node_to_json (node);

        if (node_get_base_url (node))
                baseURL = g_markup_escape_text ((gchar *)node_get_base_url (node), -1);

        liferea_browser_set_view (LIFEREA_BROWSER (cv), "node", json, baseURL, direction);
}

ContentView *
content_view_create (FeedList *feedlist, ItemList *itemlist)
{
        ContentView *cv = g_object_new (CONTENT_VIEW_TYPE, NULL);

        g_signal_connect (itemlist, "item-selected", G_CALLBACK (content_view_item_selected), cv);
        g_signal_connect (feedlist, "node-selected", G_CALLBACK (content_view_node_selected), cv);

        return cv;
}