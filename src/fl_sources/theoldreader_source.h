/**
 * @file theoldreader_source.h TheOldReader feed list source support
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
 
#ifndef _THEOLDREADER_SOURCE_H
#define _THEOLDREADER_SOURCE_H

#include "fl_sources/node_source.h"

/**
 * A nodeSource specific for TheOldReader
 */
typedef struct TheOldReaderSource {
	nodePtr		root;		/**< the root node in the feed list */

	/**
	 * A map from a subscription source to a timestamp when it was last 
	 * updated (provided by Google).
	 */
	GHashTable      *lastTimestampMap; 

	GHashTable	*folderToCategory;	/**< Lookup hash for folder node id to TheOldReader category id */
} *TheOldReaderSourcePtr;

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
#define THEOLDREADER_READER_LOGIN_POST "service=reader&Email=%s&Passwd=%s&source=liferea&continue=http://theoldreader.com"

/**
 * @returns TheOldReader source type implementation info.
 */
nodeSourceTypePtr theoldreader_source_get_type (void);

/**
 * Find a child node with the given feed source URL.
 *
 * @param gsource	TheOldReaderSource
 * @param source	a feed source URL to search
 *
 * @returns a node (or NULL)
 */
nodePtr theoldreader_source_get_node_from_source (TheOldReaderSourcePtr gsource, const gchar* source);

/**
 * Migrate a google source child-node from a Liferea 1.4 style read-only
 * google source nodes.
 *
 * @param node The node to migrate (not the nodeSource!)
 */
void theoldreader_source_migrate_node (nodePtr node);

/**
 * Perform login for the given Google source.
 *
 * @param gsource	a TheOldReaderSource
 * @param flags		network request flags
 */
void theoldreader_source_login (TheOldReaderSourcePtr gsource, guint32 flags);

#endif
