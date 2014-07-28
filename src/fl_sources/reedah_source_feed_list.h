/**
 * @file reedah_source_feed_list.h  Reedah feed list handling
 *
 * Copyright (C) 2013-2014  Lars Windolf <lars.windolf@gmx.de>
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

#include "fl_sources/reedah_source.h"

/**
 * Find a node by the source id.
 *
 * @param gsource	the Reedah source
 * @param source	the feed id to find
 *
 * @returns a node (or NULL)
 */
nodePtr reedah_source_opml_get_node_by_source(ReedahSourcePtr gsource,
					 const gchar *source);

/**
 * Perform a quick update of the Reedah source.
 *
 * @param gsource	the Reedah source
 */
gboolean reedah_source_opml_quick_update (ReedahSourcePtr gsource);
