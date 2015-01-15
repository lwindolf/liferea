/*
 * @file liferea_media_player.c  media player helpers
 *
 * Copyright (C) 2012-2015 Lars Windolf <lars.windolf@gmx.de>
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

#include "media_player.h"
#include "media_player_activatable.h"
#include "plugins_engine.h"

#include <libpeas/peas-activatable.h>

// FIXME: This should be a member of some object!
static PeasExtensionSet *extensions = NULL;

static PeasExtensionSet *
liferea_media_player_get_extension_set (void)
{
	if (!extensions) {
		extensions = peas_extension_set_new (PEAS_ENGINE (liferea_plugins_engine_get_default ()),
		                             LIFEREA_MEDIA_PLAYER_ACTIVATABLE_TYPE, NULL);

		liferea_plugins_engine_set_default_signals (extensions, NULL);
	}

	return extensions;
}

typedef struct mediaPlayerLoadData {
	GtkWidget	*parentWidget;
	GSList		*enclosures;
} mediaPlayerLoadData;

static void
liferea_media_player_load_foreach (PeasExtensionSet *set,
                                   PeasPluginInfo *info,
                                   PeasExtension *exten,
                                   gpointer user_data)
{
	liferea_media_player_activatable_load (LIFEREA_MEDIA_PLAYER_ACTIVATABLE (exten),
	                                       ((mediaPlayerLoadData *)user_data)->parentWidget,
                                               ((mediaPlayerLoadData *)user_data)->enclosures);
}

void
liferea_media_player_load (GtkWidget *parentWidget, GSList *enclosures)
{
	mediaPlayerLoadData user_data;

	user_data.parentWidget = parentWidget;
	user_data.enclosures = enclosures;

	peas_extension_set_foreach (liferea_media_player_get_extension_set (),
	                            liferea_media_player_load_foreach, &user_data);
}

