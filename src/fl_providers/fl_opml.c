/**
 * @file fl_opml.c default static feedlist provider
 * 
 * Copyright (C) 2005-2006 Lars Lindner <lars.lindner@gmx.net>
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

#include <unistd.h>
#include "support.h"
#include "common.h"
#include "debug.h"
#include "feed.h"
#include "node.h"
#include "export.h"
#include "ui/ui_feedlist.h"
#include "fl_providers/fl_opml.h"
#include "fl_providers/fl_opml-ui.h"
#include "fl_providers/fl_opml-cb.h"
#include "fl_providers/fl_plugin.h"

static struct flPlugin fpi;

static void fl_opml_handler_import(nodePtr node) {
	flNodeHandler	*handler;
	gchar		*filename;

	debug_enter("fl_opml_handler_import");

	/* create a new handler structure */
	handler = g_new0(struct flNodeHandler_, 1);
	handler->root = node;
	handler->plugin = &fpi;
	node->handler = handler;
	node->icon = create_pixbuf("fl_opml.png");

	debug1(DEBUG_CACHE, "starting import of opml plugin instance (id=%s)\n", node->id);
	filename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "plugins", node->id, "opml");
	if(g_file_test(filename, G_FILE_TEST_EXISTS)) {
		import_OPML_feedlist(filename, node, node->handler, FALSE, TRUE);
	} else {
		g_warning("cannot open \"%s\"", filename);
		node->available = FALSE;
	}
	g_free(filename);

	debug_exit("fl_opml_handler_import");
}

static void fl_opml_handler_export(nodePtr node) {

	/* Nothing to do because the OPML source
	   cannot be changed by the user */
}

static void fl_opml_handler_new(nodePtr node) {

	ui_fl_opml_get_handler_source(node);
}

static void fl_opml_handler_remove(nodePtr node) {
	gchar		*filename;

	/* step 1: delete all feed cache files */
	node_foreach_child(node, node_remove);

	/* step 2: delete plugin instance OPML cache file */
	filename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "plugins", node->id, "opml");
	unlink(filename);
	g_free(filename);
}

static void fl_opml_init(void) {

	debug_enter("fl_opml_init");

	debug_exit("fl_opml_init");
}

static void fl_opml_deinit(void) {
	
	debug_enter("fl_opml_deinit");

	debug_exit("fl_opml_deinit");
}

/* feed list provider plugin definition */

static struct flPlugin fpi = {
	FL_PLUGIN_API_VERSION,
	"fl_opml",
	"Planet/OPML Plugin",
	FL_PLUGIN_CAPABILITY_DYNAMIC_CREATION,
	fl_opml_init,
	fl_opml_deinit,
	fl_opml_handler_new,
	fl_opml_handler_remove,
	fl_opml_handler_import,
	fl_opml_handler_export
};

static struct plugin pi = {
	PLUGIN_API_VERSION,
	"Planet/OPML Plugin",
	PLUGIN_TYPE_FEEDLIST_PROVIDER,
	//"Default feed list provider. Allows users to add/remove/reorder subscriptions.",
	&fpi
};

DECLARE_PLUGIN(pi);
DECLARE_FL_PLUGIN(fpi);
