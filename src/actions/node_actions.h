/*
 * @file node_actions.h  node actions
 *
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2007-2025 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef NODE_ACTIONS_H
#define NODE_ACTIONS_H

#include <gtk/gtk.h>

#include "ui/liferea_shell.h"

/**
 * node_actions_create:
 * 
 * @shell: the Liferea shell
 * 
 * Create a new action group for node actions.
 * 
 * Returns: a new action group, to be freed by caller
 */
GActionGroup * node_actions_create (LifereaShell *shell);

#endif /* NODE_ACTIONS_H */