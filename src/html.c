/**
 * @file html.c HTML file handling / feed auto discovery
 * 
 * Copyright (C) 2004 ahmed el-helw <ahmedre@cc.gatech.edu>
 * Copyright (C) 2004-2007 Lars Lindner <lars.lindner@gmail.com>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#define _GNU_SOURCE


#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "html.h"
#include "xml.h"
#include "ui/ui_mainwindow.h"

enum {
	LINK_FAVICON,
	LINK_RSS_ALTERNATE,
	LINK_NORMAL_ANCHOR
};

static gchar *checkLinkRef(const gchar* str, gint linkType) {
	gchar	*res;
	const gchar	*tmp, *tmp2;
	size_t	len=0;
	gchar	quote; 
	
	/*debug1(DEBUG_PARSING, "checking link %s", str); */
	tmp = common_strcasestr(str, "href=");
	if(NULL == tmp) return NULL;
	tmp += 5;
	/* skip spaces up to the first quote. This is really slightly
	 wrong.  SGML allows unquoted atributes, but not if they contain
	 slashes, so 99% of all URIs will require quotes. */
	while (*tmp != '\"' &&
		  *tmp != '\'') {
		if (*tmp == '>' ||
		    *tmp == '\0' ||
		    !isspace(*tmp))
			return NULL;
		tmp++;
	}
	quote = *tmp; /* The type of quote mark used to delimit the arg */
	tmp++;
	tmp2 = tmp;
	while ((*tmp2 != quote && *(tmp2-1) != '\\') &&/* Escaped quote*/
		  *tmp2 != '\0')
		tmp2++, len++;

	res = g_strndup(tmp, len);

	if(linkType == LINK_FAVICON) {
		if(((NULL != common_strcasestr(str, "shortcut icon")) ||
		    (NULL != common_strcasestr(str, "icon"))) &&
		   ((NULL != common_strcasestr(str, "image/x-icon")) ||
		    (NULL != common_strcasestr(str, "image/png")) ||
		    (NULL != common_strcasestr(str, "image/gif"))))
			return res;
	} else if(linkType == LINK_RSS_ALTERNATE){
		if((common_strcasestr(str, "alternate")!=NULL) &&
		   ((common_strcasestr(str, "text/xml")!=NULL) || 
		    (common_strcasestr(str, "rss+xml")!=NULL) ||
		    (common_strcasestr(str, "rdf+xml")!=NULL) ||
		    (common_strcasestr(str, "atom+xml")!=NULL)))
			return res;
	} else if(linkType == LINK_NORMAL_ANCHOR){
		if((strstr(res, "rdf")) || 
		   (strstr(res, "xml")) ||
		   (strstr(res, "rss")) ||
		   (strstr(res, "atom")))
			return res;
	}
	g_free(res);
	return NULL;
}

static gchar *search_links(const gchar* data, gint linkType) {
	gchar	*ptr;
	const gchar	*tmp = data;
	gchar	*result = NULL;
	gchar	*res;
	gchar	*tstr;
	gchar	*endptr;
	
	while(1) {
		ptr = common_strcasestr(tmp, ((linkType != LINK_NORMAL_ANCHOR)? "<link " : "<a "));
		if(NULL == ptr)
			break;
		
		endptr = strchr(ptr, '>');
		*endptr = '\0';
		tstr = g_strdup(ptr);
		*endptr = '>';
		res = checkLinkRef(tstr, linkType);
		g_free(tstr);
		if(res != NULL){
			result = res;
			break;
/*		deactivated as long as we support only subscribing 
		to the first found link (BTW this code crashes on
		sites like Groklaw!)
		
			gchar* t;
			if(result == NULL)
				result = res;
			else {
				t = g_strdup_printf("%s\n%s", result, res);
				g_free(res);
				g_free(result);
				result = t;
			}*/
		}
		tmp = endptr;
	}
	result = unhtmlize(result); /* URIs can contain escaped things.... All ampersands must be escaped, for example */
	return result;
}

gchar * html_auto_discover_feed(const gchar* data, const gchar *baseUri) {
	gchar	*res, *tmp;

	debug0(DEBUG_UPDATE, "searching through link tags");
	res = search_links(data, LINK_RSS_ALTERNATE);
	debug1(DEBUG_UPDATE, "search result: %s", res? res : "none found");
	if(res == NULL) {
		debug0(DEBUG_UPDATE, "searching through href tags");
		res = search_links(data, LINK_NORMAL_ANCHOR);
		debug1(DEBUG_UPDATE, "search result: %s", res? res : "none found");
	}

	if(res == NULL) {
		ui_show_error_box(_("Feed link auto discovery failed! No feed links found!"));
	} else {
		/* turn relative URIs into absolute URIs */
		tmp = res;
		res = common_build_url(res, baseUri);
		g_free(tmp);
	}

	return res;
}

gchar * html_discover_favicon(const gchar* data, const gchar *baseUri) {
	gchar	*res, *tmp;

	debug0(DEBUG_UPDATE, "searching through link tags");
	res = search_links(data, LINK_FAVICON);
	debug1(DEBUG_UPDATE, "search result: %s", res? res : "none found");

	if (res != NULL) {
		/* turn relative URIs into absolute URIs */
		tmp = res;
		res = common_build_url(res, baseUri);
		g_free(tmp);
	}
	
	return res;
}
