/*
 * @file liferea_download_activatable.h  Download Plugin Interface
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

#ifndef _LIFEREA_DOWNLOAD_ACTIVATABLE_H__
#define _LIFEREA_DOWNLOAD_ACTIVATABLE_H__

#include <glib-object.h>

#include "liferea_activatable.h"

G_BEGIN_DECLS

#define LIFEREA_TYPE_DOWNLOAD_ACTIVATABLE (liferea_download_activatable_get_type ())
G_DECLARE_INTERFACE (LifereaDownloadActivatable, liferea_download_activatable, LIFEREA, DOWNLOAD_ACTIVATABLE, LifereaActivatable)

struct _LifereaDownloadActivatableInterface
{
	GTypeInterface g_iface;

	void (*download) (LifereaDownloadActivatable * activatable, const gchar *url);
	void (*show) (LifereaDownloadActivatable * activatable);
};

/**
 * liferea_download_activatable_download:
 * @activatable:	a #LifereaDownloadActivatable.
 * @url:		an URL to download
 *
 * Triggers a download.
 */
void liferea_download_activatable_download (LifereaDownloadActivatable *activatable,
                                     const gchar *url);

/**
 * liferea_download_activatable_show:
 * @activatable:	a #LifereaDownloadActivatable.
 *
 * Show the download GUI
 */
void liferea_download_activatable_show (LifereaDownloadActivatable * activatable);

G_END_DECLS

#endif /* __LIFEREA_DOWNLOAD_ACTIVATABLE_H__ */
