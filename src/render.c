/**
 * @file render.c  generic GTK theme and XSLT rendering handling
 *
 * Copyright (C) 2006-2024 Lars Windolf <lars.windolf@gmx.de>
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
#include <math.h>
#include <string.h>
#include <time.h>

#include "conf.h"
#include "common.h"
#include "debug.h"
#include "item.h"
#include "itemset.h"
#include "render.h"
#include "xml.h"

/* Liferea renders items and feed info using self-generated HTML in a WebkitGTK
   widget. While this provides rendering flexibility. Starting with GTK 4 there
   is no access to theme colors anymore. So Liferea has to rely one the color
   defaults provided by WebkitGTK and to refrain from any background color 
   effects.
   
   To separate code and layout and to easily localize the layout it is 
   provided in the form of automake XSL stylesheet templates.

   Using automake translations are merged into those XSL stylesheets. On
   startup Liferea loads those expanded XSL stylesheets. During startup
   Liferea initially reduces the contained translations to the currently
   used ones by stripping all others from the XSL stylesheet using the
   localization stylesheet (xslt/i18n-filter.xslt). The resulting XSLT
   instance is kept in memory and used to render each items and feeds.

   The following code uses a hash table to maintain stylesheet instance
   and performs CSS adaptions to the current GTK theme. */

static renderParamPtr	langParams = NULL;	/* the current locale settings (for localization stylesheet) */
static GHashTable	*stylesheets = NULL;	/* XSLT stylesheet cache */

static void
render_parameter_free (renderParamPtr paramSet)
{
	g_strfreev (paramSet->params);
	g_free (paramSet);
}

static xsltStylesheetPtr
render_load_stylesheet (const gchar *xsltName)
{
	xsltStylesheetPtr	i18n_filter;
	xsltStylesheetPtr	xslt;
	xmlDocPtr		xsltDoc, resDoc;
	gchar			*filename;

	if (!langParams) {
		/* Prepare localization parameters */
		gchar   **shortlang = NULL;	/* e.g. "de" */
		gchar	**lang = NULL;		/* e.g. "de_AT" */

		debug (DEBUG_HTML, "XSLT localisation: setlocale(LC_MESSAGES, NULL) reports '%s'", setlocale(LC_MESSAGES, NULL));
		lang = g_strsplit (setlocale (LC_MESSAGES, NULL), "@", 0);
		shortlang = g_strsplit (setlocale (LC_MESSAGES, NULL), "_", 0);

		langParams = render_parameter_new ();
		render_parameter_add (langParams, "lang='%s'", lang[0]);
		render_parameter_add (langParams, "shortlang='%s'", shortlang[0]);
		debug (DEBUG_HTML, "XSLT localisation: lang='%s' shortlang='%s'", lang[0], shortlang[0]);

		g_strfreev (shortlang);
		g_strfreev (lang);
	}

	if (!stylesheets)
		stylesheets = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	/* try to serve the stylesheet from the cache */
	xslt = (xsltStylesheetPtr)g_hash_table_lookup (stylesheets, xsltName);
	if (xslt)
		return xslt;

	/* or load and translate it... */

	/* 1. load localization stylesheet */
	i18n_filter = xsltParseStylesheetFile ((const xmlChar *)PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "xslt" G_DIR_SEPARATOR_S "i18n-filter.xslt");
	if (!i18n_filter) {
		g_warning ("fatal: could not load localization stylesheet!");
		return NULL;
	}

	/* 2. load and localize the rendering stylesheet */
	filename = g_strjoin (NULL, PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "xslt" G_DIR_SEPARATOR_S, xsltName, ".xml", NULL);
	xsltDoc = xmlParseFile (filename);
	if (!xsltDoc)
		g_warning ("fatal: could not load rendering stylesheet (%s)!", xsltName);

	g_free (filename);

	resDoc = xsltApplyStylesheet (i18n_filter, xsltDoc, (const gchar **)langParams->params);
	if (!resDoc)
		g_warning ("fatal: applying localization stylesheet failed (%s)!", xsltName);

	/* Use the following to debug XSLT transformation problems */
	/* xsltSaveResultToFile (stdout, resDoc, i18n_filter); */

	/* 3. create localized rendering stylesheet */
	xslt = xsltParseStylesheetDoc(resDoc);
	if (!xslt)
		g_warning("fatal: could not load rendering stylesheet (%s)!", xsltName);

	xmlFreeDoc (xsltDoc);
	xsltFreeStylesheet (i18n_filter);

	g_hash_table_insert (stylesheets, g_strdup (xsltName), xslt);

	return xslt;
}

/** cached CSS definitions */
static gchar *css = NULL;
static gchar *userCss = NULL;

const gchar *
render_get_default_css (void)
{
	if (!css) {
		gchar *filename = g_build_filename (PACKAGE_DATA_DIR, PACKAGE, "css", "liferea.css", NULL);

		g_file_get_contents (filename, &css, NULL, NULL);
		g_free (filename);
	}

	return css;
}

const gchar *
render_get_user_css (void)
{
	if (!userCss) {
		gchar *filename = common_create_config_filename ("liferea.css");

		g_file_get_contents (filename, &userCss, NULL, NULL);
		g_free (filename);
	}

	return userCss;
}

gchar *
render_xml (xmlDocPtr doc, const gchar *xsltName, renderParamPtr paramSet)
{
	gchar			*output = NULL;
	xmlDocPtr		resDoc;
	xsltStylesheetPtr	xslt;
	xmlOutputBufferPtr	buf;

	xslt = render_load_stylesheet(xsltName);
	if (!xslt)
		return NULL;

	if (!paramSet)
		paramSet = render_parameter_new ();
	render_parameter_add (paramSet, "pixmapsDir='file://" PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S "'");

	resDoc = xsltApplyStylesheet (xslt, doc, (const gchar **)paramSet->params);
	if (!resDoc) {
		g_warning ("fatal: applying rendering stylesheet (%s) failed!", xsltName);
		return NULL;
	}

	/*
	   for XLST input debugging use:

		xmlChar *buffer;
		gint buffersize;
		xmlDocDumpFormatMemory(doc, &buffer, &buffersize, 1);
		printf("%s", (char *) buffer);

           for XSLT output debugging use:

           	xsltSaveResultToFile(stdout, resDoc, xslt);
         */

	/* save results into return string */
	buf = xmlAllocOutputBuffer (NULL);
	if (-1 == xsltSaveResultTo(buf, resDoc, xslt))
		g_warning ("fatal: retrieving result of rendering stylesheet failed (%s)!", xsltName);

#ifdef LIBXML2_NEW_BUFFER
	if (xmlOutputBufferGetSize (buf) > 0)
		output = (gchar *)xmlCharStrdup ((const char *)xmlOutputBufferGetContent (buf));
#else
	if (xmlBufferLength (buf->buffer) > 0)
		output = (gchar *)xmlCharStrdup ((const char *)xmlBufferContent(buf->buffer));
#endif

	xmlOutputBufferClose (buf);
	xmlFreeDoc (resDoc);
	render_parameter_free (paramSet);

	if (output) {
		gchar *tmp;

		/* Return only the body contents */
		tmp = strstr (output, "<body");
		if (tmp) {
			tmp = g_strdup (tmp);
			xmlFree (output);
			output = tmp;
			tmp = strstr (output, "</body>");
			if (tmp) {
				tmp += 7;
				*tmp = 0;
			}
		}
	}

	return output;
}

/* parameter handling */

renderParamPtr
render_parameter_new (void)
{
	return g_new0 (struct renderParam, 1);
}

void
render_parameter_add (renderParamPtr paramSet, const gchar *fmt, ...)
{
	gchar	*new, *value, *name;
	va_list args;

	g_assert (NULL != fmt);
	g_assert (NULL != paramSet);

	va_start (args, fmt);
	new = g_strdup_vprintf (fmt, args);
	va_end (args);

	name = new;
	value = strchr (new, '=');
	g_assert (NULL != value);
	*value = 0;
	value++;

	paramSet->len += 2;
	paramSet->params = (gchar **)g_realloc (paramSet->params, (paramSet->len + 1)*sizeof(gchar *));
	paramSet->params[paramSet->len] = NULL;
	paramSet->params[paramSet->len-2] = g_strdup (name);
	paramSet->params[paramSet->len-1] = g_strdup (value);

	g_free (new);
}
