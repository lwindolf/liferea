/*
 * @file liferea_activatable.h  Base Plugin Interface
 *
 * Copyright (C) 2024 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _LIFEREA_ACTIVATABLE_H__
#define _LIFEREA_ACTIVATABLE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define LIFEREA_TYPE_ACTIVATABLE (liferea_activatable_get_type ())
G_DECLARE_INTERFACE (LifereaActivatable, liferea_activatable, LIFEREA, ACTIVATABLE, GObject)

struct _LifereaActivatableInterface
{
        GTypeInterface g_iface;

        void (*activate) (LifereaActivatable * activatable);
        void (*deactivate) (LifereaActivatable * activatable);
};

/**
 * liferea_activatable_activate:
 * @activatable: A #LifereaActivatable.
 *
 * Activates the extension.
 */
void liferea_activatable_activate (LifereaActivatable *activatable);

/**
 * liferea_activatable_deactivate:
 * @activatable: A #LifereaActivatable.
 *
 * Deactivates the extension.
 */
void liferea_activatable_deactivate (LifereaActivatable *activatable);

G_END_DECLS

#endif