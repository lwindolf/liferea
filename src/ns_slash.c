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
#include "htmlview.h"

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

static void parse_item_tag(itemPtr ip, xmlNodePtr cur) {
	gchar	*tmp = NULL, *section, *department;
	
	if(!xmlStrcmp(BAD_CAST"section", cur->name)) {
		if(NULL != (tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1))))
 			g_hash_table_insert(ip->tmpdata, "slash:section", tmp);
	} else if(!xmlStrcmp(BAD_CAST"department", cur->name)) {
		if(NULL != (tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1))))
 			g_hash_table_insert(ip->tmpdata, "slash:department", tmp);
	}
	
	if(NULL != tmp) {
		section = g_hash_table_lookup(ip->tmpdata, "slash:section");
		department = g_hash_table_lookup(ip->tmpdata, "slash:department");
		tmp = g_strdup_printf("%s,%s", section != NULL ? section : "",
						  department != NULL ? department : "" );
		metadata_list_set(&(ip->metadata), "slash", tmp);
	}
}
void ns_slash_render(gpointer data, struct displayset *displayset, gpointer user_data) {
	gchar	*section, *department;
	
	if(NULL != (department = strchr((gchar *)data, ','))) {
		department++;
		section = (gchar *)data;
		
		addToHTMLBuffer(&(displayset->head), SLASH_START);		
		if (section != NULL) {
			addToHTMLBuffer(&(displayset->head), KEY_START);
			addToHTMLBuffer(&(displayset->head), "section");
			addToHTMLBuffer(&(displayset->head), KEY_END);
			addToHTMLBuffer(&(displayset->head), VALUE_START);	
			addToHTMLBuffer(&(displayset->head), section);
			addToHTMLBuffer(&(displayset->head), VALUE_END);
		}
		if (department != NULL) {
			addToHTMLBuffer(&(displayset->head), KEY_START);
			addToHTMLBuffer(&(displayset->head), "department");
			addToHTMLBuffer(&(displayset->head), KEY_END);
			addToHTMLBuffer(&(displayset->head), VALUE_START);	
			addToHTMLBuffer(&(displayset->head), department);
			addToHTMLBuffer(&(displayset->head), VALUE_END);
		}
		addToHTMLBuffer(&(displayset->head), SLASH_END);
	}
}

NsHandler *ns_slash_getRSSNsHandler(void) {
	NsHandler 	*nsh;
	
	nsh = g_new0(NsHandler, 1);
	nsh->prefix			= "slash";
	nsh->parseItemTag		= parse_item_tag;

	return nsh;
}

