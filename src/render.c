/**
 * @file render.c generic XSLT rendering handling
 * 
 * Copyright (C) 2006 Lars Lindner <lars.lindner@gmx.net>
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

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>
#include <locale.h>
#include <string.h>

#include "conf.h"
#include "common.h"
#include "debug.h"
#include "itemlist.h"
#include "render.h"

static gchar		**langParams = NULL;	/* the current locale settings (for localization stylesheet) */
static gchar		*defaultParams = NULL;	/* some default parameters (for rendering stylesheets) */

static GHashTable	*stylesheets = NULL;	/* XSLT stylesheet cache */

void render_init(void) {
	gchar   **shortlang = NULL;	/* e.g. "de" */
	gchar	**lang = NULL;		/* e.g. "de_AT" */
	
	if(langParams)
		g_strfreev(langParams);
	if(defaultParams)
		g_free(defaultParams);

	/* prepare localization parameters */
	debug1(DEBUG_HTML, "XSLT localisation: setlocale(LC_MESSAGES, NULL) reports '%s'", setlocale(LC_MESSAGES, NULL));
	lang = g_strsplit(g_strdup(setlocale(LC_MESSAGES, NULL)), "@", 0);
	shortlang = g_strsplit(g_strdup(setlocale(LC_MESSAGES, NULL)), "_", 0);
	
	langParams = NULL;
	langParams = render_add_parameter(langParams, "lang='%s'", lang[0]);
	langParams = render_add_parameter(langParams, "shortlang='%s'", shortlang[0]);
	debug2(DEBUG_HTML, "XSLT localisation: lang='%s' shortlang='%s'", lang[0], shortlang[0]);

	g_strfreev(shortlang);
	g_strfreev(lang);
	
	/* prepare rendering default parameters */
	defaultParams = g_strdup_printf("search_link_enable='%s'", getBooleanConfValue(SEARCH_ENGINE_HIDE_LINK)?"false":"true");
	
	if(!stylesheets)
		stylesheets = g_hash_table_new(g_str_hash, g_str_equal);
}

xsltStylesheetPtr render_load_stylesheet(const gchar *xsltName) {
	xsltStylesheetPtr	i18n_filter;
	xsltStylesheetPtr	xslt;
	xmlDocPtr		xsltDoc, resDoc;
	gchar			*filename;
	
	if(!stylesheets)
		render_init();

	/* try to serve the stylesheet from the cache */	
	xslt = (xsltStylesheetPtr)g_hash_table_lookup(stylesheets, xsltName);
	if(NULL != xslt)
		return xslt;
	
	/* or load and translate it... */
	
	/* 1. load localization stylesheet */
	if(NULL == (i18n_filter = xsltParseStylesheetFile(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "xslt" G_DIR_SEPARATOR_S "i18n-filter.xslt"))) {
		g_warning("fatal: could not load localization stylesheet!");
		return NULL;
	}
	
	/* 2. load and localize the rendering stylesheet */
	filename = g_strjoin(NULL, PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "xslt" G_DIR_SEPARATOR_S, xsltName, ".xml", NULL);
	
	if(NULL == (xsltDoc = xmlParseFile(filename)))
		g_warning("fatal: could not load rendering stylesheet (%s)!", xsltName);
		
	g_free(filename);

	if(NULL == (resDoc = xsltApplyStylesheet(i18n_filter, xsltDoc, (const gchar **)langParams)))
		g_warning("fatal: applying localization stylesheet failed (%s)!", xsltName);
		
	//xsltSaveResultToFile(stdout, resDoc, i18n_filter);
	
	/* 3. create localized rendering stylesheet */
	if(NULL == (xslt = xsltParseStylesheetDoc(resDoc)))
		g_warning("fatal: could not load rendering stylesheet (%s)!", xsltName);
		
	xmlFreeDoc(xsltDoc);
	xsltFreeStylesheet(i18n_filter);

	g_hash_table_insert(stylesheets, (gpointer)xsltName, (gpointer)xslt);
	return xslt;
}

// FIXME: keep CSS in memory
GString * render_get_css(gboolean twoPane) {
	GString	*buffer;
	gchar	*font = NULL;
	gchar	*fontsize = NULL;
	gchar	*tmp;
	gchar	*styleSheetFile, *defaultStyleSheetFile, *adblockStyleSheetFile;
    
    	buffer = g_string_new("<style type=\"text/css\">\n<![CDATA[\n");
	
	/* font configuration support */
	font = getStringConfValue(USER_FONT);
	if(0 == strlen(font)) {
		g_free(font);
		font = getStringConfValue(DEFAULT_FONT);
	}

	if(font) {
		fontsize = font;
		/* the GTK2/GNOME font name format is <font name>,<font size in point>
		 Or it can also be "Font Name size*/
		strsep(&fontsize, ",");
		if(fontsize == NULL) {
			if(NULL != (fontsize = strrchr(font, ' '))) {
				*fontsize = '\0';
				fontsize++;
			}
		}
		g_string_append(buffer, "body, table, div {");
		g_string_append_printf(buffer, "font-family: %s;\n", font);
		
		if(fontsize)
			g_string_append_printf(buffer, "font-size: %spt;\n", fontsize);
		
		g_free(font);
		g_string_append(buffer, "}\n");
	}	

	if(twoPane) {
		defaultStyleSheetFile = g_strdup(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "css" G_DIR_SEPARATOR_S "liferea2.css");
		styleSheetFile = g_strdup_printf("%s" G_DIR_SEPARATOR_S "liferea2.css", common_get_cache_path());
	} else {
		defaultStyleSheetFile = g_strdup(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "css" G_DIR_SEPARATOR_S "liferea.css");
		styleSheetFile = g_strdup_printf("%s" G_DIR_SEPARATOR_S "liferea.css", common_get_cache_path());
	}
	
	if(g_file_get_contents(defaultStyleSheetFile, &tmp, NULL, NULL)) {
		g_string_append(buffer, tmp);
		g_free(tmp);
	}

	if(g_file_get_contents(styleSheetFile, &tmp, NULL, NULL)) {
		g_string_append(buffer, tmp);
		g_free(tmp);
	}
	
	g_free(defaultStyleSheetFile);
	g_free(styleSheetFile);
	
	adblockStyleSheetFile = g_strdup(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "css" G_DIR_SEPARATOR_S "adblock.css");
	
	if(g_file_get_contents(adblockStyleSheetFile, &tmp, NULL, NULL)) {
		g_string_append(buffer, tmp);
		g_free(tmp);
	}
	
	g_free(adblockStyleSheetFile);

	g_string_append(buffer, "\n]]>\n</style>\n");
	return buffer;
}

gchar * render_xml(xmlDocPtr doc, const gchar *xsltName, gchar **params) {
	gchar			*output = NULL;
	GString			*css;
	xmlDocPtr		resDoc;
	xsltStylesheetPtr	xslt;
	xmlOutputBufferPtr	buf;
	
	if(NULL == (xslt = render_load_stylesheet(xsltName)))
		return NULL;

	params = render_add_parameter(params, defaultParams);	// FIXME: merging would be better

	if(NULL == (resDoc = xsltApplyStylesheet(xslt, doc, (const gchar **)params))) {
		g_warning("fatal: applying rendering stylesheet (%s) failed!", xsltName);
		return NULL;
	}
	
	g_strfreev(params);
	//xsltSaveResultToFile(stdout, resDoc, xslt);
	
	/* save results into return string */
	buf = xmlAllocOutputBuffer(NULL);
	if(-1 == xsltSaveResultTo(buf, resDoc, xslt))
		g_warning("fatal: retrieving result of rendering stylesheet failed (%s)!", xsltName);
		
	if(xmlBufferLength(buf->buffer) > 0)
		output = xmlCharStrdup(xmlBufferContent(buf->buffer));

	xmlOutputBufferClose(buf);
	xmlFreeDoc(resDoc);
	
	/* Note: we need to do CSS insertion because GtkHTML2 does
	   not support  @import url(...) in <style> tags. Do not
	   add referenced CSS to the rendering stylesheets! */
	   
	css = render_get_css(itemlist_get_two_pane_mode());
	output = common_strreplace(output, "##STYLE_INSERT##", css->str);
	g_string_free(css, TRUE);
	
	return output;
}

gchar * render_file(const gchar *filename, const gchar *xsltName, gchar **params) {
	xmlDocPtr	srcDoc;
	gchar		*output;

	if(NULL == (srcDoc = xmlParseFile(filename))) {
		g_warning("fatal: loading source XML (%s) failed", filename);
		return NULL;
	}

	output = render_xml(srcDoc, xsltName, params);
	xmlFreeDoc(srcDoc);

	return output;
}

gchar ** render_add_parameter(gchar **params, const gchar *fmt, ...) {
	gchar	*old, *new, *merged;
	gchar	**newParams;
	va_list args;
	
	if(!fmt)
		return params;
	
	va_start (args, fmt);
	new = g_strdup_vprintf (fmt, args);
	va_end (args);
	
	if(params) {
		old = g_strjoinv(",", params);
		merged = g_strjoin(",", old, new, NULL);
		g_free(old);
		g_strfreev(params);
	} else {
		merged = g_strdup(new);
	}

	newParams = g_strsplit_set(merged, ",=", 0);
	
	g_free(merged);
	g_free(new);
	
	return newParams;
}
