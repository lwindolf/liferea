/*
   Dublin Core support for RSS and OCS
   
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

#define _XOPEN_SOURCE 	/* glibc2 needs this (man strptime) */

#include <libgtkhtml/gtkhtml.h>

#include "ns_ocs.h"
#include "ns_dc.h"
#include "conf.h"
#include "support.h"
#include "common.h"

#define TABLE_START	"<div style=\"margin-top:15px;font-size:8pt;color:#C0C0C0\">additional Dublin Core information</div><table style=\"width:100%;border-width:1px;border-top-style:solid;border-color:#D0D0D0;\">"
#define FIRSTTD		"<tr style=\"border-width:0;border-bottom-width:1px;border-style:dashed;border-color:#D0D0D0;\"><td><span style=\"font-size:8pt;color:#C0C0C0\">"
#define NEXTTD		"</span></td><td><span style=\"font-size:8pt;color:#C0C0C0\">"
#define LASTTD		"</span></td></tr>"
#define TABLE_END	"</table>"

#define HTML_WRITE(doc, tags)	{ if((NULL != tags) && (strlen(tags) > 0)) html_document_write_stream(doc, tags, strlen(tags)); }

#define TYPE_DIRECTORY	0
#define TYPE_CHANNEL	1
#define TYPE_FORMAT	2

static gchar ns_dc_prefix[] = "dc";

/* some prototypes */
static gchar * ns_dc_doItemFooterOutput(gpointer obj, gpointer htmlStream);
static gchar *	ns_dc_doChannelFooterOutput(gpointer obj, gpointer htmlStream);

/* a tag list from http://web.resource.org/rss/1.0/modules/dc/

-------------------------------------------------------
 <channel>, <item>, <image>, and <textinput> Elements:

    * <dc:title> ( #PCDATA )
    * <dc:creator> ( #PCDATA )
    * <dc:subject> ( #PCDATA )
    * <dc:description> ( #PCDATA )
    * <dc:publisher> ( #PCDATA )
    * <dc:contributor> ( #PCDATA )
    * <dc:date> ( #PCDATA ) [W3CDTF]
    * <dc:type> ( #PCDATA )
    * <dc:format> ( #PCDATA )
    * <dc:identifier> ( #PCDATA )
    * <dc:source> ( #PCDATA )
    * <dc:language> ( #PCDATA )
    * <dc:relation> ( #PCDATA )
    * <dc:coverage> ( #PCDATA )
    * <dc:rights> ( #PCDATA )
-------------------------------------------------------

The most important is the data tag for correct update 
handling. When reading this tag we adjust the items 
date. 

The following fields will be mapped to their 
corresponding RSS 0.92/RSS 2.0 fields, because
dc-tags are typically used to implement their 
functionalities with a simpler RSS version. If
both variants (e.g. <dc:language> and <language>)
appear, the last tag will overwrite the values
of the first one.

	title		-> title
	creator		-> managingEditor	
	subject 	-> category
	description	-> description
	language	-> language
	rights		-> copyright
	publisher	-> webMaster

Any other infos are simply displayed in channel info
or item view inside the header or footer of display.

*/

static gchar * taglist[] = {	"title",
				"creator",
				"subject",
				"description",
				"publisher",
				"contributor",				
				"date",
				"type",
				"format",
				"identifier",
				"source",
				"language",
				"coverage",
				"rights", 
				NULL
			   };

/* mapping of the tags specified by taglist to the backends channel
   structure taglist */
static gint mapToCP[] = { 	CHANNEL_TITLE,		/* title */ 
				CHANNEL_MANAGINGEDITOR,	/* creator */
				CHANNEL_CATEGORY,	/* subject */
				CHANNEL_DESCRIPTION,	/* description */
				CHANNEL_WEBMASTER,	/* publisher */
				-1,			/* contributor */
				-1,			/* date (seldom used, e.g. slashdot, we map it to pubdate) */
				-1,			/* type */
				-1,			/* format */
				-1,			/* identifier */
				-1,			/* source */
				CHANNEL_LANGUAGE,	/* language */
				-1,			/* coverage */
				CHANNEL_COPYRIGHT	/* rights */
			  };

static gint mapToIP[] = { 	ITEM_TITLE,		/* title */ 
				-1,			/* creator */
				ITEM_CATEGORY,		/* subject */
				ITEM_DESCRIPTION,	/* description */
				-1,			/* publisher */
				-1,			/* contributor */				
				-1,			/* date (won't be processed...) */
				-1,			/* type */
				-1,			/* format */
				-1,			/* identifier */
				-1,			/* source */
				-1,			/* language */
				-1,			/* coverage */				
				-1			/* rights */
			  };

static gint mapToDP[] = { 	OCS_TITLE,		/* title */ 
				OCS_CREATOR,		/* creator */
				OCS_SUBJECT,		/* subject */
				OCS_DESCRIPTION,	/* description */
				-1,			/* publisher */
				-1,			/* contributor */				
				-1,			/* date (won't be processed...) */
				-1,			/* type */
				-1,			/* format */
				-1,			/* identifier */
				-1,			/* source */
				OCS_LANGUAGE,		/* language */
				-1,			/* coverage */				
				-1			/* rights */
			  };

			  
/* the HTML stream the output handler write to */			   
static HtmlDocument	*doc;

gchar * ns_dc_getRSSNsPrefix(void) { return ns_dc_prefix; }
gchar * ns_dc_getOCSNsPrefix(void) { return ns_dc_prefix; }

static void ns_dc_addInfoStruct(GHashTable *nslist, gchar *tagname, gchar *tagvalue) {
	GHashTable	*nsvalues;
	
	g_assert(nslist != NULL);

	if(tagvalue == NULL)
		return;

	if(NULL == (nsvalues = (GHashTable *)g_hash_table_lookup(nslist, ns_dc_prefix))) {
		nsvalues = g_hash_table_new(g_str_hash, g_str_equal);
		g_hash_table_insert(nslist, (gpointer)ns_dc_prefix, (gpointer)nsvalues);
	}
	g_hash_table_insert(nsvalues, (gpointer)tagname, (gpointer)tagvalue);
}

/* common OCS parsing */
static void ns_dc_parseOCSTag(gint type, gpointer p, xmlDocPtr doc, xmlNodePtr cur) {
	int 		i;
	gchar		*value;
	
	if((NULL == cur) || (NULL == doc)) {
		g_warning(_("internal error: XML document pointer NULL! This should not happen!\n"));
		return;
	}

	while (cur != NULL) {

		/* compare with each possible tag name */
		for(i = 0; taglist[i] != NULL; i++) {
			if(-1 != mapToDP[i]) {
				if(!xmlStrcmp((const xmlChar *)taglist[i], cur->name)) {
					value = (gchar *)xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
					g_assert(mapToDP[i] < OCS_MAX_TAG);
					/* map the value to one of the RSS fields */
					switch(type) {
						case TYPE_DIRECTORY:
							setOCSDirectoryTag(p, mapToDP[i], g_strdup(value));
							break;
						case TYPE_CHANNEL:
							setOCSDirEntryTag(p, mapToDP[i], g_strdup(value));
							break;
						case TYPE_FORMAT:
							setOCSFormatTag(p, mapToDP[i], g_strdup(value));
							break;
						default:
							g_error(_("internal OCS namespace parsing error!"));
							break;
					}
				}
			}
		}
				
		cur = cur->next;
	}
}

static void ns_dc_parseOCSDirectoryTag(gpointer p, xmlDocPtr doc, xmlNodePtr cur) {
	ns_dc_parseOCSTag(TYPE_DIRECTORY, p, doc, cur);
}

static void ns_dc_parseOCSChannelTag(gpointer p, xmlDocPtr doc, xmlNodePtr cur) {
	ns_dc_parseOCSTag(TYPE_CHANNEL, p, doc, cur);
}

static void ns_dc_parseOCSFormatTag(gpointer p, xmlDocPtr doc, xmlNodePtr cur) {
	ns_dc_parseOCSTag(TYPE_FORMAT, p, doc, cur);
}

/* RSS channel parsing */
static void ns_dc_parseChannelTag(channelPtr cp, xmlDocPtr doc, xmlNodePtr cur) {
	int 		i;
	char		*date;
	gchar		*value;
	
	if((NULL == cur) || (NULL == doc)) {
		g_warning(_("internal error: XML document pointer NULL! This should not happen!\n"));
		return;
	}

	while (cur != NULL) {

		/* special handling for the ISO 8601 date tag */
		if(!xmlStrcmp((const xmlChar *)"date", cur->name)) {
			date = g_strdup(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1));
			date = convertDate(date);
			if(NULL != date) {
				cp->tags[CHANNEL_PUBDATE] = (gchar *)date;
			}
						
			cur = cur->next;		
			continue;
		}

		/* compare with each possible tag name */
		for(i = 0; taglist[i] != NULL; i++) {
	
			if(!xmlStrcmp((const xmlChar *)taglist[i], cur->name)) {
				value = (gchar *)xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				if(-1 == mapToCP[i]) {
					/* add it to the common DC value list, which is processed
					   by by ns_dc_output() */
					ns_dc_addInfoStruct(cp->nsinfos, taglist[i], value);
				} else {
					g_assert(mapToCP[i] <= CHANNEL_MAX_TAG);
					/* map the value to one of the RSS fields */
					g_free(cp->tags[mapToCP[i]]);
					cp->tags[mapToCP[i]] = g_strdup(value);
				}
			}
		}
				
		cur = cur->next;
	}
}

static void ns_dc_parseItemTag(itemPtr ip,xmlDocPtr doc, xmlNodePtr cur) {
	int 		i;
	gchar		*value;
	gchar		*date;
	
	if((NULL == cur) || (NULL == doc)) {
		g_warning(_("internal error: XML document pointer NULL! This should not happen!\n"));
		return;
	}

	while (cur != NULL) {												
		/* special handling for the ISO 8601 date tag */
		if(!xmlStrcmp((const xmlChar *)"date", cur->name)) {
			date = g_strdup(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1));
			date = convertDate(date);
			if(NULL != date) {
				g_free(ip->time);
				ip->time = date;
			}
						
			cur = cur->next;		
			continue;
		}
	
		/* compare with each possible tag name */
		for(i = 0; taglist[i] != NULL; i++) {
	
			if(!xmlStrcmp((const xmlChar *)taglist[i], cur->name)) {
				value = (gchar *)xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				if(-1 == mapToIP[i]) {
					/* add it to the common DC value list, which is processed
					   by by ns_dc_output() */
					ns_dc_addInfoStruct(ip->nsinfos, taglist[i], value);
				} else {
					g_assert(mapToIP[i] <= ITEM_MAX_TAG);
					/* map the value to one of the RSS fields */
					g_free(ip->tags[mapToIP[i]]);
					ip->tags[mapToIP[i]] = g_strdup(value);
				}
			}
		}
		
		cur = cur->next;
	}	
}

static void ns_dc_output(gpointer key, gpointer value, gpointer userdata) {

	HTML_WRITE(doc, FIRSTTD);
	HTML_WRITE(doc, (gchar *)key);
	HTML_WRITE(doc, NEXTTD);
	HTML_WRITE(doc, (gchar *)value);
	HTML_WRITE(doc, LASTTD);
}

static void ns_dc_doFooterOutput(GHashTable *nsinfos) {
	GHashTable	*nsvalues;
	
	/* we print all channel infos as a (key,value) table */
	if(NULL != (nsvalues = g_hash_table_lookup(nsinfos, (gpointer)ns_dc_prefix))) {
		HTML_WRITE(doc, TABLE_START);
		g_hash_table_foreach(nsvalues, ns_dc_output, NULL);
		HTML_WRITE(doc, TABLE_END);			
	}
}

static gchar * ns_dc_doItemFooterOutput(gpointer obj, gpointer htmlStream) {

	doc = (HtmlDocument *)htmlStream;
	
	if((obj != NULL) && (doc != NULL)) {
		ns_dc_doFooterOutput(((itemPtr)obj)->nsinfos);
	}
}


static gchar *	ns_dc_doChannelFooterOutput(gpointer obj, gpointer htmlStream) {
	doc = (HtmlDocument *)htmlStream;
	
	if((obj != NULL) && (doc != NULL)) {
		ns_dc_doFooterOutput(((channelPtr)obj)->nsinfos);
	}
}

RSSNsHandler *ns_dc_getRSSNsHandler(void) {
	RSSNsHandler 	*nsh;
	
	if(NULL != (nsh = (RSSNsHandler *)g_malloc(sizeof(RSSNsHandler)))) {
		nsh->parseChannelTag		= ns_dc_parseChannelTag;
		nsh->parseItemTag		= ns_dc_parseItemTag;
		nsh->doChannelHeaderOutput	= NULL;
		nsh->doChannelFooterOutput	= ns_dc_doChannelFooterOutput;
		nsh->doItemHeaderOutput		= NULL;
		nsh->doItemFooterOutput		= ns_dc_doItemFooterOutput;		
	}

	return nsh;
}

OCSNsHandler *ns_dc_getOCSNsHandler(void) {
	OCSNsHandler 	*nsh;
	
	if(NULL != (nsh = (OCSNsHandler *)g_malloc(sizeof(OCSNsHandler)))) {
		nsh->parseDirectoryTag		= ns_dc_parseOCSDirectoryTag;
		nsh->parseDirEntryTag		= ns_dc_parseOCSChannelTag;
		nsh->parseFormatTag		= ns_dc_parseOCSFormatTag;				
	}

	return nsh;
}
