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

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include "support.h"
#include "conf.h"
#include "common.h"
#include "ocs_ns.h"
#include "ocs_dir.h"
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
gpointer readOCS(gchar *url);
gpointer loadOCS(gchar *keyprefix, gchar *key);
void removeOCS(gchar *keyprefix, gchar *key, gpointer dp);


/* display an directory entry description and its formats in the HTML widget */
void	showDirEntry(gpointer dep);

/* display a directory info in the HTML widget */
void	showDirectoryInfo(gpointer dp);

gpointer getOCSFeedProp(gpointer fp, gint proptype);
void setOCSFeedProp(gpointer fp, gint proptype, gpointer data);

gpointer getOCSItemProp(gpointer ip, gint proptype);
void setOCSItemProp(gpointer ip, gint proptype, gpointer data);

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
	fhp->loadFeed		= loadOCS;
	fhp->readFeed		= readOCS;
	fhp->mergeFeed		= NULL;
	fhp->removeFeed		= removeOCS;
	fhp->getFeedProp	= getOCSFeedProp;
	fhp->setFeedProp	= setOCSFeedProp;
	fhp->showFeedInfo	= showDirectoryInfo;
	
	return fhp;
}

itemHandlerPtr initOCSItemHandler(void) {
	itemHandlerPtr	ihp;
	
	if(NULL == (ihp = (itemHandlerPtr)g_malloc(sizeof(struct itemHandler)))) {
		g_error(_("not enough memory!"));
	}
	memset(ihp, 0, sizeof(struct itemHandler));
	
	/* the OCS item handling reuses the OCS channel namespace handler */

	/* prepare item handler structure */
	ihp->getItemProp	= getOCSItemProp;	
	ihp->setItemProp	= setOCSItemProp;
	ihp->showItem		= showDirEntry;
	
	return ihp;
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

static void parseDirectoryEntry(dirEntryPtr dep, xmlDocPtr doc, xmlNodePtr cur) {
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

	/* some postprocessing */
	if(NULL != dep->tags[OCS_TITLE])
		dep->tags[OCS_TITLE] = unhtmlize((gchar *)doc->encoding, dep->tags[OCS_TITLE]);
		
	if(NULL != dep->tags[OCS_DESCRIPTION])
		dep->tags[OCS_DESCRIPTION] = convertToHTML((gchar *)doc->encoding, dep->tags[OCS_DESCRIPTION]);
}
 
static void parseDirectory(directoryPtr dp, xmlDocPtr doc, xmlNodePtr cur) {
	gchar			*encoding;
	parseOCSTagFunc		fp;
	dirEntryPtr		new_dep;
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

					/* check for <rdf:description tags, if we find one this
					   means a new channel description */
					if (!xmlStrcmp(cur->name, "description")) {
						if(NULL != (new_dep = (dirEntryPtr)g_malloc(sizeof(struct dirEntry)))) {
							memset(new_dep, 0, sizeof(struct dirEntry));						
							new_dep->source = xmlGetNoNsProp(cur, "about");
							new_dep->dp = dp;
							parseDirectoryEntry(new_dep, doc, cur);
							dp->items = g_slist_append(dp->items, new_dep);
						} else {
							g_error(_("not enough memory!"));
						}
					}		
					
		
				} else {
					if(NULL != (nsh = (OCSNsHandler *)g_hash_table_lookup(ocs_nslist, (gpointer)cur->ns->prefix))) {

						fp = nsh->parseDirectoryTag;
						if(NULL != fp) {			
							(*fp)(dp, doc, cur);
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



gpointer readOCS(gchar *url) {
	directoryPtr	dp;
	gchar		*tmpfile;
	gchar		*contentType;
		
	tmpfile = g_strdup_printf("%s/none_new.ocs", getCachePath());
		
	while(1) {
		print_status(g_strdup_printf(_("reading from %s"), url));
		
		if(-1 == xmlNanoHTTPFetch(url, tmpfile, &contentType)) {
			showErrorBox(g_strdup_printf(_("Could not fetch %s!"), url));
			print_status(g_strdup_printf(_("HTTP GET status %d\n"), xmlNanoHTTPReturnCode()));
			break;			
		}
		
		return (gpointer)loadOCS("none", "none/new");
	}
	
	return NULL;
}

gpointer loadOCS(gchar *keyprefix, gchar *key) {
	xmlDocPtr 	doc;
	xmlNodePtr 	cur;
	directoryPtr 	dp;
	gchar		*encoding;
	gchar		*filename;
	short 		rdf = 0;	
	int 		error = 0;

	filename = getCacheFileName(keyprefix, key, "ocs");

	/* initialize channel structure */
	if(NULL == (dp = (directoryPtr) malloc(sizeof(struct directory)))) {
		g_error("not enough memory!\n");
		return NULL;
	}
	memset(dp, 0, sizeof(struct directory));

	dp->updateCounter = -1;
	dp->key = NULL;
	dp->items = NULL;
	dp->available = FALSE;
	dp->type = FST_OCS;
		
	while(1) {
		doc = xmlParseFile(filename);
	
		if(NULL == doc) {
			print_status(g_strdup_printf(_("XML error wile reading directory! Directory \"%s\" could not be loaded!"),filename));
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

		if (!xmlStrcmp(cur->name, (const xmlChar *)"rdf") || 
                    !xmlStrcmp(cur->name, (const xmlChar *)"RDF")) {
			rdf = 1;
		} else {
			print_status(_("Could not find RDF header! Directory was added!"));
			xmlFreeDoc(doc);
			error = 1;
			break;			
		}

		cur = cur->xmlChildrenNode;
		while (cur && xmlIsBlankNode(cur)) {
			cur = cur->next;
		}
		
		while (cur != NULL) {
			if ((!xmlStrcmp(cur->name, (const xmlChar *) "description"))) {
				dp->source = g_strdup(xmlGetNoNsProp(cur, "about"));		
				parseDirectory(dp, doc, cur);
				break;
			}
		}

		//dp->encoding = g_strdup(doc->encoding);
		dp->available = TRUE;

		xmlFreeDoc(doc);
		break;
	}

	g_free(filename);
	return (gpointer)dp;
}

void removeOCS(gchar *keyprefix, gchar *key, gpointer dp) {
	gchar		*filename;
	
	// FIXME: free ocs data structures...
	filename = getCacheFileName(keyprefix, key, "ocs");
	g_print("deleting cache file %s\n", filename);
	if(0 != unlink(filename)) {
		showErrorBox(_("could not remove cache file of this entry!"));
	}
	g_free(filename);
}
/* ---------------------------------------------------------------------------- */
/* HTML output stuff	 							*/
/* ---------------------------------------------------------------------------- */

/* print information of a format entry in the HTML */
static void showFormatEntry(gpointer data, gpointer userdata) {
	gchar		*link, *tmp;
	formatPtr	f = (formatPtr)data;
	
	if(NULL != (link = f->source)) {
		writeHTML(FORMAT_START);

		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", link, link);
		writeHTML(FORMAT_LINK);
		writeHTML(tmp);		
		g_free(tmp);

		if(NULL != (tmp = f->tags[OCS_LANGUAGE])) {
			writeHTML(FORMAT_LANGUAGE);
			writeHTML(tmp);
		}

		if(NULL != (tmp = f->tags[OCS_UPDATEPERIOD])) {
			writeHTML(FORMAT_UPDATEPERIOD);
			writeHTML(tmp);
		}

		if(NULL != (tmp = f->tags[OCS_UPDATEFREQUENCY])) {
			writeHTML(FORMAT_UPDATEFREQUENCY);
			writeHTML(tmp);
		}
		
		if(NULL != (tmp = f->tags[OCS_CONTENTTYPE])) {
			writeHTML(FORMAT_CONTENTTYPE);
			writeHTML(tmp);
		}
		
		writeHTML(FORMAT_END);	
	}
}

/* display a directory entry description and its formats in the HTML widget */
void showDirEntry(gpointer fp) {
	dirEntryPtr	dep = (dirEntryPtr)fp;
	directoryPtr	dp = dep->dp;
	GSList		*iter;
	gchar		*link;
	gchar		*channelimage;
	gchar		*tmp;
	
	g_assert(dep != NULL);
	g_assert(dp != NULL);
	
	startHTMLOutput();
	writeHTML(HTML_HEAD_START);

	writeHTML(META_ENCODING1);
	writeHTML("UTF-8");
	writeHTML(META_ENCODING2);

	writeHTML(HTML_HEAD_END);

	if(NULL != (link = dep->source)) {
		writeHTML(ITEM_HEAD_START);
	
		writeHTML(ITEM_HEAD_ITEM);
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", link, dep->tags[OCS_TITLE]);
		writeHTML(tmp);
		g_free(tmp);
		
		writeHTML(ITEM_HEAD_END);	
	}

	if(NULL != (channelimage = dep->tags[OCS_IMAGE])) {
		writeHTML(IMG_START);
		writeHTML(channelimage);
		writeHTML(IMG_END);	
	}

	if(NULL != dep->tags[OCS_DESCRIPTION])
		writeHTML(dep->tags[OCS_DESCRIPTION]);
		
	/* output infos about the available formats */
	g_slist_foreach(dep->formats, showFormatEntry, NULL);

	writeHTML(FEED_FOOT_TABLE_START);
	FEED_FOOT_WRITE(doc, "creator",		dep->tags[OCS_CREATOR]);
	FEED_FOOT_WRITE(doc, "subject",		dep->tags[OCS_SUBJECT]);
	FEED_FOOT_WRITE(doc, "language",	dep->tags[OCS_LANGUAGE]);
	FEED_FOOT_WRITE(doc, "updatePeriod",	dep->tags[OCS_UPDATEPERIOD]);
	FEED_FOOT_WRITE(doc, "contentType",	dep->tags[OCS_CONTENTTYPE]);
	writeHTML(FEED_FOOT_TABLE_END);

	finishHTMLOutput();
}

/* writes directory info as HTML */
void showDirectoryInfo(gpointer fp) {
	directoryPtr	dp = (directoryPtr)fp;
	gchar		*description;
	gchar		*source;
	gchar		*tmp;	

	g_assert(dp != NULL);	
	
	startHTMLOutput();
	writeHTML(HTML_HEAD_START);

	writeHTML(META_ENCODING1);
	writeHTML("UTF-8");
	writeHTML(META_ENCODING2);

	writeHTML(HTML_HEAD_END);

	writeHTML(FEED_HEAD_START);
	
	writeHTML(FEED_HEAD_CHANNEL);
	writeHTML(getDefaultEntryTitle(dp->key));

	writeHTML(HTML_NEWLINE);	

	writeHTML(FEED_HEAD_SOURCE);	
	if(NULL != (source = dp->source)) {
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", source, source);
		writeHTML(tmp);
		g_free(tmp);
	}

	writeHTML(FEED_HEAD_END);	

	if(NULL != (description = dp->tags[OCS_DESCRIPTION]))
		writeHTML(description);

	writeHTML(FEED_FOOT_TABLE_START);
	FEED_FOOT_WRITE(doc, "creator",		dp->tags[OCS_CREATOR]);
	FEED_FOOT_WRITE(doc, "subject",		dp->tags[OCS_SUBJECT]);
	FEED_FOOT_WRITE(doc, "language",	dp->tags[OCS_LANGUAGE]);
	FEED_FOOT_WRITE(doc, "updatePeriod",	dp->tags[OCS_UPDATEPERIOD]);
	FEED_FOOT_WRITE(doc, "contentType",	dp->tags[OCS_CONTENTTYPE]);
	writeHTML(FEED_FOOT_TABLE_END);

	finishHTMLOutput();
}

/* ---------------------------------------------------------------------------- */
/* just some encapsulation 							*/
/* ---------------------------------------------------------------------------- */

void setOCSFeedProp(gpointer fp, gint proptype, gpointer data) {
	directoryPtr	c = (directoryPtr)fp;
	
	if(NULL != c) {
		g_assert(FST_OCS == c->type);
		switch(proptype) {
			case FEED_PROP_TITLE:
				g_free(c->tags[OCS_TITLE]);
				c->tags[OCS_TITLE] = (gchar *)data;
				break;
			case FEED_PROP_USERTITLE:
				g_free(c->usertitle);
				c->usertitle = (gchar *)data;
				break;
			case FEED_PROP_SOURCE:
				g_free(c->source);
				c->source = (gchar *)data;
				break;
			case FEED_PROP_ITEMLIST:
				g_error("please don't do this!");
				break;
			case FEED_PROP_AVAILABLE:
				c->available = (gboolean)data;
				break;
			case FEED_PROP_UPDATECOUNTER:
				c->updateCounter = (gint)data;
				break;
			case FEED_PROP_UNREADCOUNT:
			case FEED_PROP_UPDATEINTERVAL:
			case FEED_PROP_DFLTUPDINTERVAL:
				g_warning(_("this should not happen! unsupported item property type!\n"));
				break;
			default:
				g_error(g_strdup_printf(_("intenal error! unknow feed property type %d!\n"), proptype));
				break;
		}
	} else {
		return;
	}
}

gpointer getOCSFeedProp(gpointer fp, gint proptype) {
	directoryPtr	c = (directoryPtr)fp;
	
	if(NULL != c) {
		g_assert(FST_OCS == c->type);
		switch(proptype) {
			case FEED_PROP_TITLE:
				return (gpointer)c->tags[OCS_TITLE];
				break;
			case FEED_PROP_USERTITLE:
				return (gpointer)c->usertitle;
				break;
			case FEED_PROP_SOURCE:
				return (gpointer)c->source;
				break;
			case FEED_PROP_AVAILABLE:
				return (gpointer)c->available;
				break;			
			case FEED_PROP_ITEMLIST:
				return (gpointer)c->items;
				break;
			case FEED_PROP_UPDATECOUNTER:
				return (gpointer)c->updateCounter;
				break;
			case FEED_PROP_DFLTUPDINTERVAL:
			case FEED_PROP_UPDATEINTERVAL:
				return (gpointer)-1;	/* to avoid auto update */
				break;
			case FEED_PROP_UNREADCOUNT:		
				g_warning(_("this should not happen! unsupported item property type!\n"));
				return NULL;
				break;
			default:
				g_error(g_strdup_printf(_("internal error! unknow feed property type %d!\n"), proptype));
				break;
		}
	} else {
		return NULL;
	}
}

void setOCSItemProp(gpointer ip, gint proptype, gpointer data) {
	dirEntryPtr	i = (dirEntryPtr)ip;
	
	if(NULL != i) {
		g_assert(NULL != i->dp);
		g_assert(FST_OCS == ((directoryPtr)(i->dp))->type);
		switch(proptype) {
			case ITEM_PROP_TITLE:
				g_free(i->tags[OCS_TITLE]);
				i->tags[OCS_TITLE] = (gchar *)data;
				break;
			case ITEM_PROP_READSTATUS:
				break;
			case ITEM_PROP_DESCRIPTION:
			case ITEM_PROP_TIME:
			case ITEM_PROP_SOURCE:
			case ITEM_PROP_TYPE:
				g_error("please don't do this!");
				break;
			default:
				g_error(g_strdup_printf(_("internal error! unknow item property type %d!\n"), proptype));
				break;
		}
	}
}

gpointer getOCSItemProp(gpointer ip, gint proptype) {
	dirEntryPtr	i = (dirEntryPtr)ip;
	
	if(NULL != i) {
		g_assert(NULL != i->dp);

		g_assert(FST_OCS == ((directoryPtr)(i->dp))->type);	
		switch(proptype) {
			case ITEM_PROP_TITLE:
				return (gpointer)i->tags[OCS_TITLE];
				break;
			case ITEM_PROP_READSTATUS:
				return (gpointer)TRUE;
				break;
			case ITEM_PROP_DESCRIPTION:
				return (gpointer)i->tags[OCS_DESCRIPTION];
				break;
			case ITEM_PROP_TIME:
				return NULL;
				break;
			case ITEM_PROP_SOURCE:
				return (gpointer)i->source;
				break;
			case ITEM_PROP_TYPE:
				return (gpointer)FST_OCS;
				break;
			default:
				g_error(g_strdup_printf(_("internal error! unknow item property type %d!\n"), proptype));
				break;
		}
	} else {
		return NULL;
	}
}
