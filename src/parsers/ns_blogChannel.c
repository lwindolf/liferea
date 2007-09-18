/**
 * @file ns_blogChannel.c blogChannel namespace support
 *
 * Copyright (C) 2003-2006 Lars Lindner <lars.lindner@gmx.net>
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

#include "ns_blogChannel.h"
#include "common.h"
#include "update.h"
#include "feed.h"

#define BLOGROLL_START		"<p><div class=\"blogchanneltitle\"><b>BlogRoll</b></div></p>"
#define BLOGROLL_END		"" 
#define MYSUBSCR_START		"<p><div class=\"blogchanneltitle\"><b>Authors Subscriptions</b></div></p>"
#define MYSUBSCR_END		""
#define BLINK_START		"<p><div class=\"blogchanneltitle\"><b>Promoted Weblog</b></div></p>"
#define BLINK_END		""

#define TAG_BLOGROLL		1
#define TAG_MYSUBSCRIPTIONS	2

struct requestData {
	feedParserCtxtPtr	ctxt;	/**< feed parsing context */
	gint			tag;	/**< metadata id we're downloading (see TAG_*) */
};

/* the spec at Userland http://backend.userland.com/blogChannelModule

   blogChannel contains of four channel tags
   - blogRoll
   - mySubscriptions
   - blink
   - changes	(ignored)
*/

/* returns a HTML string containing the text and attributes of the outline */
static GString * getOutlineContents(xmlNodePtr cur) {
	GString		*buffer;
	gchar		*value;
	
	buffer = g_string_new(NULL);

	if(NULL != (value = common_utf8_fix(xmlGetProp(cur, BAD_CAST"text")))) {
		g_string_append(buffer, value);
		g_free(value);
	}
	
	if(NULL != (value = common_utf8_fix(xmlGetProp(cur, BAD_CAST"url")))) {
		g_string_append_printf(buffer, "&nbsp;<a href=\"%s\">%s</a>", value, value);
		g_free(value);
	}

	if(NULL != (value = common_utf8_fix(xmlGetProp(cur, BAD_CAST"htmlUrl")))) {
		g_string_append_printf(buffer, "&nbsp;(<a href=\"%s\">HTML</a>)", value);
		g_free(value);
	}
			
	if(NULL != (value = common_utf8_fix(xmlGetProp(cur, BAD_CAST"xmlUrl")))) {
		g_string_append_printf(buffer, "&nbsp;(<a href=\"%s\">XML</a>)", value);
		g_free(value);
	}		

	return buffer;
}

/* simple function to retrieve an OPML document and 
   parse and output all depth 1 outline tags as
   HTML into a buffer */
static void
ns_blogChannel_download_request_cb (const struct updateResult * const result, gpointer user_data, guint32 flags)
{
	struct requestData	*requestData = user_data;
	xmlDocPtr 		doc = NULL;
	xmlNodePtr 		cur;
	GString			*buffer = NULL;

	g_assert (NULL != requestData);

	if (result->data) {
		buffer = g_string_new (NULL);
		
		while (1) {
			doc = xmlRecoverMemory (result->data, result->size);

			if (NULL == doc)
				break;

			if (NULL == (cur = xmlDocGetRootElement (doc)))
				break;

			if (!xmlStrcmp (cur->name, BAD_CAST"opml") ||
			    !xmlStrcmp (cur->name, BAD_CAST"oml") ||
			    !xmlStrcmp (cur->name, BAD_CAST"outlineDocument")) {
		   		/* nothing */
			} else
				break;

			cur = cur->xmlChildrenNode;
			while (cur) {
				if (!xmlStrcmp (cur->name, BAD_CAST"body")) {
					/* process all <outline> tags */
					cur = cur->xmlChildrenNode;
					while (cur) {
						if (!xmlStrcmp (cur->name, BAD_CAST"outline")) {
							GString *tmp = getOutlineContents (cur);
							g_string_append_printf (buffer, "%s<br />", tmp->str);
							g_string_free (tmp, TRUE);
						}
						cur = cur->next;
					}
					break;
				}
				cur = cur->next;
			}
			break;		
		}

		if (doc)
			xmlFreeDoc (doc);
	}

	if (buffer) {
		switch (requestData->tag) {
			case TAG_BLOGROLL:
				g_string_prepend (buffer, BLOGROLL_START);
				break;
			case TAG_MYSUBSCRIPTIONS:
				g_string_prepend (buffer, MYSUBSCR_START);
				break;
		}

		switch (requestData->tag) {
			case TAG_BLOGROLL:
				g_string_append (buffer, BLOGROLL_END);
				g_hash_table_insert (requestData->ctxt->tmpdata, g_strdup ("bC:blogRoll"), buffer->str);
				break;
			case TAG_MYSUBSCRIPTIONS:
				g_string_append (buffer, MYSUBSCR_END);
				g_hash_table_insert (requestData->ctxt->tmpdata, g_strdup ("bC:mySubscriptions"), buffer->str);
				break;
		}
		g_string_free (buffer, FALSE);

		buffer = g_string_new (NULL);
		g_string_append (buffer, g_hash_table_lookup (requestData->ctxt->tmpdata, "bC:blink"));
		g_string_append (buffer, g_hash_table_lookup (requestData->ctxt->tmpdata, "bC:blogRoll"));
		g_string_append (buffer, g_hash_table_lookup (requestData->ctxt->tmpdata, "bC:mySubscriptions"));
		metadata_list_set (&(requestData->ctxt->subscription->metadata), "blogChannel", buffer->str);
		g_string_free (buffer, TRUE);
	}
	g_list_free (requestData->ctxt->items);
	feed_free_parser_ctxt (requestData->ctxt);
	g_free (requestData);
}

static void
getOutlineList (feedParserCtxtPtr ctxt, guint tag, char *url)
{
	struct requestData 	*requestData;
	updateRequestPtr	request;

	requestData = g_new0 (struct requestData, 1);
	requestData->ctxt = feed_create_parser_ctxt ();	
	requestData->ctxt->subscription = ctxt->subscription;	// FIXME
	requestData->tag = tag;

	request = update_request_new ();
	request->source = g_strdup (url);
	request->options = update_options_copy (ctxt->subscription->updateOptions);
	
	update_execute_request (ctxt->subscription, request, ns_blogChannel_download_request_cb, requestData, 0);
}

static void parse_channel_tag(feedParserCtxtPtr ctxt, xmlNodePtr cur) {
	xmlChar			*string;
	
	string = xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1);

	if(!xmlStrcmp("blogRoll", cur->name)) {	
		getOutlineList(ctxt, TAG_BLOGROLL, string);
		
	} else if(!xmlStrcmp("mySubscriptions", cur->name)) {
		getOutlineList(ctxt, TAG_MYSUBSCRIPTIONS, string);
		
	} else if(!xmlStrcmp("blink", cur->name)) {
		// nothing to do...
	}

	if(string)
		xmlFree(string);
}

static void ns_blogChannel_register_ns(NsHandler *nsh, GHashTable *prefixhash, GHashTable *urihash) {

	g_hash_table_insert(prefixhash, "blogChannel", nsh);
	g_hash_table_insert(urihash, "http://backend.userland.com/blogChannelModule", nsh);
}

NsHandler *ns_bC_getRSSNsHandler(void) {
	NsHandler 	*nsh;
	
	nsh = g_new0(NsHandler, 1);
	nsh->registerNs 	= ns_blogChannel_register_ns;
	nsh->prefix		= "blogChannel";
	nsh->parseChannelTag	= parse_channel_tag;

	return nsh;
}
