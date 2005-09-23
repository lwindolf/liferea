/**
 * @file ui_notification.h mini popup windows
 *
 * Copyright (c) 2004, Karl Soderstrom <ks@xanadunet.net>
 *	      
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
 
#ifndef _UI_NOTIFICATION_H
#define _UI_NOTIFICATION_H

#include "item.h"

void ui_notification_add_new_item(itemPtr ip);

void ui_notification_update(const feedPtr feed_p);

/* to be called when a feed is removed and needs to be removed
   from the notification window too */
void ui_notification_remove_feed(feedPtr fp);

#endif
