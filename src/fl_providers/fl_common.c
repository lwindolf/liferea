/**
 * @file fl_common.c common feedlist provider methods
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

#include "conf.h"
#include "debug.h"
#include "favicon.h"
#include "feed.h"
#include "node.h"
#include "fl_providers/fl_common.h"

extern void ui_feed_process_update_result(struct request *request);

/* loading/unloading */

void fl_common_node_load(nodePtr np) {
	feedPtr	fp;

	debug_enter("fl_common_node_load");

	switch(np->type) {
		case FST_FEED:
			fp = (feedPtr)np->data;
			/* np->itemSet will be NULL here, except when cache is disabled */
			node_set_itemset(np, feed_load_from_cache(fp, np->id));
			g_assert(NULL != np->itemSet);
			break;
		default:
			g_warning("fl_common_node_load(): This should not happen!");
			break;
	}

	debug_exit("fl_common_node_load");
}

void fl_common_node_unload(nodePtr np) {
	feedPtr	fp;

	debug_enter("fl_common_node_unload");

	switch(np->type) {
		case FST_FEED:
			fp = (feedPtr)np->data;
			if(CACHE_DISABLE == fp->cacheLimit) {
				debug1(DEBUG_CACHE, "not unloading node (%s) because cache is disabled", node_get_title(np));
			} else {
				debug1(DEBUG_CACHE, "unloading node (%s)", node_get_title(np));
				g_assert(NULL != np->itemSet);
				g_list_free(np->itemSet->items);
				g_free(np->itemSet);
				np->itemSet = NULL;	
			} 
			break;
		default:
			g_warning("fl_common_node_unload(): This should not happen!");
			break;
	}

	debug_exit("fl_common_node_unload");
}

/* update handling */

void fl_common_node_auto_update(nodePtr np) {
	feedPtr		fp = (feedPtr)np->data;
	GTimeVal	now;
	gint		interval;

	debug_enter("fl_common_node_auto_update");

	if(FST_FEED != np->type)	/* don't process folders and vfolders */
		return;

	g_get_current_time(&now);
	interval = feed_get_update_interval(fp);
	
	if(-2 >= interval)
		return;		/* don't update this feed */
		
	if(-1 == interval)
		interval = getNumericConfValue(DEFAULT_UPDATE_INTERVAL);
	
	if(interval > 0)
		if(fp->lastPoll.tv_sec + interval*60 <= now.tv_sec)
			node_schedule_update(np, ui_feed_process_update_result, 0);

	/* And check for favicon updating */
	if(fp->lastFaviconPoll.tv_sec + 30*24*60*60 <= now.tv_sec)
		favicon_download(np);

	debug_exit("fl_common_node_auto_update");
}

void fl_common_node_update(nodePtr np, guint flags) {

	if(FST_FEED == np->type)	/* don't process folders and vfolders */
		node_schedule_update(np, ui_feed_process_update_result, flags | FEED_REQ_PRIORITY_HIGH);
}

/* node saving */

void fl_common_node_save(nodePtr np) {
	switch(np->type) {
		case FST_FEED:
			feed_save_to_cache((feedPtr)np->data, node_get_itemset(np), node_get_id(np));
			break;
		case FST_FOLDER:
		case FST_VFOLDER:
		case FST_PLUGIN:
			/* nothing to do */
			break;
	}
}

/* node description rendering */

gchar *fl_common_node_render(nodePtr np) {

	switch(np->type) {
		case FST_FEED:
			return feed_render((feedPtr)np->data);
			break;
		case FST_FOLDER:
			//return folder_render(np);
			break;
		case FST_VFOLDER:
			//return vfolder_render(np);
			break;
		case FST_PLUGIN:
			/* should never happen as we are root plugin! */
			break;
	}

	return NULL;
}

