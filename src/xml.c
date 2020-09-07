/**
 * @file xml.c XML helper methods for Liferea
 * 
 * Copyright (C) 2003-2016  Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2004-2006  Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include "xml.h"

#include <string.h>
#include <errno.h>
#include <glib/gstdio.h>

#include <libxml/xmlerror.h>
#include <libxml/uri.h>
#include <libxml/parser.h>
#include <libxml/entities.h>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>

#include "common.h"
#include "debug.h"

static void xml_buffer_parse_error(void *ctxt, const gchar * msg, ...);

xmlDocPtr
xhtml_parse (const gchar *html, gint len)
{
	xmlDocPtr out = NULL;
	
	g_assert (html != NULL);
	g_assert (len >= 0);
	
	/* Note: NONET is not implemented so it will return an error
	   because it doesn't know how to handle NONET. But, it might
	   learn in the future. */
	out = htmlReadMemory (html, len, NULL, "utf-8", HTML_PARSE_RECOVER | HTML_PARSE_NONET |
	                      ((debug_level & DEBUG_HTML)?0:(HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING)));
	return out;
}

static xmlNodePtr
xhtml_find_body (xmlDocPtr doc)
{
	xmlXPathContextPtr xpathCtxt = NULL;
	xmlXPathObjectPtr xpathObj = NULL;
	xmlNodePtr node = NULL;

	xpathCtxt = xmlXPathNewContext (doc);
	if (!xpathCtxt)
		goto error;

	xpathObj = xmlXPathEvalExpression ("/html/body", xpathCtxt);
	if (!xpathObj)
		goto error;
	if (!xpathObj->nodesetval->nodeMax)
		goto error;
	
	node = xpathObj->nodesetval->nodeTab[0];
 error:
	if (xpathObj)
		xmlXPathFreeObject (xpathObj);
	if (xpathCtxt)
		xmlXPathFreeContext (xpathCtxt);
	return node;
}

gchar *
xhtml_extract (xmlNodePtr xml, gint xhtmlMode, const gchar *defaultBase)
{
	xmlBufferPtr	buf;
	xmlChar         *xml_base = NULL;
	gchar		*result = NULL;
	xmlNs		*ns;
	
	/* Create the new document and add the div tag*/
	xmlDocPtr newDoc = xmlNewDoc (BAD_CAST "1.0" );
	xmlNodePtr divNode = xmlNewNode (NULL, BAD_CAST "div");
	xmlDocSetRootElement (newDoc, divNode);
	xmlNewNs (divNode, BAD_CAST "http://www.w3.org/1999/xhtml", NULL);

	/* Set the xml:base  of the div tag */
	xml_base = xmlNodeGetBase (xml->doc, xml);
	if (xml_base) {
		xmlNodeSetBase (divNode, xml_base );
		xmlFree (xml_base);
	}
	else if (defaultBase)
		xmlNodeSetBase (divNode, defaultBase);
	
	if (xhtmlMode == 0) { /* Read escaped HTML and convert to XHTML, placing in a div tag */
		xmlDocPtr oldDoc;
		xmlNodePtr copiedNodes = NULL;
		xmlChar *escapedhtml;
		
		/* Parse the HTML into oldDoc*/
		escapedhtml = xmlNodeListGetString (xml->doc, xml->xmlChildrenNode, 1);
		if (escapedhtml) {
			escapedhtml = g_strstrip (escapedhtml);	/* stripping whitespaces to make empty string detection easier */
			if (*escapedhtml) {			/* never process empty content, xmlDocCopy() doesn't like it... */
				xmlNodePtr body;
				oldDoc = xhtml_parse (escapedhtml, strlen (escapedhtml));
				body = xhtml_find_body (oldDoc);
	
				/* Copy namespace from original documents root node. This is
				   to determine additional namespaces for item content. For
				   example to handle RSS 2.0 feeds as provided by LiveJournal:

				   <rss version='2.0' xmlns:lj='http://www.livejournal.org/rss/lj/1.0/'>
				   <channel>
				      ...
				      <item>
	        			 ...
  	        			 <description>... &lt;span class=&apos;ljuser&apos; lj:user=&apos;someone&apos; style=&apos;white-space: nowrap;&apos;&gt;&lt;a href=&apos;http://community.livejournal.com/someone/profile&apos;&gt;&lt;img src=&apos;http://stat.livejournal.com/img/community.gif&apos; alt=&apos;[info]&apos; width=&apos;16&apos; height=&apos;16&apos; style=&apos;vertical-align: bottom; border: 0; padding-right: 2px;&apos; /&gt;&lt;/a&gt;&lt;a href=&apos;http://community.livejournal.com/someone/&apos;&gt;&lt;b&gt;someone&lt;/b&gt;&lt;/a&gt;&lt;/span&gt; ...</description>
					 ...
				      </item> 
				      ...
				   </channel>

				   Then we will want to extract <description> and need to
				   honour the xmlns:lj definition...
				*/
				ns = (xmlDocGetRootElement (xml->doc))->nsDef;
				while (ns) {
					xmlNewNs (divNode, ns->href, ns->prefix);
					ns = ns->next;
				}
				
				if (body) {
					/* Copy in the html tags */
					copiedNodes = xmlDocCopyNodeList (newDoc, body->xmlChildrenNode);
					// FIXME: is the above correct? Why only operate on the first child node?
					// It might be unproblematic because all content is wrapped in a <div>...
					xmlAddChildList (divNode, copiedNodes);
				}
				xmlFreeDoc (oldDoc);
				xmlFree (escapedhtml);
			}
		}
	} else if (xhtmlMode == 1 || xhtmlMode == 2) { /* Read multiple XHTML tags and embed in div tag */
		xmlNodePtr copiedNodes = xmlDocCopyNodeList (newDoc, xml->xmlChildrenNode);
		xmlAddChildList (divNode, copiedNodes);
	}
	
	buf = xmlBufferCreate ();
	xmlNodeDump (buf, newDoc, xmlDocGetRootElement (newDoc), 0, 0 );
	
	if (xmlBufferLength(buf) > 0)
		result = xmlCharStrdup (xmlBufferContent (buf));

	xmlBufferFree (buf);
	xmlFreeDoc (newDoc);
	return result;
}

/*
 * Read HTML string and convert to XHTML, placing in a div tag 
 */
gchar *
xhtml_extract_from_string (const gchar *html, const gchar *defaultBase)
{
	xmlDocPtr doc = NULL;
	xmlNodePtr body = NULL;
	gchar *result;
	
	/* never process empty content, xmlDocCopy() doesn't like it... */
	if (html != NULL && !common_str_is_empty (html)) {
		doc = xhtml_parse (html, strlen (html));
		body = xhtml_find_body (doc);
		if (body == NULL) 
			body = xmlDocGetRootElement (doc);
	}
	
	if (body != NULL)
		result = xhtml_extract (body, 1, defaultBase);
	else
		result = xmlCharStrdup ("<div></div>");

	xmlFreeDoc (doc);
	return result;
}

gboolean
xhtml_is_well_formed (const gchar *data)
{
	gchar		*xml;
	gboolean	result;
	errorCtxtPtr	errors;
	xmlDocPtr	doc;

	if (!data)
		return FALSE;
	
	errors = g_new0 (struct errorCtxt, 1);
	errors->msg = g_string_new (NULL);

	xml = g_strdup_printf ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n<test>%s</test>", data);
	
	doc = xml_parse (xml, strlen (xml), errors);
	if (doc)
		xmlFreeDoc (doc);
		
	g_free (xml);
	g_string_free (errors->msg, TRUE);
	result = (0 == errors->errorCount);
	g_free (errors);
	
	return result;
}

static GSList *dhtml_strippers = NULL;
static GSList *unsupported_tag_strippers = NULL;

static void
xhtml_stripper_add (GSList **strippers, const gchar *pattern)
{
	GError *err = NULL;
	GRegex *expr;
	
	expr = g_regex_new (pattern, G_REGEX_CASELESS | G_REGEX_UNGREEDY | G_REGEX_DOTALL | G_REGEX_OPTIMIZE, 0, &err);
	if (err) {
		g_warning ("xhtml_strip_setup: %s\n", err->message);
		g_error_free (err);
		return;
	}
	*strippers = g_slist_append (*strippers, expr);
}

static gchar *
xhtml_strip (const gchar *html, GSList *strippers)
{
	gchar *result = g_strdup (html);
	GSList *iter = strippers;
	
	while (iter) {
		GError *err = NULL;
		GRegex *expr = (GRegex *)iter->data;
		gchar *tmp = result;
		result = g_regex_replace (expr, tmp, -1, 0, "", 0, &err);
		if (err) {
			g_warning ("xhtml_strip: %s\n", err->message);
			g_error_free (err);
			err = NULL;
		}
		g_free (tmp);
		iter = g_slist_next (iter);
	}

	return result;
}

gchar *
xhtml_strip_dhtml (const gchar *html)
{
	if (!dhtml_strippers) {
		xhtml_stripper_add (&dhtml_strippers, "\\s+onload='[^']+'");
		xhtml_stripper_add (&dhtml_strippers, "\\s+onload=\"[^\"]+\"");
		xhtml_stripper_add (&dhtml_strippers, "<\\s*script\\s*>.*</\\s*script\\s*>");
		xhtml_stripper_add (&dhtml_strippers, "<\\s*meta\\s*>.*</\\s*meta\\s*>");
		xhtml_stripper_add (&dhtml_strippers, "<\\s*iframe[^>]*\\s*>.*</\\s*iframe\\s*>");
	}
	
	return xhtml_strip (html, dhtml_strippers);
}

gchar *
xhtml_strip_unsupported_tags (const gchar *html)
{
	if (!unsupported_tag_strippers) {
		xhtml_stripper_add(&unsupported_tag_strippers, "<\\s*/?wbr[^>]*/?\\s*>");
		xhtml_stripper_add(&unsupported_tag_strippers, "<\\s*/?body[^>]*/?\\s*>");
	}
	
	return xhtml_strip(html, unsupported_tag_strippers);
}

typedef struct {
	gchar	*data;
	gint	length;
} result_buffer;

static void
unhtmlizeHandleCharacters (void *user_data, const xmlChar *string, int length)
{
	result_buffer	*buffer = (result_buffer *)user_data;
	gint 		old_length;

	old_length = buffer->length;	
	buffer->length += length;
	buffer->data = g_renew (gchar, buffer->data, buffer->length + 1);
        strncpy (buffer->data + old_length, (gchar *)string, length);
	buffer->data[buffer->length] = 0;

}

static void
_unhtmlize (gchar *string, result_buffer *buffer)
{
	htmlParserCtxtPtr	ctxt;
	htmlSAXHandlerPtr	sax_p;
	
	sax_p = g_new0 (htmlSAXHandler, 1);
 	sax_p->characters = unhtmlizeHandleCharacters;
	ctxt = htmlCreatePushParserCtxt (sax_p, buffer, string, strlen (string), "", XML_CHAR_ENCODING_UTF8);
	htmlParseChunk (ctxt, string, 0, 1);
	htmlFreeParserCtxt (ctxt);
 	g_free (sax_p);
}

static void
_unxmlize (gchar *string, result_buffer *buffer)
{
	xmlParserCtxtPtr	ctxt;
	xmlSAXHandler		*sax_p;
	
	sax_p = g_new0 (xmlSAXHandler, 1);
 	sax_p->characters = unhtmlizeHandleCharacters;
	ctxt = xmlCreatePushParserCtxt (sax_p, buffer, string, strlen (string), "");
	xmlParseChunk (ctxt, string, 0, 1);
	xmlFreeParserCtxt (ctxt);
 	g_free(sax_p);
}

/* Converts a UTF-8 strings containing any XML stuff to 
   a string without any entities or tags containing all
   text nodes of the given HTML string. The original 
   string will be freed. */
static gchar *
unmarkupize (gchar *string, void(*parse)(gchar *string, result_buffer *buffer))
{
	gchar			*result;
	result_buffer		*buffer;
	
	if (!string)
		return NULL;
		
	/* only do something if there are any entities or tags */
	if(NULL == (strpbrk (string, "&<>")))
		return string;

	buffer = g_new0 (result_buffer, 1);
	parse (string, buffer);
	result = buffer->data;
	g_free (buffer);
 
 	if (result == NULL || !g_utf8_strlen (result, -1)) {
 		/* Something went wrong in the parsing.
 		 * Use original string instead */
		g_free (result);
 		return string;
 	} else {
 		g_free (string);
 		return result;
 	}
}

gchar * unhtmlize (gchar * string) { return unmarkupize (string, _unhtmlize); }
gchar * unxmlize (gchar * string) { return unmarkupize (string, _unxmlize); }

#define MAX_PARSE_ERROR_LINES	10

/**
 * Error buffering function to be registered by 
 * xmlSetGenericErrorFunc(). This function is called on
 * each libxml2 error output and collects all output as
 * HTML in the buffer ctxt points to. 
 *
 * @param	ctxt	error context
 * @param	msg	printf like format string 
 */
static void
xml_buffer_parse_error (void *ctxt, const gchar * msg, ...)
{
	va_list		params;
	errorCtxtPtr	errors = (errorCtxtPtr)ctxt;
	gchar		*newmsg;
	gchar		*tmp;
	
	if (MAX_PARSE_ERROR_LINES > errors->errorCount++) {
		va_start (params, msg);
		newmsg = g_strdup_vprintf (msg, params);
		va_end (params);
	
		/* Do never encode any invalid characters from error messages */
		if (g_utf8_validate (newmsg, -1, NULL)) {
			tmp = g_markup_escape_text (newmsg, -1);
			g_free (newmsg);
			newmsg = tmp;
	
			g_string_append_printf(errors->msg, "<pre>%s</pre>\n", newmsg);
		}
		g_free(newmsg);
	}
	
	if (MAX_PARSE_ERROR_LINES == errors->errorCount)
		g_string_append_printf (errors->msg, "<br />%s", _("[There were more errors. Output was truncated!]"));
}

static xmlDocPtr entities = NULL;

static xmlEntityPtr
xml_process_entities (void *ctxt, const xmlChar *name)
{
	xmlEntityPtr	entity, found;
	xmlChar		*tmp;
	
	entity = xmlGetPredefinedEntity (name);
	if (!entity) {
		if(!entities) {
			/* loading HTML entities from external DTD file */
			entities = xmlNewDoc (BAD_CAST "1.0");
			xmlCreateIntSubset (entities, BAD_CAST "HTML entities", NULL, PACKAGE_DATA_DIR "/" PACKAGE "/dtd/html.ent");
			entities->extSubset = xmlParseDTD (entities->intSubset->ExternalID, entities->intSubset->SystemID);
		}
		
		if (NULL != (found = xmlGetDocEntity (entities, name))) {
			/* returning as faked predefined entity... */
			tmp = xmlStrdup (found->content);
			tmp = unhtmlize (tmp);	/* arghh ... slow... */
			entity = g_new0 (xmlEntity, 1);
			entity->type = XML_ENTITY_DECL;
			entity->name = name;
			entity->orig = NULL;
			entity->content = tmp;
			entity->length = g_utf8_strlen (tmp, -1);
			entity->etype = XML_INTERNAL_PREDEFINED_ENTITY;
		}
	}
	if (!entity) {
		g_print("unsupported entity: %s\n", name);
	}
	return entity;
}

xmlNodePtr 
xpath_find (xmlNodePtr node, const gchar *expr)
{
	xmlNodePtr	result = NULL;
	
	if (node && node->doc) {
		xmlXPathContextPtr xpathCtxt = NULL;
		xmlXPathObjectPtr xpathObj = NULL;
		
		if (NULL != (xpathCtxt = xmlXPathNewContext (node->doc))) {
			xpathCtxt->node = node;
			xpathObj = xmlXPathEval (expr, xpathCtxt);
		}
		
		if (xpathObj && !xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
			result = xpathObj->nodesetval->nodeTab[0];
		
		if (xpathObj)
			xmlXPathFreeObject(xpathObj);
		if (xpathCtxt)
			xmlXPathFreeContext(xpathCtxt);
	}
	return result;
}

gboolean
xpath_foreach_match (xmlNodePtr node, const gchar *expr, xpathMatchFunc func, gpointer user_data)
{
	
	if (node && node->doc) {
		xmlXPathContextPtr xpathCtxt = NULL;
		xmlXPathObjectPtr xpathObj = NULL;
		
		if (NULL != (xpathCtxt = xmlXPathNewContext (node->doc))) {
			xpathCtxt->node = node;
			xpathObj = xmlXPathEval (expr, xpathCtxt);
		}
			
		if (xpathObj && xpathObj->nodesetval && xpathObj->nodesetval->nodeMax) {
			int	i;
			for (i = 0; i < xpathObj->nodesetval->nodeNr; i++)
				(*func) (xpathObj->nodesetval->nodeTab[i], user_data);
		}
		
		if (xpathObj)
			xmlXPathFreeObject (xpathObj);
		if (xpathCtxt)
			xmlXPathFreeContext (xpathCtxt);
		return TRUE;
	}
	return FALSE;
}

gchar *
xml_get_attribute (xmlNodePtr node, const gchar *name)
{
	return xmlGetProp (node, BAD_CAST name);
}

gchar *
xml_get_ns_attribute (xmlNodePtr node, const gchar *name, const gchar *namespace)
{
	return xmlGetNsProp (node, BAD_CAST name, BAD_CAST namespace);
}

static void
liferea_xml_errorSAXFunc (void * ctx, const char * msg,...)
{
	va_list valist;
	gchar *parser_error = NULL;

	va_start(valist,msg);
	parser_error = g_strdup_vprintf (msg, valist);
	va_end(valist);
	debug1 (DEBUG_PARSING, "SAX parser error : %s", parser_error);
	g_free (parser_error);
}


xmlDocPtr
xml_parse (gchar *data, size_t length, errorCtxtPtr errCtx)
{
	xmlParserCtxtPtr	ctxt;
	xmlDocPtr		doc;
	
	g_assert (NULL != data);

	ctxt = xmlNewParserCtxt ();
	ctxt->sax->getEntity = xml_process_entities;
	ctxt->sax->error = liferea_xml_errorSAXFunc;
	
	if (errCtx)
		xmlSetGenericErrorFunc (errCtx, (xmlGenericErrorFunc)xml_buffer_parse_error);
	
	doc = xmlSAXParseMemory (ctxt->sax, data, length, 0);
	
	/* This seems to reset the errorfunc to its default, so that the
	   GtkHTML2 module is not unhappy because it also tries to call the
	   errorfunc on occasion. */
	xmlSetGenericErrorFunc (NULL, NULL);
	
	xmlFreeParserCtxt (ctxt);
	
	return doc;
}

xmlDocPtr
xml_parse_feed (feedParserCtxtPtr fpc)
{
	errorCtxtPtr	errors;
		
	g_assert (NULL != fpc->data);
	g_assert (NULL != fpc->feed);
	
	fpc->feed->valid = FALSE;
	
	/* we don't like no data */
	if (0 == fpc->dataLength) {
		debug1 (DEBUG_PARSING, "xml_parse_feed(): empty input while parsing \"%s\"!", fpc->subscription->node->title);
		g_string_append (fpc->feed->parseErrors, "Empty input!\n");
		return NULL;
	}

	errors = g_new0 (struct errorCtxt, 1);
	errors->msg = fpc->feed->parseErrors;
	
	fpc->doc = xml_parse (fpc->data, (size_t)fpc->dataLength, errors);
	if (!fpc->doc) {
		debug1 (DEBUG_PARSING, "xml_parse_feed(): could not parse feed \"%s\"!", fpc->subscription->node->title);
		g_string_prepend (fpc->feed->parseErrors, _("XML Parser: Could not parse document:\n"));
		g_string_append (fpc->feed->parseErrors, "\n");
	}

	fpc->feed->valid = !(errors->errorCount > 0);
	g_free (errors);
	
	return fpc->doc;
}

void
xml_init (void)
{
	/* set libxml2 to use glib allocation, so that we
	   can free() and reuse libxml2 allocated memory chunks */
	xmlMemSetup (g_free, g_malloc, g_realloc, g_strdup);
	/* has to be called for multithreaded programs */
	xmlInitParser ();
}
