/*
   OCS 0.4 support directory tag parsing. Note: this ocs_dir.c contains
   only the rdf specific OCS parsing, the dc and ocs namespaces are
   processed by the specific namespace handlers!
   
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

#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include "support.h"
#include "conf.h"
#include "common.h"
#include "feed.h"
#include "item.h"
#include "ocs_ns.h"
#include "ocs_dir.h"

#include "netio.h"
#include "htmlview.h"

/* you can find the OCS specification at

   http://internetalchemy.org/ocs/directory.html 
 */
 
/* to store the nsHandler structs for all supported RDF namespace handlers */
GHashTable	*ocs_nslist = NULL;

/* note: the tag order has to correspond with the OCS_* defines in the header file */
static gchar *directoryTagList[] = {	"title",
					"creator",
					"description",
					"subject",
					"format",
					"updatePerdiod",
					"updateFrequency",
					"updateBase",
					"language",	/* this should catch OCS 0.3 */
					"contentType",
					"image",
					NULL
				  };

/* prototypes */
feedPtr readOCS(gchar *url);

/* display an directory entry description and its formats in the HTML widget */
static gchar *	showDirEntry(dirEntryPtr dep);

/* display a directory info in the HTML widget */
static gchar *	showDirectoryInfo(directoryPtr dp, gchar *url);

feedHandlerPtr initOCSFeedHandler(void) {
	feedHandlerPtr	fhp;
	
	if(NULL == (fhp = (feedHandlerPtr)g_malloc(sizeof(struct feedHandler)))) {
		g_error(_("not enough memory!"));
	}
	memset(fhp, 0, sizeof(struct feedHandler));
	
	g_free(ocs_nslist);
	ocs_nslist = g_hash_table_new(g_str_hash, g_str_equal);
	
	/* register OCS name space handlers */
	g_hash_table_insert(ocs_nslist, (gpointer)ns_dc_getOCSNsPrefix(),
				        (gpointer)ns_dc_getOCSNsHandler());

	g_hash_table_insert(ocs_nslist, (gpointer)ns_ocs_getOCSNsPrefix(),
				        (gpointer)ns_ocs_getOCSNsHandler());

	/* prepare feed handler structure */
	fhp->readFeed		= readOCS;
	
	return fhp;
}

static void parseFormatEntry(formatPtr fep, xmlDocPtr doc, xmlNodePtr cur) {
	gchar			*tmp = NULL;
	gchar			*encoding;
	formatPtr		new_fp;	
	parseOCSTagFunc		fp;
	OCSNsHandler		*nsh;
	int			i;
	
	if((NULL == cur) || (NULL == doc)) {
		g_warning(_("internal error: XML document pointer NULL! This should not happen!\n"));
		return;
	}

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		g_assert(NULL != cur->name);	
		
		/* check namespace of this tag */
		if(NULL != cur->ns) {
			if (NULL != cur->ns->prefix) {	
				if(0 == strcmp(cur->ns->prefix, "rdf")) {
				
					g_warning("unexpected OCS hierarchy, this should never happen! ignoring third level rdf tags!\n");
					
				} else if(NULL != (nsh = (OCSNsHandler *)g_hash_table_lookup(ocs_nslist, (gpointer)cur->ns->prefix))) {

					fp = nsh->parseFormatTag;
					if(NULL != fp)
						(*fp)(fep, doc, cur);
					else
						g_print(_("no namespace handler for <%s:%s>!\n"), cur->ns->prefix, cur->name);

				}
			}
		}

		cur = cur->next;
	}

	/* some postprocessing, all format-infos will be displayed in the HTML view */
	for(i = 0; i < OCS_MAX_TAG; i++)
		if(NULL != fep->tags[i])
			fep->tags[i] = convertToHTML((gchar *)doc->encoding, fep->tags[i]);

}

static itemPtr parseDirectoryEntry(dirEntryPtr dep, xmlDocPtr doc, xmlNodePtr cur) {
	gchar			*tmp = NULL;
	gchar			*encoding;
	formatPtr		new_fp;	
	parseOCSTagFunc		fp;
	OCSNsHandler		*nsh;
	itemPtr			ip;
	int			i;
	
	if((NULL == cur) || (NULL == doc)) {
		g_warning(_("internal error: XML document pointer NULL! This should not happen!\n"));
		return;
	}
	ip = getNewItemStruct();
	
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		g_assert(NULL != cur->name);	
		
		/* check namespace of this tag */
		if(NULL != cur->ns) {
			if (NULL != cur->ns->prefix) {	
				if(0 == strcmp(cur->ns->prefix, "rdf")) {

					/* check for <rdf:description> tags, if we find one, this means
					   a new format for the actual channel */
					if (!xmlStrcmp(cur->name, "description")) {
						if(NULL != (new_fp = (formatPtr)g_malloc(sizeof(struct format)))) {
							memset(new_fp, 0, sizeof(struct format));
							new_fp->source = xmlGetNoNsProp(cur, "about");
							parseFormatEntry(new_fp, doc, cur);
							dep->formats = g_slist_append(dep->formats, (gpointer)new_fp);
						} else {
							g_error(_("not enough memory!"));
						}
					}		
					
		
				} else {
					if(NULL != (nsh = (OCSNsHandler *)g_hash_table_lookup(ocs_nslist, (gpointer)cur->ns->prefix))) {

						fp = nsh->parseDirEntryTag;
						if(NULL != fp) {
							(*fp)(dep, doc, cur);
						} else
							g_print(_("no namespace handler for <%s:%s>!\n"), cur->ns->prefix, cur->name);

					}
				}
			}
		}

		cur = cur->next;
	}

	/* after parsing we fill the infos into the itemPtr structure */
	ip->type = FST_OCS;
	ip->source = dep->source;
	ip->readStatus = TRUE;
	ip->id = NULL;

	/* some postprocessing */
	if(NULL != dep->tags[OCS_TITLE])
		dep->tags[OCS_TITLE] = unhtmlize((gchar *)doc->encoding, dep->tags[OCS_TITLE]);
		
	if(NULL != dep->tags[OCS_DESCRIPTION])
		dep->tags[OCS_DESCRIPTION] = convertToHTML((gchar *)doc->encoding, dep->tags[OCS_DESCRIPTION]);

	ip->title = dep->tags[OCS_TITLE];		
	ip->description = showDirEntry(dep);
	// FIXME: free formats!
	g_slist_free(dep->formats);
	g_free(dep);
		
	return ip;
}
 
static void parseDirectory(feedPtr fp, directoryPtr dp, xmlDocPtr doc, xmlNodePtr cur) {
	gchar			*encoding;
	parseOCSTagFunc		parseFunc;
	dirEntryPtr		new_dep;
	OCSNsHandler		*nsh;
	itemPtr			ip;
	int			i;
	
	if((NULL == cur) || (NULL == doc)) {
		g_warning(_("internal error: XML document pointer NULL! This should not happen!\n"));
		return;
	}

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		g_assert(NULL != cur->name);	
		
		/* check namespace of this tag */
		if(NULL != cur->ns) {
			if (NULL != cur->ns->prefix) {	
				if(0 == strcmp(cur->ns->prefix, "rdf")) {

					/* check for <rdf:description tags, if we find one this
					   means a new channel description */
					if (!xmlStrcmp(cur->name, "description")) {
						if(NULL != (new_dep = (dirEntryPtr)g_malloc(sizeof(struct dirEntry)))) {
							memset(new_dep, 0, sizeof(struct dirEntry));						
							new_dep->source = xmlGetNoNsProp(cur, "about");
							new_dep->dp = dp;
							ip = parseDirectoryEntry(new_dep, doc, cur);
							addItem(fp, ip);
						} else {
							g_error(_("not enough memory!"));
						}
					}		
					
		
				} else {
					if(NULL != (nsh = (OCSNsHandler *)g_hash_table_lookup(ocs_nslist, (gpointer)cur->ns->prefix))) {

						parseFunc = nsh->parseDirectoryTag;
						if(NULL != parseFunc) {
							(*parseFunc)(dp, doc, cur);
						} else
							g_print(_("no namespace handler for <%s:%s>!\n"), cur->ns->prefix, cur->name);

					}
				}
			}
		}

		cur = cur->next;
	}

	/* some postprocessing */
	if(NULL != dp->tags[OCS_TITLE])
		dp->tags[OCS_TITLE] = unhtmlize((gchar *)doc->encoding, dp->tags[OCS_TITLE]);
		
	if(NULL != dp->tags[OCS_DESCRIPTION])
		dp->tags[OCS_DESCRIPTION] = convertToHTML((gchar *)doc->encoding, dp->tags[OCS_DESCRIPTION]);
}

feedPtr readOCS(gchar *url) {
	xmlDocPtr 	doc = NULL;
	xmlNodePtr 	cur = NULL;
	directoryPtr	dp;
	feedPtr	 	fp;
	gchar		*encoding;
	gchar		*data;
	int 		error = 0;

	if(NULL == (data = downloadURL(url))) {
		showErrorBox(g_strdup_printf(_("Could not fetch %s!"), url));
		return NULL;
	}

	fp = getNewFeedStruct();

	while(1) {
		doc = xmlRecoverMemory(data, strlen(data));
		g_free(data);
		
		if(NULL == doc) {
			print_status(g_strdup_printf(_("XML error wile reading directory! Directory \"%s\" could not be loaded!"),url));
			error = 1;
			break;
		}

		if(NULL == (cur = xmlDocGetRootElement(doc))) {
			print_status(_("Empty document! Feed was not added!"));
			error = 1;
			break;			
		}

		if (!xmlStrcmp(cur->name, (const xmlChar *)"rdf") || 
                    !xmlStrcmp(cur->name, (const xmlChar *)"RDF")) {
		    	// nothing, FIXME: is the condition above correct?
		} else {
			print_status(_("Could not find RDF header! Directory was not added!"));
			error = 1;
			break;			
		}

		cur = cur->xmlChildrenNode;
		while (cur && xmlIsBlankNode(cur)) {
			cur = cur->next;
		}
		
		while (cur != NULL) {
			if ((!xmlStrcmp(cur->name, (const xmlChar *) "description"))) {

				/* initialize directory structure */
				if(NULL == (dp = (directoryPtr) malloc(sizeof(struct directory)))) {
					g_error("not enough memory!\n");
					exit(1);
				}
				memset(dp, 0, sizeof(struct directory));
				parseDirectory(fp, dp, doc, cur);
				break;
			}
		}
		xmlFreeDoc(doc);

		/* after parsing we fill in the infos into the feedPtr structure */		
		fp->type = FST_OCS;
		fp->updateInterval = fp->updateCounter = -1;
		fp->title = dp->tags[OCS_TITLE];
		
		if(0 == error) {
			fp->description = showDirectoryInfo(dp, url);
			fp->available = TRUE;
		} else
			fp->title = g_strdup(url);
			
		g_free(dp);
		break;
	}

	return fp;
}

/* ---------------------------------------------------------------------------- */
/* HTML output stuff	 							*/
/* ---------------------------------------------------------------------------- */

/* print information of a format entry in the HTML */
static void showFormatEntry(gpointer data, gpointer userdata) {
	gchar		*tmp;
	gchar		**buffer = (gchar **)userdata;
	formatPtr	f = (formatPtr)data;
	
	if(NULL != f->source) {
		addToHTMLBuffer(buffer, FORMAT_START);

		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", f->source, f->source);
		addToHTMLBuffer(buffer, FORMAT_LINK);
		addToHTMLBuffer(buffer, tmp);		
		g_free(tmp);

		if(NULL != (tmp = f->tags[OCS_LANGUAGE])) {
			addToHTMLBuffer(buffer, FORMAT_LANGUAGE);
			addToHTMLBuffer(buffer, tmp);
		}

		if(NULL != (tmp = f->tags[OCS_UPDATEPERIOD])) {
			addToHTMLBuffer(buffer, FORMAT_UPDATEPERIOD);
			addToHTMLBuffer(buffer, tmp);
		}

		if(NULL != (tmp = f->tags[OCS_UPDATEFREQUENCY])) {
			addToHTMLBuffer(buffer, FORMAT_UPDATEFREQUENCY);
			addToHTMLBuffer(buffer, tmp);
		}
		
		if(NULL != (tmp = f->tags[OCS_CONTENTTYPE])) {
			addToHTMLBuffer(buffer, FORMAT_CONTENTTYPE);
			addToHTMLBuffer(buffer, tmp);
		}
		
		addToHTMLBuffer(buffer, FORMAT_END);	
	}
}

/* display a directory entry description and its formats in the HTML widget */
static gchar * showDirEntry(dirEntryPtr dep) {
	directoryPtr	dp = dep->dp;
	GSList		*iter;
	gchar		*tmp, *buffer = NULL;
	
	g_assert(dep != NULL);
	g_assert(dp != NULL);

	if(NULL != dep->source) {
		addToHTMLBuffer(&buffer, ITEM_HEAD_START);
		addToHTMLBuffer(&buffer, FEED_HEAD_CHANNEL);
		addToHTMLBuffer(&buffer, dp->tags[OCS_TITLE]);
		addToHTMLBuffer(&buffer, HTML_NEWLINE);	
		addToHTMLBuffer(&buffer, ITEM_HEAD_ITEM);
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", dep->source, dep->tags[OCS_TITLE]);
		addToHTMLBuffer(&buffer, tmp);
		g_free(tmp);
		
		addToHTMLBuffer(&buffer, ITEM_HEAD_END);	
	}

	if(NULL != dep->tags[OCS_IMAGE]) {
		addToHTMLBuffer(&buffer, IMG_START);
		addToHTMLBuffer(&buffer, dep->tags[OCS_IMAGE]);
		addToHTMLBuffer(&buffer, IMG_END);	
	}

	if(NULL != dep->tags[OCS_DESCRIPTION])
		addToHTMLBuffer(&buffer, dep->tags[OCS_DESCRIPTION]);
		
	/* output infos about the available formats */
	g_slist_foreach(dep->formats, showFormatEntry, &buffer);

	addToHTMLBuffer(&buffer, FEED_FOOT_TABLE_START);
	FEED_FOOT_WRITE(buffer, "creator",	dep->tags[OCS_CREATOR]);
	FEED_FOOT_WRITE(buffer, "subject",	dep->tags[OCS_SUBJECT]);
	FEED_FOOT_WRITE(buffer, "language",	dep->tags[OCS_LANGUAGE]);
	FEED_FOOT_WRITE(buffer, "updatePeriod",	dep->tags[OCS_UPDATEPERIOD]);
	FEED_FOOT_WRITE(buffer, "contentType",	dep->tags[OCS_CONTENTTYPE]);
	addToHTMLBuffer(&buffer, FEED_FOOT_TABLE_END);
	
	return buffer;
}

/* writes directory info as HTML */
static gchar * showDirectoryInfo(directoryPtr dp, gchar *url) {
	gchar		*tmp, *buffer = NULL;	

	g_assert(dp != NULL);	

	addToHTMLBuffer(&buffer, FEED_HEAD_START);	
	addToHTMLBuffer(&buffer, FEED_HEAD_CHANNEL);
	addToHTMLBuffer(&buffer, dp->tags[OCS_TITLE]);
	addToHTMLBuffer(&buffer, HTML_NEWLINE);	
	addToHTMLBuffer(&buffer, FEED_HEAD_SOURCE);	
	if(NULL != url) {
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", url, url);
		addToHTMLBuffer(&buffer, tmp);
		g_free(tmp);
	}

	addToHTMLBuffer(&buffer, FEED_HEAD_END);	

	if(NULL != dp->tags[OCS_DESCRIPTION])
		addToHTMLBuffer(&buffer, dp->tags[OCS_DESCRIPTION]);

	addToHTMLBuffer(&buffer, FEED_FOOT_TABLE_START);
	FEED_FOOT_WRITE(buffer, "creator",	dp->tags[OCS_CREATOR]);
	FEED_FOOT_WRITE(buffer, "subject",	dp->tags[OCS_SUBJECT]);
	FEED_FOOT_WRITE(buffer, "language",	dp->tags[OCS_LANGUAGE]);
	FEED_FOOT_WRITE(buffer, "updatePeriod",	dp->tags[OCS_UPDATEPERIOD]);
	FEED_FOOT_WRITE(buffer, "contentType",	dp->tags[OCS_CONTENTTYPE]);
	addToHTMLBuffer(&buffer, FEED_FOOT_TABLE_END);
	
	return buffer;
}
