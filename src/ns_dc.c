/**
 * @file ns_dc.c Dublin Core support for RSS, Atom and OCS
 *
 * Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#define _XOPEN_SOURCE 	/* glibc2 needs this (man strptime) */

#include "htmlview.h"

#include "ns_ocs.h"
#include "ns_dc.h"
#include "conf.h"
#include "support.h"
#include "common.h"
#include "ui_itemlist.h"
#include "metadata.h"

#define TABLE_START	"<div class=\"feedfoottitle\">additional Dublin Core information</div><table class=\"addfoot\">"
#define FIRSTTD		"<tr class=\"feedfoot\"><td class=\"feedfootname\"><span class=\"feedfootname\">"
#define NEXTTD		"</span></td><td class=\"feedfootvalue\"><span class=\"feedfootvalue\">"
#define LASTTD		"</span></td></tr>"
#define TABLE_END	"</table>"

#define TYPE_DIRECTORY	0
#define TYPE_CHANNEL	1
#define TYPE_FORMAT	2

#define DC_PREFIX	"dc"

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
   structure taglist, NULL means no mapping but extra type that must
   be registered explicitly! */
static gchar * mapToFeedMetadata[] = {
			 	"feedTitle",		/* title */ 
				NULL,			/* creator */
				"category",		/* subject */
				"description",		/* description */
				NULL,			/* publisher */
				"contributor",		/* contributor */
				NULL,			/* date (seldom used, e.g. slashdot) */
				NULL,			/* type */
				NULL,			/* format */
				NULL,			/* identifier */
				NULL,			/* source */
				"language",		/* language */
				NULL,			/* coverage */
				"copyright"		/* rights */
			  };

static gchar * mapToItemMetadata[] = {
			 	"itemTitle",		/* title */ 
				"creator",		/* creator */
				"category",		/* subject */
				"description",		/* description */
				"publisher",			/* publisher */
				"contributor",		/* contributor */				
				NULL,			/* date (won't be processed...) */
				NULL,			/* type */
				NULL,			/* format */
				NULL,			/* identifier */
				NULL,			/* source */
				"language",		/* language */
				NULL,			/* coverage */				
				"copyright"		/* rights */
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

/* common OCS parsing */
static void parseOCSTag(gint type, gpointer p, xmlNodePtr cur) {
	directoryPtr	dp = (directoryPtr)p;
	dirEntryPtr	dep = (dirEntryPtr)p;
	formatPtr	fp = (formatPtr)p;
	int 		i;
	gchar		*value;
	
	g_assert(NULL != cur);
	
	/* compare with each possible tag name */
	for(i = 0; taglist[i] != NULL; i++) {
		if(-1 != mapToDP[i]) {
			if(!xmlStrcmp((const xmlChar *)taglist[i], cur->name)) {
 				value = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));

				g_assert(mapToDP[i] < OCS_MAX_TAG);
				/* map the value to one of the RSS fields */
				switch(type) {
					case TYPE_DIRECTORY:
						dp->tags[mapToDP[i]] = value;
						break;
					case TYPE_CHANNEL:
						dep->tags[mapToDP[i]] = value;
						break;
					case TYPE_FORMAT:
						fp->tags[mapToDP[i]] = value;
						break;
					default:
						g_error(_("internal OCS namespace parsing error!"));
						g_free(value);
						break;
				}
				return;
			}
		}
	}
}

static void parseOCSDirectoryTag(gpointer dp, xmlNodePtr cur)	{ parseOCSTag(TYPE_DIRECTORY, dp, cur); }
static void parseOCSChannelTag(gpointer cp, xmlNodePtr cur)	{ parseOCSTag(TYPE_CHANNEL, cp, cur); }
static void parseOCSFormatTag(gpointer fp, xmlNodePtr cur)	{ parseOCSTag(TYPE_FORMAT, fp, cur); }

/* generic tag parsing (used for RSS and Atom) */
static void parse_tag(gpointer obj, xmlNodePtr cur, gboolean isFeedTag) {
	feedPtr		fp = (feedPtr)obj;
	itemPtr		ip = (itemPtr)obj;
	int 		i, j;
	gchar		*date, *mapping, *value, *tmp;
	gboolean	isNotEmpty;
	
	g_assert(NULL != cur);

	/* special handling for the ISO 8601 date item tags */
	if(!isFeedTag) {
		if(!xmlStrcmp((const xmlChar *)"date", cur->name)) {
 			date = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
 			if(NULL != date) {
				i = parseISO8601Date(date);
				ip->time = i;
				g_free(date);
			}
			return;
		}
	}

	/* compare with each possible tag name */
	for(i = 0; taglist[i] != NULL; i++) {
		if(!xmlStrcmp((const xmlChar *)taglist[i], cur->name)) {
 			value = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
	 		if(NULL != value) {
				/* check if value consist of whitespaces only */				
				for(j = 0, tmp = value, isNotEmpty = FALSE; j < g_utf8_strlen(value, -1); j++) {
					if(!g_unichar_isspace(*tmp)) {
						isNotEmpty = TRUE;
						break;
					}
					tmp = g_utf8_next_char(tmp);
				}

				if(isNotEmpty) {
					if(TRUE == isFeedTag) {
						if(NULL == (mapping = mapToFeedMetadata[i]))
							fp->metadata = metadata_list_append(fp->metadata, taglist[i], value);
						else 
							metadata_list_set(&(fp->metadata), mapping, value);
					} else {
						if(NULL == (mapping = mapToItemMetadata[i]))
							ip->metadata = metadata_list_append(ip->metadata, taglist[i], value);
						else
							metadata_list_set(&(ip->metadata), mapping, value);						
					}
				} 
				g_free(value);
			}			
			return;
		}
	}
}

static void parse_channel_tag(feedPtr fp, xmlNodePtr cur)	{ parse_tag((gpointer)fp, cur, TRUE); }
static void parse_item_tag(itemPtr ip, xmlNodePtr cur)		{ parse_tag((gpointer)ip, cur, FALSE); }

NsHandler *ns_dc_getRSSNsHandler(void) {
	NsHandler 	*nsh;
	
	nsh = g_new0(NsHandler, 1);
	nsh->prefix			= g_strdup("dc");
	nsh->parseChannelTag		= parse_channel_tag;
	nsh->parseItemTag		= parse_item_tag;

	return nsh;
}

OCSNsHandler *ns_dc_getOCSNsHandler(void) {
	OCSNsHandler 	*nsh;
	
	nsh = g_new0(OCSNsHandler, 1);
	nsh->prefix			= g_strdup("dc");
	nsh->parseDirectoryTag		= parseOCSDirectoryTag;
	nsh->parseDirEntryTag		= parseOCSChannelTag;
	nsh->parseFormatTag		= parseOCSFormatTag;				

	return nsh;
}
