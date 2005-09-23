/**
 * @file node.h common feed list node handling interface
 * 
 * Copyright (C) 2003-2005 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004-2005 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include "node.h"

gboolean node_load(nodePtr np) {

	g_assert(NULL != np->handler);
	g_assert(NULL != np->handler->plugin);
	if(NULL != np->handler->plugin->load)
		np->handler->plugin->load(np);
}

void node_unload(nodePtr np) {

	g_assert(NULL != np->handler);
	g_assert(NULL != np->handler->plugin);
	if(NULL != np->handler->plugin->unload)
		np->handler->plugin->unload(np);
}

void node_render(nodePtr np) {

	g_assert(NULL != np->handler);
	g_assert(NULL != np->handler->plugin);
	np->handler->plugin->render(np);
}
