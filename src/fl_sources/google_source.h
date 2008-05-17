/**
 * @file google_source.h Google Reader feed list source support
 * 
 * Copyright (C) 2007 Lars Lindner <lars.lindner@gmail.com>
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
 
#ifndef _GOOGLE_SOURCE_H
#define _GOOGLE_SOURCE_H

#include "fl_sources/node_source.h"


typedef struct reader {
	nodePtr		root;	/**< the root node in the feed list */
	gchar		*sid;	/**< session id */
	GTimeVal	*lastSubscriptionListUpdate;
	GQueue          *editQueue;
	enum { 
		READER_STATE_NONE = 0,
		READER_STATE_IN_PROGRESS,
		READER_STATE_ACTIVE
	} loginState ; 
} *readerPtr;


enum googleSourceUpdateFlags { 
	/**
	 * Update only the subscription list, and not each node underneath it.
	 * Note: Uses higher 16 bits to avoid conflict.
	 */
	GOOGLE_SOURCE_UPDATE_ONLY_LIST = (1<<16),

	/**
	 * Only login, do not do any updates. 
	 */
	GOOGLE_SOURCE_UPDATE_ONLY_LOGIN = ( 1<<17)
} ;

/**
 * Returns Google Reader source type implementation info.
 */
nodeSourceTypePtr google_source_get_type(void);


#endif
