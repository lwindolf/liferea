/**
 * @file common.c common routines for Liferea
 * 
 * Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>
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

#define _XOPEN_SOURCE /* glibc2 needs this (man strptime) */
#include <config.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <glib.h>
#include <sys/stat.h>
#include <string.h>
#include <locale.h>
#include "support.h"
#include "common.h"
#include "conf.h"
#include "support.h"
#include "feed.h"
#include "debug.h"

static gchar *standard_encoding = { "UTF-8" };

static gchar *lifereaUserPath = NULL;

gchar * convertCharSet(gchar * from_encoding, gchar * to_encoding, gchar * string);

void addToHTMLBuffer(gchar **buffer, const gchar *string) {
	
	if(NULL == string)
		return;
	
	if(NULL != *buffer) {
		int oldlength = strlen(*buffer);
		int newlength = strlen(string);
		int allocsize = (((oldlength+newlength+1)/512)+1)*512; // Round up to nearest 512 KB
		*buffer = g_realloc(*buffer, allocsize);
		g_memmove(&((*buffer)[oldlength]), string, newlength+1 );
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
	
	if(NULL == from_encoding)
		from_encoding = standard_encoding;
		
	if(NULL != string) {		
		new = g_convert(string, strlen(string), to_encoding, from_encoding, &br, &bw, NULL);
		
		if(NULL != new)
			g_free(string);
		else
			new = string;
	} else {	
		return g_strdup("");
	}

	return new;
}

gchar * convertToHTML(gchar * string) { return convertCharSet("UTF-8", "HTML", string); }

/* Conversion function which should be applied to all read XML strings, 
   to ensure proper UTF8. This is because we use libxml2 in recovery
   mode which can produce invalid UTF-8. 
   
   The valid or a corrected string is returned, the original XML 
   string is freed. */
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

gchar * extractHTMLNode(xmlNodePtr cur) {
	xmlBufferPtr	buf = NULL;
	gchar		*result = NULL;
	
	buf = xmlBufferCreate();
	
	if(-1 != xmlNodeDump(buf, cur->doc, cur, 0, 0))
		result = xmlCharStrdup(xmlBufferContent(buf));

	xmlBufferFree(buf);
	
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

/* Converts a UTF-8 strings containing any HTML stuff to 
   a string without any entities or tags containing all
   text nodes of the given HTML string. The original 
   string will be freed. */
gchar * unhtmlize(gchar *string) {
	htmlSAXHandlerPtr	sax_p = NULL;
	htmlParserCtxtPtr	ctxt;
	gchar			*result;
	result_buffer		*buffer;
	
	if(NULL == string)
		return NULL;
		
	string = utf8_fix(string);

	/* only do something if there are any entities or tags */
	if(NULL == (strpbrk(string, "&<>")))
		return string;

	buffer = g_new0(result_buffer, 1);
	sax_p = g_new0(htmlSAXHandler, 1);
 	sax_p->characters = unhtmlizeHandleCharacters;

	/* in older versions htmlSAXParseDoc was used which caused
	   strange crashes when freeing the parser context... */
	   
	ctxt = htmlCreatePushParserCtxt(sax_p, buffer, string, strlen(string), "", XML_CHAR_ENCODING_UTF8);
	htmlParseChunk(ctxt, string, 0, 1);
	htmlFreeParserCtxt(ctxt);
	result = buffer->data;
	g_free(buffer);
 	g_free(sax_p);
 
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
void bufferParseError(void *ctxt, const gchar * msg, ...) {
	va_list		params;
	errorCtxtPtr	errors = (errorCtxtPtr)ctxt;
	gchar		*oldmsg;
	gchar		*newmsg;
	gchar		*tmp;

	g_assert(NULL != errors);

	if(MAX_PARSE_ERROR_LINES + 1 >= errors->errorCount++) {
		oldmsg = errors->buffer;
		
		va_start(params, msg);
		newmsg = g_strdup_vprintf(msg, params);
		va_end(params);

		g_assert(NULL != newmsg);
		tmp = g_markup_escape_text(newmsg, -1);
		g_free(newmsg);
		newmsg = tmp;

		if(NULL != oldmsg) 
			errors->buffer = g_strdup_printf("%s<br>%s", oldmsg, newmsg);
		else
			errors->buffer = g_strdup(newmsg);

		g_free(oldmsg);
		g_free(newmsg);
	}
	
	if(MAX_PARSE_ERROR_LINES == errors->errorCount) {
		newmsg = g_strdup_printf("%s<br>%s", errors->buffer, _("[Parser error output was truncated!]"));
		g_free(errors->buffer);
		errors->buffer = newmsg;
	}
}

/**
 * Common function to create a XML DOM object from a given
 * XML buffer. This function sets up a parser context,
 * enables recovery mode and sets up the error handler.
 * 
 * The function returns a XML document pointer or NULL
 * if the document could not be read. It also sets 
 * errormsg to the last error messages on parsing
 * errors. 
 *
 * @param data		XML source
 * @param errormsg	error buffer
 *
 * @return XML document
 */
xmlDocPtr parseBuffer(gchar *data, gchar **errormsg) {
	errorCtxtPtr		errors;
	xmlParserCtxtPtr	parser;
	xmlDocPtr		doc;
	gint			length;
	
	g_assert(NULL != data);

	/* xmlCreateMemoryParserCtxt() doesn't like no data */
	if(0 == (length = strlen(data))) {
		g_warning("parseBuffer(): Empty input!\n");
		*errormsg = g_strdup("parseBuffer(): Empty input!\n");
		return NULL;
	}
	
	if(NULL != (parser = xmlCreateMemoryParserCtxt(data, length))) {
		parser->recovery = 1;
		errors = g_new0(struct errorCtxt, 1);
		xmlSetGenericErrorFunc(errors, (xmlGenericErrorFunc)bufferParseError);
		xmlParseDocument(parser);	/* ignore returned errors */
		doc = parser->myDoc;
		xmlFreeParserCtxt(parser);		
		*errormsg = errors->buffer;
		g_free(errors);
		/* This seems to reset the errorfunc to he default, so that the
		   GtkHTML2 module is not unhappy because it also tries to call the
		   errorfunc on occasion. */
		xmlSetGenericErrorFunc(NULL, NULL);
	} else {
		g_warning("parseBuffer(): Could not create parsing context!\n");
		*errormsg = g_strdup("parseBuffer(): Could not create parsing context!\n");
		return NULL;
	}
	
	return doc;
}

/* converts a ISO 8601 time string to a time_t value */
time_t parseISO8601Date(gchar *date) {
	struct tm	tm;
	time_t		t;
	gboolean	success = FALSE;
		
	memset(&tm, 0, sizeof(struct tm));

	/* we expect at least something like "2003-08-07T15:28:19" and
	   don't require the second fractions and the timezone info

	   the most specific format:   YYYY-MM-DDThh:mm:ss.sTZD
	 */
	 
	/* full specified variant */
	if(NULL != strptime((const char *)date, "%t%Y-%m-%dT%H:%M%t", &tm))
		success = TRUE;
	/* only date */
	else if(NULL != strptime((const char *)date, "%t%Y-%m-%d", &tm))
		success = TRUE;
	/* there were others combinations too... */

	if(TRUE == success) {
		if((time_t)(-1) != (t = mktime(&tm))) {
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
gchar *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug" "Sep", "Oct", "Nov", "Dec"};

gchar *createRFC822Date(const time_t *time) {
	struct tm *tm;

	tm = gmtime(time); /* No need to free because it is statically allocated */
	return g_strdup_printf("%s, %2d %s %4d %02d:%02d:%02d GMT", dayofweek[tm->tm_wday], tm->tm_mday,
					   months[tm->tm_mon], 1900 + tm->tm_year, tm->tm_hour, tm->tm_min, tm->tm_sec);
}

/* converts a RFC822 time string to a time_t value */
time_t parseRFC822Date(gchar *date) {
	struct tm	tm;
	time_t		t;
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
	
	/* standard format with 2 digit year */
	if(NULL != (pos = strptime((const char *)date, "%d %b %y %T", &tm)))
		success = TRUE;
	/* non-standard format with 4 digit year */
	else if(NULL != (pos = strptime((const char *)date, "%d %b %Y %T", &tm)))
		success = TRUE;
	
	if(NULL != oldlocale) {
		setlocale(LC_TIME, oldlocale);	/* and reset it again */
		g_free(oldlocale);
	}
	
	if(TRUE == success) {
		if((time_t)(-1) != (t = mktime(&tm)))
			return t;
		else
			g_warning(_("internal error! time conversion error! mktime failed!\n"));
	} else {

		g_message(_("Invalid RFC822 date format! Ignoring <pubDate> information!\n"));
	}
	
	return 0;
}

void initCachePath(void) {
	gchar *cachePath;
	gchar *feedCachePath;
	gchar *faviconCachePath;

	lifereaUserPath = g_strdup_printf("%s" G_DIR_SEPARATOR_S ".liferea", g_get_home_dir());
	if(!g_file_test(lifereaUserPath, G_FILE_TEST_IS_DIR)) {
		if(0 != mkdir(lifereaUserPath, S_IRUSR | S_IWUSR | S_IXUSR)) {
			g_error(_("Cannot create cache directory %s!"), lifereaUserPath);
		}
	}
	
	cachePath = g_strdup_printf("%s" G_DIR_SEPARATOR_S "cache", lifereaUserPath);
	if(!g_file_test(cachePath, G_FILE_TEST_IS_DIR)) {
		if(0 != mkdir(cachePath, S_IRUSR | S_IWUSR | S_IXUSR)) {
			g_error(_("Cannot create cache directory %s!"), cachePath);
		}
	}
	feedCachePath = g_strdup_printf("%s" G_DIR_SEPARATOR_S "feeds", cachePath);
	if(!g_file_test(feedCachePath, G_FILE_TEST_IS_DIR)) {
		if(0 != mkdir(feedCachePath, S_IRUSR | S_IWUSR | S_IXUSR)) {
			g_error(_("Cannot create cache directory %s!"), feedCachePath);
		}
	}

	faviconCachePath = g_strdup_printf("%s" G_DIR_SEPARATOR_S "favicons", cachePath);
	if(!g_file_test(faviconCachePath, G_FILE_TEST_IS_DIR)) {
		if(0 != mkdir(faviconCachePath, S_IRUSR | S_IWUSR | S_IXUSR)) {
			g_error(_("Cannot create cache directory %s!"), faviconCachePath);
		}
	}

	g_free(cachePath);
	g_free(feedCachePath);
	g_free(faviconCachePath);

}

gchar * getCachePath(void) {
	
	if(NULL == lifereaUserPath)
		initCachePath();
		
	return lifereaUserPath;
}

gchar * common_create_cache_filename( gchar *folder, gchar *key, gchar *extension) {
	gchar *filename;

	filename = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s%s%s%s%s", getCachePath(),
						  (folder != NULL) ? folder : "",
						  (folder != NULL) ? G_DIR_SEPARATOR_S : "",
						  key,
						  (extension != NULL)? "." : "",
						  (extension != NULL)? extension : "");
	
	return filename;
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
			tmp = g_strdup_printf(newURIString, hex = byte_to_hex(string[i]));g_free(hex);
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
				// sh..!
				g_error("Internal error while converting UTF-8 chars to HTTP URI!");
			}
		}
		g_free(newURIString); 
		newURIString = tmp;
	}
	g_free(string);

	return newURIString;
}

gchar * encode_uri(gchar *uri_string) {
	gchar		*new_uri_string, *tmp;
	guint 		i;

	g_return_val_if_fail(uri_string != NULL, NULL);
	new_uri_string = g_strdup("");
	for(i = 0; i < strlen(uri_string); i++) {
		if(g_ascii_isalnum(uri_string[i]) || 
		   strchr(";/?:@&=+$,", (int)uri_string[i]) ||	/* reserved URI chars */
   		   strchr("-_.!~*'()", (int)uri_string[i]))	/* unreserved URI chars */
			tmp = g_strdup_printf("%s%c", new_uri_string, (unsigned char)uri_string[i]);
		else
			tmp = g_strdup_printf("%s%%%02x", new_uri_string, (unsigned char)uri_string[i]);
			
		g_free(new_uri_string);
		new_uri_string = tmp;
	}
	
	return new_uri_string;
}

gchar * filter_title(gchar * title) {

	return g_strstrip(title);
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
