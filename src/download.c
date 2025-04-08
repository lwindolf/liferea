/**
 * @file download.h  Managing downloads
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

#include "download.h"

#include <libpeas/peas-extension-set.h>

#include "plugins/download_activatable.h"
#include "plugins/plugins_engine.h"

static void
download_func (gpointer exten, gpointer user_data) {
        liferea_download_activatable_download (LIFEREA_DOWNLOAD_ACTIVATABLE (exten), (gchar *)user_data);
}

void
download_url (const gchar *url)
{
        liferea_plugin_call (LIFEREA_TYPE_DOWNLOAD_ACTIVATABLE, download_func, (gpointer)url);
}

static void
show_func (gpointer exten, gpointer user_data) {
        liferea_download_activatable_show (LIFEREA_DOWNLOAD_ACTIVATABLE (exten));
}

void
download_show (void)
{
        liferea_plugin_call (LIFEREA_TYPE_DOWNLOAD_ACTIVATABLE, show_func, NULL);
}