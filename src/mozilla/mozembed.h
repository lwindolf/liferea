/**
 * @file mozembed.h common gtkmozembed handling.
 *   
 * Copyright (C) 2003-2006 Lars Lindner <lars.lindner@gmx.net>   
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#ifndef _MOZEMBED_H
#define _MOZEMBED_H

#include "mozsupport.h"

void mozembed_write(GtkWidget *widget, const gchar *string, guint length, const gchar *base, const gchar *contentType);

GtkWidget * mozembed_create(gboolean forceInternalBrowsing);

void mozembed_init(void);

void mozembed_deinit(void);

void mozembed_launch_url(GtkWidget *widget, const gchar *url);

gboolean mozembed_launch_inside_possible(void);

void mozembed_set_proxy(gchar *hostname, int port, gchar *username, gchar *password);

#endif
