/**
 * @file fl_plugin.c generic feedlist provider implementation
 * 
 * Copyright (C) 2005 Lars Lindner <lars.lindner@gmx.net>
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

#include <gmodule.h>
#include <gtk/gtk.h>
#include <string.h>
#include "debug.h"
#include "plugin.h"
#include "support.h"
#include "fl_providers/fl_plugin.h"
#include "fl_providers/fl_plugin-ui.h"

flPluginInfo * fl_plugins_get_root(void) {
	gboolean	found = FALSE;
	flPluginInfo	*fpi;
	pluginInfo	*pi;
	GSList		*iter;

	debug_enter("fl_plugins_get_root");

	/* scan for root flag and return plugin if found */
	iter = plugin_mgmt_get_list();
	while(NULL != iter) {
		pi = (pluginInfo *)iter->data;
		if(pi->type == PLUGIN_TYPE_FEEDLIST_PROVIDER) {
			fpi = pi->symbols;
			debug2(DEBUG_VERBOSE, "%s capabilities=%ld", fpi->name, fpi->capabilities);
			if(0 != (fpi->capabilities & FL_PLUGIN_CAPABILITY_IS_ROOT)) {
				found = TRUE;
				break;
			}
			iter = g_slist_next(iter);
		}
	}
	
	if(FALSE == found) 
		g_error("No root capable feed list provider plugin found!");

	debug_exit("fl_plugins_get_root");

	return fpi;
}

typedef	flPluginInfo* (*infoFunc)();

void fl_plugin_load(pluginInfo *pi, GModule *handle) {
	flPluginInfo	*fpi;
	infoFunc	fl_plugin_get_info;

	if(g_module_symbol(handle, "fl_plugin_get_info", (void*)&fl_plugin_get_info)) {
		/* load feed list provider plugin info */
		if(NULL == (fpi = (*fl_plugin_get_info)()))
			return;
	}

	/* check feed list provider plugin version */
	if(FL_PLUGIN_API_VERSION != fpi->api_version) {
		debug3(DEBUG_PLUGINS, "feed list API version mismatch: \"%s\" has version %d should be %d\n", fpi->name, fpi->api_version, FL_PLUGIN_API_VERSION);
		return;
	} 

	/* check if all mandatory symbols are provided */
	if(!((NULL != fpi->plugin_init) &&
	     (NULL != fpi->plugin_deinit)))
		return;

	debug1(DEBUG_PLUGINS, "found feed list plugin: %s", fpi->name);

	/* allow the plugin to initialize */
	(*fpi->plugin_init)();

	/* assign the symbols so the caller will accept the plugin */
	pi->symbols = fpi;
}

void fl_plugin_import(nodePtr np, xmlNodePtr cur) {
	GSList		*iter;
	flPluginInfo	*fpi;
	pluginInfo	*pi;
	xmlChar		*typeStr = NULL;
	gboolean	found = FALSE;

	debug_enter("fl_plugin_import");

	if(NULL != (typeStr = xmlGetProp(cur, BAD_CAST"pluginType"))) {
		debug2(DEBUG_CACHE, "creating feed list plugin instance (type=%s,id=%s)\n", typeStr, np->id);

		np->available = FALSE;

		/* scan for matching plugin and create new instance */
		iter = plugin_mgmt_get_list();
		while(NULL != iter) {
			pi = (pluginInfo *)iter->data;
			if(pi->type == PLUGIN_TYPE_FEEDLIST_PROVIDER) {
				fpi = pi->symbols;
				if(!strcmp(fpi->id, typeStr)) {
					np->type = FST_PLUGIN;
					np->available = TRUE;
					np->handler = NULL;	/* not handled by parent plugin */
					fpi->handler_import(np);
					found = TRUE;
				}
				iter = g_slist_next(iter);
			}
		}

		if(!found)
			g_warning("Could not find plugin handler for type \"%s\"\n!", typeStr);
	} else {
		g_warning("No plugin type given for node \"%s\"!", node_get_title(np));
	}

	debug_exit("flplugin_import");
}

void fl_plugin_export(nodePtr np, xmlNodePtr cur) {

	debug_enter("fl_plugin_export");

	debug2(DEBUG_CACHE, "plugin export for node %s, id=%s\n", np->title, FL_PLUGIN(np)->id);
	xmlNewProp(cur, BAD_CAST"pluginType", BAD_CAST(FL_PLUGIN(np)->id));

	debug_exit("fl_plugin_export");
}

/* plugin instance creation dialog */

static void on_fl_plugin_type_selected(GtkDialog *dialog, gint response_id, gpointer user_data) {
	GtkTreeSelection	*selection;
	GtkTreeModel		*model;
	GtkTreeIter		iter;
	nodePtr 		np = (nodePtr)user_data;
	flPluginInfo		*fpi;

	if(response_id == GTK_RESPONSE_OK) {
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(lookup_widget(GTK_WIDGET(dialog), "plugin_type_list")));
		g_assert(NULL != selection);
		gtk_tree_selection_get_selected(selection, &model, &iter);
		gtk_tree_model_get(model, &iter, 1, &fpi, -1);
		fpi->handler_new(np);
	}
	
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

void ui_fl_plugin_type_dialog(nodePtr np) {
	GtkWidget 		*dialog, *treeview;
	GtkTreeStore		*treestore;
	GtkCellRenderer		*renderer;
	GtkTreeViewColumn 	*column;
	GtkTreeIter		treeiter;
	GSList		*iter;
	flPluginInfo	*fpi;
	pluginInfo	*pi;

	/* set up the dialog */
	dialog = create_fl_plugin_type_dialog();

	treestore = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_POINTER);
	/* add available feed list plugins to treestore */
	iter = plugin_mgmt_get_list();
	while(NULL != iter) {
		pi = (pluginInfo *)iter->data;
		fpi = pi->symbols;
		if((pi->type == PLUGIN_TYPE_FEEDLIST_PROVIDER) &&
		   (fpi->capabilities & FL_PLUGIN_CAPABILITY_DYNAMIC_CREATION)) {

			gtk_tree_store_append(treestore, &treeiter, NULL);
			gtk_tree_store_set(treestore, &treeiter, 
			                              0, pi->name, 
			                              1, fpi,
						      -1);
		}
		iter = g_slist_next(iter);
	}

	treeview = lookup_widget(dialog, "plugin_type_list");
	g_assert(NULL != treeview);

	column = gtk_tree_view_column_new();
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), -1, "Plugin Type", renderer, "text", 0, NULL);
	gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(treestore));
	g_object_unref(treestore);

	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)),
	                            GTK_SELECTION_SINGLE);

	g_signal_connect(G_OBJECT(dialog), "response",
			 G_CALLBACK(on_fl_plugin_type_selected), 
			 (gpointer)np);

	gtk_widget_show_all(dialog);
}
