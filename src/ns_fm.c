/*
   freshmeat namespace support
   
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

#include <string.h>
#include "ns_fm.h"
#include "common.h"

#define FM_IMG_START	"<br><img class=\"freshmeat\" src=\""
#define FM_IMG_END	" \">"

static gchar ns_fm_prefix[] = "fm";

/* you can find the fm DTD under http://freshmeat.net/backend/fm-releases-0.1.dtd

  it defines a lot of entities and one tag "screenshot_url", which we
  output as a HTML image in the item view footer
*/

gchar * ns_fm_getRSSNsPrefix(void) { return ns_fm_prefix; }

static void parseItemTag(RSSItemPtr ip, xmlNodePtr cur) {
	xmlChar *string;
	gchar	*tmp;
	
	if(!xmlStrcmp("screenshot_url", cur->name)) {
 		string = xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1);
 		tmp = CONVERT(string);	
		if(NULL != tmp) {
			if(strlen(tmp) > 0) {
				g_hash_table_insert(ip->nsinfos, g_strdup("fm:screenshot_url"), tmp);
			} else {
				g_free(tmp);
			}
		}
		
		if(NULL != string)
 			xmlFree(string);
	}
}

static gchar * doOutput(GHashTable *nsinfos) {
	gchar	*buffer = NULL;
	gchar	*value;
	
	g_assert(NULL != nsinfos);
	/* we print all channel infos as a (key,value) table */
	if(NULL != (value = g_hash_table_lookup(nsinfos, "fm:screenshot_url"))) {
		addToHTMLBuffer(&buffer, FM_IMG_START);
		addToHTMLBuffer(&buffer, (gchar *)value);
		addToHTMLBuffer(&buffer, FM_IMG_END);
		g_hash_table_remove(nsinfos, "fm:screenshot_url");
	}	
	return buffer;
}

static gchar * doItemOutput(gpointer obj) {

	if(NULL != obj)
		return doOutput(((RSSItemPtr)obj)->nsinfos);
		
	return NULL;
}

RSSNsHandler *ns_fm_getRSSNsHandler(void) {
	RSSNsHandler 	*nsh;
	
	if(NULL != (nsh = (RSSNsHandler *)g_malloc(sizeof(RSSNsHandler)))) {
		nsh->parseChannelTag		= NULL;
		nsh->parseItemTag		= parseItemTag;
		nsh->doChannelHeaderOutput	= NULL;
		nsh->doChannelFooterOutput	= NULL;
		nsh->doItemHeaderOutput		= NULL;
		nsh->doItemFooterOutput		= doItemOutput;
	}

	return nsh;
}
