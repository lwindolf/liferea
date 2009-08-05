/**
 * @file notification.h  generic notification interface
 * 
 * Copyright (C) 2006 Norman Jonas <liferea.sf.net@devport.codepilot.net>
 * Copyright (C) 2006-2008 Lars Lindner <lars.lindner@gmail.com>
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

#ifndef _NOTIFICATION_H
#define _NOTIFICATION_H

#include <glib.h>
#include <gmodule.h>
#include "node.h"

typedef enum {
	NOTIFICATION_TYPE_POPUP,
	NOTIFICATION_TYPE_TRAY,
} notificationType;

typedef struct notificationPlugin {
	/**
	 * Notification plugin name
	 */
	const gchar		*name;
	
	/**
	 * Called once during plugin initialization.
	 * If the plugin returns FALSE it won't be
	 * added to the list of the available 
	 * notification plugins.
	 */
	gboolean (*plugin_init)(void);
	
	/**
	 * Called upon program shutdown.
	 */
	void	(*plugin_deinit)(void);
	
	/**
	 * This callback notifies the plugin that the given
	 * node was updated and contains new items (items
	 * with newStatus set to TRUE.
	 *
	 * @param node		the updated node
	 * @param enforced	TRUE if popup is to be enforced
	 *			regardless of global preference
	 */
	void 	(*node_has_new_items)(nodePtr node, gboolean enforced);
	
} *notificationPluginPtr;


extern struct notificationPlugin libnotify_plugin;

#ifdef HAVE_LIBNOTIFY
/**
 * "New items" event callback.
 *
 * @param node		the node that has new items
 * @param enforced	TRUE if notification is to be enforced
 * 			regardless of global preference
 */
void notification_node_has_new_items (nodePtr node, gboolean enforced);
#else
static inline void notification_node_has_new_items (nodePtr node, gboolean enforced) {};
#endif


void notification_plugin_register (notificationPluginPtr plugin);

#endif
