/**
 * @file html.c HTML file handling / feed auto discovery
 * 
 * Copyright (C) 2004 ahmed el-helw <ahmedre@cc.gatech.edu>
 * Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>
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

#include <stdlib.h>
#include <string.h>
#include "support.h"
#include "callbacks.h"
#include "debug.h"
#include "html.h"

static gchar *checkLinkRef(gchar* str) {
	gchar	*res;
	gchar	*tmp, *tmp2;

	tmp = strstr(str, "href=");
	if(NULL == tmp) tmp = strstr(str, "HREF=");
	if(NULL == tmp) return NULL;
	tmp2 = strchr(tmp, '\"');
	tmp = strchr(tmp2+1, '\"');
	*tmp = '\0';
	res = g_strdup(tmp2+1);
	*tmp = '\"';

	if((strstr(str, "alternate")!=NULL) &&
	   ((strstr(str, "text/xml")!=NULL) || strstr(str, "atom+xml")!=NULL))
		return res;
	g_free(res);
	return NULL;
}

static gchar *checkNormalLink(gchar* link) {
	gchar	*res, *tmp, *tmp2;
	
	tmp = strchr(link, '\"');
	tmp2 = strchr(tmp+1, '\"');
	*tmp2 = '\0';
	res = g_strdup(tmp+1);
	*tmp2 = '\"';

	if((strstr(res, "rdf")) || (strstr(res, "xml")) ||
	   (strstr(res, "rss")))
		return res;
	g_free(res);
	return NULL;
}

static gchar *search_links(gchar* data, int type) {
	gchar	*ptr;
	gchar	*tmp = data;
	gchar	*result = NULL;
	gchar	*res;
	gchar	*tstr;
	gchar	*endptr;
	
	while(1) {
		ptr = strstr(tmp, ((type == 0)? "<link" : "<a href"));
		if(NULL == ptr)
			ptr = strstr(tmp, ((type == 0)? "<LINK" : "<A HREF"));
		if(NULL == ptr)
			break;
		
		endptr = strchr(ptr, '>');
		*endptr = '\0';
		tstr = g_strdup(ptr);
		*endptr = '>';
		res = ((type==0)? checkLinkRef(tstr) : checkNormalLink(tstr));
		g_free(tstr);
		if(res != NULL){
			gchar* t;
			if(result == NULL)
				result = res;
			else {
				t = g_strdup_printf("%s\n%s", result, res);
				g_free(res);
				g_free(result);
				result = t;
			}
		}
		tmp = endptr;
	}
	return result;
}

gchar * html_auto_discover_feed(gchar* data) {
	int	f = 0;
	gchar	*res, *tmp;

	debug0(DEBUG_UPDATE, "searching through link tags");
	res = search_links(data, 0);
	debug1(DEBUG_UPDATE, "search result: %s", res? res : "none found");
	f = res? 1 : 0;
	debug0(DEBUG_UPDATE, "searching through href tags");
	res = search_links(data, 1);
	debug1(DEBUG_UPDATE, "search result: %s", res? res : "none found");
	if(!f) 
		f = res? 1 : 0;

	if(!f) {
		ui_show_error_box(_("Feed link auto discovery failed! No feed links found!"));
	} else {
		/* the result link maybe without protocol prefix but
		   we need the prefix... */
		if(NULL == strstr(res, "://")) {
			/* the following is to handle links like "//slashdot.org/index.rss" */
			tmp = res;
			while('/' == *tmp) tmp++;
			/* now add the prefix */
			tmp = g_strdup_printf("http://%s", tmp);
			g_free(res);
			res = tmp;
		}
	}

	return res;
}
