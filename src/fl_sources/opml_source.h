/**
 * @file opml_source.h  OPML Planet/Blogroll feed list provider
 * 
 * Copyright (C) 2005-2009 Lars Windolf <lars.windolf@gmx.de>
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
#include "fl_sources/node_source.h"

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
 * Returns OPML source type implementation info.
 */
nodeSourceTypePtr opml_source_get_type(void);

#endif
