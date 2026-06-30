/**
 * @file theoldreader_source.h TheOldReader feed list source support
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
 
#ifndef _THEOLDREADER_SOURCE_H
#define _THEOLDREADER_SOURCE_H

#include "node_source.h"

/**
 * TheOldReader API URL's
 * In each of the following, the _URL indicates the URL to use, and _POST
 * indicates the corresponging postdata to send.
 * @see https://github.com/krasnoukhov/theoldreader-api
 */

/**
 * TheOldReader Login api.
 * @param Email The google account email id.
 * @param Passwd The google account password.
 * @return The return data has a line "Auth=xxxx" which will be used as an
 *         Authorization header in future requests. 
 */ 
#define THEOLDREADER_READER_LOGIN_URL "https://theoldreader.com/accounts/ClientLogin" 
#define THEOLDREADER_READER_LOGIN_POST "service=reader&Email=%s&Passwd=%s&source=liferea&continue=https://theoldreader.com"

/**
 * @returns TheOldReader source type implementation info.
 */
nodeSourceTypePtr theoldreader_source_get_type (void);

/**
 * Perform login for the given Google source.
 *
 * @param root		TheOldReaderSource node
 * @param flags		network request flags
 */
void theoldreader_source_login (Node *root, guint32 flags);

#endif
