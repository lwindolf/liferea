
/*
 * @file micropub.h  posting to micropub services
 * 
 * Copyright (C) 2026 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _MICROPUB_H
#define _MICROPUB_H

#include <glib.h>

#include "item.h"

/**
 * micropub_auth_callback: (skip)
 * @url:        GET URL of OAuth2 response
 */
void micropub_auth_callback (const gchar *url);

/**
 * micropub_post: (skip)
 * @item:       item to post to blog
 */
void micropub_post (LifereaItem *item);

#endif