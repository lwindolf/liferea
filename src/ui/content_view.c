/*
 * @file content_view.c  presenting items and feed info in HTML
 *
 * Copyright (C) 2006-2026 Lars Windolf <lars.windolf@gmx.de>
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

#include "comments.h"
#include "common.h"
#include "node_source.h"
#include "ui/liferea_browser.h"

/*
 The content view renders either node or item content and updates
 automatically when the user selects a new item or node. The content
 view is a singleton instance and is created by the layout module.

 For items with comment feeds the content view will trigger the comments
 fetching and update upon "item-updated" signal.
 */

struct _ContentView {
        LifereaBrowser parentInstance;

        gint    mode; /* 0 = none, 1 = node, 2 = item */
        gulong  currentItemId;
        gchar   *currentNodeId;
};

G_DEFINE_TYPE (ContentView, content_view, LIFEREA_BROWSER_TYPE)

static void
content_view_class_init (ContentViewClass *klass)
{
}

static void
content_view_init (ContentView *cv)
{
        cv->mode = 0;
        cv->currentItemId = 0;
        cv->currentNodeId = NULL;
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

        cv->mode = 2;

        if (item->id == cv->currentItemId)
                return;

        cv->currentItemId = item->id;

        comments_refresh (item);        // FIXME: do we want to debounce this for fast skipping?
        node = node_from_id (item->nodeId);

        direction = item_get_text_direction (item);
        json = item_to_json (item);

        if (node_get_base_url (node))
                baseURL = g_markup_escape_text ((gchar *)node_get_base_url (node), -1);

        liferea_browser_set_view (LIFEREA_BROWSER (cv), "item", json, baseURL, direction);
}

static void
content_view_item_updated (GObject *obj, gint itemId, gpointer user_data)
{
        ContentView             *cv = CONTENT_VIEW (user_data);
        itemPtr                 item = item_load (itemId);
        g_autofree gchar 	*json = NULL;

        if (!item)
                return;

        // this bail out is needed for the "item-updated" signal case
        // which we only want to process in case the currently displayed
        // item matches
        if (cv->mode != 2 || item->id != cv->currentItemId)
                return;

        json = item_to_json (item);
        liferea_browser_update_view (LIFEREA_BROWSER (cv), json);
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
        
        cv->mode = 1;

        if (cv->currentNodeId && g_str_equal (cv->currentNodeId, nodeId))
                return;
        cv->currentNodeId = g_strdup (nodeId);

        direction = common_get_app_direction ();
        json = node_to_json (node);

        if (node_get_base_url (node))
                baseURL = g_markup_escape_text ((gchar *)node_get_base_url (node), -1);

        liferea_browser_set_view (LIFEREA_BROWSER (cv), "node", json, baseURL, direction);
}

static void
content_view_node_updated (GObject *obj, gchar *nodeId, gpointer user_data)
{
        ContentView             *cv = CONTENT_VIEW (user_data);
        Node			*node = node_from_id (nodeId);
        g_autofree gchar 	*json = NULL;

        if (!node)
                return;

        // this bail out is needed for the "node-updated" signal case
        // which we only want to process in case the currently displayed
        // node matches
        if (cv->mode != 1 || g_str_equal (cv->currentNodeId, nodeId) == 0)
                return;

        json = node_to_json (node);
        liferea_browser_update_view (LIFEREA_BROWSER (cv), json);
}

ContentView *
content_view_create (FeedList *feedlist, ItemList *itemlist)
{
        ContentView *cv = g_object_new (CONTENT_VIEW_TYPE, NULL);

        g_signal_connect (itemlist, "item-selected", G_CALLBACK (content_view_item_selected), cv);
        g_signal_connect (itemlist, "item-updated", G_CALLBACK (content_view_item_updated), cv);
        g_signal_connect (feedlist, "node-selected", G_CALLBACK (content_view_node_selected), cv);
        g_signal_connect (feedlist, "node-updated", G_CALLBACK (content_view_node_updated), cv);

        return cv;
}