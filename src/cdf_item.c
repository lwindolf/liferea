/*
   CDF item parsing 
      
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
#include "support.h"
#include "common.h"
#include "cdf_channel.h"
#include "cdf_item.h"

extern GHashTable *cdf_nslist;

static gchar *CDFItemTagList[] = {	"title",
					"abstract",
					"link",
					"author",					
					"logo",
					"category",
					NULL
				  };

/* method to parse standard tags for each item element */
CDFItemPtr parseCDFItem(xmlDocPtr doc, xmlNodePtr cur) {
	gchar		*tmp = NULL;
	gchar		*value;
	CDFItemPtr 	i = NULL;
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

	/* save the item link */
	value = xmlGetNoNsProp(cur, (const xmlChar *)"href");
	i->tags[CDF_ITEM_LINK] = g_strdup(value);
	i->read = FALSE;
	g_free(value);

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		/* save first link to a channel image */
		if ((!xmlStrcmp(cur->name, (const xmlChar *) "logo"))) {
			if(NULL != i->tags[CDF_ITEM_IMAGE]) {
				value = xmlGetNoNsProp(cur, (const xmlChar *)"href");
				if(NULL != value)
					i->tags[CDF_ITEM_IMAGE] = g_strdup(value);
				g_free(value);
			}
			cur = cur->next;			
			continue;
		}
		
		/* check for RDF tags */
		for(j = 0; j < CDF_ITEM_MAX_TAG; j++) {
			g_assert(NULL != cur->name);
			if (!xmlStrcmp(cur->name, (const xmlChar *)CDFItemTagList[j])) {
				tmp = i->tags[j];
				if(NULL == (i->tags[j] = g_strdup(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1)))) {
					i->tags[j] = tmp;
				} else {
					g_free(tmp);
				}
			}		
		}		

		cur = cur->next;
	}

	/* some postprocessing */
	if(NULL != i->tags[CDF_ITEM_TITLE])
		i->tags[CDF_ITEM_TITLE] = unhtmlize((gchar *)doc->encoding, i->tags[CDF_ITEM_TITLE]);
		
	if(NULL != i->tags[CDF_ITEM_DESCRIPTION])
		i->tags[CDF_ITEM_DESCRIPTION] = convertToHTML((gchar *)doc->encoding, i->tags[CDF_ITEM_DESCRIPTION]);	
		
	return(i);
}

/* just some encapsulation */

gboolean getCDFItemReadStatus(gpointer ip) { return ((CDFItemPtr)ip)->read; }

gchar * getCDFItemTag(gpointer ip, int tag) { return ((CDFItemPtr)ip)->tags[tag]; }

void markCDFItemAsRead(gpointer ip) {

	if(NULL != ip) {
		if(NULL != ((CDFItemPtr)ip)->cp)
			if(!((CDFItemPtr)ip)->read)
				((CDFChannelPtr)(((CDFItemPtr)ip)->cp))->unreadCounter--;
	
		((CDFItemPtr)ip)->read = TRUE;
	}	
}

