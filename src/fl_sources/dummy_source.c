/**
 * @file fl_default.c dummy feedlist provider
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

#include "callbacks.h"
#include "plugin.h"
#include "fl_sources/fl_plugin.h"

static struct flPlugin fpi;

static gchar * fl_dummy_source_get_feedlist(nodePtr node) { return NULL; }

static void fl_dummy_source_import(nodePtr node) {
 
	node->icon = icons[ICON_UNAVAILABLE];
}

static void fl_dummy_source_export(nodePtr node) { }

static void fl_dummy_init(void) { }

static void fl_dummy_deinit(void) { }

/* feed list provider plugin definition */

static struct flPlugin fpi = {
	FL_PLUGIN_API_VERSION,
	FL_DUMMY_SOURCE_ID,
	"Dummy Feed List Source",
	0,
	fl_dummy_init,
	fl_dummy_deinit,
	NULL,
	NULL,
	fl_dummy_source_import,
	fl_dummy_source_export,
	fl_dummy_source_get_feedlist
};

static struct plugin pi = {
	PLUGIN_API_VERSION,
	"Dummy Feed List Source Plugin",
	PLUGIN_TYPE_FEEDLIST_PROVIDER,
	//"Dummy feed list provider. Used as place holder for plugins that could not be loaded.",
	&fpi
};

DECLARE_PLUGIN(pi);
DECLARE_FL_PLUGIN(fpi);
