/*
   admin namespace support
   
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

#include <string.h> /* For strcmp() */
#include "htmlview.h"
#include "support.h"
#include "ns_admin.h"
#include "common.h"

#define TABLE_START	"<div class=\"feedfoottitle\">administrative information</div><table class=\"addfoot\">"
#define FIRSTTD		"<tr class=\"feedfoot\"><td class=\"feedfootname\"><span class=\"feedfootname\">"
#define NEXTTD		"</span></td><td class=\"feedfootvalue\"><span class=\"feedfootvalue\">"
#define LASTTD		"</span></td></tr>"
#define TABLE_END	"</table>"

static gchar ns_admin_prefix[] = "admin";

/* you can find an admin namespace spec at:
   http://web.resource.org/rss/1.0/modules/admin/
 
  taglist for admin:
  --------------------------------
    errorReportsTo
    generatorAgent
  --------------------------------
  
  both tags usually contains URIs which we simply display in the
  feed info view footer
*/

gchar * ns_admin_getRSSNsPrefix(void) { return ns_admin_prefix; }

static void parseChannelTag(RSSChannelPtr cp, xmlNodePtr cur) {
	xmlChar		*string;
	gchar		*buffer = NULL;
	gchar		*name = NULL;
	gchar		*key, *value;
	
	if(!xmlStrcmp("errorReportsTo", cur->name)) 
		name = g_strdup(_("report errors to"));
		
	else if(!xmlStrcmp("generatorAgent", cur->name)) 
		name = g_strdup(_("feed generator"));
	
	if(NULL != name) {
		string = xmlGetProp(cur, "resource");
		value = CONVERT(string);
		xmlFree(string);
		
		if(NULL != value) {
			addToHTMLBuffer(&buffer, FIRSTTD);
			addToHTMLBuffer(&buffer, name);
			addToHTMLBuffer(&buffer, NEXTTD);
			addToHTMLBuffer(&buffer, "<a href=\"");
			addToHTMLBuffer(&buffer, value);
			addToHTMLBuffer(&buffer, "\">");
			addToHTMLBuffer(&buffer, value);
			addToHTMLBuffer(&buffer, "</a>");	
			addToHTMLBuffer(&buffer, LASTTD);
			g_free(name);
		}
	}
	
	if(NULL != buffer) {
		key = g_strdup_printf("admin:%s", cur->name);
		if(NULL != g_hash_table_lookup(cp->nsinfos, key)) 
			g_hash_table_remove(cp->nsinfos, key);	
		g_hash_table_insert(cp->nsinfos, key, buffer);
	}
}

static gchar * doOutput(GHashTable *nsinfos, gchar **buffer, gchar *tagname) {
	gchar		*output;
	gchar		*key;
	
	g_assert(NULL != nsinfos);
	key = g_strdup_printf("admin:%s", tagname);
	
	if(NULL != (output = g_hash_table_lookup(nsinfos, key))) {
		addToHTMLBuffer(buffer, output);
		g_hash_table_remove(nsinfos, key);
	}
	g_free(key);
}

static gchar * doChannelOutput(gpointer obj) {
	gchar	*buffer = NULL;
	gchar	*output = NULL;
	
	if(NULL != obj) {
		doOutput(((RSSChannelPtr)obj)->nsinfos, &output, "errorReportsTo");
		doOutput(((RSSChannelPtr)obj)->nsinfos, &output, "generatorAgent");
		
		if(NULL != output) {
			addToHTMLBuffer(&buffer, TABLE_START);
			addToHTMLBuffer(&buffer, output);
			addToHTMLBuffer(&buffer, TABLE_END);
			g_free(output);
		}
	}
	return buffer;
}

RSSNsHandler *ns_admin_getRSSNsHandler(void) {
	RSSNsHandler 	*nsh;
	
	if(NULL != (nsh = (RSSNsHandler *)g_malloc(sizeof(RSSNsHandler)))) {
		nsh->parseChannelTag		= parseChannelTag;;
		nsh->parseItemTag		= NULL;
		nsh->doChannelHeaderOutput	= NULL;
		nsh->doChannelFooterOutput	= doChannelOutput;
		nsh->doItemHeaderOutput		= NULL;
		nsh->doItemFooterOutput		= NULL;
	}
	return nsh;
}
