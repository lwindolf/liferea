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
#include "htmlview.h"
#include "item.h"
#include "itemlist.h"
#include "itemset.h"
#include "render.h"
#include "ui/ui_htmlview.h"

extern htmlviewPluginPtr htmlviewPlugin;

static struct htmlView_priv {
	GHashTable	*htmlChunks;	/**< cache of HTML chunks of all displayed items */
	nodePtr		node;		/**< the node whose items are displayed */
	guint		missingContent;	/**< counter for items without content */
} htmlView_priv;

void htmlview_init(void) {

	htmlView_priv.htmlChunks = NULL;
	htmlview_clear();
}

void htmlview_clear(void) {

	if(htmlView_priv.htmlChunks)
		g_hash_table_destroy(htmlView_priv.htmlChunks);

	htmlView_priv.htmlChunks = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
	htmlView_priv.missingContent = 0;
}

void htmlview_set_displayed_node(nodePtr node) {

	g_assert(0 == g_hash_table_size(htmlView_priv.htmlChunks));
	htmlView_priv.node = node;
}

void htmlview_add_item(itemPtr item) {

	debug1(DEBUG_HTML, "HTML view: adding \"%s\"", item_get_title(item));
	g_hash_table_insert(htmlView_priv.htmlChunks, item, NULL);
		
	if(!item_get_description(item) || (0 == strlen(item_get_description(item))))
		htmlView_priv.missingContent++;	
}

void htmlview_remove_item(itemPtr item) {

	debug1(DEBUG_HTML, "HTML view: removing \"%s\"", item_get_title(item));
	g_hash_table_remove(htmlView_priv.htmlChunks, item);
}

void htmlview_select_item(itemPtr item) {

	debug1(DEBUG_HTML, "HTML view: selecting \"%s\"", item_get_title(item));	
	/* nothing to do... */
}

void htmlview_update_item(itemPtr item) {

	/* ensure rerendering on next update by replace old HTML chunk with NULL */
	g_hash_table_insert(htmlView_priv.htmlChunks, item, NULL);
}

static xmlDocPtr itemset_to_xml(nodePtr node) {
	xmlDocPtr 	doc;
	xmlNodePtr 	itemSetNode;
	
	doc = xmlNewDoc("1.0");
	itemSetNode = xmlNewDocNode(doc, NULL, "itemset", NULL);
	
	xmlDocSetRootElement(doc, itemSetNode);
	
	xmlNewTextChild(itemSetNode, NULL, "favicon", node_get_favicon_file(node));
	xmlNewTextChild(itemSetNode, NULL, "title", node_get_title(node));

	if(NODE_TYPE_FEED == node->type) {
	       xmlNewTextChild(itemSetNode, NULL, "source", subscription_get_source(node->subscription));
	       xmlNewTextChild(itemSetNode, NULL, "link", feed_get_html_url(node->data));
	}

	return doc;
}

static gchar * htmlview_render_item(itemPtr item, gboolean summaryMode) {
	renderParamPtr	params;
	gchar		*output = NULL, *baseUrl;
	nodePtr		node;
	xmlDocPtr	doc;

	debug_enter("htmlview_render_item");

	baseUrl = common_uri_escape(node_get_base_url(htmlView_priv.node));

	/* do the XML serialization */
	doc = itemset_to_xml(htmlView_priv.node);
			
	item_to_xml(item, xmlDocGetRootElement(doc));
			
	node = node_from_id(item->nodeId);
	if(NODE_TYPE_FEED == node->type) {
		xmlNodePtr feed;
		feed = xmlNewChild(xmlDocGetRootElement(doc), NULL, "feed", NULL);
		feed_to_xml(node, feed);
	}
	
	/* do the XSLT rendering */
	params = render_parameter_new();
	render_parameter_add(params, "pixmapsDir='file://" PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S "'");
	render_parameter_add(params, "baseUrl='%s'", baseUrl);
	render_parameter_add(params, "summary='%d'", summaryMode?1:0);
	output = render_xml(doc, "item", params);
	
	/* For debugging use: xmlSaveFormatFile("/tmp/test.xml", doc, 1); */
	xmlFreeDoc(doc);
	g_free(baseUrl);
	
	debug_exit("htmlview_render_item");

	return output;
}

void htmlview_start_output(GString *buffer, const gchar *base, gboolean css, gboolean script) { 
	
	g_string_append(buffer, "<?xml version=\"1.0\" encoding=\"utf-8\"?><!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\"\n");
	g_string_append(buffer, "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n");
	g_string_append(buffer, "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n");
	g_string_append(buffer, "<head>\n<title></title>");
	g_string_append(buffer, "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />");
	
	if(base) {
		gchar *escBase = g_markup_escape_text(base, -1);
		g_string_append(buffer, "<base href=\"");
		g_string_append(buffer, escBase);
		g_string_append(buffer, "\" />\n");
		g_free(escBase);
	}

	if(css)
		g_string_append(buffer, render_get_css(htmlviewPlugin->externalCss));
	
	/* add predefined scripts to be used for item menu */
	if(script) {
		g_string_append(buffer,  "<script language=\"javascript\" type=\"text/javascript\">"
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
	
	g_string_append(buffer, "</head>\n<body>");
}

void htmlview_finish_output(GString *buffer) {

	g_string_append(buffer, "</body></html>"); 
}

void htmlview_update(GtkWidget *widget, guint mode) {
	GList		*iter = NULL;
	GString		*output;
	itemSetPtr	itemSet;
	itemPtr		item = NULL;
	gchar		*chunk,	*baseURL;
	gboolean	summaryMode;
		
	if(!htmlView_priv.node) {
		debug0(DEBUG_HTML, "clearing HTML view as nothing is selected");
		ui_htmlview_clear(widget);
		return;
	}
	
	baseURL = (gchar *)node_get_base_url(htmlView_priv.node);
	if(baseURL)
		baseURL = g_markup_escape_text(baseURL, -1);
		
	output = g_string_new(NULL);
	htmlview_start_output(output, baseURL, TRUE, TRUE);

	/* HTML view updating means checking which items
	   need to be updated, render them and then 
	   concatenate everything from cache and output it */
	switch(mode) {
		case ITEMVIEW_SINGLE_ITEM:
			item = itemlist_get_selected();
			if(item) {
				chunk = htmlview_render_item(item, FALSE);
				if(chunk)
					g_string_append(output, chunk);
				g_free(chunk);
			}
			break;
		case ITEMVIEW_ALL_ITEMS:
			/* Output optimization for feeds without item content. This
			   is not done for folders, because we only support all items
			   in summary mode or all in detailed mode. With folder item 
			   sets displaying everything in summary because of only a
			   single feed without item descriptions would make no sense. */

			summaryMode = (NODE_TYPE_FOLDER != htmlView_priv.node->type) && 
	        		      (NODE_TYPE_VFOLDER != htmlView_priv.node->type) && 
	        		      (htmlView_priv.missingContent > 3);

			/* concatenate all items */
			itemSet = node_get_itemset(htmlView_priv.node);
			iter = itemSet->ids;
			while(iter) {
				/* try to retrieve item HTML chunk from cache */
				chunk = g_hash_table_lookup(htmlView_priv.htmlChunks, iter->data);
					
				/* if not found: render new item now and add to cache */
				if(!chunk) {
					item = item_load(GPOINTER_TO_UINT(iter->data));
					debug1(DEBUG_HTML, "rendering item to HTML view: >>>%s<<<", item_get_title(item));
					chunk = htmlview_render_item(item, summaryMode);
					item_unload(item);
					g_hash_table_insert(htmlView_priv.htmlChunks, iter->data, chunk);					
				}
					
				g_string_append(output, chunk);
				iter = g_list_next(iter);
			}
			itemset_free(itemSet);
			break;
		case ITEMVIEW_NODE_INFO:
			chunk = node_render(htmlView_priv.node);
			g_string_append(output, chunk);
			g_free(chunk);
			break;
		default:
			g_warning("HTML view: invalid viewing mode!!!");
			break;
	}
	
	htmlview_finish_output(output);

	debug1(DEBUG_HTML, "writing %d bytes to HTML view", strlen(output->str));
	ui_htmlview_write(widget, output->str, baseURL);
	
	g_string_free(output, TRUE);
	g_free(baseURL);
}
