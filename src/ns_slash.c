/**
 * @file ns_slash.c slash namespace support
 *
 * Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "ns_slash.h"
#include "common.h"

#define SLASH_START	"<table class=\"slash\" cellpadding=\"0\" cellspacing=\"0\"><tr><td class=\"slash\">"
#define KEY_START	"<span class=\"slashprop\">"
#define KEY_END		"</span> "
#define VALUE_START	"<span class=\"slashvalue\">"
#define VALUE_END	"</span> "
#define SLASH_END	"</td></tr></table>"

/* a tag list from http://f3.grp.yahoofs.com/v1/YP40P2oiXvP5CAx4TM6aQw8mDrCtNDwF9_BkMwcvulZHdlhYmCk5cS66_06t9OaIVsubWpwtMUTxYNG7/Modules/Proposed/mod_slash.html

   hmm... maybe you can find a somewhat shorter URL!

-------------------------------------------------------

 <item> Elements:

    * <slash:section> ( #PCDATA )
    * <slash:department> ( #PCDATA )
    * <slash:comments> ( positive integer )
    * <slash:hit_parade> ( comma-separated integers )

-------------------------------------------------------

*/

static gchar * taglist[] = {	"section",
				"department",				
				/* "comments",   disabled to avoid unread status after each feed update */
				/* "hitparade",*/
				NULL
			   };

static void parseItemTag(RSSItemPtr ip, xmlNodePtr cur) {
	gchar		*buffer = NULL;
	gchar		*tmp;
	int 		i;
	
	/* compare with each possible tag name */
	for(i = 0; taglist[i] != NULL; i++) {
		if(!xmlStrcmp((const xmlChar *)taglist[i], cur->name)) {
 			tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			if(NULL != tmp) {
				addToHTMLBuffer(&buffer, KEY_START);
				addToHTMLBuffer(&buffer, taglist[i]);
				addToHTMLBuffer(&buffer, KEY_END);
				addToHTMLBuffer(&buffer, VALUE_START);	
				addToHTMLBuffer(&buffer, tmp);
				addToHTMLBuffer(&buffer, VALUE_END);
				g_free(tmp);
	 			g_hash_table_insert(ip->nsinfos, g_strdup_printf("slash:%s", cur->name), buffer);
				return;
			}
		}
	}
}

static void doOutput(GHashTable *nsinfos, gchar **buffer, gchar *tagname) {
	gchar		*output;
	gchar		*key;
	
	g_assert(NULL != nsinfos);
	key = g_strdup_printf("slash:%s", tagname);
	
	if(NULL != (output = g_hash_table_lookup(nsinfos, key))) {
		addToHTMLBuffer(buffer, output);
		g_free(output);
		g_hash_table_remove(nsinfos, key);
	}
	g_free(key);
}

static gchar * doItemOutput(gpointer obj) {
	gchar	*buffer = NULL;
	gchar	*output = NULL;
	
	if(NULL != obj) {
		doOutput(((RSSItemPtr)obj)->nsinfos, &output, "section");
		doOutput(((RSSItemPtr)obj)->nsinfos, &output, "department");
		
		if(NULL != output) {
			addToHTMLBuffer(&buffer, SLASH_START);
			addToHTMLBuffer(&buffer, output);
			addToHTMLBuffer(&buffer, SLASH_END);
			g_free(output);
		}
	}	
	return buffer;
}

RSSNsHandler *ns_slash_getRSSNsHandler(void) {
	RSSNsHandler 	*nsh;
	
	nsh = g_new0(RSSNsHandler, 1);
	nsh->prefix			= "slash";
	nsh->parseItemTag		= parseItemTag;
	nsh->doItemHeaderOutput		= doItemOutput;

	return nsh;
}

