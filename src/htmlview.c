/**
 * @file htmlview.c implementation of the item view interface for HTML rendering
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
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include "common.h"
#include "debug.h"
#include "feed.h"
#include "folder.h"
#include "htmlview.h"
#include "item.h"
#include "itemlist.h"
#include "itemset.h"
#include "render.h"
#include "vfolder.h"
#include "ui/ui_htmlview.h"

// FIXME: namespace clash of LifereaHtmlView *htmlview and htmlView_priv 
// clearly shows the need to merge htmlview.c and src/ui/ui_htmlview.c,
// maybe with a separate a HTML cache object...

extern htmlviewPluginPtr htmlviewPlugin;

static struct htmlView_priv 
{
	GHashTable	*chunkHash;	/**< cache of HTML chunks of all displayed items */
	GSList		*orderedChunks;	/**< ordered list of chunks */
	nodePtr		node;		/**< the node whose items are displayed */
	guint		missingContent;	/**< counter for items without content */
} htmlView_priv;

typedef struct htmlChunk 
{
	gulong 		id;	/**< item id */
	gchar		*html;	/**< the rendered HTML (or NULL if not yet rendered) */
	time_t		date;	/**< date as sorting criteria */
} *htmlChunkPtr;

static void
htmlview_chunk_free (htmlChunkPtr chunk) 
{
	g_free (chunk->html);
	g_free (chunk);
}

static gint
htmlview_chunk_sort (gconstpointer a,
                     gconstpointer b) 
{
	return (((htmlChunkPtr)a)->date) - (((htmlChunkPtr)b)->date);
}

void 
htmlview_init (void) 
{
	htmlView_priv.chunkHash = NULL;
	htmlView_priv.orderedChunks = NULL;
	htmlview_clear ();
}

void
htmlview_clear (void) 
{
	if (htmlView_priv.chunkHash)
		g_hash_table_destroy (htmlView_priv.chunkHash);

	GSList	*iter = htmlView_priv.orderedChunks;
	while (iter)
	{
		htmlview_chunk_free (iter->data);
		iter = g_slist_next (iter);
	}

	if (htmlView_priv.orderedChunks)
		g_slist_free (htmlView_priv.orderedChunks);
	
	htmlView_priv.chunkHash = g_hash_table_new (g_direct_hash, g_direct_equal);
	htmlView_priv.orderedChunks = NULL;
	htmlView_priv.missingContent = 0;
}

void
htmlview_set_displayed_node (nodePtr node) 
{
	g_assert (0 == g_hash_table_size (htmlView_priv.chunkHash));
	htmlView_priv.node = node;
}

void
htmlview_add_item (itemPtr item) 
{
	htmlChunkPtr	chunk;
	
	debug1 (DEBUG_HTML, "HTML view: adding \"%s\"", item_get_title (item));

	chunk = g_new0 (struct htmlChunk, 1);
	chunk->id = item->id;
	g_hash_table_insert (htmlView_priv.chunkHash, GUINT_TO_POINTER (item->id), chunk);
	
	htmlView_priv.orderedChunks = g_slist_insert_sorted (htmlView_priv.orderedChunks, chunk, htmlview_chunk_sort);
		
	if (!item_get_description (item) || (0 == strlen (item_get_description (item))))
		htmlView_priv.missingContent++;	
}

void
htmlview_remove_item (itemPtr item) 
{
	htmlChunkPtr	chunk;

	debug1 (DEBUG_HTML, "HTML view: removing \"%s\"", item_get_title (item));
	
	chunk = g_hash_table_lookup (htmlView_priv.chunkHash, GUINT_TO_POINTER (item->id));
	if (chunk) 
	{
		g_hash_table_remove (htmlView_priv.chunkHash, GUINT_TO_POINTER (item->id));
		htmlView_priv.orderedChunks = g_slist_remove (htmlView_priv.orderedChunks, chunk);
		htmlview_chunk_free (chunk);
	}
}

void
htmlview_select_item (itemPtr item) 
{
	debug1(DEBUG_HTML, "HTML view: selecting \"%s\"", item_get_title(item));	
	/* nothing to do... */
}

void
htmlview_update_item (itemPtr item) 
{
	htmlChunkPtr	chunk;
	
	/* ensure rerendering on next update by replace old HTML chunk with NULL */
	chunk = (htmlChunkPtr) g_hash_table_lookup (htmlView_priv.chunkHash, GUINT_TO_POINTER (item->id));
	if (chunk) 
	{
		g_free (chunk->html);
		chunk->html = NULL;
	}
}

void
htmlview_update_all_items (void)
{
	GSList	*iter = htmlView_priv.orderedChunks;
	while (iter) {
		htmlChunkPtr chunk = (htmlChunkPtr)iter->data;
		g_free (chunk->html);
		chunk->html = NULL;
		iter = g_slist_next (iter);
	}
}

// FIXME: bad naming -> unclear concept!
static xmlDocPtr
itemset_to_xml (nodePtr node) 
{
	xmlDocPtr 	doc;
	xmlNodePtr 	itemSetNode;
	
	doc = xmlNewDoc ("1.0");
	itemSetNode = xmlNewDocNode (doc, NULL, "itemset", NULL);
	
	xmlDocSetRootElement (doc, itemSetNode);
	
	xmlNewTextChild (itemSetNode, NULL, "favicon", node_get_favicon_file (node));
	xmlNewTextChild (itemSetNode, NULL, "title", node_get_title (node));

	if (node->subscription && subscription_get_source (node->subscription))
	       xmlNewTextChild (itemSetNode, NULL, "source", subscription_get_source (node->subscription));

	// FIXME: should not be node type specific! Can be fixed by moving the feed link into subscription metadata!
	if (IS_FEED (node))
	       xmlNewTextChild (itemSetNode, NULL, "link", feed_get_html_url (node->data));

	return doc;
}

static gchar *
htmlview_render_item (itemPtr item, 
                      guint viewMode,
                      gboolean summaryMode) 
{
	renderParamPtr	params;
	gchar		*output = NULL, *baseUrl;
	nodePtr		node;
	xmlDocPtr	doc;

	debug_enter ("htmlview_render_item");

	/* don't use node from htmlView_priv as this would be
	   wrong for folders and other merged item sets */
	node = node_from_id (item->nodeId);
	baseUrl = common_uri_escape (node_get_base_url (node));

	/* do the XML serialization */
	doc = itemset_to_xml (node);
			
	item_to_xml(item, xmlDocGetRootElement (doc));
			
	if (IS_FEED (node)) {
		xmlNodePtr feed;
		feed = xmlNewChild(xmlDocGetRootElement(doc), NULL, "feed", NULL);
		feed_to_xml(node, feed);
	}
	
	/* do the XSLT rendering */
	params = render_parameter_new ();
	render_parameter_add (params, "pixmapsDir='file://" PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S "'");
	render_parameter_add (params, "baseUrl='%s'", baseUrl);
	render_parameter_add (params, "summary='%d'", summaryMode?1:0);
	render_parameter_add (params, "single='%d'", (viewMode == ITEMVIEW_SINGLE_ITEM)?0:1);
	output = render_xml (doc, "item", params);
	
	/* For debugging use: xmlSaveFormatFile("/tmp/test.xml", doc, 1); */
	xmlFreeDoc (doc);
	g_free (baseUrl);
	
	debug_exit ("htmlview_render_item");

	return output;
}

void 
htmlview_start_output (GString *buffer,
                       const gchar *base,
		       gboolean css,
		       gboolean script) 
{	
	g_string_append (buffer, "<?xml version=\"1.0\" encoding=\"utf-8\"?><!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\"\n");
	g_string_append (buffer, "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n");
	g_string_append (buffer, "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n");
	g_string_append (buffer, "<head>\n<title></title>");
	g_string_append (buffer, "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />");
	
	if (base) 
	{
		gchar *escBase = g_markup_escape_text (base, -1);
		g_string_append (buffer, "<base href=\"");
		g_string_append (buffer, escBase);
		g_string_append (buffer, "\" />\n");
		g_free (escBase);
	}

	if (css)
		g_string_append (buffer, render_get_css (htmlviewPlugin->externalCss));
	
	/* add predefined scripts to be used for item menu */
	if (script) 
	{
		g_string_append (buffer,  "<script language=\"javascript\" type=\"text/javascript\">"
		"var popupTimeout;"
		"var lastId;"
		""
		"function doShowCb(id) {"
		""
		"	/* hide last id */"
		"	var obj = document.getElementById(lastId);"
		"	if(obj) {"
		"		obj.style.visibility = \"hidden\";"
		"		obj.style.display = \"none\";"
		"	}"
		""
		"	/* show new id */"
		"	obj = document.getElementById(id);"
		"	if(obj) {"
		"		obj.style.visibility = \"visible\";"
		"		obj.style.display = \"block\";"
		"		lastId = id;"
		"	}"
		"}"
		""
		"function doShow(id) {"
		""
		"	window.clearTimeout(popupTimeout);"
		"	popupTimeout = window.setTimeout(\"doShowCb('\"+id+\"')\", 1000);"
		"}"
		""
		"function stopShow() {"
		""
		"	window.clearTimeout(popupTimeout);"
		"}"
		""
		"</script>");
	}
	
	g_string_append (buffer, "</head>\n<body>");
}

void
htmlview_finish_output (GString *buffer) 
{
	g_string_append (buffer, "</body></html>"); 
}

void
htmlview_update (LifereaHtmlView *htmlview, guint mode) 
{
	GSList		*iter;
	GString		*output;
	itemPtr		item = NULL;
	gchar		*baseURL = NULL;
	gboolean	summaryMode;
		
	if (!htmlView_priv.node)
	{
		debug0 (DEBUG_HTML, "clearing HTML view as nothing is selected");
		liferea_htmlview_clear (htmlview);
		return;
	}
	
	/* determine base URL */
	switch (mode) {
		case ITEMVIEW_SINGLE_ITEM:
			item = itemlist_get_selected ();
			if(item)
				baseURL = (gchar *)node_get_base_url (node_from_id(item->nodeId));
			break;
		default:
			baseURL = (gchar *) node_get_base_url (htmlView_priv.node);
			break;
	}

	if (baseURL)
		baseURL = g_markup_escape_text (baseURL, -1);
		
	output = g_string_new (NULL);
	htmlview_start_output (output, baseURL, TRUE, TRUE);

	/* HTML view updating means checking which items
	   need to be updated, render them and then 
	   concatenate everything from cache and output it */
	switch (mode) {
		case ITEMVIEW_SINGLE_ITEM:
			item = itemlist_get_selected ();
			if (item) {
				gchar *html = htmlview_render_item (item, mode, FALSE);
				if (html) {
					g_string_append (output, html);
					g_free (html);
				}
			}
			break;
		case ITEMVIEW_ALL_ITEMS:
			/* Output optimization for feeds without item content. This
			   is not done for folders, because we only support all items
			   in summary mode or all in detailed mode. With folder item 
			   sets displaying everything in summary because of only a
			   single feed without item descriptions would make no sense. */

			summaryMode = !IS_FOLDER (htmlView_priv.node) && 
	        		      !IS_VFOLDER (htmlView_priv.node) && 
	        		      (htmlView_priv.missingContent > 3);

			/* concatenate all items */
			iter = htmlView_priv.orderedChunks;
			while (iter) {
				/* try to retrieve item HTML chunk from cache */
				htmlChunkPtr chunk = (htmlChunkPtr)iter->data;
				
				/* if not found: render new item now and add to cache */
				if (!chunk->html) {
					item = item_load (chunk->id);
					if (item) {
						debug1 (DEBUG_HTML, "rendering item to HTML view: >>>%s<<<", item_get_title (item));
						chunk->html = htmlview_render_item (item, mode, summaryMode);
						item_unload (item);
					}
				}
				
				if (chunk->html)
					g_string_append (output, chunk->html);
					
				iter = g_slist_next (iter);
			}
			break;
		case ITEMVIEW_NODE_INFO:
			{
				gchar *html = node_render (htmlView_priv.node);
				if (html) {
					g_string_append (output, html);
					g_free (html);
				}
			}
			break;
		default:
			g_warning ("HTML view: invalid viewing mode!!!");
			break;
	}
	
	htmlview_finish_output (output);

	debug1 (DEBUG_HTML, "writing %d bytes to HTML view", strlen (output->str));
	liferea_htmlview_write (htmlview, output->str, baseURL);
	
	g_string_free (output, TRUE);
	g_free (baseURL);
}
