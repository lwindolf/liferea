/**
 * @file reedah_source.h  Reedah feed list source support
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
 
#ifndef _REEDAH_SOURCE_H
#define _REEDAH_SOURCE_H

#include "fl_sources/node_source.h"

/**
 * A nodeSource specific for Reedah
 */
typedef struct ReedahSource {
	nodePtr		root;		/**< the root node in the feed list */

	/**
	 * A map from a subscription source to a timestamp when it was last
	 * updated according to API
	 */
	GHashTable      *lastTimestampMap; 

	/**
	 * A timestamp when the last Quick update took place.
	 */
	guint64         lastQuickUpdate;
} *ReedahSourcePtr;

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
#define REEDAH_READER_LOGIN_POST "service=reader&Email=%s&Passwd=%s&source=liferea&continue=http://www.reedah.com"

/** Interval (in micro seconds) for doing a Quick Update: 10min */
#define REEDAH_SOURCE_QUICK_UPDATE_INTERVAL 600 * G_USEC_PER_SEC

/**
 * @returns Reedah source type implementation info.
 */
nodeSourceTypePtr reedah_source_get_type (void);

/**
 * Find a child node with the given feed source URL.
 *
 * @param gsource	ReedahSource
 * @param source	a feed source URL to search
 *
 * @returns a node (or NULL)
 */
nodePtr reedah_source_get_node_from_source (ReedahSourcePtr gsource, const gchar* source);

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
gboolean reedah_source_quick_update_timeout (gpointer gsource);

/**
 * Migrate a google source child-node from a Liferea 1.4 style read-only
 * google source nodes.
 *
 * @param node The node to migrate (not the nodeSource!)
 */
void reedah_source_migrate_node (nodePtr node);

/**
 * Perform login for the given Google source.
 *
 * @param gsource	a ReedahSource
 * @param flags		network request flags
 */
void reedah_source_login (ReedahSourcePtr gsource, guint32 flags);

#endif
