/**
 * @file ui_vfolder.h  vfolder dialogs handling
 * 
 * Copyright (C) 2004-2006 Lars Lindner <lars.lindner@gmx.net>
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

#ifndef _UI_VFOLDER_H
#define _UI_VFOLDER_H

#include "node.h"
 
/**
 * Sets up a vfolder properties dialog 
 *
 * @param node		the node whose properties to load
 */
void ui_vfolder_properties(nodePtr node);

/** 
 * Interactively add a new vfolder to the given parent node.
 *
 * @param parent	the parent node
 */
void ui_vfolder_add(nodePtr parent);

#endif
