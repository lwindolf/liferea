/*
   PIE entry parsing 
      
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

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "support.h"
#include "common.h"
#include "pie_entry.h"
#include "pie_ns.h"
#include "htmlview.h"

/* structure for the hashtable callback which itself calls the 
   namespace output handler */
#define OUTPUT_PIE_FEED_NS_HEADER	0
#define	OUTPUT_PIE_FEED_NS_FOOTER	1
#define OUTPUT_ITEM_NS_HEADER		2
#define OUTPUT_ITEM_NS_FOOTER		3
typedef struct {
	gint		type;	
	gpointer	obj;	/* thats either a PIEFeedPtr or a PIEEntryPtr 
				   depending on the type value */
} outputRequest;

/* uses the same namespace handler as PIE_channel */
extern GHashTable *pie_nslist;

static gchar *entryTagList[] = {	"title",
					"description",
					"link",
					"copyright",
					"modified",
					NULL
				  };
				  
/* prototypes */
static gchar * getPIEEntryTag(PIEEntryPtr ip, int tag);
gpointer getPIEEntryProp(gpointer ip, gint proptype);
void setPIEEntryProp(gpointer ip, gint proptype, gpointer data);
void showPIEEntry(gpointer ip);

extern gchar * parseAuthor(xmlDocPtr doc, xmlNodePtr cur);
extern void showPIEFeedNSInfo(gpointer key, gpointer value, gpointer userdata);

itemHandlerPtr initPIEItemHandler(void) {
	itemHandlerPtr	ihp;
	
	if(NULL == (ihp = (itemHandlerPtr)g_malloc(sizeof(struct itemHandler)))) {
		g_error(_("not enough memory!"));
	}
	memset(ihp, 0, sizeof(struct itemHandler));
	
	/* the PIE/RDF item handling reuses the PIE/RDF channel
	   namespace handler */

	/* prepare item handler structure */
	ihp->getItemProp	= getPIEEntryProp;	
	ihp->setItemProp	= setPIEEntryProp;
	ihp->showItem		= showPIEEntry;
	
	return ihp;
}

/* method to parse standard tags for each item element */
PIEEntryPtr parseEntry(xmlDocPtr doc, xmlNodePtr cur) {
	gint			bw, br;
	gchar			*tmp2, *tmp = NULL;
	parseEntryTagFunc	fp;
	PIENsHandler		*nsh;	
	PIEEntryPtr 		i = NULL;
	int			j;
	gboolean		summary = FALSE;

	if((NULL == cur) || (NULL == doc)) {
		g_warning(_("internal error: XML document pointer NULL! This should not happen!\n"));
		return NULL;
	}
		
	if(NULL == (i = (PIEEntryPtr) malloc(sizeof(struct PIEEntry)))) {
		g_error("not enough memory!\n");
		return(NULL);
	}
	memset(i, 0, sizeof(struct PIEEntry));
	i->nsinfos = g_hash_table_new(g_str_hash, g_str_equal);

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		/* check namespace of this tag */
		if(NULL != cur->ns) {		
			if (NULL != cur->ns->prefix) {
				if(NULL != (nsh = (PIENsHandler *)g_hash_table_lookup(pie_nslist, (gpointer)cur->ns->prefix))) {	
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
		
		// FIXME: is <modified> or <issued> or <created> the time tag we want to display?
		if(!xmlStrcmp(cur->name, "modified")) {
			tmp = g_strdup(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1));
			i->time = convertDate(tmp);
			cur = cur->next;		
			continue;
		}
		
		/* parse feed author */
		if ((!xmlStrcmp(cur->name, (const xmlChar *) "author"))) {
			g_free(i->author);
			i->author = parseAuthor(doc, cur);
			cur = cur->next;		
			continue;
		}

		/* parse feed contributors */
		if ((!xmlStrcmp(cur->name, (const xmlChar *) "contributor"))) {
			tmp = parseAuthor(doc, cur);				
			if(NULL != i->contributors) {
				/* add another contributor */
				tmp2 = g_strdup_printf("%s<br>%s", i->contributors, tmp);
				g_free(i->contributors);
				g_free(tmp);
				tmp = tmp2;
			}
			i->contributors = tmp;
			cur = cur->next;		
			continue;
		}
		
		/* <content> support, we simple take the first content tag 
		   found.... FIXME: select HTML-<content> tag if available
		   and support multipart tags!!!! */
		if(!xmlStrcmp(cur->name, "content")) {
			if((NULL == i->tags[PIE_ENTRY_DESCRIPTION]) || (TRUE == summary)) {
				g_free(i->tags[PIE_ENTRY_DESCRIPTION]);
				summary = FALSE;
				i->tags[PIE_ENTRY_DESCRIPTION] = extractHTMLNode(cur);
			}
			cur = cur->next;		
			continue;
		}
				
		/* <summary> can be used for short text descriptions, if there is no
		   description we show the <summary> content */
		if(!xmlStrcmp(cur->name, "summary")) {
			if(NULL == i->tags[PIE_ENTRY_DESCRIPTION]) {
				summary = TRUE;
				i->tags[PIE_ENTRY_DESCRIPTION] = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			}
			cur = cur->next;		
			continue;
		}
				
		/* check for PIE tags */
		for(j = 0; j < PIE_ENTRY_MAX_TAG; j++) {
			g_assert(NULL != cur->name);
			if (!xmlStrcmp(cur->name, (const xmlChar *)entryTagList[j])) {
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
	if(NULL != i->tags[PIE_ENTRY_TITLE])
		i->tags[PIE_ENTRY_TITLE] = unhtmlize((gchar *)doc->encoding, i->tags[PIE_ENTRY_TITLE]);
		
	if(NULL != i->tags[PIE_ENTRY_DESCRIPTION])
		i->tags[PIE_ENTRY_DESCRIPTION] = convertToHTML((gchar *)doc->encoding, i->tags[PIE_ENTRY_DESCRIPTION]);	
		
	return(i);
}

/* ---------------------------------------------------------------------------- */
/* HTML output stuff	 							*/
/* ---------------------------------------------------------------------------- */

extern void showFeedNSInfo(gpointer key, gpointer value, gpointer userdata);

/* writes item description as HTML into the gtkhtml widget */
void showPIEEntry(gpointer i) {
	PIEEntryPtr	ip = (PIEEntryPtr)i;
	PIEFeedPtr	cp;
	gchar		*itemlink;
	gchar		*tmp;	
	outputRequest	request;

	g_assert(NULL != ip);	
	cp = ((PIEEntryPtr)ip)->cp;
	g_assert(NULL != cp);

	startHTMLOutput();
	writeHTML(HTML_HEAD_START);

	writeHTML(META_ENCODING1);
	writeHTML("UTF-8");
	writeHTML(META_ENCODING2);

	writeHTML(HTML_HEAD_END);

	if(NULL != (itemlink = getPIEEntryTag((PIEEntryPtr)ip, PIE_ENTRY_LINK))) {
		writeHTML(ITEM_HEAD_START);
		
		writeHTML(ITEM_HEAD_CHANNEL);
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", 
			cp->tags[PIE_FEED_LINK],
			getDefaultEntryTitle(cp->key));
		writeHTML(tmp);
		g_free(tmp);
		
		writeHTML(HTML_NEWLINE);
		
		writeHTML(ITEM_HEAD_ITEM);
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", itemlink, ip->tags[PIE_ENTRY_TITLE]);
		writeHTML(tmp);
		g_free(tmp);
		
		writeHTML(ITEM_HEAD_END);	
	}	

	/* process namespace infos */
	request.obj = ip;
	request.type = OUTPUT_ITEM_NS_HEADER;	
	if(NULL != pie_nslist)
		g_hash_table_foreach(pie_nslist, showPIEFeedNSInfo, (gpointer)&request);

	if(NULL != ip->tags[PIE_ENTRY_DESCRIPTION])
		writeHTML(ip->tags[PIE_ENTRY_DESCRIPTION]);

	writeHTML(FEED_FOOT_TABLE_START);
	FEED_FOOT_WRITE(doc, "author",			ip->author);
	FEED_FOOT_WRITE(doc, "contributors",		ip->contributors);
	FEED_FOOT_WRITE(doc, "copyright",		ip->tags[PIE_ENTRY_COPYRIGHT]);
	FEED_FOOT_WRITE(doc, "last modified",		ip->tags[PIE_ENTRY_PUBDATE]);
	writeHTML(FEED_FOOT_TABLE_END);

	request.type = OUTPUT_ITEM_NS_FOOTER;
	if(NULL != pie_nslist)
		g_hash_table_foreach(pie_nslist, showPIEFeedNSInfo, (gpointer)&request);


	finishHTMLOutput();
}

/* ---------------------------------------------------------------------------- */
/* just some encapsulation 							*/
/* ---------------------------------------------------------------------------- */

static gchar * getPIEEntryTag(PIEEntryPtr ip, int tag) {

	if(NULL == ip)
		return NULL;
	
	g_assert(NULL != ip->cp);
	g_assert(FST_PIE == ((PIEFeedPtr)(ip->cp))->type);
	return ip->tags[tag];
}

void setPIEEntryProp(gpointer ip, gint proptype, gpointer data) {
	PIEEntryPtr	i = (PIEEntryPtr)ip;
	
	if(NULL != i) {
		g_assert(NULL != i->cp);	
		g_assert(FST_PIE == ((PIEFeedPtr)(i->cp))->type);
		switch(proptype) {
			case ITEM_PROP_TITLE:
				g_free(i->tags[PIE_ENTRY_TITLE]);
				i->tags[PIE_ENTRY_TITLE] = (gchar *)data;
				break;
			case ITEM_PROP_READSTATUS:
				/* no matter what data was given... */
				if(FALSE == i->read) {
					((PIEFeedPtr)(i->cp))->unreadCounter--;
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

gpointer getPIEEntryProp(gpointer ip, gint proptype) {
	PIEEntryPtr	i = (PIEEntryPtr)ip;
	
	if(NULL != i) {
		g_assert(NULL != i->cp);	
		g_assert(FST_PIE == ((PIEFeedPtr)(i->cp))->type);
		switch(proptype) {
			case ITEM_PROP_TITLE:
				return (gpointer)getPIEEntryTag(i, PIE_ENTRY_TITLE);
				break;
			case ITEM_PROP_READSTATUS:
				return (gpointer)i->read;
				break;
			case ITEM_PROP_DESCRIPTION:
				return (gpointer)getPIEEntryTag(i, PIE_ENTRY_DESCRIPTION);
				break;
			case ITEM_PROP_TIME:
				return (gpointer)i->time;
				break;
			case ITEM_PROP_SOURCE:
				return (gpointer)getPIEEntryTag(i, PIE_ENTRY_LINK);
				break;
			case ITEM_PROP_TYPE:
				return (gpointer)FST_PIE;
				break;
			default:
				g_error(_("intenal error! unknow item property type!\n"));
				break;
		}
	} else {
		return NULL;
	}
}
