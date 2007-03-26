/**
 * @file ui_feed.h	UI actions concerning a single feed
 *
 * Copyright (C) 2004-2006 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#ifndef _UI_FEED_H
#define _UI_FEED_H

#include "feed.h"

/**
 * Start interaction to gather authentication
 * infos for a given feed node.
 *
 * @param node		the node
 */
void ui_feed_authdialog_new(nodePtr node, gint flags);

/**
 * Open feed properties dialog for the given node.
 *
 * @param node		the node
 */
void ui_feed_properties(nodePtr node);

/**
 * Start interaction to create a feed attached to
 * the given node.
 *
 * @param node		the node
 */
void ui_feed_add(nodePtr node);

#endif /* _UI_FEED_H */
