/*
 * @file media_player.h  media player helpers
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

#ifndef _LIFEREA_MEDIA_PLAYER_H
#define _LIFEREA_MEDIA_PLAYER_H

#include <glib.h>
#include <gtk/gtk.h>

/**
 * liferea_media_player_load:
 * @parentWidget:        the parent widget for the media player
 * @enclosures: (element-type gchar*): a list of enclosure strings
 *
 * Triggers the creation of a suitable media player and loads a list of
 * enclosures into it.
 */
void liferea_media_player_load (GtkWidget *parentWidget, GSList *enclosures);

#endif
