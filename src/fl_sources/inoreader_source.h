/**
 * @file inoreader_source.h  InoReader feed list source support
 * 
 * Copyright (C) 2007-2014 Lars Windolf <lars.windolf@gmx.de>
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
 
#ifndef _INOREADER_SOURCE_H
#define _INOREADER_SOURCE_H

#include "fl_sources/node_source.h"

/**
 * A nodeSource specific for InoReader
 */
typedef struct InoreaderSource {
	nodePtr		root;		/**< the root node in the feed list */

	/**
	 * A map from a subscription source to a timestamp when it was last 
	 * updated provided by remote.
	 */
	GHashTable      *lastTimestampMap; 

	/**
	 * A timestamp when the last Quick update took place.
	 */
	GTimeVal        lastQuickUpdate;
} *InoreaderSourcePtr;

/**
 * Google Reader Login api.
 * @param Email The google account email id.
 * @param Passwd The google account password.
 * @return The return data has a line "Auth=xxxx" which will be used as an
 *         Authorization header in future requests. 
 */ 
#define INOREADER_LOGIN_URL "https://www.inoreader.com/accounts/ClientLogin" 
#define INOREADER_LOGIN_POST "service=reader&Email=%s&Passwd=%s&source=liferea&continue=http://www.inoreader.com"

/** Interval (in seconds) for doing a Quick Update: 10min */
#define INOREADER_SOURCE_QUICK_UPDATE_INTERVAL 600

/**
 * @returns InoReader source type implementation info.
 */
nodeSourceTypePtr inoreader_source_get_type (void);

/**
 * Find a child node with the given feed source URL.
 *
 * @param gsource	InoreaderSource
 * @param source	a feed source URL to search
 *
 * @returns a node (or NULL)
 */
nodePtr inoreader_source_get_node_from_source (InoreaderSourcePtr gsource, const gchar* source);

/**
 * Tries to update the entire source quickly, by updating only those feeds
 * which are known to be updated. Suitable for g_timeout_add. This is an 
 * internal function.
 *
 * @param data A pointer to a node id of the source. This pointer will
 *             be g_free'd if the update fails.
 *
 * @returns FALSE on update failure
 */
gboolean inoreader_source_quick_update_timeout (gpointer gsource);

/**
 * Perform login for the given InoReader source.
 *
 * @param gsource	a InoreaderSource
 * @param flags		network request flags
 */
void inoreader_source_login (InoreaderSourcePtr gsource, guint32 flags);

#endif
