/**
 * @file google_reader_api_edit.h  Google Reader API syncing support
 * 
 * Copyright (C) 2008 Arnold Noronha <arnstein87@gmail.com>
 * Copyright (C) 2015 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _REEDAH_SOURCE_EDIT_H
#define _REEDAH_SOURCE_EDIT_H

#include "fl_sources/node_source.h"

#include <glib.h>

/**
 * Process the waiting edits on the edit queue. Call this if the state of
 * the nodeSource has changed.
 * 
 * @param gsource The nodeSource whose editQueue should be processed.
 */
void google_reader_api_edit_process (nodeSourcePtr gsource);


/** Edit wrappers */

/**
 * Mark the given item as read. 
 * 
 * @param gsource The nodeSource structure 
 * @param guid   The guid of the item whose status is to be edited
 * @param feedUrl  The feedUrl of the feed containing the item.
 * @param newStatus The new read status of the item (TRUE for read)
 */
void google_reader_api_edit_mark_read (nodeSourcePtr gsource, const gchar* guid, const gchar* feedUrl, gboolean newStatus);

/**
 * Mark the given item as starred.
 * 
 * @param gsource The nodeSource structure 
 * @param guid   The guid of the item whose status is to be edited
 * @param feedUrl  The feedUrl of the feed containing the item.
 * @param newStatus The new read status of the item (TRUE for read)
 */
void google_reader_api_edit_mark_starred (nodeSourcePtr gsource, const gchar *guid, const gchar *feedUrl, gboolean newStatus);

/**
 * Add a subscription to the google source.
 *
 * @param gsource The nodeSource structure
 * @param feedUrl the feed to add
 * @param label   label for new feed (or NULL)
 */
void google_reader_api_edit_add_subscription (nodeSourcePtr gsource, const gchar* feedUrl, const gchar* label);

/**
 * Remove a subscription from the google source.
 * 
 * @param gsource The nodeSource structure
 * @param feedUrl the feed to remove
 * @param get_stream_id_for_node a function that returns the streamId of the
 *                               given node
 */
void google_reader_api_edit_remove_subscription (nodeSourcePtr gsource, const gchar* feedUrl, gchar* (*get_stream_id_for_node) (nodePtr node));

/**
 * Add a category for a subscription
 * 
 * @param gsource The nodeSource structure
 * @param feedUrl the feed to set the category
 * @param label the label to add
 */
void google_reader_api_edit_add_label (nodeSourcePtr gsource, const gchar* feedUrl, const gchar *label);

/**
 * Remove a category for a subscription
 *
 * @param gsource The nodeSource structure
 * @param feedUrl the feed to set the category
 * @param label the label to remove
 */
void google_reader_api_edit_remove_label (nodeSourcePtr gsource, const gchar* feedUrl, const gchar *label);

/**
 * See if an item with give guid is being modified 
 * in the queue.
 *
 * @param nodeSource the nodeSource structure
 * @param guid the guid of the item
 */
gboolean google_reader_api_edit_is_in_queue (nodeSourcePtr gsource, const gchar* guid);

#endif
