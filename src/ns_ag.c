/*
   mod_aggregation support
   
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
#include "ns_ag.h"
#include "common.h"

#define TABLE_START	"<div style=\"margin-top:15px;font-size:8pt;color:#C0C0C0\">administrative information</div><table style=\"width:100%;border-width:1px;border-top-style:solid;border-color:#D0D0D0;\">"
#define FIRSTTD		"<tr style=\"border-width:0;border-bottom-width:1px;border-style:dashed;border-color:#D0D0D0;\"><td width=\"30%\"><span style=\"font-size:8pt;color:#C0C0C0\">"
#define NEXTTD		"</span></td><td><span style=\"font-size:8pt;color:#C0C0C0\">"
#define LASTTD		"</span></td></tr>"
#define TABLE_END	"</table>"

static gchar ns_ag_prefix[] = "ag";

/* some prototypes */
void ns_ag_parseChannelTag(RSSChannelPtr cp, xmlDocPtr doc, xmlNodePtr cur);

gchar * ns_ag_doChannelOutput(gpointer obj);

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


static void ns_ag_addInfoStruct(GHashTable *nslist, gchar *tagname, gchar *tagvalue) {
	GHashTable	*nsvalues;
	
	g_assert(nslist != NULL);

	if(tagvalue == NULL)
		return;
			
	if(NULL == (nsvalues = (GHashTable *)g_hash_table_lookup(nslist, ns_ag_prefix))) {
		nsvalues = g_hash_table_new(g_str_hash, g_str_equal);
		g_hash_table_insert(nslist, (gpointer)ns_ag_prefix, (gpointer)nsvalues);
	}
	g_hash_table_insert(nsvalues, (gpointer)tagname, (gpointer)tagvalue);
}

void ns_ag_parseItemTag(RSSItemPtr ip, xmlDocPtr doc, xmlNodePtr cur) {
	gchar	*date, *tmp;
	
	if(!xmlStrcmp("source", cur->name)) 
		ns_ag_addInfoStruct(ip->nsinfos, "source", CONVERT(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1)));

	if(!xmlStrcmp("sourceURL", cur->name)) 
		ns_ag_addInfoStruct(ip->nsinfos, "sourceURL", CONVERT(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1)));
		
	if(!xmlStrcmp("timestamp", cur->name)) {
		tmp = CONVERT(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
		date = formatDate(convertDate(tmp));
		g_free(tmp);
		ns_ag_addInfoStruct(ip->nsinfos, "timestamp", date);
	}
}

gchar * ns_ag_doOutput(GHashTable *nsinfos) {
	GHashTable	*nsvalues;
	gchar		*buffer = NULL;
	gchar		*tmp, *tmp1, *tmp2, *tmp3;
	
	/* we print all channel infos as a (key,value) table */
	g_assert(NULL != nsinfos);
	if(NULL != (nsvalues = g_hash_table_lookup(nsinfos, (gpointer)ns_ag_prefix))) {
		addToHTMLBuffer(&buffer, TABLE_START);

		if(NULL != (tmp = (gchar *)g_hash_table_lookup(nsvalues, (gpointer)"source"))) {
			addToHTMLBuffer(&buffer, FIRSTTD);
			addToHTMLBuffer(&buffer, _("source"));
			addToHTMLBuffer(&buffer, NEXTTD);
			if(NULL != (tmp2 = (gchar *)g_hash_table_lookup(nsvalues, (gpointer)"source"))) {
				tmp3 = g_strdup_printf("<a href=\"%s\">%s</a>", tmp2, tmp);
				addToHTMLBuffer(&buffer, tmp3);
				g_free(tmp3);
			} else {
				addToHTMLBuffer(&buffer, tmp);
			}
			addToHTMLBuffer(&buffer, LASTTD);
		}

		if(NULL != (tmp = (gchar *)g_hash_table_lookup(nsvalues, (gpointer)"source"))) {
			addToHTMLBuffer(&buffer, FIRSTTD);
			addToHTMLBuffer(&buffer, _("time"));
			addToHTMLBuffer(&buffer, NEXTTD);
			addToHTMLBuffer(&buffer, tmp);
			addToHTMLBuffer(&buffer, LASTTD);
		}
				
		addToHTMLBuffer(&buffer, TABLE_END);
	}
	
	return buffer;
}

gchar * ns_ag_doItemOutput(gpointer obj) {

	if(NULL != obj)
		return ns_ag_doOutput(((RSSItemPtr)obj)->nsinfos);
	
	return NULL;
}

RSSNsHandler *ns_ag_getRSSNsHandler(void) {
	RSSNsHandler 	*nsh;
	
	if(NULL != (nsh = (RSSNsHandler *)g_malloc(sizeof(RSSNsHandler)))) {
		nsh->parseChannelTag		= NULL;
		nsh->parseItemTag		= ns_ag_parseItemTag;
		nsh->doChannelHeaderOutput	= NULL;
		nsh->doChannelFooterOutput	= NULL;
		nsh->doItemHeaderOutput		= NULL;
		nsh->doItemFooterOutput		= ns_ag_doItemOutput;
	}

	return nsh;
}
