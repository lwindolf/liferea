/**
 * @file dummy_source.c dummy feed list source
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

#include "common.h"
#include "feedlist.h"
#include "fl_sources/node_source.h"

static gchar * dummy_source_get_feedlist(nodePtr node) { return NULL; }

static void dummy_source_import(nodePtr node) {
 
	node->icon = icons[ICON_UNAVAILABLE];
}

static void dummy_source_export(nodePtr node) { }

static void dummy_source_init(void) { }

static void dummy_source_deinit(void) { }

/* feed list provider plugin definition */

static struct nodeSourceType nst = {
	NODE_SOURCE_TYPE_API_VERSION,
	NODE_SOURCE_TYPE_DUMMY_ID,
	"Dummy Feed List Source",
	"The dummy feed list source. Should never be added manually. If you see this then something went wrong!",
	0,
	dummy_source_init,
	dummy_source_deinit,
	NULL,
	NULL,
	dummy_source_import,
	dummy_source_export,
	dummy_source_get_feedlist
};

nodeSourceTypePtr dummy_source_get_type(void) { return &nst; }
