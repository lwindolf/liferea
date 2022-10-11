/**
 * @file ns_dc.c Dublin Core support for RSS and Atom
 *
 * Copyright (C) 2003-2022 Lars Windolf <lars.windolf@gmx.de>
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

#include "ns_dc.h"

#include "date.h"
#include "common.h"
#include "metadata.h"

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

static const gchar * taglist[] = {	"title",
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
static const gchar * mapToFeedMetadata[] = {
				 	"feedTitle",		/* title */ 
					"creator",		/* creator */
					"category",		/* subject */
					"description",		/* description */
					"publisher",		/* publisher */
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

static const gchar * mapToItemMetadata[] = {
				 	NULL,			/* title */ 
					"creator",		/* creator */
					"category",		/* subject */
					"description",		/* description */
					"publisher",		/* publisher */
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
			  

/* generic tag parsing (used for RSS and Atom) */
static void
parse_tag (feedParserCtxtPtr ctxt, xmlNodePtr cur, gboolean isFeedTag)
{
	int 		i, j;
	gchar		*date, *value, *tmp;
	const gchar	*mapping;
	gboolean	isNotEmpty;
	
	if (!isFeedTag) {
		/* special handling for the ISO 8601 date item tags */
		if (!xmlStrcmp (BAD_CAST "date", cur->name)) {
 			if (NULL != (date = (gchar *)xmlNodeListGetString (cur->doc, cur->xmlChildrenNode, 1))) {
				item_set_time (ctxt->item, date_parse_ISO8601 (date));
				g_free (date);
			}
			return;
		}

		/* special handling for item titles */
		if(!xmlStrcmp (BAD_CAST "title", cur->name)) {
			value = (gchar *)xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1);
			if(value) {
				item_set_title(ctxt->item, value);
				g_free(value);
			}
			return;
		}
	}

	/* compare with each possible tag name */
	for (i = 0; taglist[i] != NULL; i++) {
		if (!xmlStrcmp ((const xmlChar *)taglist[i], cur->name)) {
 			value = (gchar *)xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1);
	 		if (value) {
				/* check if value consist of whitespaces only */				
				for (j = 0, tmp = value, isNotEmpty = FALSE; j < g_utf8_strlen (value, -1); j++) {
					if (!g_unichar_isspace (*tmp)) {
						isNotEmpty = TRUE;
						break;
					}
					tmp = g_utf8_next_char (tmp);
				}

				if (isNotEmpty) {
					if (isFeedTag) {
						if (NULL != (mapping = mapToFeedMetadata[i]))
							ctxt->subscription->metadata = metadata_list_append (ctxt->subscription->metadata, mapping, value);
					} else {
						if (NULL != (mapping = mapToItemMetadata[i]))
							ctxt->item->metadata = metadata_list_append (ctxt->item->metadata, mapping, value);
					}
				} 
				g_free (value);
			}			
			return;
		}
	}
}

static void
parse_channel_tag (feedParserCtxtPtr ctxt, xmlNodePtr cur)
{
	parse_tag(ctxt, cur, TRUE);
}

static void
parse_item_tag (feedParserCtxtPtr ctxt, xmlNodePtr cur)
{
	parse_tag(ctxt, cur, FALSE);
}

static void
ns_dc_register_ns (NsHandler *nsh, GHashTable *prefixhash, GHashTable *urihash)
{
	g_hash_table_insert (prefixhash, "dc", nsh);
	g_hash_table_insert (urihash, "http://purl.org/dc/elements/1.1/", nsh);
	g_hash_table_insert (urihash, "http://purl.org/dc/elements/1.0/", nsh);
}

NsHandler *
ns_dc_get_handler (void)
{
	NsHandler 	*nsh;
	
	nsh = g_new0 (NsHandler, 1);
	nsh->prefix			= "dc";
	nsh->registerNs			= ns_dc_register_ns;
	nsh->parseChannelTag		= parse_channel_tag;
	nsh->parseItemTag		= parse_item_tag;
	
	return nsh;
}
