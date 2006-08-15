/**
 * @file fl_plugin.h generic feed list provider interface
 * 
 * Copyright (C) 2005-2006 Lars Lindner <lars.lindner@gmx.net>
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

#include <glib.h>
#include <gmodule.h>
#include "node.h"
#include "plugin.h"

/* Liferea allows to have different sources for the feed list
   by adding different feed list provider plugins. Each plugin
   can have one or more instances each adding one sub tree to 
   the feed list.

   At least one plugin must be capable of serving as the root
   node for all other plugins. This mean it has to ensure to load
   all other node plugin instance at their insertion nodes in
   the feed list.

   Each plugin has to be able to serve user requests and is 
   responsible for keeping its feed list node's states up-to-date.
   A plugin can omit all callbacks marked as optional. */


#define FL_PLUGIN_API_VERSION 3

enum {
	FL_PLUGIN_CAPABILITY_IS_ROOT		= (1<<0),	/**< only for default feed list provider */
	FL_PLUGIN_CAPABILITY_ADD		= (1<<1),	/**< allows adding new childs */
	FL_PLUGIN_CAPABILITY_REMOVE		= (1<<2),	/**< allows removing it's childs */
	FL_PLUGIN_CAPABILITY_SUBFOLDERS		= (1<<3),	/**< allows creating/removing sub folders */
	FL_PLUGIN_CAPABILITY_REMOVE_ITEMS	= (1<<4),	/**< allows removing of single items */
	FL_PLUGIN_CAPABILITY_REORDER		= (1<<5),	/**< allows DnD to reorder childs */
	FL_PLUGIN_CAPABILITY_MULTI_INSTANCES	= (1<<6),	/**< allows multiple instances */
	FL_PLUGIN_CAPABILITY_DYNAMIC_CREATION	= (1<<7)	/**< plugin instance user created */
};

/** feed list plugin structure: defines a feed list plugin and it's methods and capabilities */
typedef struct flPlugin {
	unsigned int	api_version;
	
	gchar		*id;		/**< a unique feed list source plugin identifier */
	gchar		*name;		/**< a descriptive plugin name (for preferences and menus) */
	gulong		capabilities;	/**< bitmask of possible feed actions */

	/* plugin loading and unloading methods */
	void		(*plugin_init)(void);
	void 		(*plugin_deinit)(void);

	/**
	 * This OPTIONAL callback is used to create an instance
	 * of the implemented plugin type. It is to be called by 
	 * the parent plugin's node_request_add_*() implementation. 
	 * Mandatory for all plugin's except the root provider plugin.
	 */
	void 		(*source_new)(nodePtr parent);

	/**
	 * This OPTIONAL callback is used to delete an instance
	 * of the implemented plugin type. It is to be called
	 * by the parent plugin's node_remove() implementation.
	 * Mandatory for all plugin's except the root provider plugin.
	 */
	void 		(*source_delete)(nodePtr node);

	/**
	 * This mandatory method is called when the plugin is to
	 * create the feed list subtree attached to the plugin
	 * node.
	 */
	void 		(*source_import)(nodePtr node);

	/**
	 * This mandatory method is called when the plugin is to
	 * save it's feed list subtree (if necessary at all). This
	 * is not a request to save the data of the attached nodes!
	 */
	void 		(*source_export)(nodePtr node);
	
	/**
	 * This method is called to get an OPML representation of
	 * the feedlist of the given node source. Returns a newly
	 * allocated filename string that is to be freed by the
	 * caller.
	 */
	gchar *		(*source_get_feedlist)(nodePtr node);
	
	/**
	 * This OPTIONAL callback is called to start a user requested
	 * update of the source the node belongs to.
	 */
	void		(*source_update)(nodePtr node);
	
	/**
	 * This MANDATORY callback is called regularily to allow 
	 * the the source the node belongs to to auto-update.
	 */
	void		(*source_auto_update)(nodePtr node);
} *flPluginPtr;

/** feed list source instance */
typedef struct flNodeSource {
	flPluginPtr	plugin;		/**< feed list plugin of this source instance */
	updateStatePtr 	updateState;	/**< the update state (etags, last modified, cookies...) of this source */
	nodePtr		root;		/**< root node of this feed list source instance */
	gchar		*url;		/**< URL or filename of the feed list source instance */
} *flNodeSourcePtr;

/** Use this to cast plugin instances from a node structure. */
#define FL_PLUGIN(node) ((flNodeSourcePtr)(node->source))->plugin

/** Feed list plugins are to be declared with this macro. */
#define DECLARE_FL_PLUGIN(flPlugin) \
        G_MODULE_EXPORT flPluginPtr fl_plugin_get_info() { \
                return &flPlugin; \
        }

#define FL_DUMMY_SOURCE_ID	"fl_dummy"

/** 
 * Scans the plugin list for the feed list root source provider.
 * If found creates a new root source and starts it's import.
 *
 * @param node		the root node
 */
void fl_plugins_setup_root(nodePtr node);

/**
 * Loads a feed list provider plugin.
 *
 * @param plugin	plugin info structure
 * @param handle	GModule handle
 *
 * @returns TRUE on success
 */
gboolean fl_plugin_load(pluginPtr plugin, GModule *handle);

/**
 * Plugin specific feed list import parsing.
 *
 * @param node	the node to import
 * @param cur	DOM node to parse
 */
void fl_plugin_import(nodePtr node, xmlNodePtr cur); 

/**
 * Plugin specific feed list export.
 *
 * @param node	the node to export
 * @param cur	DOM node to write to
 */
void fl_plugin_export(nodePtr node, xmlNodePtr cur); 

/**
 * Creates a new source and assigns it to the given new node. 
 * To be used to prepare a source node before adding it to the 
 * feed list.
 *
 * @param node		a newly created node
 * @param flPlugin	plugin implementing the source
 * @param sourceUrl	URI of the source
 */
void fl_plugin_new_source(nodePtr node, flPluginPtr flPlugin, const gchar *sourceUrl);

/**
 * Launches a plugin creation dialog. The new plugin
 * instance will be added to the given node.
 *
 * @param node	the parent node
 */
void ui_fl_plugin_type_dialog(nodePtr node);

/* implementation of the node type interface */
nodeTypePtr fl_plugin_get_node_type(void);

#endif
