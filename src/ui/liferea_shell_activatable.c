/*
 * @file liferea_shell_activatable.c  Shell Plugin Type
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

#include "ui/liferea_shell_activatable.h"

#include "ui/liferea_shell.h"

/**
 * SECTION:liferea_shell_activatable
 * @short_description: Interface for activatable extensions on the shell
 * @see_also: #PeasExtensionSet
 *
 * #LifereaShellActivatable is an interface which should be implemented by
 * extensions that should be activated on the Liferea main window.
 **/

G_DEFINE_INTERFACE (LifereaShellActivatable, liferea_shell_activatable, G_TYPE_OBJECT)

void
liferea_shell_activatable_default_init (LifereaShellActivatableInterface *iface)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		/**
		 * LifereaShellActivatable:window:
		 *
		 * The window property contains the gtr window for this
		 * #LifereaShellActivatable instance.
		 */
		g_object_interface_install_property (iface,
                           g_param_spec_object ("shell",
                                                "Shell",
                                                "The Liferea shell",
                                                LIFEREA_SHELL_TYPE,
                                                G_PARAM_READWRITE |
                                                G_PARAM_CONSTRUCT_ONLY |
                                                G_PARAM_STATIC_STRINGS));
		initialized = TRUE;
	}
}

/**
 * liferea_shell_activatable_activate:
 * @activatable: A #LifereaShellActivatable.
 *
 * Activates the extension on the shell property.
 */
void
liferea_shell_activatable_activate (LifereaShellActivatable * activatable)
{
	LifereaShellActivatableInterface *iface;

	g_return_if_fail (LIFEREA_IS_SHELL_ACTIVATABLE (activatable));

	iface = LIFEREA_SHELL_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->activate)
		iface->activate (activatable);
}

/**
 * liferea_shell_activatable_deactivate:
 * @activatable: A #LifereaShellActivatable.
 *
 * Deactivates the extension on the shell property.
 */
void
liferea_shell_activatable_deactivate (LifereaShellActivatable * activatable)
{
	LifereaShellActivatableInterface *iface;

	g_return_if_fail (LIFEREA_IS_SHELL_ACTIVATABLE (activatable));

	iface = LIFEREA_SHELL_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->deactivate)
		iface->deactivate (activatable);
}

/**
 * liferea_shell_activatable_update_state:
 * @activatable: A #LifereaShellActivatable.
 *
 * Triggers an update of the extension internal state to take into account
 * state changes in the window, due to some event or user action.
 */
void
liferea_shell_activatable_update_state (LifereaShellActivatable * activatable)
{
	LifereaShellActivatableInterface *iface;

	g_return_if_fail (LIFEREA_IS_SHELL_ACTIVATABLE (activatable));

	iface = LIFEREA_SHELL_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->update_state)
		iface->update_state (activatable);
}

