/*
   fallback HTML display module, which displays the item view
   contents in a text widget
   
   Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>  

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

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "htmlview.h"
#include "conf.h"
#include "support.h"
#include "callbacks.h"
#include "common.h"

extern GtkWidget	*mainwindow;

static GtkWidget	*itemView = NULL;
static GtkWidget	*itemListView = NULL;
static GtkWidget	*htmlwidget = NULL;

static gfloat		zoomLevel = 1.0;

gchar * getModuleName(void) {
	return g_strdup(_("Text Viewer (experimental)"));
}

/* function to write HTML source given as a UTF-8 string */
void writeHTML(gchar *string) {

	g_assert(NULL != htmlwidget);
	
	if(NULL == string) 
		string = g_strdup("");
	else
		string = unhtmlize(g_strdup(string));
		
	gtk_text_buffer_set_text(
		gtk_text_view_get_buffer(GTK_TEXT_VIEW(htmlwidget)),
		string, -1);
}

/* adds a differences diff to the actual zoom level */
void changeZoomLevel(gfloat diff) {

	ui_show_error_box("Sorry, not yet implemented for the Text Viewer!");
	zoomLevel += diff;
	// FIXME
}

/* returns the currently set zoom level */
gfloat getZoomLevel(void) { return zoomLevel; }

static void setupHTMLView(GtkWidget *mainwindow, GtkWidget *scrolledwindow) {
	
	if(NULL != htmlwidget) 
		gtk_widget_destroy(htmlwidget);
		

	/* create text view widget and pack it into the scrolled window */
	htmlwidget = gtk_text_view_new();
	gtk_container_add(GTK_CONTAINER(scrolledwindow), htmlwidget);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(htmlwidget), FALSE);
	gtk_text_view_set_pixels_inside_wrap(GTK_TEXT_VIEW(htmlwidget), 5);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(htmlwidget), 5);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(htmlwidget), 5);		  
	gtk_widget_show_all(scrolledwindow);
}

void setHTMLViewMode(gboolean threePane) {

	if(FALSE == threePane)
		setupHTMLView(mainwindow, itemListView);
	else
		setupHTMLView(mainwindow, itemView);

}

void setupHTMLViews(GtkWidget *mainwindow, GtkWidget *pane1, GtkWidget *pane2, gint initialZoomLevel) {

	itemView = pane1;
	itemListView = pane2;
	setHTMLViewMode(TRUE);
	if(0 != initialZoomLevel) {
		changeZoomLevel(((gfloat)initialZoomLevel)/100 - zoomLevel);
	}

	clearHTMLView();	
}

static void link_clicked(gchar *url) {
	GError	*error = NULL;
	gchar	*cmd, *tmp;

	if(2 == getNumericConfValue(GNOME_BROWSER_ENABLED))
		cmd = getStringConfValue(BROWSER_COMMAND);
	else
		cmd = g_strdup(GNOME_DEFAULT_BROWSER_COMMAND);
		
	g_assert(NULL != cmd);
	if(NULL == strstr(cmd, "%s")) {
		ui_show_error_box(_("There is no %%s URL place holder in the browser command string you specified in the preferences dialog!!!"));
	}
	tmp = g_strdup_printf(cmd, url);
	
	g_spawn_command_line_async(tmp, &error);
	if((NULL != error) && (0 != error->code)) {
		ui_mainwindow_set_status_bar(_("browser command failed: %s"), error->message);
		g_error_free(error);
	} else	
		ui_mainwindow_set_status_bar(_("starting: \"%s\""), tmp);
		
	g_free(cmd);
	g_free(tmp);
}

/* launches the specified URL */
void launchURL(gchar *url) {

	link_clicked(url);
}
