/**
 * @file opml_source.h  OPML Planet/Blogroll feed list provider
 * 
 * Copyright (C) 2005-2026 Lars Windolf <lars.windolf@gmx.de>
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
 
#ifndef _OPML_SOURCE_H
#define _OPML_SOURCE_H

#include "node.h"
#include "subscription_type.h"
#include "node_source.h"

#define OPML_SOURCE_DEFAULT_TITLE _("New OPML Subscription")

/* Generic OPML handling functions */

/**
 * Determine OPML cache file name.
 *
 * @param node		the node of the OPML source
 *
 * @returns newly allocated filename 
 */
gchar * opml_source_get_feedlist (Node *node);

/**
 * Imports an OPML source as a node tree.
 * 
 * Does not assign any special subscription types
 * so it can be reused by other node source types.
 *
 * @param node		the node of the OPML source
 */
void opml_source_import_tree_from_file (Node *node);

/**
 * Exports an OPML source.
 * 
 * Generic function that can be reused by other node
 * soure types.
 *
 * @param node		the node of the OPML source
 */
void opml_source_export (Node *node);

/**
 * Removes a OPML export file.
 * 
 * Generic function that can be reused by other node
 * soure types.
 *
 * @param node		the node source
 */
void opml_export_remove (Node *node);

/**
 * Returns OPML source type implementation info.
 */
nodeSourceTypePtr opml_source_get_type(void);

#endif
