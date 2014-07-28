/*
 * @file liferea_media_player_activatable.c  Media Player Plugin
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

#include "media_player_activatable.h"

/**
 * SECTION:liferea_media_player_activatable
 * @short_description: Interface for activatable extensions providing a media player
 * @see_also: #PeasExtensionSet
 *
 * #LifereaMediaPlayerActivatable is an interface which should be implemented by
 * extensions that want to provide a media player
 **/
G_DEFINE_INTERFACE (LifereaMediaPlayerActivatable, liferea_media_player_activatable, G_TYPE_OBJECT)

void
liferea_media_player_activatable_default_init (LifereaMediaPlayerActivatableInterface *iface)
{
	/* No properties yet */
}

void
liferea_media_player_activatable_activate (LifereaMediaPlayerActivatable * activatable)
{
	LifereaMediaPlayerActivatableInterface *iface;

	g_return_if_fail (IS_LIFEREA_MEDIA_PLAYER_ACTIVATABLE (activatable));

	iface = LIFEREA_MEDIA_PLAYER_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->activate)
		iface->activate (activatable);
}

void
liferea_media_player_activatable_deactivate (LifereaMediaPlayerActivatable * activatable)
{
	LifereaMediaPlayerActivatableInterface *iface;

	g_return_if_fail (IS_LIFEREA_MEDIA_PLAYER_ACTIVATABLE (activatable));

	iface = LIFEREA_MEDIA_PLAYER_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->deactivate)
		iface->deactivate (activatable);
}

void
liferea_media_player_activatable_load (LifereaMediaPlayerActivatable * activatable,
                               GtkWidget *parentWidget, GSList *enclosures)
{
	LifereaMediaPlayerActivatableInterface *iface;

	g_return_if_fail (IS_LIFEREA_MEDIA_PLAYER_ACTIVATABLE (activatable));

	iface = LIFEREA_MEDIA_PLAYER_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->load)
		iface->load (activatable, parentWidget, enclosures);
}


