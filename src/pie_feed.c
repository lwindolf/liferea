/*
   Atom/Echo/PIE 0.2/0.3 channel parsing
      
   Note: the PIE parsing is copy & paste & some changes of the RSS
   code...
   
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

#include <sys/time.h>
#include <string.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include "conf.h"
#include "support.h"
#include "common.h"
#include "feed.h"
#include "pie_feed.h"
#include "pie_ns.h"
#include "ns_dc.h"
#include "callbacks.h"

#include "netio.h"
#include "htmlview.h"

/* structure for the hashtable callback which itself calls the 
   namespace output handler */
#define OUTPUT_PIE_FEED_NS_HEADER	0
#define	OUTPUT_PIE_FEED_NS_FOOTER	1
#define OUTPUT_ITEM_NS_HEADER		2
#define OUTPUT_ITEM_NS_FOOTER		3
typedef struct {
	gint		type;
	gchar		**buffer;	/* pointer to output char buffer pointer */
	gpointer	obj;		/* thats either a PIEFeedPtr or a PIEEntryPtr 
					   depending on the type value */
} outputRequest;

/* to store the PIENsHandler structs for all supported RDF namespace handlers */
GHashTable	*pie_nslist = NULL;

/* note: the tag order has to correspond with the PIE_FEED_* defines in the header file */
static gchar *feedTagList[] = {	"title",
				"tagline",
				"link",
				"language",
				"copyright",
				"generator",
				"lastBuildDate",
				"modified",
				"issued",
				"created",
				NULL
			  };

/* ---------------------------------------------------------------------------- */
/* HTML output		 							*/
/* ---------------------------------------------------------------------------- */

/* method called by g_hash_table_foreach from inside the HTML
   generator functions to output namespace specific infos 
   
   not static because its reused by pie_entry.c */
void showPIEFeedNSInfo(gpointer key, gpointer value, gpointer userdata) {
	outputRequest	*request = (outputRequest *)userdata;
	PIENsHandler	*nsh = (PIENsHandler *)value;
	gchar		*tmp;
	PIEOutputFunc	fp;

	switch(request->type) {
		case OUTPUT_PIE_FEED_NS_HEADER:
			fp = nsh->doChannelHeaderOutput;
			break;
		case OUTPUT_PIE_FEED_NS_FOOTER:
			fp = nsh->doChannelFooterOutput;
			break;
		case OUTPUT_ITEM_NS_HEADER:
			fp = nsh->doItemHeaderOutput;
			break;		
		case OUTPUT_ITEM_NS_FOOTER:
			fp = nsh->doItemFooterOutput;
			break;			
		default:	
			g_warning(_("Internal error! Invalid output request mode for namespace information!"));
			return;
			break;		
	}
	
	if(NULL == fp)
		return;
		
	if(NULL == (tmp = (*fp)(request->obj)))
		return
		
	addToHTMLBuffer(request->buffer, tmp);
}

/* writes PIE channel description as HTML into the gtkhtml widget */
static gchar * showPIEFeedInfo(PIEFeedPtr cp, gchar *url) {
	gchar		*tmp, *buffer = NULL;	
	outputRequest	request;

	g_assert(cp != NULL);	

	addToHTMLBuffer(&buffer, FEED_HEAD_START);	
	addToHTMLBuffer(&buffer, FEED_HEAD_CHANNEL);
	
	tmp = g_strdup_printf("<a href=\"%s\">%s</a>",
		cp->tags[PIE_FEED_LINK],
		cp->tags[PIE_FEED_TITLE]);
	addToHTMLBuffer(&buffer, tmp);
	g_free(tmp);
	
	addToHTMLBuffer(&buffer, HTML_NEWLINE);
	addToHTMLBuffer(&buffer, FEED_HEAD_SOURCE);
	tmp = g_strdup_printf("<a href=\"%s\">%s</a>", url, url);
	addToHTMLBuffer(&buffer, tmp);
	g_free(tmp);
	addToHTMLBuffer(&buffer, FEED_HEAD_END);	
		
	/* process namespace infos */
	request.obj = (gpointer)cp;
	request.buffer = &buffer;
	request.type = OUTPUT_PIE_FEED_NS_HEADER;	
	if(NULL != pie_nslist)
		g_hash_table_foreach(pie_nslist, showPIEFeedNSInfo, (gpointer)&request);

	if(NULL != cp->tags[PIE_FEED_DESCRIPTION])
		addToHTMLBuffer(&buffer, cp->tags[PIE_FEED_DESCRIPTION]);

	addToHTMLBuffer(&buffer, FEED_FOOT_TABLE_START);
	FEED_FOOT_WRITE(buffer, "author",		cp->author);
	FEED_FOOT_WRITE(buffer, "contributors",		cp->contributors);
	FEED_FOOT_WRITE(buffer, "language",		cp->tags[PIE_FEED_LANGUAGE]);
	FEED_FOOT_WRITE(buffer, "copyright",		cp->tags[PIE_FEED_COPYRIGHT]);
	FEED_FOOT_WRITE(buffer, "last build date",	cp->tags[PIE_FEED_LASTBUILDDATE]);
	FEED_FOOT_WRITE(buffer, "last modified",	cp->tags[PIE_FEED_MODIFIED]);
	FEED_FOOT_WRITE(buffer, "issued",		cp->tags[PIE_FEED_ISSUED]);
	FEED_FOOT_WRITE(buffer, "created",		cp->tags[PIE_FEED_CREATED]);
	addToHTMLBuffer(&buffer, FEED_FOOT_TABLE_END);
	
	/* process namespace infos */
	request.type = OUTPUT_PIE_FEED_NS_FOOTER;
	if(NULL != pie_nslist)
		g_hash_table_foreach(pie_nslist, showPIEFeedNSInfo, (gpointer)&request);
	
	return buffer;
}

/* ---------------------------------------------------------------------------- */
/* PIE parsing		 							*/
/* ---------------------------------------------------------------------------- */

/* nonstatic because used by pie_entry.c too */
gchar * parseAuthor(xmlNodePtr cur) {
	gchar	*tmp = NULL;
	gchar	*tmp2;

	g_assert(NULL != cur);
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if (!xmlStrcmp(cur->name, (const xmlChar *)"name"))
			tmp = CONVERT(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));

		if (!xmlStrcmp(cur->name, (const xmlChar *)"email")) {
			tmp2 = CONVERT(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			tmp2 = g_strdup_printf("%s <a href=\"mailto:%s\">%s</a>", tmp, tmp2, tmp2);
			g_free(tmp);
			tmp = tmp2;
		}
					
		if (!xmlStrcmp(cur->name, (const xmlChar *)"url")) {
			tmp2 = CONVERT(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			tmp2 = g_strdup_printf("%s (<a href=\"%s\">Website</a>)", tmp, tmp2);
			g_free(tmp);
			tmp = tmp2;
		}
		cur = cur->next;
	}

	return tmp;
}

/* reads a PIE feed URL and returns a new channel structure (even if
   the feed could not be read) */
static void readPIEFeed(feedPtr fp, gchar *data) {
	xmlDocPtr 		doc;
	xmlNodePtr 		cur;
	itemPtr 		ip;
	PIEFeedPtr 		cp;
	gchar			*tmp2, *tmp = NULL;
	parseFeedTagFunc	parseFunc;
	PIENsHandler		*nsh;
	int			i;
	int 			error = 0;
	
	/* initialize channel structure */
	cp = g_new0(struct PIEFeed, 1);
	cp->nsinfos = g_hash_table_new(g_str_hash, g_str_equal);		
	
	cp->updateInterval = -1;
	while(1) {
		if(NULL == (doc = parseBuffer(data, &(fp->parseErrors)))) {
			addToHTMLBuffer(&(fp->parseErrors), g_strdup_printf(_("<p>XML error while reading feed! Feed \"%s\" could not be loaded!</p>"), fp->source));
			error = 1;
			break;
		}

		cur = xmlDocGetRootElement(doc);

		if(NULL == cur) {
			addToHTMLBuffer(&(fp->parseErrors), _("<p>Empty document!</p>"));
			error = 1;
			break;			
		}

		if(xmlStrcmp(cur->name, (const xmlChar *)"feed")) {
			addToHTMLBuffer(&(fp->parseErrors), _("<p>Could not find Atom/Echo/PIE header!</p>"));
			error = 1;
			break;			
		}

		time(&(cp->time));

		/* parse feed contents */
		cur = cur->xmlChildrenNode;
		while (cur != NULL) {
			/* parse feed author */
			if ((!xmlStrcmp(cur->name, (const xmlChar *) "author"))) {
				g_free(cp->author);
				cp->author = parseAuthor(cur);
				cur = cur->next;		
				continue;
			}
			
			/* parse feed contributors */
			if ((!xmlStrcmp(cur->name, (const xmlChar *) "contributor"))) {
				tmp = parseAuthor(cur);				
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
					g_assert(NULL != pie_nslist);
					if(NULL != (nsh = (PIENsHandler *)g_hash_table_lookup(pie_nslist, (gpointer)cur->ns->prefix))) {
						parseFunc = nsh->parseChannelTag;
						if(NULL != parseFunc)
							(*parseFunc)(cp, cur);
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
					if(NULL == (cp->tags[i] = CONVERT(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1)))) {
						cp->tags[i] = tmp;
					} else {
						g_free(tmp);
					}
				}		
			}

			/* collect PIE feed entries */
			if ((!xmlStrcmp(cur->name, (const xmlChar *) "entry"))) {
				if(NULL != (ip = parseEntry(cp, cur))) {
					if(0 == ip->time)
						ip->time = cp->time;
					addItem(fp, ip);
				}
			}
			cur = cur->next;
		}

		
		/* some postprocessing */
		if(NULL != cp->tags[PIE_FEED_TITLE]) 
			cp->tags[PIE_FEED_TITLE] = unhtmlize(cp->tags[PIE_FEED_TITLE]);

		if(NULL != cp->tags[PIE_FEED_DESCRIPTION])
			cp->tags[PIE_FEED_DESCRIPTION] = convertToHTML(cp->tags[PIE_FEED_DESCRIPTION]);		

		xmlFreeDoc(doc);			
		
		/* after parsing we fill in the infos into the feedPtr structure */		
		fp->defaultInterval = fp->updateInterval = cp->updateInterval;
		fp->title = cp->tags[PIE_FEED_TITLE];

		if(0 == error) {
			fp->available = TRUE;
			fp->description = showPIEFeedInfo(cp, fp->source);
		} else {
			print_status(g_strdup(_("There were errors while parsing this feed!")));
		}
			
		g_free(cp->nsinfos);
		g_free(cp);
		break;
	}
}

/* ---------------------------------------------------------------------------- */
/* initialization		 						*/
/* ---------------------------------------------------------------------------- */

feedHandlerPtr initPIEFeedHandler(void) {
	feedHandlerPtr	fhp;
	
	fhp = g_new0(struct feedHandler, 1);
	
	g_free(pie_nslist);
	pie_nslist = g_hash_table_new(g_str_hash, g_str_equal);
	
	/* register PIE name space handlers, not sure which namespaces beside DC are common */
	if(getNameSpaceStatus(ns_dc_getPIENsPrefix()))
		g_hash_table_insert(pie_nslist, (gpointer)ns_dc_getPIENsPrefix(),
					        (gpointer)ns_dc_getPIENsHandler());

	/* prepare feed handler structure */
	fhp->readFeed		= readPIEFeed;
	fhp->merge		= TRUE;

	return fhp;
}

