/**
 * @file pie_feed.h Atom 0.3 feed parsing
 *
 * Copyright (C) 2003-2006 Lars Lindner <lars.lindner@gmx.net>
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

#ifndef _PIE_FEED_H
#define _PIE_FEED_H

#include <libxml/tree.h>
#include "feed.h"

feedHandlerPtr	pie_init_feed_handler(void);

/**
 * This parses an Atom content construct.
 *
 * @param cur the parent node of the elements to be parsed.
 * @returns g_strduped string which must be freed by the caller.
 */
gchar* pie_parse_content_construct(xmlNodePtr cur);

#endif
