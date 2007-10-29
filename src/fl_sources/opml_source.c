/**
 * @file opml_source.c OPML Planet/Blogroll feed list source
 * 
 * Copyright (C) 2006-2007 Lars Lindner <lars.lindner@gmail.com>
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
#include <gtk/gtk.h>
#include <unistd.h>

#include "common.h"
#include "db.h"
#include "debug.h"
#include "export.h"
#include "feed.h"
#include "feedlist.h"
#include "folder.h"
#include "node.h"
#include "xml.h"
#include "ui/ui_dialog.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_node.h"
#include "fl_sources/opml_source.h"
#include "fl_sources/node_source.h"
#include "notification/notif_plugin.h"

/** default OPML update interval = once a day */
#define OPML_SOURCE_UPDATE_INTERVAL 60*60*24

static void ui_opml_source_get_source_url (nodePtr parent);

gchar * opml_source_get_feedlist(nodePtr node) {

	return common_create_cache_filename("cache" G_DIR_SEPARATOR_S "plugins", node->id, "opml");
}

void opml_source_import(nodePtr node) {
	gchar		*filename;

	debug_enter("opml_source_import");

	opml_source_setup(NULL, node);
	
	debug1(DEBUG_CACHE, "starting import of opml source instance (id=%s)", node->id);
	filename = opml_source_get_feedlist(node);
	if(g_file_test(filename, G_FILE_TEST_EXISTS)) {
		import_OPML_feedlist(filename, node, node->source, FALSE, TRUE);
	} else {
		g_warning("cannot open \"%s\"", filename);
		node->available = FALSE;
	}
	g_free(filename);
	
	subscription_set_update_interval (node->subscription, OPML_SOURCE_UPDATE_INTERVAL);

	debug_exit("opml_source_import");
}

void opml_source_export(nodePtr node) {
	gchar		*filename;
	
	debug_enter("opml_source_export");

	/* Although the OPML structure won't change, it needs to
	   be saved so that the feed ids are saved to disk after
	   the first import or updates of the source OPML. */

	g_assert(node == node->source->root);

	filename = opml_source_get_feedlist(node);	   
	export_OPML_feedlist(filename, node, TRUE);
	g_free(filename);
	
	debug1(DEBUG_CACHE, "adding OPML source: title=%s", node_get_title(node));
	
	debug_exit("opml_source_export");
}

void opml_source_remove(nodePtr node) {
	gchar		*filename;
	
	g_assert(node == node->source->root);

	/* step 1: delete all feed cache files */
	node_foreach_child(node, node_request_remove);
	g_assert(!node->children);
	
	/* step 2: delete source instance OPML cache file */
	filename = opml_source_get_feedlist(node);
	unlink(filename);
	g_free(filename);
}

typedef struct mergeCtxt {
	nodePtr		rootNode;	/* root node of the OPML feed list source */
	nodePtr		parent;		/* currently processed feed list node */
	xmlNodePtr	xmlNode;	/* currently processed XML node of old OPML doc */
} *mergeCtxtPtr;

static void
opml_source_merge_feed (xmlNodePtr match, gpointer user_data)
{
	mergeCtxtPtr	mergeCtxt = (mergeCtxtPtr)user_data;
	xmlChar		*url, *title;
	gchar		*expr;
	nodePtr		node = NULL;

	url = xmlGetProp (match, "xmlUrl");
	title = xmlGetProp (match, "title");
	if (!title)
		title = xmlGetProp (match, "description");
	if (!title)
		title = xmlGetProp (match, "text");
	if (!title && !url)
		return;
		
	if (url)
		expr = g_strdup_printf ("//outline[@xmlUrl = '%s']", url);
	else			
		expr = g_strdup_printf ("//outline[@title = '%s']", title);

	if (!xpath_find (mergeCtxt->xmlNode, expr)) {
		debug2(DEBUG_UPDATE, "adding %s (%s)", title, url);
		node = node_new();
		node_set_title (node, title);
		if (url) {
			node_set_type (node, feed_get_node_type ());
			node_set_data (node, feed_new ());
			node_set_subscription (node, subscription_new (url, NULL, NULL));
		} else {
			node_set_type (node, folder_get_node_type ());
		}
		node_add_child (mergeCtxt->parent, node, -1);
		subscription_update (node->subscription, FEED_REQ_RESET_TITLE);
	}		
	
	/* Recursion if this is a folder */
	if (!url) {
		if (!node) {
			/* if the folder node wasn't created above it
			   must already exist and we search it in the 
			   parents children list */
			GSList	*iter = mergeCtxt->parent->children;
			while (iter) {
				if (g_str_equal (title, node_get_title (iter->data)))
					node = iter->data;
				iter = g_slist_next (iter);
			}
		}
		
		if (node) {
			mergeCtxtPtr mc = g_new0 (struct mergeCtxt, 1);
			mc->rootNode = mergeCtxt->rootNode;
			mc->parent = node;
			mc->xmlNode = mergeCtxt->xmlNode;	// FIXME: must be correct child!
			xpath_foreach_match (match, "./outline", opml_source_merge_feed, (gpointer)mc);
			g_free (mc);
		} else {
			g_warning ("opml_source_merge_feed(): bad! bad! very bad!");
		}
	}

	g_free (expr);
	xmlFree (title);
	xmlFree (url);
}

static void
opml_source_check_for_removal (nodePtr node, gpointer user_data)
{
	gchar		*expr = NULL;

	if (IS_FEED (node)) {
		expr = g_strdup_printf ("//outline[ @xmlUrl='%s' ]", subscription_get_source (node->subscription));
	} else if (IS_FOLDER (node)) {
		node_foreach_child_data (node, opml_source_check_for_removal, user_data);
		expr = g_strdup_printf ("//outline[ (@title='%s') or (@text='%s') or (@description='%s')]", node->title, node->title, node->title);
	} else {
		g_warning ("opml_source_check_for_removal(): This should never happen...");
		return;
	}
	
	if (!xpath_find ((xmlNodePtr)user_data, expr)) {
		debug1 (DEBUG_UPDATE, "removing %s...", node_get_title (node));
		if (feedlist_get_selected () == node)
			ui_feedlist_select (NULL);
		node_request_remove (node);
	} else {
		debug1 (DEBUG_UPDATE, "keeping %s...", node_get_title (node));
	}
	g_free (expr);
}

static void
opml_source_process_update_result (nodePtr node, const struct updateResult * const result, updateFlags flags)
{
	mergeCtxtPtr	mergeCtxt;
	xmlDocPtr	doc, oldDoc;
	xmlNodePtr	root, title;
	
	debug1 (DEBUG_UPDATE, "OPML download finished data=%d", result->data);

	node->available = FALSE;

	if (result->data) {
		doc = xml_parse (result->data, result->size, FALSE, NULL);
		if (doc) {
			gchar *filename;
			
			root = xmlDocGetRootElement (doc);
			
			/* Go through all existing nodes and remove those whose
			   URLs are not in new feed list. Also removes those URLs
			   from the list that have corresponding existing nodes. */
			node_foreach_child_data (node, opml_source_check_for_removal, (gpointer)root);
						
			opml_source_export (node);	/* save new feed list tree to disk 
			                                   to ensure correct document in 
							   next step */
			
			/* Merge up-to-date OPML feed list. */
			filename = opml_source_get_feedlist (node);
			oldDoc = xmlParseFile (filename);
			g_free (filename);
			
			mergeCtxt = g_new0 (struct mergeCtxt, 1);
			mergeCtxt->rootNode = node;
			mergeCtxt->parent = node;
			mergeCtxt->xmlNode = xmlDocGetRootElement (oldDoc);
			
			if (g_str_equal (node_get_title (node), OPML_SOURCE_DEFAULT_TITLE)) {
				title = xpath_find (root, "/opml/head/title"); 
				if (title) {
					xmlChar *titleStr = common_utf8_fix (xmlNodeListGetString(title->doc, title->xmlChildrenNode, 1));
					if (titleStr) {
						node_set_title (node, titleStr);
						xmlFree (titleStr);
					}
				}
			}
			
			xpath_foreach_match (root, "/opml/body/outline",
			                     opml_source_merge_feed,
			                     (gpointer)mergeCtxt);
			g_free (mergeCtxt);
			xmlFreeDoc (oldDoc);			
			xmlFreeDoc (doc);
			
			opml_source_export (node);	/* save new feed list tree to disk */
			
			node->available = TRUE;
		} else {
			g_warning ("Cannot parse downloaded OPML document!");
		}
	}
	
	node_foreach_child_data (node, node_update_subscription, GUINT_TO_POINTER (0));
}

static void
opml_source_schedule_update (nodePtr node, updateFlags flags)
{
	subscription_update_with_callback (node->subscription, opml_source_process_update_result, flags);
}

void
opml_source_update (nodePtr node)
{
	opml_source_schedule_update (node, 0);  // FIXME: 0 ?
}

void
opml_source_auto_update (nodePtr node)
{
	GTimeVal	now;
	
	g_get_current_time (&now);
	
	/* do daily updates for the feed list and feed updates according to the default interval */
	if (node->subscription->updateState->lastPoll.tv_sec + OPML_SOURCE_UPDATE_INTERVAL <= now.tv_sec)
		opml_source_schedule_update (node, 0);
}

/** called during import and when subscribing, we will do
    node_add_child() only when subscribing */
void
opml_source_setup (nodePtr parent, nodePtr node)
{
	gchar	*filename;
	
	node_set_type(node, node_source_get_node_type());
	
	filename = g_strdup_printf ("%s.png", NODE_SOURCE_TYPE (node)->id);
	node->icon = create_pixbuf (filename);
	g_free (filename);
	
	if(parent) {
		gint pos;
		ui_feedlist_get_target_folder(&pos);
		node_add_child(parent, node, pos);
	}
}

static void opml_source_init(void) { }

static void opml_source_deinit(void) { }

/* node source type definition */

static struct nodeSourceType nst = {
	NODE_SOURCE_TYPE_API_VERSION,
	"fl_opml",
	N_("Planet, BlogRoll, OPML"),
	N_("Integrate blogrolls or Planets in your feed list. Liferea will "
	   "automatically add and remove feeds according to the changes of "
	   "the source OPML document"),
	NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION,
	opml_source_init,
	opml_source_deinit,
	ui_opml_source_get_source_url,
	opml_source_remove,
	opml_source_import,
	opml_source_export,
	opml_source_get_feedlist,
	opml_source_update,
	opml_source_auto_update,
	NULL	/* free */
};

nodeSourceTypePtr
opml_source_get_type (void)
{
	return &nst;
}

/* GUI callbacks */

static void
on_opml_source_selected (GtkDialog *dialog, 
                         gint response_id,
                         gpointer user_data)
{
	nodePtr node, parent = (nodePtr)user_data;

	if (response_id == GTK_RESPONSE_OK) {
		node = node_new ();
		node_set_title (node, OPML_SOURCE_DEFAULT_TITLE);
		node_source_new (node, opml_source_get_type());
		opml_source_setup (parent, node);
		node_set_subscription (node, subscription_new (gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET (dialog), "location_entry"))), NULL, NULL));
		opml_source_update (node);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
on_opml_source_dialog_destroy (GtkDialog *dialog,
                               gpointer user_data) 
{
	g_object_unref (user_data);
}

static void
ui_opml_source_get_source_url (nodePtr parent) 
{
	GtkWidget	*dialog;

	dialog = liferea_dialog_new (PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "opml_source.glade", "opml_source_dialog");

	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (on_opml_source_selected), 
			  (gpointer)parent);
}

static void
on_file_select_clicked (const gchar *filename, gpointer user_data)
{
	GtkWidget	*dialog = GTK_WIDGET (user_data);

	if (filename && dialog)
		gtk_entry_set_text (GTK_ENTRY (liferea_dialog_lookup (dialog, "location_entry")), g_strdup(filename));
}

void
on_select_button_clicked (GtkButton *button, gpointer user_data)
{
	ui_choose_file (_("Choose OPML File"), GTK_STOCK_OPEN, FALSE, on_file_select_clicked, NULL, NULL, user_data);
}
