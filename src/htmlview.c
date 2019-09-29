/**
 * @file htmlview.c  item view interface for HTML rendering
 *
 * Copyright (C) 2006-2019 Lars Windolf <lars.windolf@gmx.de>
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

#include <string.h>
#include <libxml/uri.h>

#include "common.h"
#include "debug.h"
#include "feed.h"
#include "folder.h"
#include "htmlview.h"
#include "item.h"
#include "itemlist.h"
#include "render.h"
#include "vfolder.h"
#include "ui/liferea_htmlview.h"

// FIXME: namespace clash of LifereaHtmlView *htmlview and htmlView_priv
// clearly shows the need to merge htmlview.c and src/ui/ui_htmlview.c,
// maybe with a separate a HTML cache object...

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

gboolean
htmlview_contains_id (gulong id)
{
	gpointer	chunk;

	chunk = g_hash_table_lookup (htmlView_priv.chunkHash, GUINT_TO_POINTER (id));

	return (chunk?TRUE:FALSE);
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
	debug1(DEBUG_HTML, "HTML view: selecting \"%s\"", item?item_get_title(item):"none");
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

static const gchar *
htmlview_get_item_direction(itemPtr item)
{
	if (item_get_title (item))
		return (common_get_text_direction (item_get_title (item)));

	if (item_get_description (item))
		return (common_get_text_direction (item_get_description (item)));

	/* what can we do? */
	return ("ltr");
}

static gchar *
htmlview_render_item (itemPtr item,
                      guint viewMode)
{
	renderParamPtr	params;
	gchar		*output = NULL, *baseUrl = NULL;
	nodePtr		node;
	xmlDocPtr 	doc;
	xmlNodePtr 	xmlNode;
	const gchar     *text_direction = NULL;
	gboolean	isMergedItemset;

	debug_enter ("htmlview_render_item");

	/* don't use node from htmlView_priv as this would be
	   wrong for folders and other merged item sets */
	node = node_from_id (item->nodeId);

	isMergedItemset = (node != htmlView_priv.node);

	/* do the XML serialization */
	doc = xmlNewDoc (BAD_CAST "1.0");
	xmlNode = xmlNewDocNode (doc, NULL, BAD_CAST "itemset", NULL);
	xmlDocSetRootElement (doc, xmlNode);

	item_to_xml(item, xmlDocGetRootElement (doc));

	text_direction = htmlview_get_item_direction (item);

	if (IS_FEED (node)) {
		xmlNodePtr feed;
		feed = xmlNewChild (xmlDocGetRootElement (doc), NULL, BAD_CAST "feed", NULL);
		feed_to_xml (node, feed);
	}

	/* do the XSLT rendering */
	params = render_parameter_new ();

	if (NULL != node_get_base_url (node)) {
		baseUrl = (gchar *) common_uri_escape ( BAD_CAST node_get_base_url (node));
		render_parameter_add (params, "baseUrl='%s'", baseUrl);
	}

	render_parameter_add (params, "showFeedName='%d'", isMergedItemset?1:0);
	render_parameter_add (params, "single='%d'", (viewMode == ITEMVIEW_SINGLE_ITEM)?1:0);
	render_parameter_add (params, "txtDirection='%s'", text_direction);
	render_parameter_add (params, "appDirection='%s'", common_get_app_direction ());
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
	g_string_append (buffer, "<script type='text/javascript' src='file://" PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "js" G_DIR_SEPARATOR_S "Readability.js'/>");
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
		g_string_append (buffer, render_get_css (TRUE /* external CSS supported */));

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
		"</script>");
	}

	g_string_append (buffer,  "<script language=\"javascript\" type=\"text/javascript\">"
	"if (window.addEventListener) {"
	"	var documentIsReady = function() {"
	"		window.removeEventListener('load', documentIsReady);"
	"		var documentClone = document.cloneNode(true);"
    "       documentClone.body.innerHTML = documentClone.getElementById('shading').innerHTML;"
	"		var article = new Readability(documentClone).parse();"
	"		console.log(article);"
	"		document.getElementById('shading').firstChild.innerHTML=article.content"
	"	};"
	"	window.addEventListener('load', function() { window.setTimeout(documentIsReady, 0); });"
    "}"
	"</script>");

	g_string_append (buffer, "</head>");
}

void
htmlview_finish_output (GString *buffer)
{
	g_string_append (buffer, "</html>");
}

void
htmlview_update (LifereaHtmlView *htmlview, itemViewMode mode)
{
	GSList		*iter;
	GString		*output;
	itemPtr		item = NULL;
	gchar		*baseURL = NULL;

	/* determine base URL */
	switch (mode) {
		case ITEMVIEW_SINGLE_ITEM:
			item = itemlist_get_selected ();
			if(item) {
				baseURL = (gchar *)node_get_base_url (node_from_id (item->nodeId));
				item_unload (item);
			}
			break;
		default:
			if (htmlView_priv.node)
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
				gchar *html = htmlview_render_item (item, mode);
				if (html) {
					g_string_append (output, html);
					g_free (html);
				}

				item_unload (item);
			}
			break;
		case ITEMVIEW_NODE_INFO:
			{
				gchar *html;

				if (htmlView_priv.node) {
					html = node_render (htmlView_priv.node);
					if (html) {
						g_string_append (output, html);
						g_free (html);
					}
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
