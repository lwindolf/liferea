/*
 * @file content_view.h  presenting items and feeds in HTML
 *
 * Copyright (C) 2006-2025 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _CONTENT_VIEW_H
#define _CONTENT_VIEW_H

#include <glib-object.h>

#include "feedlist.h"
#include "itemlist.h"
#include "ui/liferea_browser.h"

G_BEGIN_DECLS

#define CONTENT_VIEW_TYPE (content_view_get_type ())
G_DECLARE_FINAL_TYPE (ContentView, content_view, CONTENT, VIEW, LifereaBrowser)

/**
 * content_view_create: (skip)
 * @feedlist: (transfer none): the feed list instance
 * @itemlist: (transfer none): the item list instance
 *
 * Creates a content view instance.
 *
 * Returns: (transfer none):	the content view instance
 */
ContentView * content_view_create (FeedList *feedlist, ItemList *itemlist);

/**
 * content_view_clear: (skip)
 * @cv: (transfer none): the content view instance
 *
 * Removes all currently loaded items from the content view.
 */
void content_view_clear (ContentView *cv);

G_END_DECLS

#endif /* _CONTENT_VIEW_H */