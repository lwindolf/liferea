/**
 * @file htmlview.c implementation of the item view interface for HTML rendering
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
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include "common.h"
#include "debug.h"
#include "feed.h"
#include "item.h"
#include "itemlist.h"
#include "itemset.h"
#include "htmlview.h"
#include "render.h"
#include "ui/ui_htmlview.h"

static struct htmlView_priv {
	GHashTable	*htmlChunks;	/**< cache of HTML chunks of all displayed items */
	itemSetPtr	itemSet;	/**< the item set which is displayed */
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

void htmlview_set_itemset(itemSetPtr itemSet) {

	htmlView_priv.itemSet = itemSet;
}

void htmlview_add_item(itemPtr item) {

	g_hash_table_insert(htmlView_priv.htmlChunks, item, NULL);
		
	if(!item_get_description(item) || (0 == strlen(item_get_description(item))))
		htmlView_priv.missingContent++;	
}

void htmlview_remove_item(itemPtr item) {

	g_hash_table_remove(htmlView_priv.htmlChunks, item);
}

void htmlview_update_item(itemPtr item) {

	g_hash_table_insert(htmlView_priv.htmlChunks, item, NULL);
}

gchar * htmlview_render_item(itemPtr item) {
	gchar		**params = NULL, *output = NULL;
	gboolean	summaryMode = FALSE;
	xmlDocPtr	doc;

	debug_enter("htmlview_render_item");
	
	/* Output optimization for feeds without item content. This
	   is not done for folders, because we only support all items
	   in summary mode or all in detailed mode. With folder item 
	   sets displaying everything in summary because of only a
	   single feed without item descriptions would make no sense. */
	   
	summaryMode = (ITEMSET_TYPE_FOLDER != htmlView_priv.itemSet->type) && 
	              (htmlView_priv.missingContent > 3);
	
	/* do the XML serialization */
	doc = itemset_to_xml(htmlView_priv.itemSet);
			
	item_to_xml(item, xmlDocGetRootElement(doc), TRUE);
			
	if(NODE_TYPE_FEED == item->sourceNode->type) {
		xmlNodePtr feed;
		feed = xmlNewChild(xmlDocGetRootElement(doc), NULL, "feed", NULL);
		feed_to_xml(item->sourceNode, feed, TRUE);
	}
		
	/* do the XSLT rendering */
	params = render_add_parameter(params, "pixmapsDir='file://" PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S "'");
	params = render_add_parameter(params, "baseUrl='%s'", itemset_get_base_url(htmlView_priv.itemSet));
	params = render_add_parameter(params, "summary='%d'", summaryMode?1:0);
	output = render_xml(doc, "item", params);
	//xmlSaveFormatFile("/tmp/test.xml", doc, 1);
	xmlFree(doc);
	
	g_hash_table_insert(htmlView_priv.htmlChunks, item, output);
	
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
		g_string_append(buffer, render_get_css());
	
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
	itemPtr		item = NULL;
	gchar		*chunk;
	const gchar	*baseURL = NULL;
	
	if(!htmlView_priv.itemSet) {
		debug0(DEBUG_HTML, "clearing HTML view as nothing is selected");
		ui_htmlview_clear(widget);
		return;
	}

	baseURL = itemset_get_base_url(htmlView_priv.itemSet);	
	output = g_string_new(NULL);
	htmlview_start_output(output, baseURL, TRUE, TRUE);

	/* HTML view updating means checking which items
	   need to be updated, render them and then 
	   concatenate everything from cache and output it */
	switch(mode) {
		case ITEMVIEW_SINGLE_ITEM:
			item = itemlist_get_selected();
			if(item) {
				chunk = htmlview_render_item(item);
				if(chunk)
					g_string_append(output, chunk);
			}
			break;
		case ITEMVIEW_ALL_ITEMS:
			/* concatenate all items */
			iter = htmlView_priv.itemSet->items;
			while(iter) {
				debug1(DEBUG_HTML, "rendering item to HTML view: >>>%s<<<", item_get_title(iter->data));

				/* try to retrieve item HTML chunk from cache */
				gchar *chunk = g_hash_table_lookup(htmlView_priv.htmlChunks, iter->data);
					
				/* if not found: render new item now and add to cache */
				if(!chunk)
					chunk = htmlview_render_item(iter->data);
					
				g_string_append(output, chunk);
				iter = g_list_next(iter);
			}
			break;
		case ITEMVIEW_NODE_INFO:
			chunk = node_render(htmlView_priv.itemSet->node);
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
}
