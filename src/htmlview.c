/*
   common interface for all HTML view implementations

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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <gmodule.h>
#include "ui_queue.h"
#include "support.h"
#include "htmlview.h"

/* function types for the imported symbols */
typedef void	(*setupHTMLViewsFunc)	(GtkWidget *mainwindow, GtkWidget *pane1, GtkWidget *pane2, gint initialZoomLevel);
typedef void	(*setHTMLViewModeFunc)	(gboolean threePane);
typedef void	(*writeHTMLFunc)	(gchar *string);
typedef void	(*launchURLFunc)	(gchar *url);
typedef gfloat	(*getZoomLevelFunc)	(void);
typedef void	(*changeZoomLevelFunc)	(gfloat diff);

/* structure to hold the function pointers */
static struct htmlStub {
	setupHTMLViewsFunc	setupHTMLViews;
	setHTMLViewModeFunc	setHTMLViewMode;
	writeHTMLFunc		writeHTML;
	launchURLFunc		launchURL;
	getZoomLevelFunc	getZoomLevel;
	changeZoomLevelFunc	changeZoomLevel;
} htmlStub;

struct html_module_info html_modules[] = {
	{ HTML_MODULE_GTKHTML,	"liblihtmlg",	"GtkHTML" },
	{ HTML_MODULE_TEXT,	"liblihtmlt",	"text view" },
	{ HTML_MODULE_MOZILLA,	"liblihtmlm",	"embedded Mozilla (experimental)" },
};

static gpointer getSymbol(GModule *handle, gchar *name) {
	gpointer symbol;

	if(!g_module_symbol(handle, name, &symbol)) {
		g_error("Unable to find symbol \"%s\"!", name);
	}	
	return symbol;
}

/* function to load the module specified by module */
void	loadHTMLViewModule(gint module) {
	GModule *handle;
	gchar	*filename;
	
	/* Check to see if gmodule is supported */
	if(!g_module_supported()) {
		g_error("Modules are not supported. Cannot load a HTML widget module!");
	}
	
	filename = g_strdup_printf("%s/%s.%s", PACKAGE_LIB_DIR, html_modules[module].libname, G_MODULE_SUFFIX);
	g_print("loading HTML widget module \"%s\" (%s)\n", 
		html_modules[module].description,
		filename);
		
	if((handle = g_module_open(filename, G_MODULE_BIND_LAZY)) == NULL) {
		g_error("Could not open module (%s) specified in configuration!", filename);
	}
	g_free(filename);
	
	memset(&htmlStub, 0, sizeof(struct htmlStub));
	htmlStub.setupHTMLViews	= (setupHTMLViewsFunc)getSymbol(handle, "setupHTMLViews");
	htmlStub.setHTMLViewMode = (setHTMLViewModeFunc)getSymbol(handle, "setHTMLViewMode");
	htmlStub.writeHTML	= (writeHTMLFunc)getSymbol(handle, "writeHTML");
	htmlStub.launchURL	= (launchURLFunc)getSymbol(handle, "launchURL");
	htmlStub.getZoomLevel	= (getZoomLevelFunc)getSymbol(handle, "getZoomLevel");
	htmlStub.changeZoomLevel = (changeZoomLevelFunc)getSymbol(handle, "changeZoomLevel");
}

void	setupHTMLViews(GtkWidget *mainwindow, GtkWidget *pane, GtkWidget *pane2, gint initialZoomLevel) {

	(*htmlStub.setupHTMLViews)(mainwindow, pane, pane2, initialZoomLevel); 
}

void	setHTMLViewMode(gboolean threePane) {

	(*htmlStub.setHTMLViewMode)(threePane); 
}

static void writeStyleSheetLink(gchar **buffer, gchar *styleSheetFile) {

	if(g_file_test(styleSheetFile, G_FILE_TEST_EXISTS)) {
		addToHTMLBuffer(buffer, "<link rel='stylesheet' type='text/css' href='file://");
		addToHTMLBuffer(buffer, styleSheetFile);
		addToHTMLBuffer(buffer, "'>");
	}
}

static void writeStyleSheetLinks(gchar **buffer) {
	gchar	*styleSheetFile;
    
	writeStyleSheetLink(buffer,
				PACKAGE_DATA_DIR "/" PACKAGE "/css/liferea.css");
    
	styleSheetFile = g_strdup_printf("%s/liferea.css", getCachePath());
	writeStyleSheetLink(buffer, styleSheetFile);
	g_free(styleSheetFile);
}

void	startHTML(gchar **buffer, gboolean padded) { 
	gchar	*encoding;

	addToHTMLBuffer(buffer, HTML_START);
	addToHTMLBuffer(buffer, HTML_HEAD_START);
	
	encoding = g_strdup_printf("%s%s%s", META_ENCODING1, "UTF-8", META_ENCODING2);	// FIXME
	addToHTMLBuffer(buffer, encoding);
	g_free(encoding);
        writeStyleSheetLinks(buffer);

	if(padded)
		addToHTMLBuffer(buffer, HTML_HEAD_END);
	else
		addToHTMLBuffer(buffer, HTML_HEAD_END2);	
}

void	writeHTML(gchar *string) { 

	if(!g_utf8_validate(string, -1, NULL))
		g_warning("Invalid encoded UTF8 string passed to HTML widget!");

	/* this is a dirty workaround for the GtkHTML problems with strange
	   HTML endings with special characters (caused by bugs in Liferea 
	   HTML generation) */
	(*htmlStub.writeHTML)(g_strdup_printf("%s                ",string));
}

void	finishHTML(gchar **buffer) {

	addToHTMLBuffer(buffer, HTML_END); 
}

void	clearHTMLView(void) {
	gchar	*buffer = NULL;

	startHTML(&buffer, FALSE);
	finishHTML(&buffer); 
	writeHTML(buffer);
}

void 	launchURL(gchar *url) {

	if(NULL != url) {		
		(*htmlStub.launchURL)(url); 
	} else {
		showErrorBox(_("This item does not have a link assigned!"));
	}
}

void	changeZoomLevel(gfloat diff) {

	(*htmlStub.changeZoomLevel)(diff); 
}

gfloat	getZoomLevel(void) {

	return (*htmlStub.getZoomLevel)(); 
}
