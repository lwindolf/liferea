/**
 * @file htmlview.c common interface for browser module implementations
 * and module loading functions
 *
 * Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>
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

#include <string.h>
#include <glib.h>
#include <gmodule.h>
#include "common.h"
#include "conf.h"
#include "callbacks.h"
#include "ui_queue.h"
#include "support.h"
#include "ui_htmlview.h"
#include "ui_tabs.h"
#include "debug.h"

/* function types for the imported symbols */
typedef htmlviewPluginInfo* (*infoFunction)();
htmlviewPluginInfo *htmlviewInfo;

GSList *availableBrowserModules = NULL;

static GModule *handle;

extern GtkWidget *mainwindow;

extern char	*proxyname;
extern char	*proxyusername;
extern char	*proxypassword;
extern int	proxyport;

/* -------------------------------------------------------------------- */
/* module loading and initialisation					*/
/* -------------------------------------------------------------------- */

/* Method which tries to load the functions listed in the array
   symbols from the specified module name libname.  If testmode
   is true no error messages are issued. The function returns
   TRUE on success. */
static gboolean ui_htmlview_load_symbols(gchar *libname, gboolean testmode) {
	infoFunction	ptr;
	gchar		*filename;
	
	/* print some warnings concerning Mozilla */
	if((0 == strncmp(libname, "liblihtmlm", 10)) && !testmode) {
		g_print(_("\nTrying to load the Mozilla browser module... Note that this\n"
		          "might not work with every Mozilla version. If you have problems\n"
		          "and Liferea does not start, try to set MOZILLA_FIVE_HOME to\n"
		          "another Mozilla installation or delete the gconf configuration\n"
		          "key /apps/liferea/browser-module!\n\n"));
	}
	
	filename = g_strdup_printf("%s%s%s", PACKAGE_LIB_DIR, G_DIR_SEPARATOR_S, libname);
	/*g_print(_("loading HTML widget module (%s)\n"), filename);*/
	
	if((handle = g_module_open(filename, 0)) == NULL) {
		if(!testmode)
			g_warning(_("Failed to open HTML widget module (%s) specified in configuration!\n%s\n"), filename, g_module_error());
		else
			debug2(DEBUG_GUI, "Failed to open HTML widget module (%s) specified in configuration!\n%s\n", filename, g_module_error());
		g_free(filename);
		return FALSE;
	}
	g_free(filename);
	
	if(g_module_symbol(handle, "htmlview_plugin_getinfo", (void*)&ptr)) {
		htmlviewInfo = (*ptr)();
		if (htmlviewInfo->api_version != HTMLVIEW_API_VERSION) {
			if(!testmode)
				g_warning(_("Htmlview API mismatch!"));
			else
				debug0(DEBUG_GUI, "Htmlview API mismatch!");
			g_module_close(handle);
			return FALSE;
		}
	} else {
		if(!testmode)
			g_warning(_("Detected module is not a valid htmlview module!"));
		else
			debug0(DEBUG_GUI, "Detected module is not a valid htmlview module!");
		g_module_close(handle);
		return FALSE;
	}

	return TRUE;
}

/* function to load the module specified by module */
void ui_htmlview_init(void) {
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
	g_print(_("Available browser modules (%s):\n"), PACKAGE_LIB_DIR);
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
					if(TRUE == ui_htmlview_load_symbols(filename, TRUE)) {
						info = g_new0(struct browserModule, 1);
						info->libname = g_strdup(filename);
						info->description = g_strdup(htmlviewInfo->name);
						availableBrowserModules = g_slist_append(availableBrowserModules, (gpointer)info);
						g_print("-> %s (%s)\n", info->description, info->libname);
						g_module_close(handle);
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
		g_print(_("Loading configured browser module (%s)!\n"), filename);
		success = ui_htmlview_load_symbols(filename, FALSE);
	} else {
		g_print(_("No browser module configured!\n"));
	}
	g_free(filename);
	if(!success) {
		/* try to load one of the available modules */
		tmp = availableBrowserModules;
		while(NULL != tmp) {
			info = (struct browserModule *)tmp->data;
			g_print(_("trying to load browser module %s (%s)\n"), info->description, info->libname);
			if(TRUE == (success = ui_htmlview_load_symbols(info->libname, FALSE)))
				break;
			tmp = g_slist_next(tmp);
		}
	}
	
	if(success) {
		htmlviewInfo->init();
		ui_htmlview_set_proxy(proxyname, proxyport, proxyusername, proxypassword);
	} else
		g_error(_("Sorry, I was not able to load any installed browser modules! Try the --debug-all option to get debug information!"));
}

void ui_htmlview_deinit() {
	(htmlviewInfo->deinit)();
}

/* -------------------------------------------------------------------- */
/* browser module interface functions					*/
/* -------------------------------------------------------------------- */

GtkWidget *ui_htmlview_new() {
	GtkWidget *htmlview = htmlviewInfo->create();
	
	ui_htmlview_clear(htmlview);
	
	return htmlview;
}

static void ui_htmlview_write_css(gchar **buffer, gboolean twoPane) {
	gchar	*font = NULL;
	gchar	*fontsize = NULL;
	gchar	*tmp;
	gchar	*styleSheetFile, *defaultStyleSheetFile;
    
	addToHTMLBuffer(buffer,	"<style type=\"text/css\">\n"
				 "<!--\n");
	
	/* font configuration support */
	font = getStringConfValue(USER_FONT);
	if(0 == strlen(font)) {
		g_free(font);
		font = getStringConfValue(DEFAULT_FONT);
	}

	if(NULL != font) {
		fontsize = font;
		/* the GTK2/GNOME font name format is <font name>,<font size in point>
		 Or it can also be "Font Name size*/
		strsep(&fontsize, ",");
		if (fontsize == NULL) {
			if (NULL != (fontsize = strrchr(font, ' '))) {
				*fontsize = '\0';
				fontsize++;
			}
		}
		addToHTMLBuffer(buffer, "body, table, div {");

		addToHTMLBuffer(buffer, "font-family:");
		addToHTMLBuffer(buffer, font);
		addToHTMLBuffer(buffer, ";\n");
		
		if(NULL != fontsize) {
			addToHTMLBuffer(buffer, "font-size:");
			addToHTMLBuffer(buffer, fontsize);
			addToHTMLBuffer(buffer, "pt;\n");
		}		
		
		g_free(font);
		addToHTMLBuffer(buffer, "}\n");
	}	

	if(!twoPane) {
		addToHTMLBuffer(buffer, "body { style=\"padding:0px;\" }\n");
		defaultStyleSheetFile = g_strdup(PACKAGE_DATA_DIR "/" PACKAGE "/css/liferea2.css");
		styleSheetFile = g_strdup_printf("%s/liferea2.css", common_get_cache_path());
	} else {
		defaultStyleSheetFile = g_strdup(PACKAGE_DATA_DIR "/" PACKAGE "/css/liferea.css");
		styleSheetFile = g_strdup_printf("%s/liferea.css", common_get_cache_path());
	}
	
	if(g_file_get_contents(defaultStyleSheetFile, &tmp, NULL, NULL)) {
		addToHTMLBuffer(buffer, tmp);
		g_free(tmp);
	}

	if(g_file_get_contents(styleSheetFile, &tmp, NULL, NULL)) {
		addToHTMLBuffer(buffer, tmp);
		g_free(tmp);
	}

	g_free(defaultStyleSheetFile);
	g_free(styleSheetFile);
	
	addToHTMLBuffer(buffer, "\n//-->\n</style>\n");
}

void ui_htmlview_start_output(gchar **buffer, gboolean twoPane) { 
	
	addToHTMLBuffer(buffer, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n<html>\n");
	addToHTMLBuffer(buffer, "<head>\n<title></title>");
	addToHTMLBuffer(buffer, "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n");

	ui_htmlview_write_css(buffer, twoPane);
	
	addToHTMLBuffer(buffer, "</head>\n<body>");
}

void ui_htmlview_write(GtkWidget *htmlview,const gchar *string, const gchar *base) { 

	if(base == NULL)
		base = "file:///";	

	g_assert(htmlview != NULL);
	
	if(!g_utf8_validate(string, -1, NULL)) {
		gchar *buffer = g_strdup(string);
		
		/* Its really a bug if we get invalid encoded UTF-8 here!!! */
		g_warning("Invalid encoded UTF8 buffer passed to HTML widget!");
		
		/* to prevent crashes inside the browser */
		buffer = utf8_fix(buffer);
		(htmlviewInfo->write)(htmlview, buffer, base);
		g_free(buffer);
	} else
		(htmlviewInfo->write)(htmlview, string, base);
	
}

void ui_htmlview_finish_output(gchar **buffer) {

	addToHTMLBuffer(buffer, "</body></html>"); 
}

void ui_htmlview_clear(GtkWidget *htmlview) {
	gchar	*buffer = NULL;

	ui_htmlview_start_output(&buffer, FALSE);
	ui_htmlview_finish_output(&buffer); 
	ui_htmlview_write(htmlview, buffer, NULL);
	g_free(buffer);
}

void ui_htmlview_launch_URL(GtkWidget *htmlview, gchar *url, gboolean force_external) {
	
	if(NULL == url) {
		/* FIXME: bad because this is not only used for item links! */
		ui_show_error_box(_("This item does not have a link assigned!"));
		return;
	}
	
	debug3(DEBUG_GUI, "launch URL: %s  %s %s\n", getBooleanConfValue(BROWSE_INSIDE_APPLICATION)?"true":"false",
		  (htmlviewInfo->launchInsidePossible)()?"true":"false",
		  force_external?"true":"false");
	if(getBooleanConfValue(BROWSE_INSIDE_APPLICATION) &&
	   (htmlviewInfo->launchInsidePossible)() &&
	   !force_external) {
		(htmlviewInfo->launch)(htmlview, url);
	} else {
		ui_htmlview_launch_in_external_browser(url);
	}
}

void ui_htmlview_set_zoom(GtkWidget *htmlview, gfloat diff) {

	(htmlviewInfo->zoomLevelSet)(htmlview, diff); 
}

gfloat ui_htmlview_get_zoom(GtkWidget *htmlview) {

	return htmlviewInfo->zoomLevelGet(htmlview);
}

gboolean ui_htmlview_launch_in_external_browser(const gchar *uri) {
	GError		*error = NULL;
	gchar		*cmd, *tmp;
	gint		status;
	gboolean	done = FALSE;	
	
	cmd = prefs_get_browser_remotecmd();

	if(cmd != NULL) {
		if(NULL == strstr(cmd, "%s")) /* If there is no %s, then just append the URL */
			tmp = g_strdup_printf("%s %s", cmd, uri);
		else
			tmp = strreplace(cmd, "%s", uri);
		g_free(cmd);
		debug1(DEBUG_GUI, "Running the browser-remote command '%s'", tmp);
		g_spawn_command_line_sync(tmp, NULL, NULL, &status, &error);
		if((NULL != error) && (0 != error->code)) {
			ui_mainwindow_set_status_bar(_("Browser command failed: %s"), error->message);
			g_error_free(error);
		} else if (status == 0) {
			ui_mainwindow_set_status_bar(_("Starting: \"%s\""), tmp);
			done = TRUE;
		}
		
		g_free(tmp);
	}
	
	if(!done) {
		cmd = prefs_get_browser_cmd();
		g_assert(cmd != NULL);
		if(NULL == strstr(cmd, "%s")) /* If there is no %s, then just append the URL */
			tmp = g_strdup_printf("%s \"%s\"", cmd, uri);
		else
			tmp = strreplace(cmd, "%s", uri);
		g_free(cmd);
		debug1(DEBUG_GUI, "Running the browser command '%s'", tmp);
		g_spawn_command_line_async(tmp, &error);
		if((NULL != error) && (0 != error->code)) {
			ui_mainwindow_set_status_bar(_("Browser command failed: %s"), error->message);
			g_error_free(error);
		} else
			ui_mainwindow_set_status_bar(_("Starting: \"%s\""), tmp);
		g_free(tmp);
	}
	
	return TRUE;
}

gboolean ui_htmlview_scroll() {

	return htmlviewInfo->scrollPagedown(ui_mainwindow_get_active_htmlview());
}

void ui_htmlview_set_proxy(gchar *hostname, int port, gchar *username, gchar *password) {
	if (htmlviewInfo != NULL && htmlviewInfo->setProxy != NULL)
		htmlviewInfo->setProxy(hostname, port, username, password);
}

/* -------------------------------------------------------------------- */
/* htmlview callbacks 							*/
/* -------------------------------------------------------------------- */

void on_popup_launch_link_selected(gpointer url, guint callback_action, GtkWidget *widget) {

	ui_htmlview_launch_URL(ui_tabs_get_active_htmlview(), url, TRUE);
}

void on_popup_copy_url_selected(gpointer url, guint callback_action, GtkWidget *widget) {
	GtkClipboard *clipboard;

	clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	gtk_clipboard_set_text(clipboard, url, -1);
 
	clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(clipboard, url, -1);
	
	g_free(url);
}

void on_popup_subscribe_url_selected(gpointer url, guint callback_action, GtkWidget *widget) {

	ui_feedlist_new_subscription(url, NULL, FEED_REQ_SHOW_PROPDIALOG | FEED_REQ_RESET_TITLE | FEED_REQ_RESET_UPDATE_INT);
	g_free(url);
}

void on_popup_zoomin_selected(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	GtkWidget	*htmlview;
	gfloat		zoom;
	
	htmlview = ui_tabs_get_active_htmlview();
	zoom = ui_htmlview_get_zoom(htmlview);
	zoom *= 1.2;
	
	ui_htmlview_set_zoom(htmlview, zoom);
}

void on_popup_zoomout_selected(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	GtkWidget	*htmlview;
	gfloat		zoom;

	htmlview = ui_tabs_get_active_htmlview();	
	zoom = ui_htmlview_get_zoom(htmlview);
	zoom /= 1.2;
	
	ui_htmlview_set_zoom(htmlview, zoom);
}
