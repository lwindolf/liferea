/*
   content namespace support
   
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

#include "ns_content.h"
#include "common.h"

static gchar ns_content_prefix[] = "content";


/* a namespace documentation can be found at 
   http://web.resource.org/rss/1.0/modules/content/

   This namespace handler is (for now) only used to handle
   <content:encoding> tags. If such a tag appears the originial
   description will be replaced by the encoded content.
   
*/

static void ns_content_parseItemTag(RSSItemPtr ip, xmlNodePtr cur) {

  	if(!xmlStrcmp(cur->name, "encoded")) { 		
  		g_free(ip->tags[RSS_ITEM_DESCRIPTION]);
 		ip->tags[RSS_ITEM_DESCRIPTION] = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
  	}
}

gchar * ns_content_getRSSNsPrefix(void) { return ns_content_prefix; }

RSSNsHandler *ns_content_getRSSNsHandler(void) {
	RSSNsHandler 	*nsh;
	
	nsh = g_new0(RSSNsHandler, 1);
	nsh->parseChannelTag		= NULL;
	nsh->parseItemTag		= ns_content_parseItemTag;
	nsh->doChannelHeaderOutput	= NULL;
	nsh->doChannelFooterOutput	= NULL;
	nsh->doItemHeaderOutput		= NULL;
	nsh->doItemFooterOutput		= NULL;

	return nsh;
}
