/*
   synndication namespace support
   
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

#include "rss_channel.h"
#include "ns_syn.h"

static gchar ns_syn_prefix[] = "syn";

/* some prototypes */
void ns_syn_parseChannelTag(RSSChannelPtr cp,xmlDocPtr doc, xmlNodePtr cur);

/* you can find the syn module documentation at
   http://web.resource.org/rss/1.0/modules/synndication/

   the tag list
   -------------------------------------------------------
    <syn:updatePeriod>
    <syn:updateFrequency>
    <syn:updateBase>
   -------------------------------------------------------
*/

gchar * ns_syn_getRSSNsPrefix(void) { return ns_syn_prefix; }

RSSNsHandler *ns_syn_getRSSNsHandler(void) {
	RSSNsHandler 	*nsh;
	
	if(NULL != (nsh = (RSSNsHandler *)g_malloc(sizeof(RSSNsHandler)))) {
		nsh->parseChannelTag		= ns_syn_parseChannelTag;
		nsh->parseItemTag		= NULL;
		nsh->doChannelHeaderOutput	= NULL;
		nsh->doChannelFooterOutput	= NULL;
		nsh->doItemHeaderOutput		= NULL;
		nsh->doItemFooterOutput		= NULL;
	}

	return nsh;
}

void ns_syn_parseChannelTag(RSSChannelPtr cp,xmlDocPtr doc, xmlNodePtr cur) {
	gchar	*tmp;
	int	period = 60*24;	/* daily is default */
	int	frequency = 1;
	
	if(!xmlStrcmp("updatePeriod", cur->name)) {

		if(NULL != (tmp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {

			if(0 == strcmp("hourly", tmp))
				period = 60;
			else if(0 == strcmp("daily", tmp))
				period = 60*24;
			else if(0 == strcmp("weekly", tmp))
				period = 7*24*60;
			else if(0 == strcmp("monthly", tmp))
				// FIXME: not really exact...
				period = 31*7*24*60;	
			else if(0 == strcmp("yearly", tmp))
				period = 365*24*60;

			g_free(tmp);
		}
	}

	if(!xmlStrcmp("updateFrequency", cur->name)) {

		if(NULL != (tmp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
			frequency = atoi(tmp);
		}
	}
	
	/* postprocessing */
	if(0 != frequency)
		period /= frequency;

	cp->updateInterval = period;
}
