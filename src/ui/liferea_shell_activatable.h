/*
 * @file liferea_shell_activatable.h  Shell Plugin Type
 *
 * Copyright (C) 2012 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _LIFEREA_SHELL_ACTIVATABLE_H__
#define _LIFEREA_SHELL_ACTIVATABLE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define LIFEREA_TYPE_SHELL_ACTIVATABLE		(liferea_shell_activatable_get_type ())
#define LIFEREA_SHELL_ACTIVATABLE(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LIFEREA_TYPE_SHELL_ACTIVATABLE, LifereaShellActivatable))
#define LIFEREA_SHELL_ACTIVATABLE_IFACE(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), LIFEREA_TYPE_SHELL_ACTIVATABLE, LifereaShellActivatableInterface))
#define LIFEREA_IS_SHELL_ACTIVATABLE(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIFEREA_TYPE_SHELL_ACTIVATABLE))
#define LIFEREA_SHELL_ACTIVATABLE_GET_IFACE(obj)	(G_TYPE_INSTANCE_GET_INTERFACE ((obj), LIFEREA_TYPE_SHELL_ACTIVATABLE, LifereaShellActivatableInterface))

typedef struct _LifereaShellActivatable LifereaShellActivatable;
typedef struct _LifereaShellActivatableInterface LifereaShellActivatableInterface;

struct _LifereaShellActivatableInterface
{
	GTypeInterface g_iface;

	void (*activate) (LifereaShellActivatable * activatable);
	void (*deactivate) (LifereaShellActivatable * activatable);
	void (*update_state) (LifereaShellActivatable * activatable);
};

GType liferea_shell_activatable_get_type (void) G_GNUC_CONST;

void liferea_shell_activatable_activate (LifereaShellActivatable *activatable);

void liferea_shell_activatable_deactivate (LifereaShellActivatable *activatable);

void liferea_shell_activatable_update_state (LifereaShellActivatable *activatable);

G_END_DECLS

#endif /* __LIFEREA_SHELL_ACTIVATABLE_H__ */
