/**
 * @file feed_parser.c  parsing of different feed formats
 *
 * Copyright (C) 2008-2020 Lars Windolf <lars.windolf@gmx.de>
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

#include "common.h"
#include "debug.h"
#include "html.h"
#include "metadata.h"
#include "xml.h"
#include "parsers/atom10.h"
#include "parsers/html5_feed.h"
#include "parsers/rss_channel.h"

static GSList *feedHandlers = NULL;	/**< list of available parser implementations */

struct feed_type {
	gint id_num;
	gchar *id_str;
};

static GSList *
feed_parsers_get_list (void)
{
	if (feedHandlers)
		return feedHandlers;

	feedHandlers = g_slist_append (feedHandlers, rss_init_feed_handler ());
	feedHandlers = g_slist_append (feedHandlers, atom10_init_feed_handler ());  /* Must be before pie */

	// Do not register HTML5 feed parser here, as it is a HTML and not a feed parser

	return feedHandlers;
}

const gchar *
feed_type_fhp_to_str (feedHandlerPtr fhp)
{
	if (!fhp)
		return NULL;
	return fhp->typeStr;
}

feedHandlerPtr
feed_type_str_to_fhp (const gchar *str)
{
	GSList *iter;
	feedHandlerPtr fhp = NULL;

	if (!str)
		return NULL;

	for (iter = feed_parsers_get_list (); iter != NULL; iter = iter->next) {
		fhp = (feedHandlerPtr)iter->data;
		if (!strcmp(str, fhp->typeStr))
			return fhp;
	}

	return NULL;
}

feedParserCtxtPtr
feed_create_parser_ctxt (void)
{
	feedParserCtxtPtr ctxt;

	ctxt = g_new0 (struct feedParserCtxt, 1);
	ctxt->tmpdata = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
	return ctxt;
}

void
feed_free_parser_ctxt (feedParserCtxtPtr ctxt)
{
	if (ctxt) {
		/* Don't free the itemset! */
		g_hash_table_destroy (ctxt->tmpdata);
		g_free (ctxt->title);
		g_free (ctxt);
	}
}

/**
 * This function tries to find a feed link for a given HTTP URI. It
 * tries to download it. If it finds a valid feed source it parses
 * this source instead into the given feed parsing context. It also
 * replaces the HTTP URI with the found feed source.
 *
 * Mutates ctxt->failed and ctxt->feed->parseErrors
 */
static gboolean
feed_parser_auto_discover (feedParserCtxtPtr ctxt)
{
	gchar	*source = NULL;
	GSList	*links;

	if (ctxt->feed->parseErrors)
		g_string_truncate (ctxt->feed->parseErrors, 0);
	else
		ctxt->feed->parseErrors = g_string_new(NULL);

	debug1 (DEBUG_UPDATE, "Starting feed auto discovery (%s)", subscription_get_source (ctxt->subscription));

	links = html_auto_discover_feed (ctxt->data, subscription_get_source (ctxt->subscription));
	if (links)
		source = links->data;	// FIXME: let user choose feed!

	/* FIXME: we only need the !g_str_equal as a workaround after a 404 */
	if (source && !g_str_equal (source, subscription_get_source (ctxt->subscription))) {
		debug1 (DEBUG_UPDATE, "Discovered link: %s", source);
		ctxt->failed = FALSE;
		subscription_set_source (ctxt->subscription, source);

		/* The feed that was processed wasn't the correct one, we need to redownload it.
		 * Cancel the update in case there's one in progress */
		subscription_cancel_update (ctxt->subscription);
		subscription_update (ctxt->subscription, FEED_REQ_RESET_TITLE);
		g_free (source);
	} else {
		debug0 (DEBUG_UPDATE, "No feed link found!");
		g_string_append (ctxt->feed->parseErrors, _("The URL you want Liferea to subscribe to points to a webpage and the auto discovery found no feeds on this page. Maybe this webpage just does not support feed auto discovery."));
	}
}

static void
feed_parser_ctxt_cleanup (feedParserCtxtPtr ctxt)
{
	/* free old temp. parsing data, don't free right after parsing because
	it can be used until the last feed request is finished, move me
	to the place where the last request in list otherRequests is
	finished :-) */
	g_hash_table_destroy (ctxt->tmpdata);
	ctxt->tmpdata = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);

	/* we always drop old metadata */
	metadata_list_free (ctxt->subscription->metadata);

	ctxt->subscription->metadata = NULL;
	ctxt->failed = FALSE;
}

/**
 * General feed source parsing function. Parses the passed feed source
 * and tries to determine the source type. If all feed handlers fail
 * tries to do HTML5 feed extraction. If this also fails starts feed
 * link auto-discovery.
 *
 * @param ctxt		feed parsing context
 *
 * @returns FALSE if auto discovery is indicated,
 *          TRUE if feed type was recognized and parsing was successful
 */
gboolean
feed_parse (feedParserCtxtPtr ctxt)
{
	xmlNodePtr	cur;
	gboolean	success = FALSE;

	debug_enter ("feed_parse");

	g_assert (NULL == ctxt->items);

	ctxt->failed = TRUE;	/* reset on success ... */

	if (ctxt->feed->parseErrors)
		g_string_truncate (ctxt->feed->parseErrors, 0);
	else
		ctxt->feed->parseErrors = g_string_new (NULL);

	/* 1.) try to parse downloaded data as XML and try to read a feed format */
	do {
		if (NULL == xml_parse_feed (ctxt)) {
			g_string_append_printf (ctxt->feed->parseErrors, _("XML error while reading feed! Feed \"%s\" could not be loaded!"), subscription_get_source (ctxt->subscription));
			break;
		}

		if (NULL == (cur = xmlDocGetRootElement(ctxt->doc))) {
			g_string_append(ctxt->feed->parseErrors, _("Empty document!"));
			break;
		}

		while (cur && xmlIsBlankNode(cur)) {
			cur = cur->next;
		}

		if (!cur)
			break;

		if (!cur->name) {
			g_string_append(ctxt->feed->parseErrors, _("Invalid XML!"));
			break;
		}

		/* determine the syndication format and start parser */
		GSList *handlerIter = feed_parsers_get_list ();
		while(handlerIter) {
			feedHandlerPtr handler = (feedHandlerPtr)(handlerIter->data);
			if(handler && handler->checkFormat && (*(handler->checkFormat))(ctxt->doc, cur)) {
				ctxt->feed->fhp = handler;
				feed_parser_ctxt_cleanup (ctxt);
				(*(handler->feedParser)) (ctxt, cur);
				break;
			}
			handlerIter = handlerIter->next;
		}
	} while(0);


	if (ctxt->doc) {
		xmlFreeDoc(ctxt->doc);
		ctxt->doc = NULL;
	}

	/* 2.) None of the feed formats did work, chance is high that we are
           working on a HTML documents. Let's look for feed links inside it! */
	if (ctxt->failed)
		feed_parser_auto_discover (ctxt);

	/* 3.) alternatively try to parse the HTML document we are on as HTML5 feed */
	if (ctxt->failed) {
		do {
			if (ctxt->doc = xhtml_parse (ctxt->data, ctxt->dataLength))
				g_string_truncate (ctxt->feed->parseErrors, 0);
			else
				break;

			if (NULL == (cur = xmlDocGetRootElement (ctxt->doc))) {
				g_string_append(ctxt->feed->parseErrors, _("Empty document!"));
				break;
			}

			ctxt->feed->fhp = html5_init_feed_handler ();
			if ((*(ctxt->feed->fhp->checkFormat)) (ctxt->doc, cur)) {
					feed_parser_ctxt_cleanup (ctxt);
					(*(ctxt->feed->fhp->feedParser)) (ctxt, cur);
			}
		} while(0);
	}

	if (ctxt->doc) {
		xmlFreeDoc(ctxt->doc);
		ctxt->doc = NULL;
	}

	/* 4.) We give up and inform the user */
	if (ctxt->failed) {
		/* test if we have a HTML page */
		if((strstr (ctxt->data, "<html>") || strstr (ctxt->data, "<HTML>") ||
		    strstr (ctxt->data, "<html ") || strstr (ctxt->data, "<HTML "))) {
			debug0(DEBUG_UPDATE, "HTML document detected!");
			g_string_append (ctxt->feed->parseErrors, _("Source points to HTML document."));
		} else {
			debug0(DEBUG_UPDATE, "neither a known feed type nor a HTML document!");
			g_string_append (ctxt->feed->parseErrors, _("Could not determine the feed type."));
		}
	} else {
		debug1 (DEBUG_UPDATE, "discovered feed format: %s", feed_type_fhp_to_str(ctxt->feed->fhp));
		success = TRUE;
	}

	debug_exit ("feed_parse");

	return success;
}
