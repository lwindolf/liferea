/**
 * @file fl_opml.c OPML Planet/Blogroll feedlist provider
 * 
 * Copyright (C) 2006 Lars Lindner <lars.lindner@gmx.net>
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

#include <glib.h>
#include <libxml/xpath.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <unistd.h>

#include "support.h"
#include "common.h"
#include "debug.h"
#include "feed.h"
#include "feedlist.h"
#include "node.h"
#include "export.h"
#include "ui/ui_feedlist.h"
#include "fl_sources/fl_opml.h"
#include "fl_sources/fl_opml-ui.h"
#include "fl_sources/fl_opml-cb.h"
#include "fl_sources/fl_plugin.h"
#include "notification/notif_plugin.h"
#include "ui/ui_node.h"

/** default OPML update interval = once a day */
#define OPML_SOURCE_UPDATE_INTERVAL 60*60*24

static struct flPlugin fpi;

static gchar * fl_opml_source_get_feedlist(nodePtr node) {

	return common_create_cache_filename("cache" G_DIR_SEPARATOR_S "plugins", node->id, "opml");
}

static void fl_opml_source_import(nodePtr node) {
	gchar		*filename;

	debug_enter("fl_opml_source_import");

	fl_opml_source_setup(NULL, node);
	
	debug1(DEBUG_CACHE, "starting import of opml plugin instance (id=%s)\n", node->id);
	filename = fl_opml_source_get_feedlist(node);
	if(g_file_test(filename, G_FILE_TEST_EXISTS)) {
		import_OPML_feedlist(filename, node, node->source, FALSE, TRUE);
	} else {
		g_warning("cannot open \"%s\"", filename);
		node->available = FALSE;
	}
	g_free(filename);

	debug_exit("fl_opml_source_import");
}

static void fl_opml_source_export(nodePtr node) {
	gchar		*filename;
	
	debug_enter("fl_opml_source_export");

	/* Although the OPML structure won't change, it needs to
	   be saved so that the feed ids are saved to disk after
	   the first import or updates of the source OPML. */

	g_assert(node == node->source->root);

	filename = fl_opml_source_get_feedlist(node);	   
	export_OPML_feedlist(filename, node, TRUE);
	g_free(filename);
	
	debug_exit("fl_opml_source_export");
}

static void fl_opml_source_new(nodePtr parent) {

	ui_fl_opml_get_source_url(parent);
}

static void fl_opml_source_remove(nodePtr node) {
	gchar		*filename;
	
	g_assert(node == node->source->root);

	/* step 1: delete all feed cache files */
	node_foreach_child(node, node_request_remove);
	g_assert(!node->children);
	
	/* step 2: delete plugin instance OPML cache file */
	filename = fl_opml_source_get_feedlist(node);
	unlink(filename);
	g_free(filename);
}

typedef struct mergeCtxt {
	nodePtr		rootNode;	/* root node of the OPML feed list source */
	nodePtr		parent;		/* currently processed feed list node */
	xmlNodePtr	xmlNode;	/* currently processed XML node of old OPML doc */
} *mergeCtxtPtr;

static void fl_opml_source_merge_feed(xmlNodePtr match, gpointer user_data) {
	mergeCtxtPtr	mergeCtxt = (mergeCtxtPtr)user_data;
	xmlChar		*url, *title;
	gchar		*expr;
	nodePtr		node = NULL;

	url = xmlGetProp(match, "xmlUrl");
	title = xmlGetProp(match, "title");
	if(!title)
		title = xmlGetProp(match, "description");
	if(!title)
		title = xmlGetProp(match, "text");
	if(!title && !url)
		return;
		
	if(url)
		expr = g_strdup_printf("//outline[@xmlUrl = '%s']", url);
	else			
		expr = g_strdup_printf("//outline[@title = '%s']", title);

	if(!common_xpath_find(mergeCtxt->xmlNode, expr)) {
		debug2(DEBUG_UPDATE, "adding %s (%s)\n", title, url);
		node = node_new();
		node_set_title(node, title);
		if(url)
			node_add_data(node, FST_FEED, feed_new(url, NULL));
		else
			node_add_data(node, FST_FOLDER, NULL);
		node_add_child(mergeCtxt->parent, node, -1);
		node_request_update(node, FEED_REQ_RESET_TITLE);
	}		
	
	/* Recursion if this is a folder */
	if(!url) {
		if(!node) {
			/* if the folder node wasn't created above it
			   must already exist and we search it in the 
			   parents children list */
			GSList	*iter = mergeCtxt->parent->children;
			while(iter) {
				if(g_str_equal(title, node_get_title(iter->data)))
					node = iter->data;
				iter = g_slist_next(iter);
			}
		}
		
		if(node) {
			mergeCtxtPtr mc = g_new0(struct mergeCtxt, 1);
			mc->rootNode = mergeCtxt->rootNode;
			mc->parent = node;
			mc->xmlNode = mergeCtxt->xmlNode;	// FIXME: must be correct child!
			common_xpath_foreach_match(match, "./outline", fl_opml_source_merge_feed, (gpointer)mc);
			g_free(mc);
		} else {
			g_warning("fl_opml_source_merge_feed(): bad! bad! very bad!");
		}
	}

	g_free(expr);
	xmlFree(title);
	xmlFree(url);
}

// FIXME: broken for empty feed lists!
static void fl_opml_source_check_for_removal(nodePtr node, gpointer user_data) {
	feedPtr		feed = node->data;
	gchar		*expr = NULL;

	switch(node->type) {
		case FST_FEED:
			expr = g_strdup_printf("//outline[ @xmlUrl='%s' ]", feed->source);
			break;
		case FST_FOLDER:
			node_foreach_child_data(node, fl_opml_source_check_for_removal, user_data);
			expr = g_strdup_printf("//outline[ (@title='%s') or (@text='%s') or (@description='%s')]", node->title, node->title, node->title);
			break;
		default:
			g_warning("fl_opml_source_check_for_removal(): This should never happen...");
			return;
			break;
	}
	
	if(!common_xpath_find((xmlNodePtr)user_data, expr)) {
		debug1(DEBUG_UPDATE, "removing %s...\n", node_get_title(node));
		if(feedlist_get_selected() == node)
			ui_feedlist_select(NULL);
		node_request_remove(node);
	} else {
		debug1(DEBUG_UPDATE, "keeping %s...\n", node_get_title(node));
	}
	g_free(expr);
}

static void fl_opml_process_update_results(struct request *request) {
	nodePtr		node = (nodePtr)request->user_data;
	mergeCtxtPtr	mergeCtxt;
	xmlDocPtr	doc, oldDoc;
	
	debug1(DEBUG_UPDATE, "OPML download finished data=%d", request->data);

	node->available = FALSE;

	if(request->data) {
		doc = common_parse_xml(request->data, request->size, NULL);
		if(doc) {		
			/* Go through all existing nodes and remove those whose
			   URLs are not in new feed list. Also removes those URLs
			   from the list that have corresponding existing nodes. */
			node_foreach_child_data(node, fl_opml_source_check_for_removal, 
			                        (gpointer)xmlDocGetRootElement(doc));
						
			fl_opml_source_export(node);	/* save new feed list tree to disk 
			                                   to ensure correct document in 
							   next step */
			
			/* Merge up-to-date OPML feed list. */
			oldDoc = xmlParseFile(fl_opml_source_get_feedlist(node));
			
			mergeCtxt = g_new0(struct mergeCtxt, 1);
			mergeCtxt->rootNode = node;
			mergeCtxt->parent = node;
			mergeCtxt->xmlNode = xmlDocGetRootElement(oldDoc);
			
			common_xpath_foreach_match(xmlDocGetRootElement(doc),
			                           "/opml/body/outline",
						   fl_opml_source_merge_feed,
						   (gpointer)mergeCtxt);

			g_free(mergeCtxt);
			xmlFreeDoc(oldDoc);			
			xmlFreeDoc(doc);
			
			fl_opml_source_export(node);	/* save new feed list tree to disk */
			
			node->available = TRUE;
		} else {
			g_warning("Cannot parse downloaded OPML document!");
		}
	}
	
	node_foreach_child(node, node_request_update);
}

void fl_opml_source_update(nodePtr node) {
	requestPtr	request;
	
	if(node->source->url) {
	g_assert(node->source->updateState);
		request = update_request_new();
		request->updateState = node->source->updateState;
		request->source = g_strdup(node->source->url);
		request->priority = 1;
		request->callback = fl_opml_process_update_results;
		request->user_data = node;
		debug2(DEBUG_UPDATE, "updating OPML source %s (node id %s)", node->source->url, node->id);
		update_execute_request(request);
		g_get_current_time(&request->updateState->lastPoll);	
	} else {
		g_warning("Cannot update feed list source %s: missing URL!\n", node->title);
	}
}

static void fl_opml_source_auto_update(nodePtr node) {
	GTimeVal	now;
	
	g_get_current_time(&now);
	
	if(node->source->updateState->lastPoll.tv_sec + OPML_SOURCE_UPDATE_INTERVAL <= now.tv_sec)
		fl_opml_source_update(node);	
}

/** called during import and when subscribing, we will do
    node_add_child() only when subscribing */
void fl_opml_source_setup(nodePtr parent, nodePtr node) {

	node->icon = create_pixbuf("fl_opml.png");
	
	node_add_data(node, FST_PLUGIN, NULL);
	if(parent)
		node_add_child(parent, node, 0);
}

static void fl_opml_init(void) { }

static void fl_opml_deinit(void) { }

/* feed list provider plugin definition */

static struct flPlugin fpi = {
	FL_PLUGIN_API_VERSION,
	"fl_opml",
	"Planet, BlogRoll, OPML",
	FL_PLUGIN_CAPABILITY_DYNAMIC_CREATION,
	fl_opml_init,
	fl_opml_deinit,
	fl_opml_source_new,
	fl_opml_source_remove,
	fl_opml_source_import,
	fl_opml_source_export,
	fl_opml_source_get_feedlist,
	fl_opml_source_update,
	fl_opml_source_auto_update
};

static struct plugin pi = {
	PLUGIN_API_VERSION,
	"OPML Feed List Source Plugin",
	PLUGIN_TYPE_FEEDLIST_PROVIDER,
	&fpi
};

DECLARE_PLUGIN(pi);
DECLARE_FL_PLUGIN(fpi);
