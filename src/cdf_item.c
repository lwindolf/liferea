/*
   CDF item parsing 
      
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

#include <string.h>

#include "support.h"
#include "common.h"
#include "cdf_channel.h"
#include "cdf_item.h"
#include "htmlview.h"

extern GHashTable *cdf_nslist;

static gchar *CDFItemTagList[] = {	"title",
					"abstract",
					"link",
					"author",					
					"logo",
					"category",
					NULL
				  };

/* prototypes */
gchar * showCDFItem(feedPtr fp, CDFChannelPtr cp, CDFItemPtr ip);

/* method to parse standard tags for each item element */
itemPtr parseCDFItem(feedPtr fp, CDFChannelPtr cp, xmlDocPtr doc, xmlNodePtr cur) {
	gchar		*tmp = NULL;
	CDFItemPtr 	i;
	itemPtr		ip;
	int		j;

	if((NULL == cur) || (NULL == doc)) {
		g_warning(_("internal error: XML document pointer NULL! This should not happen!\n"));
		return NULL;
	}
		
	if(NULL == (i = (CDFItemPtr) malloc(sizeof(struct CDFItem)))) {
		g_error("not enough memory!\n");
		return(NULL);
	}
	memset(i, 0, sizeof(struct CDFItem));
	i->nsinfos = g_hash_table_new(g_str_hash, g_str_equal);
	ip = getNewItemStruct();
	
	/* save the item link */
	i->tags[CDF_ITEM_LINK] = CONVERT(xmlGetNoNsProp(cur, (const xmlChar *)"href"));

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		/* save first link to a channel image */
		if ((!xmlStrcmp(cur->name, (const xmlChar *) "logo"))) {
			if(NULL != i->tags[CDF_ITEM_IMAGE])
				i->tags[CDF_ITEM_IMAGE] = CONVERT(xmlGetNoNsProp(cur, (const xmlChar *)"href"));
			cur = cur->next;			
			continue;
		}
		
		/* check for RDF tags */
		for(j = 0; j < CDF_ITEM_MAX_TAG; j++) {
			g_assert(NULL != cur->name);
			if (!xmlStrcmp(cur->name, (const xmlChar *)CDFItemTagList[j])) {
				tmp = i->tags[j];
				if(NULL == (i->tags[j] = CONVERT(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1)))) {
					i->tags[j] = tmp;
				} else {
					g_free(tmp);
				}
			}		
		}		

		cur = cur->next;
	}

	/* after parsing we fill the infos into the itemPtr structure */
	ip->type = FST_CDF;
	ip->time = i->time;
	ip->source = i->tags[CDF_ITEM_LINK];
	ip->readStatus = FALSE;
	ip->id = NULL;

	/* some postprocessing */
	if(NULL != i->tags[CDF_ITEM_TITLE])
		i->tags[CDF_ITEM_TITLE] = unhtmlize(i->tags[CDF_ITEM_TITLE]);
		
	if(NULL != i->tags[CDF_ITEM_DESCRIPTION])
		i->tags[CDF_ITEM_DESCRIPTION] = convertToHTML(i->tags[CDF_ITEM_DESCRIPTION]);	
		
	ip->title = i->tags[CDF_ITEM_TITLE];		
	ip->description = showCDFItem(fp, cp, i);

	g_free(i->nsinfos);
	g_free(i);
	return ip;
}

/* ---------------------------------------------------------------------------- */
/* HTML output stuff	 							*/
/* ---------------------------------------------------------------------------- */

/* writes CDF item description as HTML into the gtkhtml widget */
gchar * showCDFItem(feedPtr fp, CDFChannelPtr cp, CDFItemPtr ip) {
	gchar		*buffer = NULL;
	gchar		*tmp;
	
	g_assert(ip != NULL);
	g_assert(cp != NULL);
	g_assert(fp != NULL);

	if(NULL != ip->tags[CDF_ITEM_LINK]) {
		addToHTMLBuffer(&buffer, ITEM_HEAD_START);		
		addToHTMLBuffer(&buffer, ITEM_HEAD_CHANNEL);
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", 
			fp->source,
			cp->tags[CDF_CHANNEL_TITLE]);
		addToHTMLBuffer(&buffer, tmp);
		g_free(tmp);
		
		addToHTMLBuffer(&buffer, HTML_NEWLINE);		
		addToHTMLBuffer(&buffer, ITEM_HEAD_ITEM);
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", ip->tags[CDF_ITEM_LINK], 
			ip->tags[CDF_ITEM_TITLE]);
		addToHTMLBuffer(&buffer, tmp);
		g_free(tmp);
		
		addToHTMLBuffer(&buffer, ITEM_HEAD_END);	
	}	

	if(NULL != ip->tags[CDF_ITEM_IMAGE]) {
		addToHTMLBuffer(&buffer, IMG_START);
		addToHTMLBuffer(&buffer, ip->tags[CDF_ITEM_IMAGE]);
		addToHTMLBuffer(&buffer, IMG_END);	
	}

	if(NULL != ip->tags[CDF_ITEM_DESCRIPTION])
		addToHTMLBuffer(&buffer, ip->tags[CDF_ITEM_DESCRIPTION]);
	
	return buffer;
}
