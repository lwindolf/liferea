/*
   mod_aggregation support
   
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

#include "support.h"
#include "ns_ag.h"
#include "common.h"

#define TABLE_START	"<div class=\"feedfoottitle\">aggregation information</div><table class=\"addfoot\">"
#define FIRSTTD		"<tr class=\"feedfoot\"><td class=\"feedfootname\"><span class=\"feedfootname\">"
#define NEXTTD		"</span></td><td class=\"feedfootvalue\"><span class=\"feedfootvalue\">"
#define LASTTD		"</span></td></tr>"
#define TABLE_END	"</table>"

static gchar ns_ag_prefix[] = "ag";

/* you can find an aggregation namespace spec at:
   http://web.resource.org/rss/1.0/modules/aggregation/
 
  taglist for mod_aggregation:
  --------------------------------
     source
     sourceURL
     timestamp
  --------------------------------
  
  both tags usually contains URIs which we simply display in the
  feed info view footer
*/

gchar * ns_ag_getRSSNsPrefix(void) { return ns_ag_prefix; }

static void parseItemTag(RSSItemPtr ip, xmlNodePtr cur) {
	gchar	*date, *tmp;
	
	tmp = CONVERT(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
	if(!xmlStrcmp("source", cur->name))  {
		/* do nothing */
	} else if(!xmlStrcmp("sourceURL", cur->name)) {
		/* do nothing */
	} else if(!xmlStrcmp("timestamp", cur->name)) {
		date = formatDate(parseISO8601Date(tmp));
		g_free(tmp);
		tmp = date;
	} else {
		g_free(tmp);
		tmp = NULL;
	}
	
	if(NULL != tmp)
		g_hash_table_insert(ip->nsinfos, g_strdup_printf("ag:%s", cur->name), tmp);
}

static gchar * doOutput(GHashTable *nsinfos) {
	gchar		*buffer = NULL;
	gchar		*source, *sourceURL, *timestamp;
	
	/* we print all channel infos as a (key,value) table */
	g_assert(NULL != nsinfos);
	source = g_hash_table_lookup(nsinfos, "ag:source");
	sourceURL = g_hash_table_lookup(nsinfos, "ag:sourceURL");
	timestamp = g_hash_table_lookup(nsinfos, "ag:timestamp");
	
	if((NULL != timestamp) || (NULL != sourceURL) || (NULL != source)) {
		addToHTMLBuffer(&buffer, TABLE_START);

		if((NULL != source) || (NULL != sourceURL)) {
			addToHTMLBuffer(&buffer, FIRSTTD);
			addToHTMLBuffer(&buffer, _("source"));
			addToHTMLBuffer(&buffer, NEXTTD);
			if(NULL != sourceURL) {
				addToHTMLBuffer(&buffer, "<a href=\"");
				addToHTMLBuffer(&buffer, sourceURL);
				addToHTMLBuffer(&buffer, "\">");
				if(NULL != source)
					addToHTMLBuffer(&buffer, source);
				else 
					addToHTMLBuffer(&buffer, sourceURL);
				addToHTMLBuffer(&buffer, "</a>");
			} else {
				addToHTMLBuffer(&buffer, source);				
			}
			addToHTMLBuffer(&buffer, LASTTD);
		}

		if(NULL != timestamp) {
			addToHTMLBuffer(&buffer, FIRSTTD);
			addToHTMLBuffer(&buffer, _("time"));
			addToHTMLBuffer(&buffer, NEXTTD);
			addToHTMLBuffer(&buffer, timestamp);
			addToHTMLBuffer(&buffer, LASTTD);
		}
				
		addToHTMLBuffer(&buffer, TABLE_END);
	}
	
	if(NULL != timestamp)
		g_hash_table_remove(nsinfos, "ag:timestamp");
	if(NULL != source)
		g_hash_table_remove(nsinfos, "ag:source");
	if(NULL != sourceURL)
		g_hash_table_remove(nsinfos, "ag:sourceURL");	
		
	return buffer;
}

static gchar * doItemOutput(gpointer obj) {

	if(NULL != obj)
		return doOutput(((RSSItemPtr)obj)->nsinfos);
	
	return NULL;
}

RSSNsHandler *ns_ag_getRSSNsHandler(void) {
	RSSNsHandler 	*nsh;
	
	nsh = g_new0(RSSNsHandler, 1);
	nsh->parseChannelTag		= NULL;
	nsh->parseItemTag		= parseItemTag;
	nsh->doChannelHeaderOutput	= NULL;
	nsh->doChannelFooterOutput	= NULL;
	nsh->doItemHeaderOutput		= NULL;
	nsh->doItemFooterOutput		= doItemOutput;

	return nsh;
}
