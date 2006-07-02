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

#include "render.h"

static gchar		**langParams = NULL;	/* the current locale settings */

static GHashTable	*stylesheets = NULL;	/* stylesheet cache */

static void render_init(void) {
	gchar   **shortlang = NULL;	/* e.g. "de" */
	gchar	**lang = NULL;		/* e.g. "de_AT" */

	lang = g_strsplit(g_strdup(setlocale(LC_ALL, NULL)), "@", 0);
	shortlang = g_strsplit(g_strdup(setlocale(LC_ALL, NULL)), "_", 0);
	
	langParams = g_strsplit(g_strjoin(NULL,"lang,'",lang[0],"',",
	                                       "shortlang,'",shortlang[0],"'",
	                                       NULL),",",0);

	g_strfreev(shortlang);
	g_strfreev(lang);

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
	if(xslt = (xsltStylesheetPtr)g_hash_table_lookup(stylesheets, xsltName))
		return xslt;
	
	/* or load and translate it... */
	
	/* 1. load localization stylesheet */
	if(NULL == (i18n_filter = xsltParseStylesheetFile(PACKAGE_DATA_DIR "/" PACKAGE "/xslt/i18n-filter.xslt"))) {
		g_warning("fatal: could not load localization stylesheet!");
		return NULL;
	}
	
	/* 2. load and localize the rendering stylesheet */
	filename = g_strjoin(NULL, PACKAGE_DATA_DIR "/" PACKAGE "/xslt/", xsltName, ".xml", NULL);
	
	if(NULL == (xsltDoc = xmlParseFile(filename)))
		g_warning("fatal: could not load rendering stylesheet (%s)!", xsltName);
		
	g_free(filename);

	if(NULL == (resDoc = xsltApplyStylesheet(i18n_filter, xsltDoc, langParams)))
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

gchar * render_xml(xmlDocPtr doc, const gchar *xsltName, const gchar **params) {
	gchar			*output = NULL;
	xmlDocPtr		resDoc;
	xsltStylesheetPtr	xslt;
	xmlOutputBufferPtr	buf;
	
	if(NULL == (xslt = render_load_stylesheet(xsltName)))
		return NULL;
		
	if(NULL == (resDoc = xsltApplyStylesheet(xslt, doc, NULL))) {
		g_warning("fatal: applying rendering stylesheet (%s) failed!", xsltName);
		return NULL;
	}
	
	//xsltSaveResultToFile(stdout, resDoc, xslt);
	
	/* save results into return string */
	buf = xmlAllocOutputBuffer(NULL);
	if(-1 == xsltSaveResultTo(buf, resDoc, xslt))
		g_warning("fatal: retrieving result of rendering stylesheet failed (%s)!", xsltName);
		
	if(xmlBufferLength(buf->buffer) > 0)
		output = xmlCharStrdup(xmlBufferContent(buf->buffer));

	xmlOutputBufferClose(buf);
	xmlFreeDoc(resDoc);
	
	return output;
}

gchar * render_file(const gchar *filename, const gchar *xsltName, const gchar **params) {
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
