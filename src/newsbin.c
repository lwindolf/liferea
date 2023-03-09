/**
 * @file newsbin.c  news bin node type implementation
 *
 * Copyright (C) 2006-2016 Lars Windolf <lars.windolf@gmx.de>
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

#include "newsbin.h"

#include <gtk/gtk.h>

#include "common.h"
#include "db.h"
#include "feed.h"
#include "feedlist.h"
#include "itemlist.h"
#include "metadata.h"
#include "render.h"
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
newsbin_import (nodePtr node, nodePtr parent, xmlNodePtr cur, gboolean trusted)
{
	xmlChar		*tmp;
	feed_get_node_type ()->import (node, parent, cur, trusted);

	/* but we don't need a subscription (created by feed_import()) */
	g_free (node->subscription);
	node->subscription = NULL;

	tmp = xmlGetProp (cur, BAD_CAST"alwaysShowInReducedMode");
	if (tmp && !xmlStrcmp (tmp, BAD_CAST"true"))
		((feedPtr)node->data)->alwaysShowInReduced = TRUE;
	xmlFree (tmp);

	((feedPtr)node->data)->cacheLimit = CACHE_UNLIMITED;

	newsbin_list = g_slist_append(newsbin_list, node);
}


static void
newsbin_export (nodePtr node, xmlNodePtr xml, gboolean trusted)
{
	feedPtr feed = (feedPtr) node->data;

	if (trusted) {
		if (feed->alwaysShowInReduced)
			xmlNewProp (xml, BAD_CAST"alwaysShowInReducedMode", BAD_CAST"true");
	}
}


static void
newsbin_remove (nodePtr node)
{
	newsbin_list = g_slist_remove(newsbin_list, node);
	feed_get_node_type()->remove(node);
}

static gchar *
newsbin_render (nodePtr node)
{
	gchar		*output = NULL;
	xmlDocPtr	doc;

	doc = feed_to_xml(node, NULL);
	output = render_xml(doc, "newsbin", NULL);
	xmlFreeDoc(doc);

	return output;
}


static void
on_newsbin_common_btn_clicked (GtkButton *button, gpointer user_data)
{
	GtkWidget	*dialog = gtk_widget_get_toplevel (GTK_WIDGET (button));
	GtkWidget	*nameentry = liferea_dialog_lookup (dialog, "newsbinnameentry");
	GtkWidget	*showinreduced = liferea_dialog_lookup (dialog, "newsbinalwaysshowinreduced");
	nodePtr		newsbin = (nodePtr) user_data;
	gboolean	newly_created = FALSE;

	if (newsbin == NULL) {
		newsbin = node_new (newsbin_get_node_type ());
		node_set_data (newsbin, (gpointer)feed_new ());
		newsbin_list = g_slist_append(newsbin_list, newsbin);
		newly_created = TRUE;
	}

	node_set_title (newsbin, (gchar *)gtk_entry_get_text (GTK_ENTRY (nameentry)));
	if (newsbin->data) {
		((feedPtr)newsbin->data)->alwaysShowInReduced = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (showinreduced));
	}
	if (newly_created) {
		feedlist_node_added (newsbin);
	}

	feedlist_schedule_save ();
	gtk_widget_destroy (dialog);
}


static gboolean
ui_newsbin_common (nodePtr node)
{
	GtkWidget	*dialog = liferea_dialog_new ("new_newsbin");
	GtkWidget	*nameentry = liferea_dialog_lookup (dialog, "newsbinnameentry");
	GtkWidget	*showinreduced = liferea_dialog_lookup (dialog, "newsbinalwaysshowinreduced");
	GtkWidget	*okbutton = liferea_dialog_lookup (dialog, "newnewsbinbtn");

	if (node) {
		gtk_window_set_title (GTK_WINDOW (dialog), _("News Bin Properties"));
		gtk_entry_set_text (GTK_ENTRY (nameentry), node_get_title (node));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (showinreduced), ((feedPtr)node->data)->alwaysShowInReduced);
	} else {
		gtk_window_set_title (GTK_WINDOW (dialog), _("Create News Bin"));
		gtk_entry_set_text (GTK_ENTRY (nameentry), "");
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (showinreduced), FALSE);
	}

	g_signal_connect (G_OBJECT (okbutton), "clicked",
	                  G_CALLBACK (on_newsbin_common_btn_clicked), node);

	gtk_window_present (GTK_WINDOW (dialog));

	return TRUE;
}


static gboolean
ui_newsbin_add (void)
{
	return ui_newsbin_common(NULL);
}


static void
ui_newsbin_properties (nodePtr node)
{
	ui_newsbin_common(node);
}


void
on_action_copy_to_newsbin (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	nodePtr		newsbin;
	itemPtr		item = NULL, copy;
	guint32 	newsbin_index;
	guint64 	item_id;
	gboolean 	maybe_item_id;

	g_variant_get (parameter, "(umt)", &newsbin_index, &maybe_item_id, &item_id);
	if (maybe_item_id)
		item = item_load (item_id);
	else
		item = itemlist_get_selected();

	newsbin = g_slist_nth_data(newsbin_list, newsbin_index);
	if(item) {
		copy = item_copy(item);
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
}

nodeTypePtr
newsbin_get_node_type (void)
{
	static nodeTypePtr	nodeType;

	if (!nodeType) {
		/* derive the plugin node type from the folder node type */
		nodeType = g_new0 (struct nodeType, 1);
		nodeType->capabilities		= NODE_CAPABILITY_RECEIVE_ITEMS |
		                                  NODE_CAPABILITY_SHOW_UNREAD_COUNT |
		                                  NODE_CAPABILITY_SHOW_ITEM_COUNT |
		                                  NODE_CAPABILITY_EXPORT_ITEMS;
		nodeType->id			= "newsbin";
		nodeType->icon			= ICON_NEWSBIN;
		nodeType->load			= feed_get_node_type()->load;
		nodeType->import		= newsbin_import;
		nodeType->export		= newsbin_export;
		nodeType->save			= feed_get_node_type()->save;
		nodeType->update_counters	= feed_get_node_type()->update_counters;
		nodeType->remove		= newsbin_remove;
		nodeType->render		= newsbin_render;
		nodeType->request_add		= ui_newsbin_add;
		nodeType->request_properties	= ui_newsbin_properties;
		nodeType->free			= feed_get_node_type()->free;
	}

	return nodeType;
}
