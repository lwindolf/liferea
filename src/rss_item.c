/*
   RSS item parsing 
      
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
#include "rss_item.h"
#include "rss_ns.h"

extern GHashTable *rss_nslist;

static gchar *itemTagList[] = {		"title",
					"description",
					"link",
					"author",
					"comments",
					"enclosure",
					"category",
					NULL
				  };

/* method to parse standard tags for each item element */
itemPtr parseItem(xmlDocPtr doc, xmlNodePtr cur) {
	gint			bw, br;
	gchar			*tmp = NULL;
	parseItemTagFunc	fp;
	RSSNsHandler		*nsh;	
	itemPtr 		i = NULL;
	int			j;

	if((NULL == cur) || (NULL == doc)) {
		g_warning(_("internal error: XML document pointer NULL! This should not happen!\n"));
		return NULL;
	}
		
	if(NULL == (i = (itemPtr) malloc(sizeof(struct item)))) {
		g_error("not enough memory!\n");
		return(NULL);
	}
	memset(i, 0, sizeof(struct item));
	i->nsinfos = g_hash_table_new(g_str_hash, g_str_equal);

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		/* check namespace of this tag */
		if(NULL != cur->ns) {		
			if (NULL != cur->ns->prefix) {
				if(NULL != (nsh = (RSSNsHandler *)g_hash_table_lookup(rss_nslist, (gpointer)cur->ns->prefix))) {	
					fp = nsh->parseItemTag;
					if(NULL != fp)
						(*fp)(i, doc, cur);
					cur = cur->next;
					continue;						
				} else {
					g_print("unsupported namespace \"%s\"\n", cur->ns->prefix);
				}
			}
		}
		
		/* check for RDF tags */
		for(j = 0; j < ITEM_MAX_TAG; j++) {
			g_assert(NULL != cur->name);
			if (!xmlStrcmp(cur->name, (const xmlChar *)itemTagList[j])) {
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
	if(NULL != i->tags[ITEM_TITLE])
		i->tags[ITEM_TITLE] = unhtmlize((gchar *)doc->encoding, i->tags[ITEM_TITLE]);
		
	if(NULL != i->tags[ITEM_DESCRIPTION])
		i->tags[ITEM_DESCRIPTION] = convertToHTML((gchar *)doc->encoding, i->tags[ITEM_DESCRIPTION]);	
		
	return(i);
}

/* just some encapsulation */

gboolean getRSSItemReadStatus(gpointer ip) { return ((itemPtr)ip)->read; }

gchar * getRSSItemTag(gpointer ip, int tag) { return ((itemPtr)ip)->tags[tag]; }

void markRSSItemAsRead(gpointer ip) {

	if(NULL != ip) {
		if(NULL != ((itemPtr)ip)->cp)
			if(!((itemPtr)ip)->read)
				((channelPtr)(((itemPtr)ip)->cp))->unreadCounter--;
	
		((itemPtr)ip)->read = TRUE;
	}	
}

