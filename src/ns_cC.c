/*
   creativeCommon RSS namespace support
   
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

#include "htmlview.h"
#include "support.h"
#include "ns_cC.h"
#include "common.h"

#define TABLE_START	"<div class=\"feedfoottitle\">creativeCommon license information</div><table class=\"addfoot\">"
#define FIRSTTD		"<tr class=\"feedfoot\"><td class=\"feedfootname\"><span class=\"feedfootname\">"
#define NEXTTD		"</span></td><td class=\"feedfootvalue\"><span class=\"feedfootvalue\">"
#define LASTTD		"</span></td></tr>"
#define TABLE_END	"</table>"

static gchar ns_cC_prefix[] = "creativeCommons";

/* you can find an creativeCommon namespace spec at:
   http://backend.userland.com/creativeCommonsRssModule
 
   there is only one tag which can appear inside
   channel and items:

    license

*/

gchar * ns_cC_getRSSNsPrefix(void) { return ns_cC_prefix; }

gchar * ns_cC_parseTag(xmlNodePtr cur) {
	gchar	*buffer = NULL;
	gchar	*tmp;
	
 	if(!xmlStrcmp("license", cur->name)) {
 		if(NULL != (tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1)))) {
			addToHTMLBuffer(&buffer, FIRSTTD);
			addToHTMLBuffer(&buffer, (gchar *)_("license"));
			addToHTMLBuffer(&buffer, NEXTTD);
			addToHTMLBuffer(&buffer, "<a href=\"");
			addToHTMLBuffer(&buffer, tmp);
			addToHTMLBuffer(&buffer, "\">");
			addToHTMLBuffer(&buffer, tmp);
			addToHTMLBuffer(&buffer, "</a>");	
			addToHTMLBuffer(&buffer, LASTTD);
			g_free(tmp);
 		}
 	}	
	return buffer;
}

void ns_cC_parseChannelTag(RSSChannelPtr cp, xmlNodePtr cur) {

	g_assert(NULL != cp->nsinfos);
	g_hash_table_insert(cp->nsinfos, g_strdup(ns_cC_prefix), ns_cC_parseTag(cur));
}

void ns_cC_parseItemTag(RSSItemPtr ip, xmlNodePtr cur) {

	g_assert(NULL != ip->nsinfos);
	g_hash_table_insert(ip->nsinfos, g_strdup(ns_cC_prefix), ns_cC_parseTag(cur));
}

gchar * ns_cC_doOutput(GHashTable *nsinfos) {
	gchar		*output;
	gchar		*buffer = NULL;
	
	/* we print all channel infos as a (key,value) table */
	g_assert(NULL != nsinfos);
	if(NULL != (output = g_hash_table_lookup(nsinfos, (gpointer)ns_cC_prefix))) {
		addToHTMLBuffer(&buffer, TABLE_START);
		addToHTMLBuffer(&buffer, output);
		addToHTMLBuffer(&buffer, TABLE_END);
		g_hash_table_remove(nsinfos, (gpointer)ns_cC_prefix);
	}	
	return buffer;
}

gchar * ns_cC_doChannelOutput(gpointer obj) {

	if(NULL != obj)
		return ns_cC_doOutput(((RSSChannelPtr)obj)->nsinfos);
	
	return NULL;
}

gchar * ns_cC_doItemOutput(gpointer obj) {

	if(NULL != obj)
		return ns_cC_doOutput(((RSSItemPtr)obj)->nsinfos);
	
	return NULL;
}

RSSNsHandler *ns_cC_getRSSNsHandler(void) {
	RSSNsHandler 	*nsh;
	
	nsh = g_new0(RSSNsHandler, 1);
	nsh->parseChannelTag		= ns_cC_parseChannelTag;
	nsh->parseItemTag		= ns_cC_parseItemTag;
	nsh->doChannelHeaderOutput	= NULL;
	nsh->doChannelFooterOutput	= ns_cC_doChannelOutput;
	nsh->doItemHeaderOutput		= NULL;
	nsh->doItemFooterOutput		= ns_cC_doItemOutput;

	return nsh;
}

