/*
   common routines
   
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "support.h"
#include "common.h"
#include "conf.h"
#include "support.h"

#define	TIMESTRLEN	256

static gchar *CACHEPATH = NULL;

/* converts the string string encoded in from_encoding (which
   can be NULL) to to_encoding, frees the original string and 
   returns the result */
gchar * convertCharSet(gchar * from_encoding, gchar * to_encoding, gchar * string) {
	gint	bw, br;
	gchar	*new = NULL;
	
	if(NULL == from_encoding)
		from_encoding = standard_encoding;
		
	if(NULL != string) {		
		// FIXME: is this thread safe?	
		new = g_convert(string, strlen(string), from_encoding, to_encoding, &br, &bw, NULL);
		
		if(NULL != new)
			g_free(string);
		else
			new = string;
	} else {	
		return g_strdup("");
	}

	return new;
}

gchar * convertToUTF8(gchar * from_encoding, gchar * string) { return convertCharSet(from_encoding, "UTF-8", string); }
gchar * convertToHTML(gchar * from_encoding, gchar * string) { return convertCharSet(from_encoding, "HTML", string); }

gchar * parseHTML(htmlDocPtr doc, htmlNodePtr cur, gchar *string) {
	gchar	*newstring = NULL;
	gchar	*oldstring = NULL;
	
	if((NULL == cur) || (NULL == doc)) {
		g_warning(_("internal error: XML document pointer NULL! This should not happen!\n"));
		return;
	}
	
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if(cur->content != NULL) {
			// g_print("%s >>>%s<<<\n", cur->name, cur->content);
			oldstring = newstring;
			if(NULL != newstring)
				newstring = g_strdup_printf("%s%s", newstring, cur->content);
			else
				newstring = g_strdup(cur->content);
			g_free(oldstring);
		}
		
		if(cur->xmlChildrenNode != NULL)
			newstring = parseHTML(doc, cur->xmlChildrenNode, newstring);
			
		cur = cur->next;		
	}
	
	return newstring;
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

/* converts strings containing any HTML stuff to proper HTML

   FIXME: still buggy, correctly converts entities and
   preserves encodings, but does loose text inside enclosing
   formatting tags like "<b>Hallo</b>" 
 */
gchar * unhtmlize(gchar * from_encoding, gchar *string) {
	htmlParserCtxtPtr	ctxt; 
	xmlDocPtr		pDoc;
	int			length;
	gchar			*newstring = NULL;
	
	if(NULL == string)
		return NULL;
	
	/* only do something if there are any entities */
	if(NULL == (strchr(string, '&')))
		return string;
g_print("before:%s\n", string);
	string = convertToUTF8(from_encoding, string);
g_print("after:%s\n", string);		
	length = strlen(string);
	newstring = (gchar *)g_malloc(length + 1);
	memset(newstring, 0, length + 1);
	
	ctxt = htmlCreatePushParserCtxt(NULL, NULL, newstring, length, 0, (xmlCharEncoding)from_encoding);
	ctxt->sax->warning = NULL;	/* disable XML errors and warnings */
	ctxt->sax->error = NULL;
	
        htmlParseChunk(ctxt, string, length, 0);
        htmlParseChunk(ctxt, string, 0, 1);
        pDoc = ctxt->myDoc;
	newstring = parseHTML(pDoc, xmlDocGetRootElement(pDoc), "");	
        htmlFreeParserCtxt(ctxt);
	
	g_free(string);
	
	return newstring;
}

/* converts a ISO 8601 time string to a time_t value */
time_t convertDate(char *date) {
	struct tm	tm;
	time_t		t;
	char		*pos;
	
	memset(&tm, 0, sizeof(struct tm));

	/* we expect at least something like "2003-08-07T15:28:19" and
	   don't require the second fractions and the timezone info

	   the most specific format:   YYYY-MM-DDThh:mm:ss.sTZD
	 */
	pos = (char *)strptime((const char *)date, "%Y-%m-%dT%H:%M", &tm);				
	g_free(date);

	if(pos != NULL) {
		if((time_t)(-1) != (t = mktime(&tm))) {
			return t;
		} else {
			g_warning(_("time conversion error! mktime failed!\n"));
		}
	} else {
		g_print(_("Invalid date format! Ignoring <dc:date> information!\n"));				
	}
	
	return 0;
}

gchar * getActualTime(void) {
	time_t		t;
	gchar		*timestr;
	gchar		*timeformat;
	
	/* get receive time */
	if((time_t)-1 != time(&t)) {
		if(NULL != (timestr = (gchar *)g_malloc(TIMESTRLEN+1))) {
			if(NULL != (timeformat = getStringConfValue(TIME_FORMAT))) {
				strftime(timestr, TIMESTRLEN, (char *)timeformat, gmtime(&t));
				g_free(timeformat);
			}
		}
	}
	
	return timestr;
}

gchar * formatDate(time_t t) {
	gchar		*timestr;
	gchar		*timeformat;
	
	if(NULL != (timestr = (gchar *)g_malloc(TIMESTRLEN+1))) {
		if(NULL != (timeformat = getStringConfValue(TIME_FORMAT))) {
			if(0 == strlen(timeformat)) {
				/* if not configured use default format */
				g_free(timeformat);
				timeformat = g_strdup("%H:%M");
			}
			strftime(timestr, TIMESTRLEN, (char *)timeformat, gmtime(&t));
			g_free(timeformat);
		}
	}
	
	return timestr;
}

void initCachePath(void) {
	struct stat	statinfo;
	struct passwd	*pwent;

	if(NULL != (pwent = getpwuid(getuid()))) {
		CACHEPATH = g_strdup_printf("%s/.liferea", pwent->pw_dir);
	
		if(0 != stat(CACHEPATH, &statinfo)) {
			if(0 != mkdir(CACHEPATH, S_IRUSR | S_IWUSR | S_IXUSR)) {
				g_error(g_strdup_printf(_("Cannot create cache directory %s!"), CACHEPATH));
			}
		}
//		free(pwent);
	} else {
		g_error(g_strerror(errno));
	}
}

gchar * getCachePath(void) {
	
	if(NULL == CACHEPATH)
		initCachePath();
		
	return CACHEPATH;
}

gchar * getCacheFileName(gchar *keyprefix, gchar *key, gchar *extension) {
	gchar	*keypos;
	
	/* build filename */	
	keypos = strrchr(key, '/');
	if(NULL == keypos)
		keypos = key;
	else
		keypos++;
		
	return g_strdup_printf("%s/%s_%s.%s", getCachePath(), keyprefix, keypos, extension);
}
