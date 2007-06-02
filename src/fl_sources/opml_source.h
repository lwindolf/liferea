/**
 * @file opml_source.h OPML Planet/Blogroll feed list provider
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
 
#ifndef _OPML_SOURCE_H
#define _OPML_SOURCE_H

#include "node.h"

#define OPML_SOURCE_DEFAULT_TITLE _("New OPML Subscription")

/* General OPML source handling functions */

/**
 * Determine OPML cache file name.
 *
 * @param node		the node of the OPML source
 *
 * @returns newly allocated filename 
 */
gchar * opml_source_get_feedlist(nodePtr node);

/**
 * Imports an OPML source.
 *
 * @param node		the node of the OPML source
 */
void opml_source_import(nodePtr node);

/**
 * Exports an OPML source.
 *
 * @param node		the node of the OPML source
 */
void opml_source_export(nodePtr node);

/**
 * Removes a OPML source.
 *
 * @param node		the node of the OPML source
 */
void opml_source_remove(nodePtr node);

/**
 * Sets up the given node as a OPML source node.
 *
 * @param parent	parent node (or NULL when importing)
 * @param node		the node
 */
void opml_source_setup(nodePtr parent, nodePtr node);

/**
 * Request processing callback for OPML source updates.
 *
 * @param request	finished requests
 */
void opml_source_process_update_results(requestPtr request);

/**
 * Updates the OPML source of the given node.
 *
 * @param node		the node
 * @param now		current time
 */
void opml_source_update(nodePtr node, GTimeVal *now);

/**
 * Returns OPML source type implementation info.
 */
nodeSourceTypePtr opml_source_get_type(void);

#endif
