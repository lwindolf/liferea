/*
 * @file node_view.h  node view modes
 *
 * Copyright (C) 2009-2023 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _NODE_VIEW_H
#define _NODE_VIEW_H

typedef enum {
	NODE_VIEW_MODE_NORMAL	= 0,
	NODE_VIEW_MODE_WIDE	= 1,
	NODE_VIEW_MODE_AUTO	= 2
} nodeViewType;

typedef enum {
	NODE_VIEW_SORT_BY_TIME = 0,	/* default */
	NODE_VIEW_SORT_BY_TITLE,
	NODE_VIEW_SORT_BY_PARENT,
	NODE_VIEW_SORT_BY_STATE
} nodeViewSortType;

#endif
