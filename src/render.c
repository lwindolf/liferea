/**
 * @file render.c  generic GTK theme and XSLT rendering handling
 *
 * Copyright (C) 2006-2018 Lars Windolf <lars.windolf@gmx.de>
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

#include "conf.h"
#include "common.h"
#include "debug.h"
#include "item.h"
#include "itemset.h"
#include "render.h"
#include "xml.h"
#include "ui/liferea_htmlview.h"

/* Liferea provides special screens and the item and the feed displays
   using self-generated HTML. To separate code and layout and to easily
   localize the layout it is provided by automake XSL stylesheet templates.

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

static void
render_init (void)
{
	gchar   	**shortlang = NULL;	/* e.g. "de" */
	gchar		**lang = NULL;		/* e.g. "de_AT" */
	gchar		*filename;

	if (langParams)
		render_parameter_free (langParams);

	/* Install default stylesheet if it does not yet exist */
	filename = common_create_config_filename ("liferea.css");
	if (!g_file_test (filename, G_FILE_TEST_EXISTS))
		common_copy_file (PACKAGE_DATA_DIR "/" PACKAGE "/css/user.css", filename);
	g_free(filename);

	/* Prepare localization parameters */
	debug1 (DEBUG_HTML, "XSLT localisation: setlocale(LC_MESSAGES, NULL) reports '%s'", setlocale(LC_MESSAGES, NULL));
	lang = g_strsplit (setlocale (LC_MESSAGES, NULL), "@", 0);
	shortlang = g_strsplit (setlocale (LC_MESSAGES, NULL), "_", 0);

	langParams = render_parameter_new ();
	render_parameter_add (langParams, "lang='%s'", lang[0]);
	render_parameter_add (langParams, "shortlang='%s'", shortlang[0]);
	debug2 (DEBUG_HTML, "XSLT localisation: lang='%s' shortlang='%s'", lang[0], shortlang[0]);

	g_strfreev (shortlang);
	g_strfreev (lang);

	if (!stylesheets)
		stylesheets = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
}

static xsltStylesheetPtr
render_load_stylesheet (const gchar *xsltName)
{
	xsltStylesheetPtr	i18n_filter;
	xsltStylesheetPtr	xslt;
	xmlDocPtr		xsltDoc, resDoc;
	gchar			*filename;

	if (!stylesheets)
		render_init ();

	/* try to serve the stylesheet from the cache */
	xslt = (xsltStylesheetPtr)g_hash_table_lookup (stylesheets, xsltName);
	if (xslt)
		return xslt;

	/* or load and translate it... */

	/* 1. load localization stylesheet */
	i18n_filter = xsltParseStylesheetFile (PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "xslt" G_DIR_SEPARATOR_S "i18n-filter.xslt");
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
static GString	*css = NULL;

/** widget background theme colors as 8bit HTML RGB code */
typedef struct themeColor {
	const gchar	*name;
	gchar		*value;
} *themeColorPtr;

static GSList *themeColors = NULL;
static gboolean darkTheme = FALSE;

/* Determining of theme colors, to be inserted in CSS */
static themeColorPtr
render_calculate_theme_color (const gchar *name, GdkColor themeColor)
{
	themeColorPtr	tc;
	gushort		r, g, b;

	r = themeColor.red / 256;
	g = themeColor.green / 256;
	b = themeColor.blue / 256;

	tc = g_new0 (struct themeColor, 1);
	tc->name = name;
	tc->value = g_strdup_printf ("%.2X%.2X%.2X", r, g, b);
	debug2 (DEBUG_HTML, "theme color \"%s\" is %s", tc->name, tc->value);

	return tc;
}

static gint
render_get_rgb_distance (GdkColor *c1, GdkColor *c2)
{
	return abs(
		(299 * c1->red/256 +
		 587 * c1->green/256 +
		 114 * c1->blue/256) -
		(299 * c2->red/256 +
		 587 * c2->green/256 +
		 114 * c2->blue/256)
	       ) / 1000;
}

static void
rgba_to_color (GdkColor *color, GdkRGBA *rgba)
{
	color->red   = lrint (rgba->red   * 65535);
	color->green = lrint (rgba->green * 65535);
	color->blue  = lrint (rgba->blue  * 65535);
}

void
render_init_theme_colors (GtkWidget *widget)
{
	GtkStyle	*style;
	GtkStyleContext	*sctxt;
	GdkColor	color;
	GdkRGBA		rgba;
	gint		textAvg, bgAvg;

	style = gtk_widget_get_style (widget);
	sctxt = gtk_widget_get_style_context (widget);

	g_assert (NULL == themeColors);
	themeColors = g_slist_append (themeColors, render_calculate_theme_color ("GTK-COLOR-FG",    style->fg[GTK_STATE_NORMAL]));
	themeColors = g_slist_append (themeColors, render_calculate_theme_color ("GTK-COLOR-BG",    style->bg[GTK_STATE_NORMAL]));
	themeColors = g_slist_append (themeColors, render_calculate_theme_color ("GTK-COLOR-LIGHT", style->light[GTK_STATE_NORMAL]));
	themeColors = g_slist_append (themeColors, render_calculate_theme_color ("GTK-COLOR-DARK",  style->dark[GTK_STATE_NORMAL]));
	themeColors = g_slist_append (themeColors, render_calculate_theme_color ("GTK-COLOR-MID",   style->mid[GTK_STATE_NORMAL]));

	/* Sanity check text+base color as this causes many problems on dark
	   themes. If brightness distance is not enough we set text to fg/bg
	   which is always safe. */
	if (render_get_rgb_distance (&style->base[GTK_STATE_NORMAL], &style->text[GTK_STATE_NORMAL]) > 150) {
		// FIXME: Use theme labels instead of GTK-COLOR-<something> (e.g. CSS-BACKGROUND)
		themeColors = g_slist_append (themeColors, render_calculate_theme_color ("GTK-COLOR-BASE", style->base[GTK_STATE_NORMAL]));
		themeColors = g_slist_append (themeColors, render_calculate_theme_color ("GTK-COLOR-TEXT", style->text[GTK_STATE_NORMAL]));
	} else {
		themeColors = g_slist_append (themeColors, render_calculate_theme_color ("GTK-COLOR-BASE", style->bg[GTK_STATE_NORMAL]));
		themeColors = g_slist_append (themeColors, render_calculate_theme_color ("GTK-COLOR-TEXT", style->fg[GTK_STATE_NORMAL]));
	}

	gtk_style_context_get_color (sctxt, GTK_STATE_FLAG_LINK, &rgba);
	rgba_to_color (&color, &rgba);
	themeColors = g_slist_append (themeColors, render_calculate_theme_color ("GTK-COLOR-NORMAL-LINK", color));

	gtk_style_context_get_color (sctxt, GTK_STATE_FLAG_VISITED, &rgba);
	rgba_to_color (&color, &rgba);
	themeColors = g_slist_append (themeColors, render_calculate_theme_color ("GTK-COLOR-VISITED-LINK", color));

	/* As there doesn't seem to be a safe way to determine wether we have a
	   dark GTK theme, let's guess it from the foreground vs. background
	   color average */

	textAvg = style->text[GTK_STATE_NORMAL].red / 256 +
	        style->text[GTK_STATE_NORMAL].green / 256 +
	        style->text[GTK_STATE_NORMAL].blue / 256;

	bgAvg = style->bg[GTK_STATE_NORMAL].red / 256 +
	        style->bg[GTK_STATE_NORMAL].green / 256 +
	        style->bg[GTK_STATE_NORMAL].blue / 256;

	if (textAvg > bgAvg) {
		debug0 (DEBUG_HTML, "Dark GTK theme detected.");
		darkTheme = TRUE;
	}

	if (darkTheme) {
		themeColors = g_slist_append (themeColors, render_calculate_theme_color ("FEEDLIST_UNREAD_BG", style->text[GTK_STATE_NORMAL]));
		/* Try nice foreground with 'fg' color (note: distance 50 is enough because it should be non-intrusive) */
		if (render_get_rgb_distance (&style->text[GTK_STATE_NORMAL], &style->fg[GTK_STATE_NORMAL]) > 50)
			themeColors = g_slist_append (themeColors, render_calculate_theme_color ("FEEDLIST_UNREAD_FG", style->fg[GTK_STATE_NORMAL]));
		else
			themeColors = g_slist_append (themeColors, render_calculate_theme_color ("FEEDLIST_UNREAD_FG", style->bg[GTK_STATE_NORMAL]));
	} else {
		themeColors = g_slist_append (themeColors, render_calculate_theme_color ("FEEDLIST_UNREAD_FG", style->bg[GTK_STATE_NORMAL]));
		/* Try nice foreground with 'dark' color (note: distance 50 is enough because it should be non-intrusive) */
		if (render_get_rgb_distance (&style->dark[GTK_STATE_NORMAL], &style->bg[GTK_STATE_NORMAL]) > 50)
			themeColors = g_slist_append (themeColors, render_calculate_theme_color ("FEEDLIST_UNREAD_BG", style->dark[GTK_STATE_NORMAL]));
		else
			themeColors = g_slist_append (themeColors, render_calculate_theme_color ("FEEDLIST_UNREAD_BG", style->fg[GTK_STATE_NORMAL]));
	}
}

static gchar *
render_set_theme_colors (gchar *css)
{
	GSList	*iter = themeColors;

	while (iter) {
		themeColorPtr tc = (themeColorPtr)iter->data;
		css = common_strreplace (css, tc->name, tc->value);
		iter = g_slist_next (iter);
	}

	return css;
}

const gchar *
render_get_theme_color (const gchar *name)
{
	GSList	*iter;

	if (!themeColors)
		return NULL;

	iter = themeColors;
	while (iter) {
		themeColorPtr tc = (themeColorPtr)iter->data;
		if (g_str_equal (name, tc->name))
			return tc->value;
		iter = g_slist_next (iter);
	}

	return NULL;
}

gboolean
render_is_dark_theme (void)
{
	if (!themeColors)
		return FALSE;

	return darkTheme;
}

const gchar *
render_get_css (gboolean externalCss)
{
	if (!css) {
		gchar	*defaultStyleSheetFile;
		gchar	*userStyleSheetFile;
		gchar	*adblockStyleSheetFile;
		gchar	*tmp;

		if (!themeColors)
			return NULL;

		css = g_string_new(NULL);

		defaultStyleSheetFile = g_build_filename (PACKAGE_DATA_DIR, PACKAGE, "css", "liferea.css", NULL);

		if (g_file_get_contents(defaultStyleSheetFile, &tmp, NULL, NULL)) {
			tmp = render_set_theme_colors(tmp);
			g_string_append(css, tmp);
			g_free(tmp);
		} else {
			g_error ("Loading %s failed.", defaultStyleSheetFile);
		}

		g_free(defaultStyleSheetFile);

		userStyleSheetFile = common_create_config_filename ("liferea.css");

		if (g_file_get_contents(userStyleSheetFile, &tmp, NULL, NULL)) {
			tmp = render_set_theme_colors(tmp);
			g_string_append(css, tmp);
			g_free(tmp);
		}

		g_free(userStyleSheetFile);

		adblockStyleSheetFile = g_build_filename(PACKAGE_DATA_DIR, PACKAGE, "css", "adblock.css", NULL);

		if (g_file_get_contents(adblockStyleSheetFile, &tmp, NULL, NULL)) {
			g_string_append(css, tmp);
			g_free(tmp);
		}

		g_free(adblockStyleSheetFile);

		if (externalCss) {
			/* dump CSS to cache file and create a <style> tag to use it */
			gchar *filename = common_create_cache_filename (NULL, "style", "css");
			if (!g_file_set_contents(filename, css->str, -1, NULL))
				g_warning("Cannot write temporary CSS file \"%s\"!", filename);

			g_string_free(css, TRUE);

			css = g_string_new("<style type=\"text/css\"> @import url(file://");
			g_string_append(css, filename);
			g_string_append(css, "); </style> ");

			g_free(filename);
		} else {
			/* keep the CSS in memory to serve it as a part of each HTML output */
			g_string_prepend(css, "<style type=\"text/css\">\n<![CDATA[\n");
			g_string_append(css, "\n]]>\n</style>\n");
		}
	}

	return css->str;
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
	render_parameter_add (paramSet, "jsDir='file://" PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "js" G_DIR_SEPARATOR_S "'");

	resDoc = xsltApplyStylesheet (xslt, doc, (const gchar **)paramSet->params);
	if (!resDoc) {
		g_warning ("fatal: applying rendering stylesheet (%s) failed!", xsltName);
		return NULL;
	}

	/* for debugging use: xsltSaveResultToFile(stdout, resDoc, xslt); */

	/* save results into return string */
	buf = xmlAllocOutputBuffer (NULL);
	if (-1 == xsltSaveResultTo(buf, resDoc, xslt))
		g_warning ("fatal: retrieving result of rendering stylesheet failed (%s)!", xsltName);

#ifdef LIBXML2_NEW_BUFFER
	if (xmlOutputBufferGetSize (buf) > 0)
		output = xmlCharStrdup (xmlOutputBufferGetContent (buf));
#else
	if (xmlBufferLength (buf->buffer) > 0)
		output = xmlCharStrdup (xmlBufferContent(buf->buffer));
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
