/*
 * @file liferea_media_player_activatable.h  Media Player Plugin Type
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

#ifndef _LIFEREA_MEDIA_PLAYER_ACTIVATABLE_H__
#define _LIFEREA_MEDIA_PLAYER_ACTIVATABLE_H__

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define LIFEREA_MEDIA_PLAYER_ACTIVATABLE_TYPE		(liferea_media_player_activatable_get_type ())
#define LIFEREA_MEDIA_PLAYER_ACTIVATABLE(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LIFEREA_MEDIA_PLAYER_ACTIVATABLE_TYPE, LifereaMediaPlayerActivatable))
#define LIFEREA_MEDIA_PLAYER_ACTIVATABLE_IFACE(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), LIFEREA_MEDIA_PLAYER_ACTIVATABLE_TYPE, LifereaMediaPlayerActivatableInterface))
#define IS_LIFEREA_MEDIA_PLAYER_ACTIVATABLE(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIFEREA_MEDIA_PLAYER_ACTIVATABLE_TYPE))
#define LIFEREA_MEDIA_PLAYER_ACTIVATABLE_GET_IFACE(obj)	(G_TYPE_INSTANCE_GET_INTERFACE ((obj), LIFEREA_MEDIA_PLAYER_ACTIVATABLE_TYPE, LifereaMediaPlayerActivatableInterface))

typedef struct _LifereaMediaPlayerActivatable LifereaMediaPlayerActivatable;
typedef struct _LifereaMediaPlayerActivatableInterface LifereaMediaPlayerActivatableInterface;

struct _LifereaMediaPlayerActivatableInterface
{
	GTypeInterface g_iface;

	void (*activate) (LifereaMediaPlayerActivatable * activatable);
	void (*deactivate) (LifereaMediaPlayerActivatable * activatable);
	void (*load) (LifereaMediaPlayerActivatable * activatable, GtkWidget *parentWidget, GSList *enclosures);
};

GType liferea_media_player_activatable_get_type (void) G_GNUC_CONST;

void liferea_media_player_activatable_activate (LifereaMediaPlayerActivatable *activatable);

void liferea_media_player_activatable_deactivate (LifereaMediaPlayerActivatable *activatable);

/**
 * liferea_media_player_activatable_load:
 * @parentWidget:			the parent widget for the media player
 * @enclosures: (element-type gchar*):	a list of enclosures
 *
 * Triggers the creation of a suitable media player and loads a list of
 * enclosures into it.
 */
void liferea_media_player_activatable_load (LifereaMediaPlayerActivatable *activatable, GtkWidget *parentWidget, GSList *enclosures);

G_END_DECLS

#endif /* __LIFEREA_MEDIA_PLAYER_ACTIVATABLE_H__ */
