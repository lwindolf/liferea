/**
 * @file dummy_source.c  dummy feed list source
 * 
 * Copyright (C) 2006-2025 Lars Windolf <lars.windolf@gmx.de>
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

#include "node_sources/dummy_source.h"

#include <glib.h>
#include "node.h"
#include "node_source.h"
#include "ui/icons.h"

static gchar * dummy_source_get_feedlist (Node *node) { return NULL; }

static void dummy_source_noop (Node *node) { }

static void dummy_source_import (Node *node) {

	node->icon = (gpointer)icon_get (ICON_UNAVAILABLE);
	node_set_title (node, g_strdup_printf ("Unknown source type '%s'", node->source->type->id));
}

/* node source provider definition */

typedef struct {
	GObject parent_instance;
} DummySource;

typedef struct {
	GObjectClass parent_class;
} DummySourceClass;

static void dummy_source_init(DummySource *self) { }
static void dummy_source_class_init(DummySourceClass *klass) { }
static void dummy_source_interface_init(NodeSourceProviderInterface *iface) {
	iface->id			= NODE_SOURCE_TYPE_DUMMY_ID;
	iface->name			= "Dummy Feed List Source";
	iface->source_import		= dummy_source_import;
	iface->source_export		= dummy_source_noop;
	iface->source_get_feedlist	= dummy_source_get_feedlist;
	iface->source_auto_update	= dummy_source_noop;
}

#define DUMMY_TYPE_SOURCE (dummy_source_get_type())

G_DEFINE_TYPE_WITH_CODE(DummySource, dummy_source, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE(NODE_TYPE_SOURCE_PROVIDER, dummy_source_interface_init))

void
dummy_source_register (void)
{
	NodeSourceProviderInterface *iface = NODE_SOURCE_PROVIDER_GET_IFACE (g_object_new (DUMMY_TYPE_SOURCE, NULL));
	node_source_type_register (iface);
}