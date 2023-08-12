/**
 * @file dummy_source.c  dummy feed list source
 * 
 * Copyright (C) 2006-2022 Lars Windolf <lars.windolf@gmx.de>
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

#include "fl_sources/dummy_source.h"

#include <glib.h>
#include "ui/icons.h"

static gchar * dummy_source_get_feedlist(nodePtr node) { return NULL; }

static void dummy_source_noop (nodePtr node) { }

static void dummy_source_import(nodePtr node) {

	node->icon = (gpointer)icon_get (ICON_UNAVAILABLE);
	node_set_title (node, g_strdup_printf ("Unknown source type '%s'", node->source->type->id));
}

static void dummy_source_init(void) { }

static void dummy_source_deinit(void) { }

/* feed list provider plugin definition */

static struct nodeSourceType nst = {
	.id			= NODE_SOURCE_TYPE_DUMMY_ID,
	.name			= "Dummy Feed List Source",
	.source_type_init	= dummy_source_init,
	.source_type_deinit	= dummy_source_deinit,
	.source_import		= dummy_source_import,
	.source_export		= dummy_source_noop,
	.source_get_feedlist	= dummy_source_get_feedlist,
	.source_auto_update	= dummy_source_noop
};

nodeSourceTypePtr dummy_source_get_type(void) { return &nst; }
