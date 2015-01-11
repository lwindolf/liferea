/**
 * @file google_reader_api.h  Interface for implementing the Google Reader API
 * 
 * Copyright (C) 2014-2015 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _GOOGLE_READER_API_H
#define _GOOGLE_READER_API_H

/** A set of tags (states) defined by Google Reader */

#define GOOGLE_READER_TAG_KEPT_UNREAD          "user/-/state/com.google/kept-unread"
#define GOOGLE_READER_TAG_READ                 "user/-/state/com.google/read"
#define GOOGLE_READER_TAG_TRACKING_KEPT_UNREAD "user/-/state/com.google/tracking-kept-unread"
#define GOOGLE_READER_TAG_STARRED              "user/-/state/com.google/starred"

typedef struct googleReaderApi {
	gboolean	json;	/**< Returns mostly JSON */

	/** Endpoint definitions */
	const char	*unread_count;
	const char	*subscription_list;
	const char	*add_subscription;
	const char	*add_subscription_post;
	const char	*remove_subscription;
	const char	*remove_subscription_post;
	const char	*edit_tag;
	const char	*edit_tag_add_post;
	const char	*edit_tag_ar_tag_post;
	const char	*edit_tag_remove_post;
	const char	*edit_add_label;
	const char	*edit_add_label_post;
	const char	*token;
	/* when extending this list add assertions in node_source_type_register! */
} googleReaderApi;

#endif
