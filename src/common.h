/*
   common routines
      
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

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
#include <libgtkhtml/gtkhtml.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <glib.h>

#define htmlToCDATA(buffer) g_strdup_printf("<![CDATA[%s]]>", buffer)

/* the encoding all item descriptions are converted to for display with libGtkHTML */
static gchar	*standard_encoding = { "UTF-8" };

gchar * convertCharSet(gchar * from_encoding, gchar * to_encoding, gchar * string);
gchar * convertToUTF8(gchar * from_encoding, gchar * string);
gchar * convertToHTML(gchar * from_encoding, gchar * string);
gchar * parseHTML(htmlDocPtr doc, htmlNodePtr cur, gchar *string);

/* to extract not escaped XHTML from a node */
gchar * extractHTMLNode(xmlNodePtr cur);

void	addToHTMLBuffer(gchar **buffer, gchar *string);

gchar * unhtmlize(gchar * from_encoding, gchar *string);
gchar * getActualTime(void);
time_t 	convertDate(char *date);
gchar * formatDate(time_t t);

gchar *	getExtension(gint type);
gchar *	getCachePath(void);
gchar * getCacheFileName(gchar *keyprefix, gchar *key, gchar *extension);
#endif
