/**
 * @file theoldreader_source_edit.c  TheOldReader feed list source syncing support
 * 
 * Copyright (C) 2008 Arnold Noronha <arnstein87@gmail.com>
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

#ifndef _THEOLDREADER_SOURCE_EDIT_H
#define _THEOLDREADER_SOURCE_EDIT_H

#include "theoldreader_source.h"

#include <glib.h>

/**
 * Process the waiting edits on the edit queue. Call this if the state of
 * the TheOldReaderSource has changed.
 * 
 * @param gsource The TheOldReaderSource whose editQueue should be processed.
 */
void theoldreader_source_edit_process (TheOldReaderSourcePtr gsource);


/** Edit wrappers */

/**
 * Mark the given item as read. 
 * 
 * @param gsource The TheOldReaderSource structure 
 * @param guid   The guid of the item whose status is to be edited
 * @param feedUrl  The feedUrl of the feed containing the item.
 * @param newStatus The new read status of the item (TRUE for read)
 */
void theoldreader_source_edit_mark_read (TheOldReaderSourcePtr gsource, const gchar* guid, const gchar* feedUrl, gboolean newStatus);

/**
 * Mark the given item as starred.
 * 
 * @param gsource The TheOldReaderSource structure 
 * @param guid   The guid of the item whose status is to be edited
 * @param feedUrl  The feedUrl of the feed containing the item.
 * @param newStatus The new read status of the item (TRUE for read)
 */
void theoldreader_source_edit_mark_starred (TheOldReaderSourcePtr gsource, const gchar *guid, const gchar *feedUrl, gboolean newStatus);


/**
 * Add a subscription to the google source.
 *
 * @param gsource The TheOldReaderSource structure
 * @param feedUrl the feed to add
 */
void theoldreader_source_edit_add_subscription (TheOldReaderSourcePtr gsource, const gchar* feedUrl);


/**
 * Remove a subscription from the google source.
 * 
 * @param gsource The TheOldReaderSource structure
 * @param feedUrl the feed to remove
 */
void theoldreader_source_edit_remove_subscription (TheOldReaderSourcePtr gsource, const gchar* feedUrl);

/**
 * See if an item with give guid is being modified 
 * in the queue.
 *
 * @param TheOldReaderSource the TheOldReaderSource structure
 * @param guid the guid of the item
 */
gboolean theoldreader_source_edit_is_in_queue (TheOldReaderSourcePtr gsource, const gchar* guid);

#endif
