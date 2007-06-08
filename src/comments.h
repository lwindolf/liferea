/**
 * @file comments.h comment feed handling
 * 
 * Copyright (C) 2007 Lars Lindner <lars.lindner@gmail.com>
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

#ifndef _COMMENTS_H
#define _COMMENTS_H

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include "item.h"

/**
 * Frees everything related with comments 
 */
void comments_deinit (void);

/**
 * Triggers immediate comments retrieval (or update) for the given item.
 *
 * @param item		the item
 */
void comments_refresh (itemPtr item);

/**
 * Adds the comments and state of the given comment feed id to the 
 * passed XML node.
 *
 * @param parentNode	XML parent node
 * @param id		the comment feed id
 */
void comments_to_xml (xmlNodePtr parentNode, const gchar *id);

/**
 * Removes a comment feed from DB and free's all memory. Usually
 * called when removing the parent item.
 *
 * @param id		the comment feed id
 */
void comments_remove (const gchar *id);

#endif
