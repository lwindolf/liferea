/**
 * @file xml.c XML helper methods for Liferea
 * 
 * Copyright (C) 2003-2007  Lars Lindner <lars.lindner@gmail.com>
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
#include <unistd.h>	/* fsync */
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

static xmlDocPtr
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
				   ro determine additional namespaces for item content. For
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

/* Convert the given string to proper XHTML content.*/
gchar *
xhtml_from_text (const gchar *sourceText)
{
	gchar		*text, *result = NULL;
	xmlDocPtr	doc = NULL;
	xmlBufferPtr	buf;
	xmlNodePtr 	body, cur;
	
	if (!sourceText)
		return g_strdup ("");
	
	text = g_strstrip (g_strdup (sourceText));	/* stripping whitespaces to make empty string detection easier */
	if (*text) {	
		doc = xhtml_parse (text, strlen (text));
		
		buf = xmlBufferCreate ();
		body = xhtml_find_body (doc);
		if(body) {
			cur = body->xmlChildrenNode;
			while(cur) {
				xmlNodeDump(buf, doc, cur, 0, 0 );
				cur = cur->next;
			}
		}

		if(xmlBufferLength(buf) > 0)
			result = g_strdup(xmlBufferContent(buf));
		else
			result = g_strdup("");

		xmlBufferFree(buf);
		xmlFreeDoc(doc);
		g_free(text);
	} else {
		result = text;
	}
	
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
	
	doc = xml_parse (xml, strlen (xml), FALSE, errors);
	if (doc)
		xmlFreeDoc (doc);
		
	g_free (xml);
	g_string_free (errors->msg, TRUE);
	result = (0 == errors->errorCount);
	g_free (errors);
	
	return result;
}

gchar *
xhtml_strip_dhtml (const gchar *html)
{
	gchar *tmp;

	// FIXME: move to XSLT stylesheet that post processes
	// generated XHTML. The solution below might break harmless
	// escaped HTML.
	
	/* remove some nasty DHTML stuff from the given HTML content */
	tmp = g_strdup (html);
	tmp = common_strreplace (tmp, " onload=", " no_onload=");
	tmp = common_strreplace (tmp, " onLoad=", " no_onLoad=");
	tmp = common_strreplace (tmp, "script>", "no_script>");		
	tmp = common_strreplace (tmp, "<script ", "<no_script ");
	tmp = common_strreplace (tmp, "<meta ", "<no_meta ");
	tmp = common_strreplace (tmp, "/meta>", "/no_meta>");
	tmp = common_strreplace (tmp, "<iframe ", "<no_iframe ");
	tmp = common_strreplace (tmp, "/iframe>", "/no_iframe>");
	
	return tmp;
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
		
	string = common_utf8_fix (string);

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

		g_assert (NULL != newmsg);
		newmsg = common_utf8_fix (newmsg);
		tmp = g_markup_escape_text (newmsg, -1);
		g_free (newmsg);
		newmsg = tmp;
	
		g_string_append_printf(errors->msg, "<pre>%s</pre>\n", newmsg);
		g_free(newmsg);
	}
	
	if (MAX_PARSE_ERROR_LINES == errors->errorCount)
		g_string_append_printf (errors->msg, "<br />%s", _("[There were more errors. Output was truncated!]"));
}

static xmlDocPtr entities = NULL;

xmlEntityPtr
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
			entity = (xmlEntityPtr)g_new0 (xmlEntity, 1);
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
xpath_find (xmlNodePtr node, gchar *expr)
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
xpath_foreach_match (xmlNodePtr node, gchar *expr, xpathMatchFunc func, gpointer user_data)
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

xmlDocPtr
xml_parse (gchar *data, size_t length, gboolean recovery, errorCtxtPtr errCtx)
{
	xmlParserCtxtPtr	ctxt;
	xmlDocPtr		doc;
	
	g_assert (NULL != data);

	ctxt = xmlNewParserCtxt ();
	ctxt->sax->getEntity = xml_process_entities;
	
	if (errCtx)
		xmlSetGenericErrorFunc (errCtx, (xmlGenericErrorFunc)xml_buffer_parse_error);
	
	doc = xmlSAXParseMemory (ctxt->sax, data, length, /* recovery = */ recovery);
	
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
		debug1 (DEBUG_PARSING, "common_parse_xml_feed(): empty input while parsing \"%s\"!", fpc->subscription->node->title);
		g_string_append (fpc->feed->parseErrors, "Empty input!\n");
		return NULL;
	}

	errors = g_new0 (struct errorCtxt, 1);
	errors->msg = fpc->feed->parseErrors;
	
	fpc->doc = xml_parse (fpc->data, (size_t)fpc->dataLength, fpc->recovery, errors);
	if (!fpc->doc) {
		debug1 (DEBUG_PARSING, "xmlReadMemory: could not parse feed \"%s\"!", fpc->subscription->node->title);
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

/* The following slightly modified function was originally taken 
 * from GConf 2.16.0 backends/xml-dir.c: Copyright (C) 1999, 2000 Red Hat Inc. */

/* for info on why this is used rather than xmlDocDump or xmlSaveFile
 * and friends, see http://bugzilla.gnome.org/show_bug.cgi?id=108329 */
gint
xml_save_to_file (xmlDocPtr doc, gchar *filename)
{
	FILE	*fp;
	char	*xmlbuf;
	int	fd, n;

	fp = g_fopen (filename, "w");
	if (NULL == fp)
		return -1;
  
	xmlDocDumpFormatMemory (doc, (xmlChar **)&xmlbuf, &n, TRUE);
	if (n <= 0) {
		errno = ENOMEM;
		return -1;
	}

	if (fwrite (xmlbuf, sizeof (xmlChar), n, fp) < n) {
		xmlFree (xmlbuf);
		return -1;
	}

	xmlFree (xmlbuf);

	/* From the fflush(3) man page:
	*
	* Note that fflush() only flushes the user space buffers provided by the
	* C library. To ensure that the data is physically stored on disk the
	* kernel buffers must be flushed too, e.g. with sync(2) or fsync(2).
	*/

	/* flush user-space buffers */
	if (fflush (fp) != 0)
		return -1;

	if ((fd = fileno (fp)) == -1)
		return -1;

#ifdef HAVE_FSYNC
	/* sync kernel-space buffers to disk */
	if (fsync (fd) == -1)
		return -1;
#endif

	fclose (fp);

	return 0;
}
