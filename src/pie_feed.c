/*
   PIE channel parsing
      
   Note: the PIE parsing is copy & paste & some changes of the RSS
   code...
   
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

#include <sys/time.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include "conf.h"
#include "support.h"
#include "common.h"
#include "pie_feed.h"

#include "pie_ns.h"
#include "ns_dc.h"

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

extern GMutex * entries_lock;	// FIXME
extern GHashTable *entries;	// FIXME

/* to store the PIENsHandler structs for all supported RDF namespace handlers */
GHashTable	*pie_nslist = NULL;

/* note: the tag order has to correspond with the PIE_FEED_* defines in the header file */
static gchar *feedTagList[] = {	"title",
					"tagline",
					"link",
					"copyright",
					"language",
					"generator",
					"lastBuildDate",
					"modified",			
					NULL
				  };

/* prototypes */
void		setPIEFeedProp(gpointer fp, gint proptype, gpointer data);
gpointer 	getPIEFeedProp(gpointer fp, gint proptype);
gpointer	mergePIEFeed(gpointer old_cp, gpointer new_cp);
gpointer 	loadPIEFeed(gchar *keyprefix, gchar *key);
gpointer 	readPIEFeed(gchar *url);
void		showPIEFeedInfo(gpointer cp);

feedHandlerPtr initPIEFeedHandler(void) {
	feedHandlerPtr	fhp;
	
	if(NULL == (fhp = (feedHandlerPtr)g_malloc(sizeof(struct feedHandler)))) {
		g_error(_("not enough memory!"));
	}
	memset(fhp, 0, sizeof(struct feedHandler));
	
	g_free(pie_nslist);
	pie_nslist = g_hash_table_new(g_str_hash, g_str_equal);
	
	/* register PIE name space handlers, not sure which namespaces beside DC are common */
	if(getNameSpaceStatus(ns_dc_getPIENsPrefix()))
		g_hash_table_insert(pie_nslist, (gpointer)ns_dc_getPIENsPrefix(),
					        (gpointer)ns_dc_getPIENsHandler());

	/* prepare feed handler structure */
	fhp->loadFeed		= loadPIEFeed;
	fhp->readFeed		= readPIEFeed;
	fhp->mergeFeed		= mergePIEFeed;
	fhp->removeFeed		= NULL; // FIXME
	fhp->getFeedProp	= getPIEFeedProp;	
	fhp->setFeedProp	= setPIEFeedProp;
	fhp->showFeedInfo	= showPIEFeedInfo;
	
	return fhp;
}

/* nonstatic because used by pie_entry.c too */
gchar * parseAuthor(xmlDocPtr doc, xmlNodePtr cur) {
	gchar	*tmp = NULL;
	gchar	*tmp2;

	if((NULL == cur) || (NULL == doc)) {
		g_warning(_("internal error: XML document pointer NULL! This should not happen!\n"));
		return NULL;
	}

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if (!xmlStrcmp(cur->name, (const xmlChar *)"name"))
			tmp = g_strdup(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1));

		if (!xmlStrcmp(cur->name, (const xmlChar *)"email")) {
			tmp2 = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			tmp2 = g_strdup_printf("%s <a href=\"mailto:%s\">%s</a>", tmp, tmp2, tmp2);
			g_free(tmp);
			tmp = tmp2;
		}
					
		if (!xmlStrcmp(cur->name, (const xmlChar *)"url")) {
			tmp2 = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			tmp2 = g_strdup_printf("%s (<a href=\"%s\">Website</a>)", tmp, tmp2);
			g_free(tmp);
			tmp = tmp2;
		}
		cur = cur->next;
	}

	return tmp;
}

/* loads a saved PIE feed from disk */
gpointer loadPIEFeed(gchar *keyprefix, gchar *key) {
	PIEFeedPtr	new_cp = NULL;

	// workaround as long loading is not implemented
	if(NULL == (new_cp = (PIEFeedPtr) malloc(sizeof(struct PIEFeed)))) {
		g_error("not enough memory!\n");
		return NULL;
	}

	memset(new_cp, 0, sizeof(struct PIEFeed));
	new_cp->updateInterval = -1;
	new_cp->updateCounter = 0;	/* to enforce immediate reload */
	new_cp->type = FST_PIE;
	
	return (gpointer)new_cp;
}

/* reads a PIE feed URL and returns a new channel structure (even if
   the feed could not be read) */
gpointer readPIEFeed(gchar *url) {
	xmlDocPtr 		doc;
	xmlNodePtr 		cur;
	PIEEntryPtr 		ip;
	PIEFeedPtr 		cp;
	gchar			*tmp2, *tmp = NULL;
	gchar			*encoding;
	parseFeedTagFunc	fp;
	PIENsHandler		*nsh;
	int			i;
	int 			error = 0;
	
	/* initialize channel structure */
	if(NULL == (cp = (PIEFeedPtr) malloc(sizeof(struct PIEFeed)))) {
		g_error("not enough memory!\n");
		return NULL;
	}
	memset(cp, 0, sizeof(struct PIEFeed));
	cp->nsinfos = g_hash_table_new(g_str_hash, g_str_equal);		
	
	cp->updateInterval = -1;
	cp->updateCounter = -1;
	cp->key = NULL;	
	cp->items = NULL;
	cp->available = FALSE;
	cp->source = g_strdup(url);
	cp->type = FST_PIE;
	
	while(1) {
		print_status(g_strdup_printf(_("reading from %s"),url));
		
		doc = xmlParseFile(url);

		if(NULL == doc) {
			print_status(g_strdup_printf(_("XML error wile reading feed! Feed \"%s\" could not be loaded!"), url));
			error = 1;
			break;
		}

		cur = xmlDocGetRootElement(doc);

		if(NULL == cur) {
			print_status(_("Empty document! Feed was not added!"));
			xmlFreeDoc(doc);
			error = 1;
			break;			
		}

		if(xmlStrcmp(cur->name, (const xmlChar *)"feed")) {
			print_status(_("Could not find PIE header! Feed was not added!"));
			xmlFreeDoc(doc);
			error = 1;
			break;			
		}

		time(&(cp->time));
		cp->encoding = g_strdup(doc->encoding);
		cp->available = TRUE;

		/* parse feed contents */
		cur = cur->xmlChildrenNode;
		while (cur != NULL) {
			/* parse feed author */
			if ((!xmlStrcmp(cur->name, (const xmlChar *) "author"))) {
				g_free(cp->author);
				cp->author = parseAuthor(doc, cur);
				cur = cur->next;		
				continue;
			}
			
			/* parse feed contributors */
			if ((!xmlStrcmp(cur->name, (const xmlChar *) "contributor"))) {
				tmp = parseAuthor(doc, cur);				
				if(NULL != cp->contributors) {
					/* add another contributor */
					tmp2 = g_strdup_printf("%s<br>%s", cp->contributors, tmp);
					g_free(cp->contributors);
					g_free(tmp);
					tmp = tmp2;
				}
				cp->contributors = tmp;
				cur = cur->next;		
				continue;
			}

			/* check namespace and if we found one, do namespace parsing */
			if(NULL != cur->ns) {
				if (NULL != cur->ns->prefix) {
					if(NULL != (nsh = (PIENsHandler *)g_hash_table_lookup(pie_nslist, (gpointer)cur->ns->prefix))) {
						fp = nsh->parseChannelTag;
						if(NULL != fp)
							(*fp)(cp, doc, cur);
						cur = cur->next;
						continue;
					} else {
							g_print("unsupported namespace \"%s\"\n", cur->ns->prefix);
					}
				}
			}

			/* now check for simple tags like <tagline>,<title>... */
			/* check for PIE tags */
			for(i = 0; i < PIE_FEED_MAX_TAG; i++) {
				g_assert(NULL != cur->name);
				if (!xmlStrcmp(cur->name, (const xmlChar *)feedTagList[i])) {
					tmp = cp->tags[i];
					if(NULL == (cp->tags[i] = g_strdup(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1)))) {
						cp->tags[i] = tmp;
					} else {
						g_free(tmp);
					}
				}		
			}

			/* collect PIE feed entries */
			if ((!xmlStrcmp(cur->name, (const xmlChar *) "entry"))) {
				if(NULL != (ip = (PIEEntryPtr)parseEntry(doc, cur))) {
					cp->unreadCounter++;
					ip->cp = cp;
					if(0 == ip->time)
						ip->time = cp->time;
					ip->next = NULL;
					cp->items = g_slist_append(cp->items, ip);
				}
			}
			cur = cur->next;
		}
		
		/* some postprocessing */
		if(NULL != cp->tags[PIE_FEED_TITLE]) 
			cp->tags[PIE_FEED_TITLE] = unhtmlize((gchar *)doc->encoding, cp->tags[PIE_FEED_TITLE]);

		if(NULL != cp->tags[PIE_FEED_DESCRIPTION])
			cp->tags[PIE_FEED_DESCRIPTION] = convertToHTML((gchar *)doc->encoding, cp->tags[PIE_FEED_DESCRIPTION]);		
			
		xmlFreeDoc(doc);
		break;
	}

	return cp;
}


/* used to merge two PIEFeedPtr structures after while
   updating a feed, returns a PIEFeedPtr to the merged
   structure and frees (FIXME) all unneeded memory */
gpointer mergePIEFeed(gpointer old_fp, gpointer new_fp) {
	PIEFeedPtr	new = (PIEFeedPtr) new_fp;
	PIEFeedPtr	old = (PIEFeedPtr) old_fp;
		
	// FIXME: compare items, merge appropriate
	// actually this function does almost nothing
	
	new->updateInterval = old->updateInterval;
	new->updateCounter = old->updateInterval;	/* resetting the counter */
	new->usertitle = old->usertitle;
	new->key = old->key;
	new->source = old->source;
	new->type = old->type;
	new->keyprefix = old->keyprefix;
	
	// FIXME: free old_cp memory
		
	return new_fp;
}

/* ---------------------------------------------------------------------------- */
/* HTML output stuff	 							*/
/* ---------------------------------------------------------------------------- */

/* method called by g_hash_table_foreach from inside the HTML
   generator functions to output namespace specific infos 
   
   not static because its reused by pie_entry.c */
void showPIEFeedNSInfo(gpointer key, gpointer value, gpointer userdata) {
	outputRequest	*request = (outputRequest *)userdata;
	PIENsHandler	*nsh = (PIENsHandler *)value;
	PIEOutputFunc	fp;

	switch(request->type) {
		case OUTPUT_PIE_FEED_NS_HEADER:
			fp = nsh->doChannelHeaderOutput;
			if(NULL != fp)
				(*fp)(request->obj);
			break;
		case OUTPUT_PIE_FEED_NS_FOOTER:
			fp = nsh->doChannelFooterOutput;
			if(NULL != fp)
				(*fp)(request->obj);
			break;
		case OUTPUT_ITEM_NS_HEADER:
			fp = nsh->doItemHeaderOutput;
			if(NULL != fp)
				(*fp)(request->obj);
			break;		
		case OUTPUT_ITEM_NS_FOOTER:
			fp = nsh->doItemFooterOutput;
			if(NULL != fp)
				(*fp)(request->obj);
			break;			
		default:	
			g_warning(_("Internal error! Invalid output request mode for namespace information!"));
			break;		
	}
}

/* writes PIE channel description as HTML into the gtkhtml widget */
void showPIEFeedInfo(gpointer fp) {
	PIEFeedPtr	cp = (PIEFeedPtr)fp;
	gchar		*feeddescription;
	gchar		*tmp;	
	outputRequest	request;

	g_assert(cp != NULL);	
	
	startHTMLOutput();
	writeHTML(HTML_HEAD_START);

	writeHTML(META_ENCODING1);
	writeHTML("UTF-8");
	writeHTML(META_ENCODING2);

	writeHTML(HTML_HEAD_END);

	writeHTML(FEED_HEAD_START);
	
	writeHTML(FEED_HEAD_CHANNEL);
	tmp = g_strdup_printf("<a href=\"%s\">%s</a>",
		cp->tags[PIE_FEED_LINK],
		getDefaultEntryTitle(cp->key));
	writeHTML(tmp);
	g_free(tmp);
	
	writeHTML(HTML_NEWLINE);	

	writeHTML(FEED_HEAD_SOURCE);
	tmp = g_strdup_printf("<a href=\"%s\">%s</a>", cp->source, cp->source);
	writeHTML(tmp);
	g_free(tmp);

	writeHTML(FEED_HEAD_END);	
		
	/* process namespace infos */
	request.obj = (gpointer)cp;
	request.type = OUTPUT_PIE_FEED_NS_HEADER;	
	if(NULL != pie_nslist)
		g_hash_table_foreach(pie_nslist, showPIEFeedNSInfo, (gpointer)&request);

	if(NULL != (feeddescription = cp->tags[PIE_FEED_DESCRIPTION]))
		writeHTML(feeddescription);

	writeHTML(FEED_FOOT_TABLE_START);
	FEED_FOOT_WRITE(doc, "author",			cp->author);
	FEED_FOOT_WRITE(doc, "contributors",		cp->contributors);
	FEED_FOOT_WRITE(doc, "copyright",		cp->tags[PIE_FEED_COPYRIGHT]);
	FEED_FOOT_WRITE(doc, "last modified",		cp->tags[PIE_FEED_PUBDATE]);
	writeHTML(FEED_FOOT_TABLE_END);
	
	/* process namespace infos */
	request.type = OUTPUT_PIE_FEED_NS_FOOTER;
	if(NULL != pie_nslist)
		g_hash_table_foreach(pie_nslist, showPIEFeedNSInfo, (gpointer)&request);

	finishHTMLOutput();
}

/* ---------------------------------------------------------------------------- */
/* just some encapsulation 							*/
/* ---------------------------------------------------------------------------- */

void setPIEFeedProp(gpointer fp, gint proptype, gpointer data) {
	PIEFeedPtr	c = (PIEFeedPtr)fp;
	
	if(NULL != c) {
		g_assert(FST_PIE == c->type);
		switch(proptype) {
			case FEED_PROP_TITLE:
				g_free(c->tags[PIE_FEED_TITLE]);
				c->tags[PIE_FEED_TITLE] = (gchar *)data;
				break;
			case FEED_PROP_USERTITLE:
				g_free(c->usertitle);
				c->usertitle = (gchar *)data;
				break;
			case FEED_PROP_SOURCE:
				g_free(c->source);
				c->source = (gchar *)data;
				break;
			case FEED_PROP_DFLTUPDINTERVAL:
			case FEED_PROP_UPDATEINTERVAL:
				c->updateInterval = (gint)data;
				break;
			case FEED_PROP_UPDATECOUNTER:
				c->updateCounter = (gint)data;
				break;
			case FEED_PROP_UNREADCOUNT:
				c->unreadCounter = (gint)data;
				break;
			case FEED_PROP_AVAILABLE:
				c->available = (gboolean)data;
				break;
			case FEED_PROP_ITEMLIST:
				g_error("please don't do this!");
				break;
			default:
				g_error(g_strdup_printf(_("intenal error! unknow feed property type %d!\n"), proptype));
				break;
		}
	}
}

gpointer getPIEFeedProp(gpointer fp, gint proptype) {
	PIEFeedPtr	c = (PIEFeedPtr)fp;

	if(NULL != c) {
		g_assert(FST_PIE == c->type);
		switch(proptype) {
			case FEED_PROP_TITLE:
				return (gpointer)c->tags[PIE_FEED_TITLE];
				break;
			case FEED_PROP_USERTITLE:
				return (gpointer)c->usertitle;
				break;
			case FEED_PROP_SOURCE:
				return (gpointer)c->source;
				break;
			case FEED_PROP_DFLTUPDINTERVAL:
			case FEED_PROP_UPDATEINTERVAL:
				return (gpointer)c->updateInterval;
				break;
			case FEED_PROP_UPDATECOUNTER:
				return (gpointer)c->updateCounter;
				break;
			case FEED_PROP_UNREADCOUNT:
				return (gpointer)c->unreadCounter;
				break;
			case FEED_PROP_AVAILABLE:
				return (gpointer)c->available;
				break;
			case FEED_PROP_ITEMLIST:
				return (gpointer)c->items;
				break;
			default:
				g_error(g_strdup_printf(_("intenal error! unknow feed property type %d!\n"), proptype));
				break;
		}
	} else {
		return NULL;
	}
}
