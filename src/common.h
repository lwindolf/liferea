/*
   common routines
      
   Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef _COMMON_H
#define _COMMON_H

#include <time.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <glib.h>

#define htmlToCDATA(buffer) g_strdup_printf("<![CDATA[%s]]>", buffer)

struct folder;

typedef struct node {
	gint type;
	gpointer *ui_data;
} *nodePtr;

/* Conversion function which should be applied to all read XML strings, 
   to ensure proper UTF8. doc points to the xml document and its encoding and
   string is a xmlChar pointer to the read string. The result gchar
   string is returned, the original XML string is freed. */
gchar * CONVERT(xmlChar * string);

/* converts a UTF-8 string to HTML (resolves XML entities) */
gchar * convertToHTML(gchar * string);

/* converts a UTF-8 string containing HTML tags to plain text */
gchar * unhtmlize(gchar *string);

/* parses a XML node and returns its contents as a string */
//gchar * parseHTML(htmlNodePtr cur);

/* to extract not escaped XHTML from a node */
gchar * extractHTMLNode(xmlNodePtr cur);

void	addToHTMLBuffer(gchar **buffer, gchar *string);

/* Common function to create a XML DOM object from a given
   XML buffer. This function sets up a parser context,
   enables recovery mode and sets up the error handler.
   
   The function returns a XML document and (if errors)
   occur sets the errormsg to the last error message. */
xmlDocPtr parseBuffer(gchar *data, gchar **errormsg);

time_t 	parseISO8601Date(gchar *date);
time_t 	parseRFC822Date(gchar *date);
// FIXME: formatDate used by several functions not only
// to format date column, dontt use always date column format!!!
// maybe gchar * formatDate(time_t, gchar *format)
gchar * formatDate(time_t t);

gchar *	getExtension(gint type);
gchar *	getCachePath(void);
gchar * getCacheFileName(gchar *key, gchar *extension);

gchar * encodeURIString(gchar *uriString);
#endif
