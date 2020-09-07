/**
 * @file htmlview.c  item view interface for HTML rendering
 *
 * Copyright (C) 2006-2020 Lars Windolf <lars.windolf@gmx.de>
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
#include "feedlist.h"
#include "folder.h"
#include "htmlview.h"
#include "item.h"
#include "itemlist.h"
#include "render.h"
#include "ui/liferea_htmlview.h"

// FIXME: namespace clash of LifereaHtmlView *htmlview and htmlView_priv
// clearly shows the need to merge htmlview.c and src/ui/ui_htmlview.c,
// maybe with a separate a HTML cache object...

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
	nodePtr		node, selected;
	xmlDocPtr 	doc;
	xmlNodePtr 	xmlNode;
	const gchar     *text_direction = NULL;

	debug_enter ("htmlview_render_item");

	/* don't use node from htmlView_priv as this would be
	   wrong for folders and other merged item sets */
	node = node_from_id (item->nodeId);

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

	render_parameter_add (params, "showFeedName='%d'", (node != feedlist_get_selected ())?1:0);
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
		       gboolean css)
{
	/* Prepare HTML boilderplate */
	g_string_append (buffer, "<!DOCTYPE html>\n");
	g_string_append (buffer, "<html>\n");
	g_string_append (buffer, "<head>\n<title>HTML View</title>");
	g_string_append (buffer, "<script src='file://" PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "js" G_DIR_SEPARATOR_S "Readability.js'></script>");
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

	g_string_append (buffer,  "<script language=\"javascript\" type=\"text/javascript\">"
	"\nif (window.addEventListener) {"
	"\n	var documentIsReady = function() {"
        "\n       	try {"
	"\n			window.removeEventListener('load', documentIsReady);"
	"\n"
	"\n			// Add all content in shadow DOM and split decoration from content"
	"\n			// only pass the content to Readability.js"
	"\n			var documentClone = document.cloneNode(true);"
	"\n			documentClone.body.innerHTML = decodeURIComponent(content);"
	"\n			documentClone.getElementById('content').innerHTML = '';"
	"\n			document.body.innerHTML = documentClone.body.innerHTML;"
	"\n			documentClone.body.innerHTML = decodeURIComponent(content);"
	"\n			documentClone.body.innerHTML = documentClone.getElementById('content').innerHTML;"
	"\n"
	"\n			// Drop Readability.js created <header>"
	"\n			var header = documentClone.getElementsByTagName('header');"
	"\n			if(header.length > 0)"
	"\n				header[0].parentNode.removeChild(header[0]);"
	"\n"
	"\n			// Show the results"
	"\n			var article = new Readability(documentClone).parse();"
	"\n			document.getElementById('content').innerHTML=article.content"
	"\n		} catch(e) {"
	"\n			console.log('Reader mode failed ('+e+')');"
	"\n			document.body.innerHTML = decodeURIComponent(content);"
	"\n		}"
	"\n	};"
	"\n	window.addEventListener('load', function() { window.setTimeout(documentIsReady, 0); });"
	"\n}"
	"\n</script>");
}

void
htmlview_finish_output (GString *buffer, gchar *content)
{
	if (content) {
		/* URI escape our content for safe transfer to Readability.js
		   URI escaping is needed for UTF-8 conservation and for JS stringification */
		gchar *uri_escaped = g_uri_escape_string (content, NULL, TRUE);
		g_string_append_printf (buffer, "<script type='text/javascript'>\nvar content = '%s';</script>", uri_escaped);
		g_free (uri_escaped);
	}

	g_string_append (buffer, "</head><body></body></html>");
}

void
htmlview_update (LifereaHtmlView *htmlview, itemViewMode mode)
{
	GSList		*iter;
	GString		*output;
	nodePtr		node = feedlist_get_selected ();
	itemPtr		item = NULL;
	gchar		*baseURL = NULL;
	gchar		*content = NULL;

	/* determine base URL */
	switch (mode) {
		case ITEMVIEW_SINGLE_ITEM:
			item = itemlist_get_selected ();
			if(item) {
				baseURL = (gchar *)node_get_base_url (node_from_id (item->nodeId));
				item_unload (item);
			}
			break;
		case ITEMVIEW_NODE_INFO:
			if (!node)
				return;

			baseURL = (gchar *) node_get_base_url (node);
			break;
	}

	if (baseURL)
		baseURL = g_markup_escape_text (baseURL, -1);

	output = g_string_new (NULL);
	htmlview_start_output (output, baseURL, TRUE);

	/* HTML view updating means checking which items
	   need to be updated, render them and then
	   concatenate everything from cache and output it */
	switch (mode) {
		case ITEMVIEW_SINGLE_ITEM:
			item = itemlist_get_selected ();
			if (item) {
				content = htmlview_render_item (item, mode);

				item_unload (item);
			}
			break;
		case ITEMVIEW_NODE_INFO:
			if (node)
				content = node_render (node);
			break;
		default:
			g_warning ("HTML view: invalid viewing mode!!!");
			break;
	}

	htmlview_finish_output (output, content);
	g_free (content);

	debug1 (DEBUG_HTML, "writing %d bytes to HTML view", strlen (output->str));
	liferea_htmlview_write (htmlview, output->str, baseURL);

	g_string_free (output, TRUE);
	g_free (baseURL);
}
