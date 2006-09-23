/**
 * @file newsbin.c  news bin node type implementation
 * 
 * Copyright (C) 2006 Lars Lindner <lars.lindner@gmx.net>
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
#include "feed.h"
#include "feedlist.h"
#include "interface.h"
#include "newsbin.h"
#include "support.h"
#include "ui/ui_feedlist.h"

static GtkWidget *newnewsbindialog = NULL;
static GtkWidget *newsbinnamedialog = NULL;

static gchar * newsbin_render(nodePtr node) {
g_warning("newsbin_render()");
	return g_strdup("FIXME");
}

static void ui_newsbin_add(nodePtr parent) {
	GtkWidget	*nameentry;
	
	if(!newnewsbindialog || !G_IS_OBJECT(newnewsbindialog))
		newnewsbindialog = create_newnewsbindialog();

	nameentry = lookup_widget(newnewsbindialog, "nameentry");
	gtk_entry_set_text(GTK_ENTRY(nameentry), "");
		
	gtk_widget_show(newnewsbindialog);
}

void on_newnewsbinbtn_clicked(GtkButton *button, gpointer user_data) {
	nodePtr		newsbin;
	int		pos;
	
	newsbin = node_new();
	node_set_title(newsbin, (gchar *)gtk_entry_get_text(GTK_ENTRY(lookup_widget(newnewsbindialog, "nameentry"))));
	node_set_type(newsbin, newsbin_get_node_type());
	node_set_data(newsbin, (gpointer)feed_new("newsbin", NULL, 0));

	ui_feedlist_get_target_folder(&pos);
	node_add_child(feedlist_get_insertion_point(), newsbin, pos);
	ui_feedlist_select(newsbin);
}

static void ui_newsbin_properties(nodePtr node) {
	GtkWidget	*nameentry;
	
	if(!newsbinnamedialog || !G_IS_OBJECT(newsbinnamedialog))
		newsbinnamedialog = create_newsbinnamedialog();

	nameentry = lookup_widget(newsbinnamedialog, "nameentry");
	gtk_entry_set_text(GTK_ENTRY(nameentry), node_get_title(node));
		
	gtk_widget_show(newsbinnamedialog);
}

void newsbin_request_auto_update_dummy(nodePtr node) { }
void newsbin_request_update_dummy(nodePtr node, guint flags) { }

nodeTypePtr newsbin_get_node_type(void) {
	nodeTypePtr	nodeType;

	/* derive the plugin node type from the folder node type */
	nodeType = (nodeTypePtr)g_new0(struct nodeType, 1);
	nodeType->capabilities		= NODE_CAPABILITY_RECEIVE_ITEMS;
	nodeType->id			= "newsbin";
	nodeType->type			= NODE_TYPE_NEWSBIN;
	nodeType->import		= feed_get_node_type()->import;
	nodeType->export		= feed_get_node_type()->export;
	nodeType->initial_load		= feed_get_node_type()->initial_load;
	nodeType->load			= feed_get_node_type()->load;
	nodeType->save			= feed_get_node_type()->save;
	nodeType->unload		= feed_get_node_type()->unload;
	nodeType->reset_update_counter	= feed_get_node_type()->reset_update_counter;
	nodeType->request_update	= newsbin_request_update_dummy;
	nodeType->request_auto_update	= newsbin_request_auto_update_dummy;
	nodeType->remove		= feed_get_node_type()->remove;
	nodeType->mark_all_read		= feed_get_node_type()->mark_all_read;
	nodeType->render		= newsbin_render;
	nodeType->request_add		= ui_newsbin_add;
	nodeType->request_properties	= ui_newsbin_properties;

	return nodeType; 
}
