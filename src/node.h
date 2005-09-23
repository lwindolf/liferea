/**
 * @file node.h common feed list node handling interface
 * 
 * Copyright (C) 2003-2005 Lars Lindner <lars.lindner@gmx.net>
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

#ifndef _NODE_H
#define _NODE_H

#include "fl_providers/fl_plugin.h"

/** feed list view entry types (FS_TYPE) */
enum node_types {
	FST_INVALID 	= 0,		/**< invalid type */
	FST_FOLDER 	= 1,		/**< the folder type */

	FST_VFOLDER 	= 9,		/**< special type for VFolders */
	FST_FEED	= 10,		/**< Any type of feed */
};

/** generic feed list node structure */
typedef struct node {
	gpointer	data;		/**< node type specific data structure */
	guint		type;		/**< node type */
	gpointer	ui_data;	/**< UI data */
	flNodeHandler 	*handler;	/**< pointer to feed list plugin if this is a plugin node */

	/* feed list state properties of this node */
	gpointer	icon;		/**< pointer to pixmap, if there is a favicon */

	/* item list state properties of this node */
	gboolean	twoPane;	/**< Flag if three pane or condensed mode is set for this feed */
	gint		sortColumn;	/**< Sorting column. Set to either IS_TITLE, or IS_TIME */
	gboolean	sortReversed;	/**< Sort in the reverse order? */

} *nodePtr;
 
/**
 * Node loading/unloading from/to cache 
 */
gboolean node_load(nodePtr np);
void node_unload(nodePtr np);

/**
 * Node content rendering
 */
void node_render(nodePtr np);

#endif
