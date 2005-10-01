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
#include "conf.h"
#include "fl_providers/fl_plugin.h"
#include "ui/ui_itemlist.h"

nodePtr node_new() {
	nodePtr	np;

	np = (nodePtr)g_new0(struct node, 1);
	np->sortColumn = IS_TIME;
	np->sortReversed = TRUE;	/* default sorting is newest date at top */
	np->available = FALSE;
	np->unreadCount = 0;
	np->title = g_strdup("FIXME");

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

	switch(np->type) {
		FST_FOLDER:
			ui_feedlist_do_foreach_data(np, node_load_cb, (gpointer)sp);
			break;
		FST_FEED:
		FST_PLUGIN:
			if(NULL != FL_PLUGIN(np)->node_load)
				FL_PLUGIN(np)->node_load(np);
			break;
		FST_VFOLDER:
			// FIXME:
			break;
		default:
			g_warning("internal error: unknown node type!");
			break;
	}

	sp->items = g_slist_concat(sp->items, np->itemSet->items);
	sp->newCount += np->itemSet->newCount;
	sp->unreadCount += np->itemSet->unreadCount;
}

void node_load(nodePtr np) {

	np->loaded++;

	if(1 < np->loaded)
		return;

	g_assert(NULL == np->itemSet);
	ui_feedlist_do_foreach_data(np, node_load_cb, (gpointer)&(np->itemSet));
}

void node_save(nodePtr np) {

	g_assert(0 < np->loaded);

	if(FALSE == np->needsCacheSave)
		return;

	FL_PLUGIN(np)->node_save(np);
	np->needsCacheSave = FALSE;
}

void node_unload(nodePtr np) {

	g_assert(0 < np->loaded);

	node_save(np);	/* save before unloading */

	if(!getBooleanConfValue(KEEP_FEEDS_IN_MEMORY)) {
		if(1 == np->loaded) {
			if(FST_FEED == np->type) {
				FL_PLUGIN(np)->node_unload(np);
				/* never reset the unread/new counter! */
				g_slist_free(np->itemSet->items);
				np->itemSet->items = NULL;
				np->loaded--;
			} else {
				/* not unloading vfolders and other types! */
			}
		} else {
			/* not unloading when multiple references */
		}
	}
}

gchar * node_render(nodePtr np) {

	return FL_PLUGIN(np)->node_render(np);
}

void node_request_update(nodePtr np, guint flags) {

	if(FST_VFOLDER == np->type)
		return;

	FL_PLUGIN(np)->node_update(np, flags);
}

void node_request_auto_update(nodePtr np) {

	if(FST_VFOLDER == np->type)
		return;

	FL_PLUGIN(np)->node_auto_update(np);
}

void node_remove(nodePtr np) {

	if(NULL != np->icon) {
		g_object_unref(np->icon);
		favicon_remove(np);
	}

	switch(np->type) {
		case FST_FEED:
		case FST_VFOLDER:
			FL_PLUGIN(np)->feed_delete((feedPtr)np->data);
			break;
		case FST_FOLDER:
			FL_PLUGIN(np)->folder_delete((folderPtr)np->data);
			break;
		case FST_PLUGIN:
			FL_PLUGIN(np)->handler_delete(np);
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
	fp = FL_PLUGIN(parent)->feed_add(child);
	node_add_data(child, FST_FEED, (gpointer)fp);

	return child;
}

nodePtr node_add_folder(nodePtr parent) {
	nodePtr		subfolder;
	folderPtr	fp;

	subfolder = node_new();
	fp = FL_PLUGIN(parent)->folder_add(subfolder);
	node_add_data(subfolder, FST_FOLDER, (gpointer)fp);

	return subfolder;
}

/* ---------------------------------------------------------------------------- */
/* node attributes encapsulation						*/
/* ---------------------------------------------------------------------------- */

void node_set_title(nodePtr np, const gchar *title) {

	g_free(np->title);
	np->title = g_strdup(title);
}

const gchar * node_get_title(nodePtr np) { return np->title }

void node_set_unread_count(nodePtr np, guint unreadCount) {

	/* unread count propagation to folders
	   is done by specific implementations
	   to be more flexible */
	np->unreadCount = unreadCount;
}

guint node_get_unread_count(nodePtr np) { return np->unreadCount; }

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


