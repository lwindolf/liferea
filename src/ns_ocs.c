/*
   OCS namespace support
   
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

#include "ns_ocs.h"
#include "ocs_ns.h"
#include "support.h"
#include "common.h"

#define TYPE_DIRECTORY	0
#define TYPE_CHANNEL	1
#define TYPE_FORMAT	2

static gchar ns_ocs_prefix[] = "ocs";

/* you can find the OCS specification at

   http://internetalchemy.org/ocs/directory.html 
 */

static gchar * taglist[] = {	"image",
				"format",
				"contentType",
				"updatePeriod",
				"updateFrequency",				
				"updateBase",
				"language",	/* only in OCS 0.3 */
				NULL
			   };

/* mapping of the tags specified by taglist to the backends channel
   structure taglist */
static gint mapTo[] = { 	OCS_IMAGE,		/* image */ 
				OCS_FORMAT,		/* format */
				OCS_CONTENTTYPE,	/* contentType */
				OCS_UPDATEPERIOD,	/* updatePeriod */
				OCS_UPDATEFREQUENCY,	/* updateFrequency */
				OCS_UPDATEBASE,		/* updateBase */
				OCS_LANGUAGE		/* language */
			  };

			  
gchar * ns_ocs_getOCSNsPrefix(void) { return ns_ocs_prefix; }

/* set the tag property of any OCS element (directory, channel, format)

   Note: this demands that each element types tags array has to be
         the same structure and content order... */
void ns_ocs_parseTag(gint type, gpointer p, xmlDocPtr doc, xmlNodePtr cur) {
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
			if(-1 != mapTo[i]) {			
				if(!xmlStrcmp((const xmlChar *)taglist[i], cur->name)) {
					value = (gchar *)xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
					g_assert(mapTo[i] < OCS_MAX_TAG);
					/* map the value to one of the RSS fields */
					switch(type) {
						case TYPE_DIRECTORY:
							dp->tags[mapTo[i]] = g_strdup(value);
							break;
						case TYPE_CHANNEL:
							dep->tags[mapTo[i]] = g_strdup(value);
							break;
						case TYPE_FORMAT:
							fp->tags[mapTo[i]] = g_strdup(value);
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

void ns_ocs_parseDirectoryTag(gpointer p, xmlDocPtr doc, xmlNodePtr cur) {
	ns_ocs_parseTag(TYPE_DIRECTORY, p, doc, cur);
}

void ns_ocs_parseChannelTag(gpointer p, xmlDocPtr doc, xmlNodePtr cur) {
	ns_ocs_parseTag(TYPE_CHANNEL, p, doc, cur);
}

void ns_ocs_parseFormatTag(gpointer p, xmlDocPtr doc, xmlNodePtr cur) {
	ns_ocs_parseTag(TYPE_FORMAT, p, doc, cur);
}

OCSNsHandler *ns_ocs_getOCSNsHandler(void) {
	OCSNsHandler 	*nsh;
	
	if(NULL != (nsh = (OCSNsHandler *)g_malloc(sizeof(OCSNsHandler)))) {
		nsh->parseDirectoryTag	= ns_ocs_parseDirectoryTag;
		nsh->parseDirEntryTag	= ns_ocs_parseChannelTag;
		nsh->parseFormatTag	= ns_ocs_parseFormatTag;				
	}

	return nsh;
}

