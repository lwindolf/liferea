/**
 * @file node.c common feed list node handling
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

#include "node.h"

nodePtr node_new() {
	nodePtr	np;

	np = (nodePtr)g_new0(struct node, 1);
	np->sortColumn = IS_TIME;
	np->sortReveresed = TRUE;	/* default sorting is newest date at top */

	return np;
}

void node_add_data(nodePtr np, guint type, gpointer data) {

	np->type = type;
	np->data = data;
}

void node_free(nodePtr np) {

	// nothing to do
}

static void node_load_cb(nodePtr np, gpointer user_data) {
	itemSetPtr	sp = (itemSetPtr)user_data;
	GSList		*result = NULL;

	switch(np->type) {
		FST_FOLDER:
			result = ui_feedlist_do_foreach_data(np, node_load_cb, (gpointer)sp);
			break;
		FST_FEED:
		FST_PLUGIN
			if(NULL != np->handler->plugin->node_load)
				result = np->handler->plugin->node_load(np);
			break;
		FST_VFOLDER:
			// FIXME:
			break;
		default:
			g_warning("internal error: unknown node type!");
			break;
	}

	if(NULL != result)
		sp->items = g_slist_concat(sp->items, result);

	// FIXME: add unread and new count
}

void node_load(nodePtr np) {

	fp->loaded++;

	if(1 < np->loaded)
		return;

	g_slist_free(np->sp->items);
	np->sp->items = NULL;
	np->sp->newCount = 0;
	np->sp->unreadCount = 0;
	ui_feedlist_do_foreach_data(np, node_load_cb, &(np->sp));
}

void node_save(nodePtr np) {

	g_assert(0 < np->loaded);

	if(FALSE == np->needsCacheSave)
		return;

	np->handler->plugin->node_save(np);
	np->needsCacheSave = FALSE:
}

void node_unload(nodePtr np) {

	g_assert(0 < np->loaded);

	node_save(np);	/* save before unloading */

	if(!getBooleanConfValue(KEEP_FEEDS_IN_MEMORY)) {
		if(1 == np->loaded) {
			if(FST_FEED == np->type) {
				np->handler->plugin->node_unload(np);
				/* never reset the unread/new counter! */
				g_slist_free(np->sp->items);
				np->sp->items = NULL;
				np->loaded--;
			} else {
				/* not unloading vfolders and other types! */
			}
		} else {
			/* not unloading when multiple references */
		}
	}
}

void node_render(nodePtr np) {

	np->handler->plugin->node_render(np);
}

void node_update(nodePtr np, guint flags) {

	if(FST_VFOLDER == np->type)
		return;

	np->handler->plugin->node_update(np, flags);
}

void node_remove(nodePtr np) {

	if(NULL != np->icon) {
		g_object_unref(np->icon);
		favicon_remove(np);
	}

	switch(np->type) {
		case FST_FEED:
		case FST_VFOLDER:
			np->handler->plugin->feed_delete(np);
			break;
		case FST_FOLDER:
			np->handler->plugin->folder_delete(np);
			break;
		case FST_PLUGIN:
			np->handler->plugin->handler_delete(np);
			break;
		default:
			g_warning("internal error: unknown node type!");
			break;
	}

	node_free(np);
}

nodePtr node_add_feed(nodePtr parent) {
	nodePtr	child;
	feedPtr	fp;

	child = node_new();
	fp = parent->handler->plugin->feed_add(child);
	node_add_data(child, FST_FEED, (gpointer)fp);

	return child;
}

nodePtr node_add_folder(nodePtr parent) {
	nodePtr		subfolder;
	folderPtr	fp;

	subfolder = node_new();
	fp = parent->handler->plugin->folder_add(subfolder);
	node_add_data(subfolder, FST_FOLDER, (gpointer)fp);

	return subfolder;
}

/* ---------------------------------------------------------------------------- */
/* node attributes encapsulation						*/
/* ---------------------------------------------------------------------------- */

void node_set_id(nodePtr np, const gchar *id) {

	g_free(np->id);
	np->id = g_strdup(id);
}

const gchar *node_get_id(nodePtr np) { return np->id; }

void node_set_sort_column(nodePtr np, gint sortColumn, gboolean reversed) {

	np->sortColumn = sortColumn;
	np->sortReversed = reversed;
	feedlist_schedule_save();
}

void node_set_two_pane_mode(nodePtr np, gboolean newMode) { np->twoPane = newMode; }

gboolean node_get_two_pane_mode(nodePtr np) { return np->twoPane; }


