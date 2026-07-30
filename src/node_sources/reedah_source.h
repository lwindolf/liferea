/**
 * @file reedah_source.h  Reedah feed list source support
 * 
 * Copyright (C) 2007-2026 Lars Windolf <lars.windolf@gmx.de>
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
 
#ifndef _REEDAH_SOURCE_H
#define _REEDAH_SOURCE_H

#include "node_source.h"

/**
 * API documentation, Reedah is closely modelled after Google Reader, the
 * differences are outlined under the first link, the second is a documentation
 * of the original Google Reader API.
 *
 * @see https://www.reedah.com/developers.php
 * @see http://code.google.com/p/pyrfeed/wiki/GoogleReaderAPI
 */

/**
 * Reedah Login API.
 * @param Email The account email id.
 * @param Passwd The account password.
 * @return The return data has a line "Auth=xxxx" which will be used as an
 *         Authorization header in future requests. 
 */ 
#define REEDAH_READER_LOGIN_URL "https://www.reedah.com/accounts/ClientLogin" 
#define REEDAH_READER_LOGIN_POST "service=reader&Email=%s&Passwd=%s&source=liferea&continue=https://www.reedah.com"

/** Interval (in micro seconds) for doing a Quick Update: 10min */
#define REEDAH_SOURCE_QUICK_UPDATE_INTERVAL 600 * G_USEC_PER_SEC

/**
 * @returns Reedah source type implementation info.
 */
nodeSourceTypePtr reedah_source_get_type (void);

/**
 * Perform login for the given Reedah source.
 *
 * @param root		Reedah source
 * @param flags		network request flags
 */
void reedah_source_login (Node *root, guint32 flags);

#endif
