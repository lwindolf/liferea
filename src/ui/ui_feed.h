/**
 * @file ui_feed.h	UI actions concerning a single feed
 *
 * Copyright (C) 2004-2005 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004-2005 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#ifndef _UI_FEED_H
#define _UI_FEED_H

#include "feed.h"

/* feed handling dialog implementations */
GtkWidget* ui_feed_authdialog_new(nodePtr np, gint flags);
GtkWidget* ui_feed_propdialog_new(nodePtr np);
void ui_feed_newdialog(nodePtr np);

/**
 * Add a feed to the feed list.
 *
 * @parem node		the node the feed belongs to
 * @param source	URI of the feed to add
 * @param filter	filter command (optional)
 * @param flags		download request flags
 */
void ui_feed_add(nodePtr np, const gchar *source, const gchar *filter, gint flags);

#endif /* _UI_FEED_H */
