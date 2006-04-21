/**
 * @file common.c common routines for Liferea
 * 
 * Copyright (C) 2003-2006  Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004-2006  Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004       Karl Soderstrom <ks@xanadunet.net>
 *
 * parts of the RFC822 timezone decoding were taken from the gmime 
 * source written by 
 *
 * Authors: Michael Zucchi <notzed@helixcode.com>
 *          Jeffrey Stedfast <fejj@helixcode.com>
 *
 * Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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

#define _XOPEN_SOURCE /* glibc2 needs this (man strptime) */

#include <libxml/xmlerror.h>
#include <libxml/uri.h>
#include <libxml/parser.h>
#include <libxml/entities.h>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <glib.h>
#include <sys/stat.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>
#include "support.h"
#include "common.h"
#include "conf.h"
#include "feed.h"
#include "support.h"
#include "debug.h"

static gchar *standard_encoding = { "UTF-8" };

static gchar *lifereaUserPath = NULL;

gchar * convertCharSet(gchar * from_encoding, gchar * to_encoding, gchar * string);

// Do we really need these functions? What about g_string_append()
//void addToHTMLBufferFast(gchar **buffer, const gchar *string) {
//	
//	if(NULL == string)
//		return;
//	
//	if(NULL != *buffer) {
//		gulong oldlength = strlen(*buffer);
//		gulong newlength = strlen(string);
//		gulong allocsize = (((oldlength+newlength+1L)/512L)+1L)*512L; /* Round up to nearest 512 KB */
//		*buffer = g_realloc(*buffer, allocsize);
//		g_memmove(&((*buffer)[oldlength]), string, newlength+1L );
//	} else {
//		*buffer = g_strdup(string);
//	}
//}

#define addToHTMLBufferFast addToHTMLBuffer

void addToHTMLBuffer(gchar **buffer, const gchar *string) {
	
	if(NULL == string)
		return;
	
	if(NULL != *buffer) {
		gulong oldlength = strlen(*buffer);
		gulong newlength = strlen(string);
		gulong allocsize = (oldlength+newlength+1L);
		*buffer = g_realloc(*buffer, allocsize);
		g_memmove(&((*buffer)[oldlength]), string, newlength+1L );
	} else {
		*buffer = g_strdup(string);
	}
}

/* converts the string string encoded in from_encoding (which
   can be NULL) to to_encoding, frees the original string and 
   returns the result */
gchar * convertCharSet(gchar * from_encoding, gchar * to_encoding, gchar * string) {
	gint	bw, br;
	gchar	*new = NULL;
	GError *err = NULL;

	if(NULL == from_encoding)
		from_encoding = standard_encoding;
		
	if(NULL != string) {		
		new = g_convert(string, strlen(string), to_encoding, from_encoding, &br, &bw, &err);
		if (err != NULL) {
			g_warning("error converting character set: %s\n", err->message);
			g_error_free (err);
		}
		if(NULL != new)
			g_free(string);
		else
			new = string;
	} else {	
		return g_strdup("");
	}

	return new;
}

gchar * convertToHTML(gchar * string) { return string; } /* FIXME: BROKEN! return convertCharSet("UTF-8", "HTML", string); } */

/* Conversion function which should be applied to all read XML strings, 
   to ensure proper UTF8. This is because we use libxml2 in recovery
   mode which can produce invalid UTF-8. 
   
   The valid or a corrected string is returned. The original XML 
   string is modified (FIXME: not sure if this is good). */
gchar * utf8_fix(xmlChar *string) {
	const gchar	*invalid_offset;

	if(NULL == string)
		return NULL;
		
	if(!g_utf8_validate(string, -1, &invalid_offset)) {
		/* if we have an invalid string we try to shorten
		   it until it is valid UTF-8 */
		debug0(DEBUG_PARSING, "parser delivered invalid UTF-8!");
		debug1(DEBUG_PARSING, "	>>>%s<<<\n", string);
		debug1(DEBUG_PARSING, "first invalid char is: >>>%s<<<\n" , invalid_offset);
		debug0(DEBUG_PARSING, "removing invalid bytes");
		
		do {
			memmove((void *)invalid_offset, invalid_offset + 1, strlen(invalid_offset + 1) + 1);			
		} while(!g_utf8_validate(string, -1, &invalid_offset));
		
		debug0(DEBUG_PARSING, "result is:\n");
		debug1(DEBUG_PARSING, "	>>>%s<<<\n", string);		
	}
	
	return string;
}

static xmlDocPtr common_parse_html(const gchar *html, gint len) {
	xmlDocPtr out = NULL;
	
	g_assert(html != NULL);
	g_assert(len >= 0);
	

	/* Note: NONET is not implemented so it will return an error
	   because it doesn't know how to handle NONET. But, it might
	   learn in the future. */
	out = htmlReadMemory(html, len, NULL, "utf-8", HTML_PARSE_RECOVER | HTML_PARSE_NONET |
	                     ((debug_level & DEBUG_HTML)?0:(HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING)));
	return out;
}

static xmlNodePtr common_html_doc_find_body(xmlDocPtr doc) {
	xmlXPathContextPtr xpathCtxt = NULL;
	xmlXPathObjectPtr xpathObj = NULL;
	xmlNodePtr node;
	xpathCtxt = xmlXPathNewContext(doc);
	if(!xpathCtxt) goto error;

	xpathObj = xmlXPathEvalExpression("/html/body", xpathCtxt);
	if(!xpathObj) goto error;
	if(!xpathObj->nodesetval->nodeMax) goto error;
	
	node = xpathObj->nodesetval->nodeTab[0];
 error:
	if (xpathObj) xmlXPathFreeObject(xpathObj);
	if (xpathCtxt) xmlXPathFreeContext(xpathCtxt);
	return node;
}

/* Extract XHTML from the children of the passed node. */
gchar * extractHTMLNode(xmlNodePtr cur, gint xhtmlMode, const gchar *defaultBase) {
	xmlBufferPtr	buf;
	gchar		*result = NULL;

	/* Create the new document and add the div tag*/
	xmlDocPtr newDoc = xmlNewDoc( BAD_CAST "1.0" );;
	xmlNodePtr divNode = xmlNewNode(NULL, BAD_CAST "div");;
	xmlDocSetRootElement( newDoc, divNode );
	xmlNewNs(divNode, BAD_CAST "http://www.w3.org/1999/xhtml", NULL);

	/* Set the xml:base  of the div tag */
	if(xmlNodeGetBase(cur->doc, cur))
		xmlNodeSetBase( divNode, xmlNodeGetBase(cur->doc, cur) );
	else if(defaultBase)
		xmlNodeSetBase( divNode, defaultBase);
	
	if(xhtmlMode == 0) { /* Read escaped HTML and convert to XHTML, placing in a div tag */
		xmlDocPtr oldDoc;
		xmlNodePtr copiedNodes;
		xmlChar *escapedhtml;
		/* Parse the HTML into oldDoc*/
		escapedhtml = xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1);//xmlNodeDump(tmpBuf, cur->doc, cur, 0, 0);
		if(escapedhtml) {
			escapedhtml = g_strstrip(escapedhtml);	/* stripping whitespaces to make empty string detection easier */
			if(*escapedhtml) {			/* never process empty content, xmlDocCopy() doesn't like it... */
				oldDoc = common_parse_html(escapedhtml, strlen(escapedhtml));
				/* Copy in the html tags */
				copiedNodes = xmlDocCopyNodeList( newDoc, common_html_doc_find_body(oldDoc)->xmlChildrenNode);
				xmlAddChildList(divNode, copiedNodes);
				xmlFreeDoc(oldDoc);
				xmlFree(escapedhtml);
			}
		}
	} else if(xhtmlMode == 1 || xhtmlMode == 2){ /* Read multiple XHTML tags and embed in div tag */
		xmlNodePtr copiedNodes = xmlDocCopyNodeList( newDoc, cur->xmlChildrenNode);
		xmlAddChildList(divNode, copiedNodes);
	}
	
	buf = xmlBufferCreate();
	xmlNodeDump(buf, newDoc, xmlDocGetRootElement(newDoc), 0, 0 );
	
	if(xmlBufferLength(buf) > 0)
		result = xmlCharStrdup(xmlBufferContent(buf));

	xmlBufferFree(buf);
	xmlFreeDoc(newDoc);
	return result;
}

/* Convert the given string to proper XHTML content.*/
gchar * common_text_to_xhtml(const gchar *text) {
	gchar		*result = NULL;
	xmlDocPtr	doc = NULL;
	xmlBufferPtr	buf;
	
	if(!text)
		return g_strdup("");
	
	text = g_strstrip(text);	/* stripping whitespaces to make empty string detection easier */
	if(*text) {
		doc = common_parse_html(text, strlen(text));
		
		buf = xmlBufferCreate();
		xmlNodeDump(buf, doc, common_html_doc_find_body(doc)->xmlChildrenNode, 0, 0 );

		if(xmlBufferLength(buf) > 0)
			result = xmlCharStrdup(xmlBufferContent(buf));
		else
			result = g_strdup("");

		xmlBufferFree(buf);
		xmlFreeDoc(doc);
	} else {
		result = g_strdup(text);
	}
	
	return result;
}

typedef struct {
	gchar	*data;
	gint	length;
} result_buffer;

void unhtmlizeHandleCharacters (void *user_data, const xmlChar *string, int length) {
	result_buffer	*buffer = (result_buffer *)user_data;
	gint 		old_length;

	old_length = buffer->length;	
	buffer->length += length;
	buffer->data = g_renew(gchar, buffer->data, buffer->length + 1);
        strncpy(buffer->data + old_length, (gchar *)string, length);
	buffer->data[buffer->length] = 0;

}

void _unhtmlize(gchar *string, gchar *result, result_buffer *buffer) {
	htmlParserCtxtPtr	ctxt;
	htmlSAXHandlerPtr	sax_p = g_new0(htmlSAXHandler, 1);
 	sax_p->characters = unhtmlizeHandleCharacters;
	ctxt = htmlCreatePushParserCtxt(sax_p, buffer, string, strlen(string), "", XML_CHAR_ENCODING_UTF8);
	htmlParseChunk(ctxt, string, 0, 1);
	htmlFreeParserCtxt(ctxt);
 	g_free(sax_p);
}

void _unxmlize(gchar *string, gchar *result, result_buffer *buffer) {
	xmlParserCtxtPtr	ctxt;
	xmlSAXHandler	*sax_p = g_new0(xmlSAXHandler, 1);
 	sax_p->characters = unhtmlizeHandleCharacters;
	ctxt = xmlCreatePushParserCtxt(sax_p, buffer, string, strlen(string), "");
	xmlParseChunk(ctxt, string, 0, 1);
	xmlFreeParserCtxt(ctxt);
 	g_free(sax_p);
}

/* Converts a UTF-8 strings containing any XML stuff to 
   a string without any entities or tags containing all
   text nodes of the given HTML string. The original 
   string will be freed. */
static gchar * unmarkupize(gchar *string, void(*parse)(gchar *string, gchar *result, result_buffer *buffer)) {
	gchar			*result;
	result_buffer		*buffer;
	
	if(NULL == string)
		return NULL;
		
	string = utf8_fix(string);

	/* only do something if there are any entities or tags */
	if(NULL == (strpbrk(string, "&<>")))
		return string;

	buffer = g_new0(result_buffer, 1);
	parse(string, result, buffer);
	result = buffer->data;
	g_free(buffer);
 
 	if(result == NULL || !g_utf8_strlen(result, -1)) {
 		/* Something went wrong in the parsing.
 		 * Use original string instead */
		g_free(result);
 		return string;
 	} else {
 		g_free(string);
 		return result;
 	}
}

gchar * unhtmlize(gchar * string) { return unmarkupize(string, _unhtmlize); }
gchar * unxmlize(gchar * string) { return unmarkupize(string, _unxmlize); }

#define MAX_PARSE_ERROR_LINES	10

/** used by bufferParseError to keep track of error messages */
typedef struct errorCtxt {
	gchar	*buffer;
	gint	errorCount;
} *errorCtxtPtr;

/**
 * Error buffering function to be registered by 
 * xmlSetGenericErrorFunc(). This function is called on
 * each libxml2 error output and collects all output as
 * HTML in the buffer ctxt points to. 
 *
 * @param	ctxt	error context
 * @param	msg	printf like format string 
 */
static void common_buffer_parse_error(void *ctxt, const gchar * msg, ...) {
	va_list		params;
	errorCtxtPtr	errors = (errorCtxtPtr)ctxt;
	gchar		*newmsg;
	gchar		*tmp;

	g_assert(NULL != errors);
	
	if(MAX_PARSE_ERROR_LINES > errors->errorCount++) {
		
		va_start(params, msg);
		newmsg = g_strdup_vprintf(msg, params);
		va_end(params);

		g_assert(NULL != newmsg);
		newmsg = utf8_fix(newmsg);
		tmp = g_markup_escape_text(newmsg, -1);
		g_free(newmsg);
		newmsg = tmp;
	
		addToHTMLBufferFast(&errors->buffer, "<pre>");
		addToHTMLBufferFast(&errors->buffer, newmsg);
		addToHTMLBufferFast(&errors->buffer, "</pre>");

		g_free(newmsg);
	}
	
	if(MAX_PARSE_ERROR_LINES == errors->errorCount) {
		newmsg = g_strdup_printf("%s<br/>%s", errors->buffer, _("[Parser error output was truncated!]"));
		g_free(errors->buffer);
		errors->buffer = newmsg;
	}
}

static xmlDocPtr entities = NULL;

/* and I thought writing such functions is evil... */
xmlEntityPtr common_process_entities(void *ctxt, const xmlChar *name) {
	xmlEntityPtr	entity, found;
	xmlChar		*tmp;
	
	entity = xmlGetPredefinedEntity(name);
	if(NULL == entity) {
		if(NULL == entities) {
			/* loading HTML entities */
			entities = xmlNewDoc(BAD_CAST "1.0");
			xmlCreateIntSubset(entities, BAD_CAST "HTML entities", NULL, PACKAGE_DATA_DIR "/" PACKAGE "/dtd/html.ent");
			entities->extSubset = xmlParseDTD(entities->intSubset->ExternalID, entities->intSubset->SystemID);
		}
		
		if(NULL != (found = xmlGetDocEntity(entities, name))) {
			/* returning as faked predefined entity... */
			tmp = xmlStrdup(found->content);
			tmp = unhtmlize(tmp);	/* arghh ... slow... */
			entity = (xmlEntityPtr)g_new0(xmlEntity, 1);
			entity->type = XML_ENTITY_DECL;
			entity->name = name;
			entity->orig = tmp;	/* ??? */
			entity->content = tmp;
			entity->length = g_utf8_strlen(tmp, -1);
			entity->etype = XML_INTERNAL_PREDEFINED_ENTITY;
		}
	}
	if(NULL == entity) {		
		g_print("unsupported entity: %s\n", name);
	}
	return entity;
}

xmlDocPtr common_parse_xml_feed(feedParserCtxtPtr fpc) {
	xmlParserCtxtPtr	ctxt;
	errorCtxtPtr		errors;
	
	g_assert(NULL != fpc->data);
	g_assert(NULL != fpc->feed);
	g_assert(NULL != fpc->itemSet);
	
	/* we don't like no data */
	if(0 == fpc->dataLength) {
		g_warning("common_parse_xml_feed(): Empty input while parsing \"%s\"!", fpc->node->title);
		fpc->feed->parseErrors = g_strdup("Empty input!\n");
		return NULL;
	}
	
	errors = g_new0(struct errorCtxt, 1);
	xmlSetGenericErrorFunc(errors, (xmlGenericErrorFunc)common_buffer_parse_error);
	ctxt = xmlNewParserCtxt();
	ctxt->sax->getEntity = common_process_entities;
	fpc->doc = xmlSAXParseMemory(ctxt->sax, fpc->data, fpc->dataLength, /* recovery = */ TRUE);
	if(!fpc->doc) {
		g_warning("xmlReadMemory: Could not parse document!\n");
		fpc->feed->parseErrors = g_strdup_printf(_("xmlReadMemory(): Could not parse document:\n%s%s"), 
		                            errors->buffer != NULL ? errors->buffer : "",
		                            errors->buffer != NULL ? "\n" : "");
		g_free(errors->buffer);
		errors->buffer = fpc->feed->parseErrors;
	}
	
	/* This seems to reset the errorfunc to its default, so that the
	   GtkHTML2 module is not unhappy because it also tries to call the
	   errorfunc on occasion. */
	xmlSetGenericErrorFunc(NULL, NULL);

	fpc->itemSet->valid = (NULL == errors->buffer);
	fpc->feed->parseErrors = errors->buffer;
	g_free(errors);
	xmlFreeParserCtxt(ctxt);
	
	return fpc->doc;
}

/* converts a ISO 8601 time string to a time_t value */
time_t parseISO8601Date(gchar *date) {
	struct tm	tm;
	time_t		t, t2, offset = 0;
	gboolean	success = FALSE;
	gchar *pos;
	
	g_assert(date != NULL);
	
	memset(&tm, 0, sizeof(struct tm));
	
	/* we expect at least something like "2003-08-07T15:28:19" and
	   don't require the second fractions and the timezone info

	   the most specific format:   YYYY-MM-DDThh:mm:ss.sTZD
	 */
	 
	/* full specified variant */
	if(NULL != (pos = strptime((const char *)date, "%t%Y-%m-%dT%H:%M%t", &tm))) {
		/* Parse seconds */
		if (*pos == ':')
			pos++;
		if (isdigit(pos[0]) && !isdigit(pos[1])) {
			tm.tm_sec = pos[0] - '0';
			pos++;
		} else if (isdigit(pos[0]) && isdigit(pos[1])) {
			tm.tm_sec = 10*(pos[0]-'0') + pos[1] - '0';
			pos +=2;
		}
		/* Parse timezone */
		if (*pos == 'Z')
			offset = 0;
		else if ((*pos == '+' || *pos == '-') && isdigit(pos[1]) && isdigit(pos[2]) && strlen(pos) >= 3) {
			offset = (10*(pos[1] - '0') + (pos[2] - '0')) * 60 * 60;
			
			if (pos[3] == ':' && isdigit(pos[4]) && isdigit(pos[5]))
				offset +=  (10*(pos[4] - '0') + (pos[5] - '0')) * 60;
			else if (isdigit(pos[3]) && isdigit(pos[4]))
				offset +=  (10*(pos[3] - '0') + (pos[4] - '0')) * 60;
			
			offset *= (pos[0] == '+') ? 1 : -1;

		}
		success = TRUE;
	/* only date */
	} else if(NULL != strptime((const char *)date, "%t%Y-%m-%d", &tm))
		success = TRUE;
	/* there were others combinations too... */

	if(TRUE == success) {
		if((time_t)(-1) != (t = mktime(&tm))) {
			/* Correct for the local timezone*/
			t = t - offset;
			t2 = mktime(gmtime(&t));
			t = t - (t2 - t);
			
			return t;
		} else {
			g_message(_("internal error! time conversion error! mktime failed!\n"));
		}
	} else {
		g_message(_("Invalid ISO8601 date format! Ignoring <dc:date> information!\n"));
	}
	
	return 0;
}

gchar *dayofweek[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
gchar *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

gchar *createRFC822Date(const time_t *time) {
	struct tm *tm;

	tm = gmtime(time); /* No need to free because it is statically allocated */
	return g_strdup_printf("%s, %2d %s %4d %02d:%02d:%02d GMT", dayofweek[tm->tm_wday], tm->tm_mday,
					   months[tm->tm_mon], 1900 + tm->tm_year, tm->tm_hour, tm->tm_min, tm->tm_sec);
}

/* this table of RFC822 timezones is from gmime-utils.c of the gmime API */
static struct {
	char *name;
	int offset;
} tz_offsets [] = {
	{ "UT", 0 },
	{ "GMT", 0 },
	{ "EST", -500 },        /* these are all US timezones.  bloody yanks */
	{ "EDT", -400 },
	{ "CST", -600 },
	{ "CDT", -500 },
	{ "MST", -700 },
	{ "MDT", -600 },
	{ "PST", -800 },
	{ "PDT", -700 },
	{ "Z", 0 },
	{ "A", -100 },
	{ "M", -1200 },
	{ "N", 100 },
	{ "Y", 1200 }
};
/** @returns timezone offset in seconds */
static time_t common_parse_rfc822_tz(char *token) {
	int offset = 0;
	const char *inptr = token;
	
	if (*inptr == '+' || *inptr == '-') {
		offset = atoi (inptr);
	} else {
		int t;

		if (*inptr == '(')
			inptr++;

		for (t = 0; t < 15; t++)
			if (!strncmp (inptr, tz_offsets[t].name, strlen (tz_offsets[t].name)))
				offset = tz_offsets[t].offset;
	}
	
	return 60 * ((offset / 100) * 60 + (offset % 100));
}


/* converts a RFC822 time string to a time_t value */
time_t parseRFC822Date(gchar *date) {
	struct tm	tm;
	time_t		t, t2;
	char 		*oldlocale;
	char		*pos;
	gboolean	success = FALSE;

	memset(&tm, 0, sizeof(struct tm));

	/* we expect at least something like "03 Dec 12 01:38:34" 
	   and don't require a day of week or the timezone

	   the most specific format we expect:  "Fri, 03 Dec 12 01:38:34 CET"
	 */
	/* skip day of week */
	if(NULL != (pos = g_utf8_strchr(date, -1, ',')))
		date = ++pos;

	/* we expect English month names, so we set the locale */
	oldlocale = g_strdup(setlocale(LC_TIME, NULL));
	setlocale(LC_TIME, "C");
	
	/* standard format with seconds and 4 digit year */
	if(NULL != (pos = strptime((const char *)date, " %d %b %Y %T", &tm)))
		success = TRUE;
	/* non-standard format without seconds and 4 digit year */
	if(NULL != (pos = strptime((const char *)date, " %d %b %Y %H:%M", &tm)))
		success = TRUE;
	/* non-standard format with seconds and 2 digit year */
	else if(NULL != (pos = strptime((const char *)date, " %d %b %y %T", &tm)))
		success = TRUE;
	/* non-standard format without seconds 2 digit year */
	else if(NULL != (pos = strptime((const char *)date, " %d %b %y %H:%M", &tm)))
		success = TRUE;
	
	while(pos != NULL && *pos != '\0' && isspace((int)*pos))       /* skip whitespaces before timezone */
		pos++;
	
	if(NULL != oldlocale) {
		setlocale(LC_TIME, oldlocale);	/* and reset it again */
		g_free(oldlocale);
	}
	
	if(TRUE == success) {
		if((time_t)(-1) != (t = mktime(&tm))) {
			/* GMT time, with no daylight savings time
			   correction. (Usually, there is no daylight savings
			   time since the input is GMT.) */
			t = t - common_parse_rfc822_tz(pos);
			t2 = mktime(gmtime(&t));
			t = t - (t2 - t);
			return t;
		} else
			g_warning(_("internal error! time conversion error! mktime failed!\n"));
	}
	
	return 0;
}

static void common_check_dir(gchar *path) {

	if(!g_file_test(path, G_FILE_TEST_IS_DIR)) {
		if(0 != mkdir(path, S_IRUSR | S_IWUSR | S_IXUSR)) {
			g_error(_("Cannot create cache directory \"%s\"!"), path);
		}
	}
	g_free(path);
}

void initCachePath(void) {
	gchar *cachePath;

	/* until the 1.1 code stabilizes let's use a parallel cache storage */
	lifereaUserPath = g_strdup_printf("%s" G_DIR_SEPARATOR_S ".liferea_1.1", g_get_home_dir());
	cachePath = g_strdup_printf("%s" G_DIR_SEPARATOR_S "cache", lifereaUserPath);

	common_check_dir(g_strdup(lifereaUserPath));
	common_check_dir(g_strdup(cachePath));
	common_check_dir(g_strdup_printf("%s" G_DIR_SEPARATOR_S "feeds", cachePath));
	common_check_dir(g_strdup_printf("%s" G_DIR_SEPARATOR_S "favicons", cachePath));
	common_check_dir(g_strdup_printf("%s" G_DIR_SEPARATOR_S "plugins", cachePath));

	g_free(cachePath);
	/* lifereaUserPath reused globally */
}

gchar * common_get_cache_path(void) {
	
	if(NULL == lifereaUserPath)
		initCachePath();
		
	return lifereaUserPath;
}

gchar * common_create_cache_filename(const gchar *folder, const gchar *filename, const gchar *extension) {
	gchar *result;

	result = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s%s%s%s%s", common_get_cache_path(),
	                         (folder != NULL) ? folder : "",
	                         (folder != NULL) ? G_DIR_SEPARATOR_S : "",
	                         filename,
	                         (extension != NULL)? "." : "",
	                         (extension != NULL)? extension : "");

	return result;
}

static gchar * byte_to_hex(unsigned char nr) {
	gchar *result = NULL;

	result = g_strdup_printf("%%%x%x", nr / 0x10, nr % 0x10);
	return result;
}

gchar * encode_uri_string(gchar *string) {
	gchar		*newURIString;
	gchar		*hex, *tmp = NULL;
	int		i, j, len, bytes;

	/* the UTF-8 string is casted to ASCII to treat
	   the characters bytewise and convert non-ASCII
	   compatible chars to URI hexcodes */
	newURIString = g_strdup("");
	len = strlen(string);
	for(i = 0; i < len; i++) {
		if(g_ascii_isalnum(string[i]) || strchr("-_.!~*'()", (int)string[i]))
		   	tmp = g_strdup_printf("%s%c", newURIString, string[i]);
		else if(string[i] == ' ')
			tmp = g_strdup_printf("%s%%20", newURIString);
		else if((unsigned char)string[i] <= 127) {
			tmp = g_strdup_printf("%s%s", newURIString, hex = byte_to_hex(string[i]));g_free(hex);
		} else {
			bytes = 0;
			if(((unsigned char)string[i] >= 192) && ((unsigned char)string[i] <= 223))
				bytes = 2;
			else if(((unsigned char)string[i] > 223) && ((unsigned char)string[i] <= 239))
				bytes = 3;
			else if(((unsigned char)string[i] > 239) && ((unsigned char)string[i] <= 247))
				bytes = 4;
			else if(((unsigned char)string[i] > 247) && ((unsigned char)string[i] <= 251))
				bytes = 5;
			else if(((unsigned char)string[i] > 247) && ((unsigned char)string[i] <= 251))
				bytes = 6;
				
			if(0 != bytes) {
				if((i + (bytes - 1)) > len) {
					g_warning(_("Unexpected end of character sequence or corrupt UTF-8 encoding! Some characters were dropped!"));
					break;
				}

				for(j=0; j < (bytes - 1); j++) {
					tmp = g_strdup_printf("%s%s", newURIString, hex = byte_to_hex((unsigned char)string[i++]));
					g_free(hex);
					g_free(newURIString);
					newURIString = tmp;
				}
				tmp = g_strdup_printf("%s%s", newURIString, hex = byte_to_hex((unsigned char)string[i]));
				g_free(hex);
			} else {
				/* sh..! */
				g_error("Internal error while converting UTF-8 chars to HTTP URI!");
			}
		}
		g_free(newURIString); 
		newURIString = tmp;
	}
	g_free(string);

	return newURIString;
}

xmlChar * common_uri_escape(const xmlChar *url) {
	xmlChar	*result;

	result = xmlURIEscape(url);
	
	/* workaround for libxml2 problem... */
	if(NULL == result)
		result = g_strdup(url);

	return result;	
}

/* to correctly escape and expand URLs, does not touch the
   passed strings */
xmlChar * common_build_url(const gchar *url, const gchar *baseURL) {
	xmlChar	*escapedURL, *absURL, *escapedBaseURL;

	escapedURL = common_uri_escape(url);

	if(NULL != baseURL) {
		escapedBaseURL = common_uri_escape(baseURL);	
		absURL = xmlBuildURI(escapedURL, escapedBaseURL);
		xmlFree(escapedURL);
		xmlFree(escapedBaseURL);
	} else {
		absURL = escapedURL;
	}

	return absURL;
}

#ifndef HAVE_STRSEP
/* code taken from glibc-2.2.1/sysdeps/generic/strsep.c */
char* strsep (char **stringp, const char *delim) {
	char *begin, *end;

	begin = *stringp;
	if (begin == NULL)
		return NULL;

	/* A frequent case is when the delimiter string contains only one
	   character.  Here we don't need to call the expensive `strpbrk'
	   function and instead work using `strchr'.  */
	if (delim[0] == '\0' || delim[1] == '\0')
		{
			char ch = delim[0];

			if (ch == '\0')
				end = NULL;
			else
				{
					if (*begin == ch)
						end = begin;
					else if (*begin == '\0')
						end = NULL;
					else
						end = strchr (begin + 1, ch);
				}
		}
	else
		/* Find the end of the token.  */
		end = strpbrk (begin, delim);

	if (end)
		{
			/* Terminate the token and set *STRINGP past NUL character.  */
			*end++ = '\0';
			*stringp = end;
		}
	else
		/* No more delimiters; this is the last token.  */
		*stringp = NULL;
	return begin;
}
#endif /*HAVE_STRSEP*/

/* Taken from gaim 24 June 2004, copyrighted by the gaim developers
   under the GPL, etc.... */
gchar *strreplace(const char *string, const char *delimiter,
			   const char *replacement) {
	gchar **split;
	gchar *ret;

	g_return_val_if_fail(string      != NULL, NULL);
	g_return_val_if_fail(delimiter   != NULL, NULL);
	g_return_val_if_fail(replacement != NULL, NULL);

	split = g_strsplit(string, delimiter, 0);
	ret = g_strjoinv(replacement, split);
	g_strfreev(split);

	return ret;
}

typedef unsigned chartype;

/* strcasestr is Copyright (C) 1994, 1996-2000, 2004 Free Software
   Foundation, Inc.  It was taken from the GNU C Library, which is
   licenced under the GPL v2.1 or (at your option) newer version. */
char *liferea_strcasestr (const char *phaystack, const char *pneedle)
{
	register const unsigned char *haystack, *needle;
	register chartype b, c;

	haystack = (const unsigned char *) phaystack;
	needle = (const unsigned char *) pneedle;

	b = tolower(*needle);
	if (b != '\0') {
		haystack--;             /* possible ANSI violation */
		do {
			c = *++haystack;
			if (c == '\0')
				goto ret0;
		} while (tolower(c) != (int) b);
		
		c = tolower(*++needle);
		if (c == '\0')
			goto foundneedle;
		++needle;
		goto jin;
		
		for (;;) {
			register chartype a;
			register const unsigned char *rhaystack, *rneedle;
			
			do {
				a = *++haystack;
				if (a == '\0')
					goto ret0;
				if (tolower(a) == (int) b)
					break;
				a = *++haystack;
				if (a == '\0')
					goto ret0;
			shloop:
				;
			}
			while (tolower(a) != (int) b);
			
		jin:      a = *++haystack;
			if (a == '\0')
				goto ret0;
			
			if (tolower(a) != (int) c)
				goto shloop;
			
			rhaystack = haystack-- + 1;
			rneedle = needle;
			a = tolower(*rneedle);
			
			if (tolower(*rhaystack) == (int) a)
				do {
					if (a == '\0')
						goto foundneedle;
					++rhaystack;
					a = tolower(*++needle);
					if (tolower(*rhaystack) != (int) a)
						break;
					if (a == '\0')
						goto foundneedle;
					++rhaystack;
					a = tolower(*++needle);
				} while (tolower (*rhaystack) == (int) a);
			
			needle = rneedle;             /* took the register-poor approach */
			
			if (a == '\0')
				break;
		}
	}
 foundneedle:
	return (char*) haystack;
 ret0:
	return 0;
}
