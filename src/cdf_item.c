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

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
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
gchar * getCDFItemTag(CDFItemPtr ip, int tag);
gpointer getCDFItemProp(gpointer fp, gint proptype);
void setCDFItemProp(gpointer fp, gint proptype, gpointer data);
void showCDFItem(gpointer ip);

itemHandlerPtr initCDFItemHandler(void) {
	itemHandlerPtr	ihp;
	
	if(NULL == (ihp = (itemHandlerPtr)g_malloc(sizeof(struct itemHandler)))) {
		g_error(_("not enough memory!"));
	}
	memset(ihp, 0, sizeof(struct itemHandler));
	
	/* CDF uses no namespaces */

	/* prepare item handler structure */
	ihp->getItemProp	= getCDFItemProp;	
	ihp->setItemProp	= setCDFItemProp;
	ihp->showItem		= showCDFItem;
	
	return ihp;
}

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

/* ---------------------------------------------------------------------------- */
/* HTML output stuff	 							*/
/* ---------------------------------------------------------------------------- */

/* writes CDF item description as HTML into the gtkhtml widget */
void showCDFItem(gpointer ip) {
	CDFChannelPtr	cp;
	gchar		*itemlink;
	gchar		*feedimage;
	gchar		*tmp;	
	
	g_assert(ip != NULL);
	cp = ((CDFItemPtr)ip)->cp;
	g_assert(cp != NULL);
		
	startHTMLOutput();
	writeHTML(HTML_START);
	writeHTML(HTML_HEAD_START);

	writeHTML(META_ENCODING1);
	writeHTML("UTF-8");
	writeHTML(META_ENCODING2);

	writeHTML(HTML_HEAD_END);

	if(NULL != (itemlink = getCDFItemTag(ip, CDF_ITEM_LINK))) {
		writeHTML(ITEM_HEAD_START);
		
		writeHTML(ITEM_HEAD_CHANNEL);
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", 
			cp->source,
			getDefaultEntryTitle(cp->key));
		writeHTML(tmp);
		g_free(tmp);
		
		writeHTML(HTML_NEWLINE);
		
		writeHTML(ITEM_HEAD_ITEM);
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", itemlink, 
			getCDFItemTag(ip, CDF_ITEM_TITLE));
		writeHTML(tmp);
		g_free(tmp);
		
		writeHTML(ITEM_HEAD_END);	
	}	

	if(NULL != (feedimage = getCDFItemTag(ip, CDF_ITEM_IMAGE))) {
		writeHTML(IMG_START);
		writeHTML(feedimage);
		writeHTML(IMG_END);	
	}

	if(NULL != getCDFItemTag(ip, CDF_ITEM_DESCRIPTION))
		writeHTML(getCDFItemTag(ip, CDF_ITEM_DESCRIPTION));

	writeHTML(HTML_END);
	finishHTMLOutput();
}

/* ---------------------------------------------------------------------------- */
/* just some encapsulation 							*/
/* ---------------------------------------------------------------------------- */

gchar * getCDFItemTag(CDFItemPtr ip, int tag) {

	if(NULL == ip)
		return NULL;
	
	g_assert(NULL != ip->cp);
	g_assert(FST_CDF == ((CDFChannelPtr)(ip->cp))->type);
	return ip->tags[tag];
}

void setCDFItemProp(gpointer ip, gint proptype, gpointer data) {
	CDFItemPtr	i = (CDFItemPtr)ip;
	
	if(NULL != i) {
		g_assert(NULL != i->cp);	
		g_assert(FST_CDF == ((CDFChannelPtr)(i->cp))->type);
		switch(proptype) {
			case ITEM_PROP_TITLE:
				g_free(i->tags[CDF_ITEM_TITLE]);
				i->tags[CDF_ITEM_TITLE] = (gchar *)data;
				break;
			case ITEM_PROP_READSTATUS:
				/* no matter what data was given... */
				if(FALSE == i->read) {
					((CDFChannelPtr)(i->cp))->unreadCounter--;
					i->read = TRUE;
				}
				break;
			case ITEM_PROP_DESCRIPTION:
			case ITEM_PROP_TIME:
			case ITEM_PROP_SOURCE:
			case ITEM_PROP_TYPE:
				g_error("please don't do this!");
				break;
			default:
				g_error(_("intenal error! unknow item property type!\n"));
				break;
		}
	}
}

gpointer getCDFItemProp(gpointer ip, gint proptype) {
	CDFItemPtr	i = (CDFItemPtr)ip;
	
	if(NULL != i) {
		g_assert(NULL != i->cp);	
		g_assert(FST_CDF == ((CDFChannelPtr)(i->cp))->type);
		switch(proptype) {
			case ITEM_PROP_TITLE:
				return (gpointer)getCDFItemTag(i, CDF_ITEM_TITLE);
				break;
			case ITEM_PROP_READSTATUS:
				return (gpointer)i->read;
				break;
			case ITEM_PROP_DESCRIPTION:
				return (gpointer)getCDFItemTag(i, CDF_ITEM_DESCRIPTION);
				break;
			case ITEM_PROP_TIME:
				return (gpointer)i->time;
				break;
			case ITEM_PROP_SOURCE:
				return (gpointer)getCDFItemTag(i, CDF_ITEM_LINK);
				break;
			case ITEM_PROP_TYPE:
				return (gpointer)FST_CDF;
				break;
			default:
				g_error(_("intenal error! unknow item property type!\n"));
				break;
		}
	} else {
		return NULL;
	}
}
