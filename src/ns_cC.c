/*
   creativeCommon RSS namespace support
   
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

#include "htmlview.h"
#include "support.h"
#include "ns_cC.h"
#include "common.h"

#define TABLE_START	"<div style=\"margin-top:15px;font-size:8pt;color:#C0C0C0\">creativeCommon license information</div><table style=\"width:100%;border-width:1px;border-top-style:solid;border-color:#D0D0D0;\">"
#define FIRSTTD		"<tr style=\"border-width:0;border-bottom-width:1px;border-style:dashed;border-color:#D0D0D0;\"><td width=\"30%\"><span style=\"font-size:8pt;color:#C0C0C0\">"
#define NEXTTD		"</span></td><td><span style=\"font-size:8pt;color:#C0C0C0\">"
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

static void ns_cC_addInfoStruct(GHashTable *nslist, gchar *tagname, gchar *tagvalue) {
	GHashTable	*nsvalues;
	
	g_assert(nslist != NULL);

	if(tagvalue == NULL)
		return;
			
	if(NULL == (nsvalues = (GHashTable *)g_hash_table_lookup(nslist, ns_cC_prefix))) {
		nsvalues = g_hash_table_new(g_str_hash, g_str_equal);
		g_hash_table_insert(nslist, (gpointer)ns_cC_prefix, (gpointer)nsvalues);
	}
	g_hash_table_insert(nsvalues, (gpointer)tagname, (gpointer)tagvalue);
}

void ns_cC_parseChannelTag(RSSChannelPtr cp, xmlDocPtr doc, xmlNodePtr cur) {
	
	if(!xmlStrcmp("license", cur->name)) 
		ns_cC_addInfoStruct(cp->nsinfos, "license", CONVERT(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1)));
}

void ns_cC_parseItemTag(RSSItemPtr ip, xmlDocPtr doc, xmlNodePtr cur) {
	
	if(!xmlStrcmp("license", cur->name)) 
		ns_cC_addInfoStruct(ip->nsinfos, "license", CONVERT(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1)));
}

void ns_cC_output(gpointer key, gpointer value, gpointer userdata) {
	gchar 	**buffer = (gchar **)userdata;
	
	if(!strcmp(key, "license")) {
		addToHTMLBuffer(buffer, FIRSTTD);
		addToHTMLBuffer(buffer, (gchar *)_("license"));
		addToHTMLBuffer(buffer, NEXTTD);
		addToHTMLBuffer(buffer, "<a href=\"");
		addToHTMLBuffer(buffer, value);
		addToHTMLBuffer(buffer, "\">");
		addToHTMLBuffer(buffer, value);
		addToHTMLBuffer(buffer, "</a>");	
		addToHTMLBuffer(buffer, LASTTD);		
	}
}

gchar * ns_cC_doOutput(GHashTable *nsinfos) {
	GHashTable	*nsvalues;
	gchar		*buffer = NULL;
	
	/* we print all channel infos as a (key,value) table */
	g_assert(NULL != nsinfos);
	if(NULL != (nsvalues = g_hash_table_lookup(nsinfos, (gpointer)ns_cC_prefix))) {
		addToHTMLBuffer(&buffer, TABLE_START);
		g_hash_table_foreach(nsvalues, ns_cC_output, (gpointer)&buffer);
		addToHTMLBuffer(&buffer, TABLE_END);
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
	
	if(NULL != (nsh = (RSSNsHandler *)g_malloc(sizeof(RSSNsHandler)))) {
		nsh->parseChannelTag		= ns_cC_parseChannelTag;
		nsh->parseItemTag		= ns_cC_parseItemTag;
		nsh->doChannelHeaderOutput	= NULL;
		nsh->doChannelFooterOutput	= ns_cC_doChannelOutput;
		nsh->doItemHeaderOutput		= NULL;
		nsh->doItemFooterOutput		= ns_cC_doItemOutput;
	}

	return nsh;
}

