/**
 * @file render.c generic XSLT rendering handling
 * 
 * Copyright (C) 2006-2007 Lars Lindner <lars.lindner@gmail.com>
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
#include "item.h"
#include "itemlist.h"
#include "itemset.h"
#include "render.h"
#include "ui/ui_htmlview.h"
#include "ui/ui_mainwindow.h"

static renderParamPtr	langParams = NULL;	/* the current locale settings (for localization stylesheet) */
static gchar		*defaultParams = NULL;	/* some default parameters (for rendering stylesheets) */

static GHashTable	*stylesheets = NULL;	/* XSLT stylesheet cache */

void render_init(void) {
	gchar   **shortlang = NULL;	/* e.g. "de" */
	gchar	**lang = NULL;		/* e.g. "de_AT" */

	if(langParams)
		render_parameter_free(langParams);
	if(defaultParams)
		g_free(defaultParams);

	/* prepare localization parameters */
	debug1(DEBUG_HTML, "XSLT localisation: setlocale(LC_MESSAGES, NULL) reports '%s'", setlocale(LC_MESSAGES, NULL));
	lang = g_strsplit(setlocale(LC_MESSAGES, NULL), "@", 0);
	shortlang = g_strsplit(setlocale(LC_MESSAGES, NULL), "_", 0);
	
	langParams = render_parameter_new();
	render_parameter_add(langParams, "lang='%s'", lang[0]);
	render_parameter_add(langParams, "shortlang='%s'", shortlang[0]);
	debug2(DEBUG_HTML, "XSLT localisation: lang='%s' shortlang='%s'", lang[0], shortlang[0]);

	g_strfreev(shortlang);
	g_strfreev(lang);
	
	/* prepare rendering default parameters */
	defaultParams = g_strdup_printf("search_link_enable='%s'", getBooleanConfValue(SEARCH_ENGINE_HIDE_LINK)?"false":"true");
	
	if(!stylesheets)
		stylesheets = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
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
	if(xslt)
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

	if(NULL == (resDoc = xsltApplyStylesheet(i18n_filter, xsltDoc, (const gchar **)langParams->params)))
		g_warning("fatal: applying localization stylesheet failed (%s)!", xsltName);
		
	//xsltSaveResultToFile(stdout, resDoc, i18n_filter);
	
	/* 3. create localized rendering stylesheet */
	if(NULL == (xslt = xsltParseStylesheetDoc(resDoc)))
		g_warning("fatal: could not load rendering stylesheet (%s)!", xsltName);
		
	xmlFreeDoc(xsltDoc);
	xsltFreeStylesheet(i18n_filter);

	g_hash_table_insert(stylesheets, g_strdup(xsltName), xslt);
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

/* Determining of theme colors, to be inserted in CSS */

static themeColorPtr render_get_theme_color(const gchar *name, GdkColor themeColor) {
	themeColorPtr	tc;
	gushort		r, g, b;
	
	r = themeColor.red / 256;
	g = themeColor.green / 256;
	b = themeColor.blue / 256;
	
	tc = g_new0(struct themeColor, 1);
	tc->name = name;
	tc->value = g_strdup_printf("%.2X%.2X%.2X", r, g, b);
	debug2(DEBUG_HTML, "theme color \"%s\" is %s", tc->name, tc->value);
	
	return tc;
}

static void
render_get_theme_colors (void)
{
	GtkWidget	*htmlview;
	GtkStyle	*style;
	GdkColor	*color;
	
	htmlview = liferea_htmlview_get_widget (ui_mainwindow_get_active_htmlview ());
	style = gtk_widget_get_style (htmlview);
	g_assert (NULL != style);

	g_assert (NULL == themeColors);
	themeColors = g_slist_append (themeColors, render_get_theme_color("GTK-COLOR-FG",    style->fg[GTK_STATE_NORMAL]));
	themeColors = g_slist_append (themeColors, render_get_theme_color("GTK-COLOR-BG",    style->bg[GTK_STATE_NORMAL]));
	themeColors = g_slist_append (themeColors, render_get_theme_color("GTK-COLOR-LIGHT", style->light[GTK_STATE_NORMAL]));
	themeColors = g_slist_append (themeColors, render_get_theme_color("GTK-COLOR-DARK",  style->dark[GTK_STATE_NORMAL]));
	themeColors = g_slist_append (themeColors, render_get_theme_color("GTK-COLOR-MID",   style->mid[GTK_STATE_NORMAL]));
	themeColors = g_slist_append (themeColors, render_get_theme_color("GTK-COLOR-BASE",  style->base[GTK_STATE_NORMAL]));
	themeColors = g_slist_append (themeColors, render_get_theme_color("GTK-COLOR-TEXT",  style->text[GTK_STATE_NORMAL]));
	
	/* the following code is GTK 2.10 only, but current dependency is 2.8
	color = NULL;
	gtk_widget_style_get (GTK_WIDGET (mainwindow), "link-color", &color, NULL);
	if (color) {
		themeColors = g_slist_append (themeColors, render_get_theme_color ("GTK-COLOR-NORMAL-LINK", *color));
		debug0 (DEBUG_HTML, "successfully set the color for links");
		g_free (color);
	}

	color = NULL;	
	gtk_widget_style_get(GTK_WIDGET(mainwindow), "visited-link-color", &color, NULL);
	if (color) {
		themeColors = g_slist_append(themeColors, render_get_theme_color("GTK-COLOR-VISITED-LINK", *color));
		debug0 (DEBUG_HTML, "successfully set the color for visited links");
		g_free (color);
	}
	*/
}

static gchar * render_set_theme_colors(gchar *css) {
	GSList	*iter = themeColors;
	
	while(iter) {
		themeColorPtr tc = (themeColorPtr)iter->data;
		css = common_strreplace(css, tc->name, tc->value);
		iter = g_slist_next(iter);
	}
	
	return css;
}

static void
render_remove_css_comments (void)
{
 	gssize i, len = css->len;
 	gchar *str = css->str;
 	GString *newcss;
 
 	newcss = g_string_new(NULL);
 	i = 0;
 	while (i < len) {
 		if (i+1 < len && str[i] == '/' && str[i+1] == '*') {
 			i += 4;
 			while (i-1 < len && (str[i-2] != '*' || str[i-1] != '/'))
 				i++;
 			continue;
 		}
 		if (i+1 < len && str[i] == '/' && str[i+1] == '/') {
 			i += 2;
 			while (i < len && str[i] != '\n')
 				i++;
 			continue;
 		}
 		newcss = g_string_append_c(newcss, str[i]);
 		i++;
 	}
 	g_string_free(css, TRUE);
 	css = newcss;
}

const gchar *
render_get_css (gboolean externalCss)
{	
	if(!css) {   
		gchar	*styleSheetFile, *defaultStyleSheetFile, *adblockStyleSheetFile;
		gchar	*font = NULL;
		gchar	*fontsize = NULL;
		gchar	*tmp;

		render_get_theme_colors();		

    		css = g_string_new(NULL);

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
			g_string_append(css, "body, table, div {");
			g_string_append_printf(css, "font-family: %s;\n", font);

			if(fontsize)
				g_string_append_printf(css, "font-size: %spt;\n", fontsize);

			g_free(font);
			g_string_append(css, "}\n");
		}	

		defaultStyleSheetFile = g_strdup(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "css" G_DIR_SEPARATOR_S "liferea.css");
		styleSheetFile = g_strdup_printf("%s" G_DIR_SEPARATOR_S "liferea.css", common_get_cache_path());

		if(g_file_get_contents(defaultStyleSheetFile, &tmp, NULL, NULL)) {
			tmp = render_set_theme_colors(tmp);
			g_string_append(css, tmp);
			g_free(tmp);
		}

		if(g_file_get_contents(styleSheetFile, &tmp, NULL, NULL)) {
			tmp = render_set_theme_colors(tmp);
			g_string_append(css, tmp);
			g_free(tmp);
		}

		g_free(defaultStyleSheetFile);
		g_free(styleSheetFile);

		adblockStyleSheetFile = g_strdup(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "css" G_DIR_SEPARATOR_S "adblock.css");

		if(g_file_get_contents(adblockStyleSheetFile, &tmp, NULL, NULL)) {
			g_string_append(css, tmp);
			g_free(tmp);
		}

		g_free(adblockStyleSheetFile);
		
		/* remove comments from the CSS to circumvent libgtkhtml2 freezing bug */
 		render_remove_css_comments();
		
		if(externalCss) {
			/* dump CSS to cache file and create a <style> tag to use the it */
			gchar *filename = common_create_cache_filename("cache", "style", "css");
			if(!g_file_set_contents(filename, css->str, -1, NULL))
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

gchar * render_xml(xmlDocPtr doc, const gchar *xsltName, renderParamPtr paramSet) {
	gchar			*output = NULL;
	xmlDocPtr		resDoc;
	xsltStylesheetPtr	xslt;
	xmlOutputBufferPtr	buf;
	
	if(NULL == (xslt = render_load_stylesheet(xsltName)))
		return NULL;

	if(!paramSet)
		paramSet = render_parameter_new();
	render_parameter_add(paramSet, "%s", defaultParams);

	if(NULL == (resDoc = xsltApplyStylesheet(xslt, doc, (const gchar **)paramSet->params))) {
		g_warning("fatal: applying rendering stylesheet (%s) failed!", xsltName);
		return NULL;
	}
	
	/* for debugging use: xsltSaveResultToFile(stdout, resDoc, xslt); */
	
	/* save results into return string */
	buf = xmlAllocOutputBuffer(NULL);
	if(-1 == xsltSaveResultTo(buf, resDoc, xslt))
		g_warning("fatal: retrieving result of rendering stylesheet failed (%s)!", xsltName);
		
	if(xmlBufferLength(buf->buffer) > 0)
		output = xmlCharStrdup(xmlBufferContent(buf->buffer));

	xmlOutputBufferClose(buf);
	xmlFreeDoc(resDoc);
	render_parameter_free(paramSet);
		
	/* Return only the body contents */
	if(output) {
		gchar *tmp = strstr(output, "<body>");
		if(tmp) {
			tmp = g_strdup(tmp + 6);
			xmlFree(output);
			output = tmp;
			tmp = strstr(output, "</body>");
			if(tmp)
				*tmp = 0;
		}
	}

	return output;
}

gchar * render_file(const gchar *filename, const gchar *xsltName, renderParamPtr paramSet) {
	xmlDocPtr	srcDoc;
	gchar		*output;

	if(NULL == (srcDoc = xmlParseFile(filename))) {
		g_warning("fatal: loading source XML (%s) failed", filename);
		return NULL;
	}

	output = render_xml(srcDoc, xsltName, paramSet);
	xmlFreeDoc(srcDoc);

	return output;
}

/* parameter handling */

renderParamPtr render_parameter_new(void) {

	return g_new0(struct renderParam, 1);
}

void render_parameter_add(renderParamPtr paramSet, const gchar *fmt, ...) {
	gchar	*new, *value, *name;
	va_list args;
	
	g_assert(NULL != fmt);
	g_assert(NULL != paramSet);
	
	va_start (args, fmt);
	new = g_strdup_vprintf (fmt, args);
	va_end (args);

	name = new;
	value = strchr(new, '=');
	g_assert(NULL != value);
	*value = 0;
	value++;
	
	paramSet->len += 2;
	paramSet->params = (gchar **)g_realloc(paramSet->params, (paramSet->len + 1)*sizeof(gchar *));
	paramSet->params[paramSet->len] = NULL;
	paramSet->params[paramSet->len-2] = g_strdup(name);
	paramSet->params[paramSet->len-1] = g_strdup(value);

	g_free(new);
}

void render_parameter_free(renderParamPtr paramSet) {

	g_strfreev(paramSet->params);
	g_free(paramSet);
}
