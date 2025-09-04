/**
 * @file newsbin.c  news bin node type implementation
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

#include "node_providers/newsbin.h"

#include <gtk/gtk.h>

#include "common.h"
#include "db.h"
#include "node.h"
#include "node_provider.h"
#include "node_providers/feed.h"
#include "feedlist.h"
#include "itemlist.h"
#include "metadata.h"
#include "ui/icons.h"
#include "ui/feed_list_view.h"
#include "ui/liferea_dialog.h"

static GSList * newsbin_list = NULL;

GSList *
newsbin_get_list (void)
{
	return newsbin_list;
}

static void
newsbin_import (Node *node, Node *parent, xmlNodePtr cur, gboolean trusted)
{
	feed_get_provider ()->import (node, parent, cur, trusted);

	node->subscription->cacheLimit = CACHE_UNLIMITED;

	newsbin_list = g_slist_append (newsbin_list, node);
}

static void
newsbin_export (Node *node, xmlNodePtr xml, gboolean trusted)
{
}

static void
newsbin_remove (Node *node)
{
	newsbin_list = g_slist_remove(newsbin_list, node);
	feed_get_provider()->remove(node);
}

static void
on_newsbin_common_btn_clicked (GtkDialog *dialog, gint response_id, gpointer user_data)
{
	GtkWidget	*showinreduced = liferea_dialog_lookup (GTK_WIDGET (dialog), "newsbinalwaysshowinreduced");
	Node		*newsbin = (Node *)user_data;
	gboolean	newly_created = FALSE;

	if (response_id != GTK_RESPONSE_OK)
		return;

	if (!newsbin) {
		newsbin = node_new ("newsbin");
		newsbin_list = g_slist_append(newsbin_list, newsbin);
		node_set_subscription (newsbin, subscription_new (NULL, NULL, NULL));
		newly_created = TRUE;
	}

	node_set_title (newsbin, liferea_dialog_entry_get (GTK_WIDGET (dialog), "newsbinnameentry"));
	newsbin->subscription->alwaysShowInReduced = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (showinreduced));

	if (newly_created)
		feedlist_node_added (newsbin);

	feedlist_schedule_save ();
}

static gboolean
ui_newsbin_common (Node *node)
{
	GtkWidget	*dialog = liferea_dialog_new ("new_newsbin");
	GtkWidget	*showinreduced = liferea_dialog_lookup (GTK_WIDGET (dialog), "newsbinalwaysshowinreduced");

	if (node) {
		gtk_window_set_title (GTK_WINDOW (dialog), _("News Bin Properties"));
		liferea_dialog_entry_set (dialog, "newsbinnameentry", node_get_title (node));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (showinreduced), node->subscription->alwaysShowInReduced);
	} else {
		gtk_window_set_title (GTK_WINDOW (dialog), _("Create News Bin"));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (showinreduced), FALSE);
	}

	g_signal_connect (dialog, "response",
	                  G_CALLBACK (on_newsbin_common_btn_clicked), node);

	gtk_window_present (GTK_WINDOW (dialog));

	return TRUE;
}

static gboolean
ui_newsbin_add (void)
{
	return ui_newsbin_common (NULL);
}

static void
ui_newsbin_properties (Node *node)
{
	ui_newsbin_common (node);
}

void
newsbin_add_item (guint32 newsbin_index, itemPtr item) {
	Node *newsbin = (Node *) g_slist_nth_data (newsbin_list, newsbin_index);

	if (!item || !newsbin)
		return;	

	itemPtr	copy = item_copy(item);
	copy->nodeId = newsbin->id;	/* necessary to become independent of original item */
	copy->parentNodeId = g_strdup (item->nodeId);

	/* To avoid item doubling in vfolders we reset
		simple vfolder match attributes */
	copy->readStatus = TRUE;
	copy->flagStatus = FALSE;

	/* To provide a hint in the rendered output what the orginial
		feed was the original website link/title are added */
	if(!metadata_list_get (copy->metadata, "realSourceUrl"))
		metadata_list_set (&(copy->metadata), "realSourceUrl", node_get_base_url(node_from_id(item->nodeId)));
	if(!metadata_list_get (copy->metadata, "realSourceTitle"))
		metadata_list_set (&(copy->metadata), "realSourceTitle", node_get_title(node_from_id(item->nodeId)));

	/* do the same as in node_merge_item(s) */
	db_item_update(copy);
	node_update_counters(newsbin);
}

nodeProviderPtr
newsbin_get_provider (void)
{
	static nodeProviderPtr	nodeType;

	if (!nodeType) {
		/* derive the plugin node type from the folder node type */
		nodeType = g_new0 (struct nodeProvider, 1);
		nodeType->capabilities		= NODE_CAPABILITY_RECEIVE_ITEMS |
		                                  NODE_CAPABILITY_SHOW_UNREAD_COUNT |
		                                  NODE_CAPABILITY_SHOW_ITEM_COUNT |
		                                  NODE_CAPABILITY_EXPORT_ITEMS;
		nodeType->id			= "newsbin";
		nodeType->icon			= ICON_NEWSBIN;
		nodeType->load			= feed_get_provider()->load;
		nodeType->import		= newsbin_import;
		nodeType->export		= newsbin_export;
		nodeType->save			= feed_get_provider()->save;
		nodeType->update_counters	= feed_get_provider()->update_counters;
		nodeType->remove		= newsbin_remove;
		nodeType->request_add		= ui_newsbin_add;
		nodeType->request_properties	= ui_newsbin_properties;
		nodeType->free			= feed_get_provider()->free;
	}

	return nodeType;
}
