/*
   Dublin Core support for RSS, Echo/Atom/PIE and OCS
   
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

#define _XOPEN_SOURCE 	/* glibc2 needs this (man strptime) */

#include "htmlview.h"

#include "ns_ocs.h"
#include "ns_dc.h"
#include "conf.h"
#include "support.h"
#include "common.h"

#define TABLE_START	"<div style=\"margin-top:15px;font-size:8pt;color:#C0C0C0\">additional Dublin Core information</div><table style=\"width:100%;border-width:1px;border-top-style:solid;border-color:#D0D0D0;\">"
#define FIRSTTD		"<tr style=\"border-width:0;border-bottom-width:1px;border-style:dashed;border-color:#D0D0D0;\"><td width=\"30%\"><span style=\"font-size:8pt;color:#C0C0C0\">"
#define NEXTTD		"</span></td><td><span style=\"font-size:8pt;color:#C0C0C0\">"
#define LASTTD		"</span></td></tr>"
#define TABLE_END	"</table>"

#define TYPE_DIRECTORY	0
#define TYPE_CHANNEL	1
#define TYPE_FORMAT	2

static gchar ns_dc_prefix[] = "dc";

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
static gint mapToCP[] = { 	RSS_CHANNEL_TITLE,		/* title */ 
				RSS_CHANNEL_MANAGINGEDITOR,	/* creator */
				RSS_CHANNEL_CATEGORY,		/* subject */
				RSS_CHANNEL_DESCRIPTION,	/* description */
				RSS_CHANNEL_WEBMASTER,	/* publisher */
				-1,			/* contributor */
				-1,			/* date (seldom used, e.g. slashdot, we map it to pubdate) */
				-1,			/* type */
				-1,			/* format */
				-1,			/* identifier */
				-1,			/* source */
				RSS_CHANNEL_LANGUAGE,	/* language */
				-1,			/* coverage */
				RSS_CHANNEL_COPYRIGHT	/* rights */
			  };

static gint mapToIP[] = { 	RSS_ITEM_TITLE,		/* title */ 
				-1,			/* creator */
				RSS_ITEM_CATEGORY,	/* subject */
				RSS_ITEM_DESCRIPTION,	/* description */
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
			  
static gint mapToPIECP[] = { 	PIE_FEED_TITLE,		/* title */ 
				-1,			/* creator */
				-1,			/* subject */
				PIE_FEED_DESCRIPTION,	/* description */
				-1,			/* publisher */
				-1,			/* contributor */	// FIXME: should be mapped, too!
				-1,			/* date (seldom used, e.g. slashdot, we map it to pubdate) */
				-1,			/* type */
				-1,			/* format */
				-1,			/* identifier */
				-1,			/* source */
				-1,			/* language */
				-1,			/* coverage */
				PIE_FEED_COPYRIGHT	/* rights */
			  };

static gint mapToPIEIP[] = { 	PIE_ENTRY_TITLE,	/* title */ 
				-1,			/* creator */
				-1,			/* subject */
				PIE_ENTRY_DESCRIPTION,	/* description */
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

/* some prototypes */
static gchar * ns_dc_doRSSItemFooterOutput(gpointer obj);
static gchar *	ns_dc_doRSSChannelFooterOutput(gpointer obj);
static gchar * ns_dc_doPIEEntryFooterOutput(gpointer obj);
static gchar *	ns_dc_doPIEFeedFooterOutput(gpointer obj);

gchar * ns_dc_getRSSNsPrefix(void) { return ns_dc_prefix; }
gchar * ns_dc_getPIENsPrefix(void) { return ns_dc_prefix; }
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

// FIXME: the parsing is stupid overkill! unify feed parsing by using setFeedTag callbacks !!!!

/* common OCS parsing */
static void ns_dc_parseOCSTag(gint type, gpointer p, xmlDocPtr doc, xmlNodePtr cur) {
	directoryPtr	dp = (directoryPtr)p;
	dirEntryPtr	dep = (dirEntryPtr)p;
	formatPtr	fp = (formatPtr)p;
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
							dp->tags[mapToDP[i]] = g_strdup(value);
							break;
						case TYPE_CHANNEL:
							dep->tags[mapToDP[i]] = g_strdup(value);
							break;
						case TYPE_FORMAT:
							fp->tags[mapToDP[i]] = g_strdup(value);
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

/* PIE channel parsing (basically the same as RSS parsing) */
static void ns_dc_parsePIEFeedTag(PIEFeedPtr cp, xmlDocPtr doc, xmlNodePtr cur) {
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
			i = convertDate(date);
			date = formatDate(i);
			if(NULL != date) {
				cp->tags[PIE_FEED_PUBDATE] = date;
			}
						
			cur = cur->next;		
			continue;
		}

		/* compare with each possible tag name */
		for(i = 0; taglist[i] != NULL; i++) {
	
			if(!xmlStrcmp((const xmlChar *)taglist[i], cur->name)) {
				value = (gchar *)xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				if(-1 == mapToPIECP[i]) {
					/* add it to the common DC value list, which is processed
					   by by ns_dc_output() */
					ns_dc_addInfoStruct(cp->nsinfos, taglist[i], value);
				} else {
					g_assert(mapToPIECP[i] <= PIE_FEED_MAX_TAG);
					/* map the value to one of the PIE fields */
					g_free(cp->tags[mapToPIECP[i]]);
					cp->tags[mapToPIECP[i]] = g_strdup(value);
				}
			}
		}
				
		cur = cur->next;
	}
}

static void ns_dc_parsePIEEntryTag(PIEEntryPtr ip,xmlDocPtr doc, xmlNodePtr cur) {
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
			ip->time = convertDate(date);
						
			cur = cur->next;		
			continue;
		}
	
		/* compare with each possible tag name */
		for(i = 0; taglist[i] != NULL; i++) {
	
			if(!xmlStrcmp((const xmlChar *)taglist[i], cur->name)) {
				value = (gchar *)xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				if(-1 == mapToPIEIP[i]) {
					/* add it to the common DC value list, which is processed
					   by by ns_dc_output() */
					ns_dc_addInfoStruct(ip->nsinfos, taglist[i], value);
				} else {
					g_assert(mapToPIEIP[i] <= PIE_ENTRY_MAX_TAG);
					/* map the value to one of the PIE fields */
					g_free(ip->tags[mapToPIEIP[i]]);
					ip->tags[mapToPIEIP[i]] = g_strdup(value);
				}
			}
		}
		
		cur = cur->next;
	}	
}


/* RSS channel parsing */
static void ns_dc_parseChannelTag(RSSChannelPtr cp, xmlDocPtr doc, xmlNodePtr cur) {
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
			i = convertDate(date);
			date = formatDate(i);
			if(NULL != date) {
				cp->tags[RSS_CHANNEL_PUBDATE] = (gchar *)date;
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
					g_assert(mapToCP[i] <= RSS_CHANNEL_MAX_TAG);
					/* map the value to one of the RSS fields */
					g_free(cp->tags[mapToCP[i]]);
					cp->tags[mapToCP[i]] = g_strdup(value);
				}
			}
		}
				
		cur = cur->next;
	}
}

static void ns_dc_parseItemTag(RSSItemPtr ip,xmlDocPtr doc, xmlNodePtr cur) {
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
			ip->time = convertDate(date);
						
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
					g_assert(mapToIP[i] <= RSS_ITEM_MAX_TAG);
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
	gchar 	**buffer = (gchar **)userdata;
	
	addToHTMLBuffer(buffer, FIRSTTD);
	addToHTMLBuffer(buffer, (gchar *)key);
	addToHTMLBuffer(buffer, NEXTTD);
	addToHTMLBuffer(buffer, (gchar *)value);
	addToHTMLBuffer(buffer, LASTTD);
}

static gchar * ns_dc_doFooterOutput(GHashTable *nsinfos) {
	GHashTable	*nsvalues;
	gchar		*buffer = NULL;
	
	/* we print all channel infos as a (key,value) table */
	if(NULL != (nsvalues = g_hash_table_lookup(nsinfos, (gpointer)ns_dc_prefix))) {
		addToHTMLBuffer(&buffer, TABLE_START);
		g_hash_table_foreach(nsvalues, ns_dc_output, (gpointer)&buffer);
		addToHTMLBuffer(&buffer, TABLE_END);			
	}
	
	return buffer;
}

static gchar * ns_dc_doRSSItemFooterOutput(gpointer obj) {

	if(NULL != obj)
		return ns_dc_doFooterOutput(((RSSItemPtr)obj)->nsinfos);
		
	return NULL;
}


static gchar *	ns_dc_doRSSChannelFooterOutput(gpointer obj) {
	
	if(NULL != obj)
		return ns_dc_doFooterOutput(((RSSChannelPtr)obj)->nsinfos);
		
	return NULL;
}

static gchar * ns_dc_doPIEEntryFooterOutput(gpointer obj) {

	if(NULL != obj)
		return ns_dc_doFooterOutput(((PIEEntryPtr)obj)->nsinfos);

	return NULL;
}


static gchar *	ns_dc_doPIEFeedFooterOutput(gpointer obj) {
	
	if(NULL != obj) {
		ns_dc_doFooterOutput(((PIEFeedPtr)obj)->nsinfos);
	}
}

RSSNsHandler *ns_dc_getRSSNsHandler(void) {
	RSSNsHandler 	*nsh;
	
	if(NULL != (nsh = (RSSNsHandler *)g_malloc(sizeof(RSSNsHandler)))) {
		nsh->parseChannelTag		= ns_dc_parseChannelTag;
		nsh->parseItemTag		= ns_dc_parseItemTag;
		nsh->doChannelHeaderOutput	= NULL;
		nsh->doChannelFooterOutput	= ns_dc_doRSSChannelFooterOutput;
		nsh->doItemHeaderOutput		= NULL;
		nsh->doItemFooterOutput		= ns_dc_doRSSItemFooterOutput;		
	}

	return nsh;
}

PIENsHandler *ns_dc_getPIENsHandler(void) {
	PIENsHandler 	*nsh;
	
	if(NULL != (nsh = (PIENsHandler *)g_malloc(sizeof(PIENsHandler)))) {
		nsh->parseChannelTag		= ns_dc_parsePIEFeedTag;
		nsh->parseItemTag		= ns_dc_parsePIEEntryTag;
		nsh->doChannelHeaderOutput	= NULL;
		nsh->doChannelFooterOutput	= ns_dc_doPIEFeedFooterOutput;
		nsh->doItemHeaderOutput		= NULL;
		nsh->doItemFooterOutput		= ns_dc_doPIEEntryFooterOutput;		
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
