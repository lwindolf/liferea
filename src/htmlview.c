/*
   common interface for browser module implementations
   and module loading functions

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
#include <glib.h>
#include <gmodule.h>
#include "conf.h"
#include "ui_queue.h"
#include "support.h"
#include "htmlview.h"

/* function types for the imported symbols */
typedef gchar *	(*getModuleNameFunc)	(void);
typedef void	(*setupHTMLViewsFunc)	(GtkWidget *mainwindow, GtkWidget *pane1, GtkWidget *pane2, gint initialZoomLevel);
typedef void	(*setHTMLViewModeFunc)	(gboolean threePane);
typedef void	(*writeHTMLFunc)	(gchar *string);
typedef void	(*launchURLFunc)	(gchar *url);
typedef gfloat	(*getZoomLevelFunc)	(void);
typedef void	(*changeZoomLevelFunc)	(gfloat diff);

#define GETMODULENAME		0
#define SETUPHTMLVIEWS		1
#define SETHTMLVIEWMODE		2
#define WRITEHTML		3
#define LAUNCHURL		4
#define GETZOOMLEVEL		5
#define CHANGEZOOMLEVEL		6
#define MAXFUNCTIONS		7

static gchar *symbols[MAXFUNCTIONS] = {
	"getModuleName",
	"setupHTMLViews",
	"setHTMLViewMode",
	"writeHTML",
	"launchURL",
	"getZoomLevel",
	"changeZoomLevel"
};

static gpointer methods[MAXFUNCTIONS];

GSList * availableBrowserModules = NULL;

/* Method which tries to load the functions listed in the array
   symbols from the specified module name libname.  If testmode
   is true no error messages are issued. The function returns
   TRUE on success. */
static gboolean loadSymbols(gchar *libname, gboolean testmode) {
	gpointer	ptr;
	GModule 	*handle;
	gchar		*filename;
	int		i;
	
	filename = g_strdup_printf("%s%s%s", PACKAGE_LIB_DIR, G_DIR_SEPARATOR_S, libname);
	/* g_print(_("loading HTML widget module (%s)\n"), filename); */
		
	if((handle = g_module_open(filename, 0)) == NULL) {
		if(!testmode)
			g_warning(_("Failed to open HTML widget module (%s) specified in configuration!\n%s\n"), filename, g_module_error());
		return FALSE;
	}
	g_free(filename);
	
	for(i = 0; i < MAXFUNCTIONS; i++) {
		if(g_module_symbol(handle, symbols[i], &ptr)) {
			methods[i] = ptr;
		} else {
			if(!testmode)
				g_warning(_("Missing symbol \"%s\" in configured HTML module!"), symbols[i]);
			g_module_close(handle);
			return FALSE;
		}
	}
	return TRUE;
}

/* function to load the module specified by module */
void	initHTMLViewModule(void) {
	gboolean		success = FALSE;
	gint			filenamelen;
	gchar			*filename;
	struct browserModule	*info;
	GSList			*tmp;
	GError			*error  = NULL;
	GDir			*dir;

	/* Check to see if gmodule is supported */
	if(!g_module_supported())
		g_error(_("Cannot load HTML widget module (%s)!"), g_module_error());
	
	/* now we determine a list of all available modules
	   to present in the preferences dialog and to load
	   one just in case there was no configured module
	   or it did not load when trying... */	
	g_print("available browser modules (%s):\n", PACKAGE_LIB_DIR);
	dir = g_dir_open(PACKAGE_LIB_DIR, 0, &error);
	if(!error) {
		/* maybe no good solution, library name syntax: 
		   liblihtml<one letter code>.<library extension> */	
		filenamelen = 11 + strlen(G_MODULE_SUFFIX);
		filename = (gchar *)g_dir_read_name(dir);
		while(NULL != filename) {
			if((filenamelen == strlen(filename)) && (0 == strncmp("liblihtml", filename, 9))) {	
			   	/* now lets filter the files with correct library suffix */
				if(0 == strncmp(G_MODULE_SUFFIX, filename + 11, strlen(G_MODULE_SUFFIX))) {
					/* if we find one, try to load all symbols and if successful
					   add it to the available module list */
					if(TRUE == loadSymbols(filename, TRUE)) {
						if(NULL != (info = (struct browserModule *)g_malloc(sizeof(struct browserModule)))) {
							info->libname = g_strdup(filename);
							info->description = ((getModuleNameFunc)methods[GETMODULENAME])();
							availableBrowserModules = g_slist_append(availableBrowserModules, (gpointer)info);
							g_print("-> %s (%s)\n", info->description, info->libname);
						} else {
							g_error("could not allocate memory!");
						}
					}
				}
			}
			filename = (gchar *)g_dir_read_name(dir);
		}
		g_dir_close(dir);
	} else {
		g_warning("g_dir_open(%s) failed. Reason: %s\n", PACKAGE_LIB_DIR, error->message );
		g_error_free(error);
		error = NULL;
	}

	/* load configured module, we get a empty string if nothing is configured */
	filename = getStringConfValue(BROWSER_MODULE);
	if(0 != strlen(filename)) {
		success = loadSymbols(filename, FALSE);
	} else {
		g_print(_("No browser module configured!\n"));
		/* try to load one of the available modules */
		tmp = availableBrowserModules;
		while(NULL != tmp) {
			info = (struct browserModule *)tmp->data;
			g_print(_("trying to load browser module %s (%s)\n"), info->description, info->libname);
			if(TRUE == (success = loadSymbols(info->libname, FALSE)))
				break;
			tmp = g_slist_next(tmp);
		}
	}
	
	if(!success) {
		g_error(_("Sorry, I was not able to load any installed browser modules!"));
	}
}

void setupHTMLViews(GtkWidget *mainwindow, GtkWidget *pane, GtkWidget *pane2, gint initialZoomLevel) {

	((setupHTMLViewsFunc)methods[SETUPHTMLVIEWS])(mainwindow, pane, pane2, initialZoomLevel); 
}

void setHTMLViewMode(gboolean threePane) {

	((setHTMLViewModeFunc)methods[SETHTMLVIEWMODE])(threePane); 
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
    
	writeStyleSheetLink(buffer, PACKAGE_DATA_DIR "/" PACKAGE "/css/liferea.css");
    
	styleSheetFile = g_strdup_printf("%s/liferea.css", getCachePath());
	writeStyleSheetLink(buffer, styleSheetFile);
	g_free(styleSheetFile);
}

void startHTML(gchar **buffer, gboolean padded) { 
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

void writeHTML(gchar *string) { 

	if(!g_utf8_validate(string, -1, NULL))
		g_warning("Invalid encoded UTF8 string passed to HTML widget!");

	/* this is a dirty workaround for the GtkHTML problems with strange
	   HTML endings with special characters (caused by bugs in Liferea 
	   HTML generation) */
	((writeHTMLFunc)methods[WRITEHTML])(g_strdup_printf("%s                ",string));
}

void finishHTML(gchar **buffer) {

	addToHTMLBuffer(buffer, HTML_END); 
}

void clearHTMLView(void) {
	gchar	*buffer = NULL;

	startHTML(&buffer, FALSE);
	finishHTML(&buffer); 
	writeHTML(buffer);
}

void launchURL(gchar *url) {

	if(NULL != url) {		
		((launchURLFunc)methods[LAUNCHURL])(url); 
	} else {
		showErrorBox(_("This item does not have a link assigned!"));
	}
}

void changeZoomLevel(gfloat diff) {

	((changeZoomLevelFunc)methods[CHANGEZOOMLEVEL])(diff); 
}

gfloat getZoomLevel(void) {

	return ((getZoomLevelFunc)methods[GETZOOMLEVEL])(); 
}
