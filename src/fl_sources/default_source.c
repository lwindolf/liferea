/**
 * @file default_source.c default static feedlist provider
 * 
 * Copyright (C) 2005-2007 Lars Lindner <lars.lindner@gmail.com>
 * Copyright (C) 2005-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>
#include <libxml/uri.h>
#include "support.h"
#include "common.h"
#include "conf.h"
#include "feed.h"
#include "feedlist.h"
#include "export.h"
#include "debug.h"
#include "update.h"
#include "plugin.h"
#include "fl_sources/default_source.h"
#include "fl_sources/node_source.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_node.h"
#include "ui/ui_subscription.h"
#include "ui/ui_tray.h"

/** lock to prevent feed list saving while loading */
static gboolean feedlistImport = FALSE;

extern gboolean cacheMigrated;	/* feedlist.c */

static void default_source_copy_dir(const gchar *from, const gchar *to, const gchar *subdir) {
	gchar *dirname10, *dirname12;
	gchar *srcfile, *destfile;
   	GDir *dir;
		
	dirname10 = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s" G_DIR_SEPARATOR_S "%s", g_get_home_dir(), from, subdir);
	dirname12 = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s" G_DIR_SEPARATOR_S "%s", g_get_home_dir(), to, subdir);
	
	dir = g_dir_open(dirname10, 0, NULL);
	while(NULL != (srcfile = (gchar *)g_dir_read_name(dir))) {
		gchar	*content;
		gsize	length;
		destfile = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s", dirname12, srcfile);
		srcfile = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s", dirname10, srcfile);
		g_print("copying %s to %s\n", srcfile, destfile);
		if(g_file_get_contents(srcfile, &content, &length, NULL))
			g_file_set_contents(destfile, content, length, NULL);
		g_free(content);
		g_free(destfile);
		g_free(srcfile);
	}
	g_dir_close(dir);
	
	g_free(dirname10);
	g_free(dirname12);
}

static gchar * default_source_source_get_feedlist(nodePtr node) {

	return common_create_cache_filename(NULL, "feedlist", "opml");
}

static void default_source_source_import(nodePtr node) {
	gchar		*filename13;

	debug_enter("default_source_source_import");

	/* start the import */
	feedlistImport = TRUE;

	/* build test file names */	
	filename13 = default_source_source_get_feedlist(node);

	/* check for 1.0->1.3 migration */
	// FIXME

	/* check for 1.2->1.3 migration */
	// FIXME
	
	/* check for default feed list import */
	if(!g_file_test(filename13, G_FILE_TEST_EXISTS)) {
		/* if there is no feedlist.opml we provide a default feed list */
		g_free(filename13);
		/* "feedlist.opml" is translatable so that translators can provide a localized default feed list */
		filename13 = g_strdup_printf(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "opml" G_DIR_SEPARATOR_S "%s", _("feedlist.opml"));
	}
	if(!import_OPML_feedlist(filename13, node, node->source, FALSE, TRUE))
		g_error("Fatal: Feed list import failed!");
	g_free(filename13);
	feedlistImport = FALSE;

	debug_exit("default_source_source_import");
}

static void default_source_source_export(nodePtr node) {
	gchar	*filename;
	
	if(feedlistImport)
		return;

	debug_enter("default_source_source_export");
	
	g_assert(node->source->root == feedlist_get_root());

	filename = default_source_source_get_feedlist(node);
	export_OPML_feedlist(filename, node->source->root, TRUE);
	g_free(filename);

	debug_exit("default_source_source_export");
}

static void default_source_source_auto_update(nodePtr node) {

	node_foreach_child(node, node_request_auto_update);
}

/* root node type definition */

static void default_source_init(void) {

	debug_enter("default_source_init");

	debug_exit("default_source_init");
}

static void default_source_deinit(void) {
	
	debug_enter("default_source_deinit");

	debug_exit("default_source_deinit");
}

/* feed list provider plugin definition */

static struct nodeSourceType nst = {
	NODE_SOURCE_TYPE_API_VERSION,
	"fl_default",
	"Static Feed List",
	"The default feed list source. Should never be added manually. If you see this then something went wrong!",
	NODE_SOURCE_CAPABILITY_IS_ROOT |
	NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST,
	default_source_init,
	default_source_deinit,
	NULL,
	NULL,
	default_source_source_import,
	default_source_source_export,
	default_source_source_get_feedlist,
	NULL,
	default_source_source_auto_update
};

nodeSourceTypePtr default_source_get_type(void) { return &nst; }
