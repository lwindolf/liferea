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
#include "common.h"
#include "ocs_ns.h"
#include "ocs_dir.h"

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

/* you can find the OCS specification at

   http://internetalchemy.org/ocs/directory.html 
 */

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
							new_fp->source = g_strdup(xmlGetNoNsProp(cur, "about"));
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
							new_dep->source = g_strdup(xmlGetNoNsProp(cur, "about"));
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



directoryPtr readOCS(gchar *url) {
	directoryPtr	dp;
	gchar		*tmpfile;
	gchar		*contentType;
		
	tmpfile = g_strdup_printf("%s/new.ocs", getCachePath());
		
	while(1) {
		print_status(g_strdup_printf(_("reading from %s"), url));
		
		if(-1 == xmlNanoHTTPFetch(url, tmpfile, &contentType)) {
			showErrorBox(g_strdup_printf(_("Could not fetch %s!"), url));
			print_status(g_strdup_printf(_("HTTP GET status %d\n"), xmlNanoHTTPReturnCode()));
			break;			
		}
		
		return loadOCS(tmpfile);
	}
	
	return NULL;
}

directoryPtr loadOCS(gchar *filename) {		
	xmlDocPtr 	doc;
	xmlNodePtr 	cur;
	directoryPtr 	dp;
	gchar		*encoding;
	gchar		*keypos;
	short 		rdf = 0;	
	int 		error = 0;
	
	/* initialize channel structure */
	if(NULL == (dp = (directoryPtr) malloc(sizeof(struct directory)))) {
		g_error("not enough memory!\n");
		return NULL;
	}
	memset(dp, 0, sizeof(struct directory));

	dp->updateCounter = -1;
	dp->key = NULL;	
	dp->available = FALSE;
		
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

	return dp;
}

gchar * getOCSDirectorySource(gpointer dp) { return ((directoryPtr)dp)->source; }
gchar * getOCSDirectoryTag(gpointer dp, gint tag) { return ((directoryPtr)dp)->tags[tag]; }
void setOCSDirectoryTag(gpointer dp, gint tag, gchar *value) { ((directoryPtr)dp)->tags[tag] = value; }

gchar * getOCSDirEntrySource(gpointer dep) { return ((dirEntryPtr)dep)->source; }
gchar * getOCSDirEntryTag(gpointer dep, gint tag) { return ((dirEntryPtr)dep)->tags[tag]; }
void setOCSDirEntryTag(gpointer dep, gint tag, gchar *value) { ((dirEntryPtr)dep)->tags[tag] = value; }

gchar * getOCSFormatSource(gpointer fp) { return ((formatPtr)fp)->source; }
gchar * getOCSFormatTag(gpointer fp, gint tag) { return ((formatPtr)fp)->tags[tag]; }
void setOCSFormatTag(gpointer fp, gint tag, gchar *value) { ((formatPtr)fp)->tags[tag] = value; }
