/**
 * @file fl_plugin.h generic feedlist provider interface
 * 
 * Copyright (C) 2005 Lars Lindner <lars.lindner@gmx.net>
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

#ifndef _FL_PLUGIN_H
#define _FL_PLUGIN_H

#include "node.h"
#include "feed.h"
#include "folder.h"
#include "plugin.h"

#define FL_PLUGIN_API_VERSION 1

enum {
	FL_PLUGIN_CAPABILITY_IS_ROOT,		/**< only for default feed list provider */
	FL_PLUGIN_CAPABILITY_ADD,		/**< allows adding new childs */
	FL_PLUGIN_CAPABILITY_REMOVE,		/**< allows removing it's childs */
	FL_PLUGIN_CAPABILITY_ADD_FOLDER,	/**< allows adding new folders */
	FL_PLUGIN_CAPABILITY_REMOVE_FOLDER,	/**< allows removing it's folders */
	FL_PLUGIN_CAPABILITY_REMOVE_ITEMS,	/**< allows removing of single items */
	FL_PLUGIN_CAPABILITY_REORDER,		/**< allows DnD to reorder childs */
	FL_PLUGIN_CAPABILITY_MULTI_INSTANCES,	/**< allows multiple instances */
	FL_PLUGIN_CAPABILITY_DYNAMIC_CREATION	/**< plugin instance user created */
};

typedef struct flPluginInfo_ flPluginInfo;

/** feed list plugin structure: defines a feed list plugin and it's methods and capabilities */
struct flPluginInfo_ {
	unsigned int	api_version;

	/* bitmask of possible feed actions */
	gulong		capabilites;

	/* plugin loading and unloading methods */
	void		(*plugin_init)(void);
	void 		(*plugin_deinit)(void);

	/** callback for instance creation request (optional) */
	nodePtr 	(*handler_new)(void);
	/** callback for instance deletion request (optional) */
	void	 	(*handler_delete)(nodePtr ptr);
	
	/** callback for node loading (optional) */
	gboolean	(*node_load)(nodePtr np);
	/** callback for node unloading (optional) */
	void 		(*node_unload)(nodePtr np);

	/** callback for save requests */
	void		(*node_save)(nodePtr ptr);
	/** callback for node rendering */
	gchar *		(*node_render)(nodePtr np);

	/** user interaction callback add/subscribe feeds (optional) */
	feedPtr		(*feed_add)(nodePtr np);
	/** user interaction callback delete/unsubscribe feeds (optional) */
	void		(*feed_delete)(feedPtr fp);

	/** user interaction callback add folder (optional) */
	folderPtr	(*folder_add)(nodePtr np);
	/** user interaction callback delete folder (optional) */
	void		(*folder_delete)(folderPtr fp);

};

typedef struct flNodeHandler_ flNodeHandler;

/** feed list node handler (instance of a feed list plugin) */
struct flNodeHandler_ {
	flPluginInfo	*plugin;	/**< feed list plugin of this handler instance */
	nodePtr		root;		/**< root node of this plugin instance */
};

/** 
 * Scans the plugin list for the feed list root provider.
 *
 * @param plugin_list	list of all plugins
 *
 * @returns pointer to feed list root provider plugin
 */
flPluginInfo * fl_plugins_get_root(GSList *plugin_list);

#endif
