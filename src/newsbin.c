/**
 * @file newsbin.c  news bin node type implementation
 * 
 * Copyright (C) 2006-2007 Lars Lindner <lars.lindner@gmail.com>
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

#include <gtk/gtk.h>

#include "common.h"
#include "db.h"
#include "feed.h"
#include "feedlist.h"
#include "itemlist.h"
#include "newsbin.h"
#include "render.h"
#include "vfolder.h"
#include "ui/ui_dialog.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_node.h"
#include "ui/ui_popup.h"

static GtkWidget *newnewsbindialog = NULL;
static GSList * newsbin_list = NULL;

GSList *
newsbin_get_list (void)
{
	return newsbin_list;
}

static void
newsbin_new(nodePtr node)
{
	feedlist_schedule_save();
}

static void
newsbin_import (nodePtr node, nodePtr parent, xmlNodePtr cur, gboolean trusted)
{
	feed_get_node_type ()->import (node, parent, cur, trusted);
	
	/* but we don't need a subscription (created by feed_import()) */
	g_free (node->subscription);
	node->subscription = NULL;
	
	((feedPtr)node->data)->cacheLimit = CACHE_UNLIMITED;
	
	newsbin_list = g_slist_append(newsbin_list, node);
}

static void
newsbin_remove (nodePtr node)
{
	newsbin_list = g_slist_remove(newsbin_list, node);
	feed_get_node_type()->remove(node);
	ui_popup_update_menues();
}

static gchar *
newsbin_render (nodePtr node)
{
	renderParamPtr	params;
	gchar		*output = NULL;
	xmlDocPtr	doc;

	doc = feed_to_xml(node, NULL);
	params = render_parameter_new();
	render_parameter_add(params, "pixmapsDir='file://" PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S "'");
	output = render_xml(doc, "newsbin", params);
	xmlFreeDoc(doc);

	return output;
}

static void
ui_newsbin_add (nodePtr parent)
{
	GtkWidget	*nameentry;
	
	if (!newnewsbindialog || !G_IS_OBJECT (newnewsbindialog))
		newnewsbindialog = liferea_dialog_new (NULL, "newnewsbindialog");

	nameentry = liferea_dialog_lookup(newnewsbindialog, "nameentry");
	gtk_entry_set_text(GTK_ENTRY(nameentry), "");
		
	gtk_widget_show(newnewsbindialog);
}

void 
on_newnewsbinbtn_clicked (GtkButton *button, gpointer user_data)
{
	nodePtr		newsbin;
	int		pos;
	
	newsbin = node_new();
	node_set_title(newsbin, (gchar *)gtk_entry_get_text(GTK_ENTRY(liferea_dialog_lookup(newnewsbindialog, "nameentry"))));
	node_set_type(newsbin, newsbin_get_node_type());
	node_set_data(newsbin, (gpointer)feed_new());
	newsbin_new(newsbin);

	ui_feedlist_get_target_folder(&pos);
	node_add_child(feedlist_get_insertion_point(), newsbin, pos);
	feedlist_schedule_save();
	ui_feedlist_select(newsbin);
	
	newsbin_list = g_slist_append(newsbin_list, newsbin);
	ui_popup_update_menues();
}

void 
on_popup_copy_to_newsbin (gpointer user_data, guint callback_action, GtkWidget *widget)
{
	nodePtr		newsbin;
	itemPtr		item, copy;

	newsbin = g_slist_nth_data(newsbin_list, callback_action);
	item = itemlist_get_selected();
	if(item) {
		copy = item_copy(item);
		copy->nodeId = newsbin->id;	/* necessary to become independent of original item */
		
		/* To avoid item doubling in vfolders we reset
		   simple vfolder match attributes */
		copy->readStatus = TRUE;
		copy->flagStatus = FALSE;
		
		/* To provide a hint in the rendered output what the orginial 
		   feed was the original website link/title are added */		
		if(!copy->real_source_url)
			copy->real_source_url = g_strdup(node_get_base_url(node_from_id(item->nodeId)));
		if(!copy->real_source_title)
			copy->real_source_title = g_strdup(node_get_title(node_from_id(item->nodeId)));
		
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
		nodeType = (nodeTypePtr)g_new0(struct nodeType, 1);
		nodeType->capabilities		= NODE_CAPABILITY_RECEIVE_ITEMS |
		                                  NODE_CAPABILITY_SHOW_UNREAD_COUNT |
		                                  NODE_CAPABILITY_SHOW_ITEM_COUNT;
		nodeType->id			= "newsbin";
		nodeType->icon			= icons[ICON_NEWSBIN];
		nodeType->load			= feed_get_node_type()->load;		
		nodeType->import		= newsbin_import;
		nodeType->export		= feed_get_node_type()->export;
		nodeType->save			= feed_get_node_type()->save;
		nodeType->update_counters	= feed_get_node_type()->update_counters;
		nodeType->remove		= newsbin_remove;
		nodeType->render		= newsbin_render;
		nodeType->request_add		= ui_newsbin_add;
		nodeType->request_properties	= ui_node_rename;
		nodeType->free			= feed_get_node_type()->free;
	}

	return nodeType; 
}
