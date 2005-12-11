/**
 * @file fl_opml.c default static feedlist provider
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

#include "support.h"
#include "debug.h"
#include "fl_providers/fl_opml.h"
#include "fl_providers/fl_plugin.h"

static void fl_opml_node_load(nodePtr np) {

	if(FST_PLUGIN == np->type) {
		/* there should be nothing to do */
		return;
	}

	fl_default_node_load(np);
}

static void fl_opml_node_unload(nodePtr np) {

	if(FST_PLUGIN == np->type) {
		/* there should be nothing to do */
		return;
	}

	fl_default_node_unload(np);
}

static gchar *fl_opml_node_render(nodePtr np) {

	if(FST_PLUGIN == np->type) {
		return "FIXME: return something meaningful...";
	}

	return fl_default_node_render(np);
}

static void fl_opml_node_save(nodePtr np) {

	switch(np->type) {
		case FST_FEED:
			feed_save_to_cache((feedPtr)np->data, node_get_itemset(np), node_get_id(np));
			break;
		case FST_FOLDER:
		case FST_VFOLDER:
		case FST_PLUGIN:
			/* nothing to do */
			break;
	}
}

#define fl_opml_node_auto_update(np) fl_default_node_auto_update(np)

#define fl_opml_node_update(np, flags) fl_default_node_update(np, flags)

static void fl_opml_init(void) {

	debug_enter("fl_opml_init");

	debug_exit("fl_opml_init");
}

static void fl_opml_deinit(void) {
	
	debug_enter("fl_opml_deinit");

	debug_exit("fl_opml_deinit");
}

/* feed list provider plugin definition */

static flPluginInfo fpi = {
	FL_PLUGIN_API_VERSION,
	"fl_opml",
	"Planet/OPML Plugin",
	0,
	fl_opml_init,
	fl_opml_deinit,
	fl_opml_handler_new,
	fl_opml_handler_delete,
	fl_opml_node_load,
	fl_opml_node_unload,
	fl_opml_node_save,
	fl_opml_node_render,
	fl_opml_node_auto_update,
	fl_opml_node_update,
	NULL,
	NULL
};

static pluginInfo pi = {
	PLUGIN_API_VERSION,
	"Planet/OPML Plugin",
	PLUGIN_TYPE_FEEDLIST_PROVIDER,
	//"Default feed list provider. Allows users to add/remove/reorder subscriptions.",
	&fpi
};

DECLARE_PLUGIN(pi);
DECLARE_FL_PLUGIN(fpi);
