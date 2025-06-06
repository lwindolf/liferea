/*
 * @file liferea_shell.h  UI action handling
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

#ifndef _SHELL_ACTIONS_H
#define _SHELL_ACTIONS_H

#include <gtk/gtk.h>

#include "ui/liferea_shell.h"

/**
 * shell_actions_create: (skip)
 * 
 * @shell: the Liferea shell
 * 
 * Setup all shell actions
 * 
 * Returns: the new action group to be freed by the caller
 */
GActionGroup * shell_actions_create (LifereaShell *shell);

#endif /* _SHELL_ACTIONS_H */