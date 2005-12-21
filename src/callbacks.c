/**
 * @file callbacks.c misc UI stuff
 *
 * Most of the GUI code is distributed over the ui_*.c
 * files but what didn't fit somewhere else stayed here.
 * 
 * Copyright (C) 2003-2005 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004-2005 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004 Christophe Barbe <christophe.barbe@ufies.org>	
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

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <string.h>

#include "debug.h"
#include "interface.h"
#include "support.h"
#include "callbacks.h"
#include "ui/ui_htmlview.h"
#include "ui/ui_tabs.h"
	
/*------------------------------------------------------------------------------*/
/* status bar callback, error box function					*/
/*------------------------------------------------------------------------------*/

void ui_show_error_box(const char *format, ...) {
	GtkWidget	*dialog;
	va_list		args;
	gchar		*msg;

	g_return_if_fail(format != NULL);

	va_start(args, format);
	msg = g_strdup_vprintf(format, args);
	va_end(args);
	
	dialog = gtk_message_dialog_new(GTK_WINDOW(mainwindow),
                  GTK_DIALOG_DESTROY_WITH_PARENT,
                  GTK_MESSAGE_ERROR,
                  GTK_BUTTONS_CLOSE,
                  "%s", msg);
	(void)gtk_dialog_run(GTK_DIALOG (dialog));
	gtk_widget_destroy(dialog);
	g_free(msg);
}

void ui_show_info_box(const char *format, ...) { 
	GtkWidget	*dialog;
	va_list		args;
	gchar		*msg;

	g_return_if_fail(format != NULL);

	va_start(args, format);
	msg = g_strdup_vprintf(format, args);
	va_end(args);
		
	dialog = gtk_message_dialog_new(GTK_WINDOW(mainwindow),
                  GTK_DIALOG_DESTROY_WITH_PARENT,
                  GTK_MESSAGE_INFO,
                  GTK_BUTTONS_CLOSE,
                  "%s", msg);
	(void)gtk_dialog_run(GTK_DIALOG (dialog));
	gtk_widget_destroy(dialog);
	g_free(msg);
}

/*------------------------------------------------------------------------------*/
/* exit handler									*/
/*------------------------------------------------------------------------------*/

void on_popup_quit(gpointer callback_data, guint callback_action, GtkWidget *widget) {

	(void)on_quit(NULL, NULL, NULL);
}

void on_about_activate(GtkMenuItem *menuitem, gpointer user_data) {
	
	gtk_widget_show(create_aboutdialog());
}

void on_homepagebtn_clicked(GtkButton *button, gpointer user_data) {

	/* launch the homepage when button in about dialog is pressed */
	ui_htmlview_launch_in_external_browser(_("http://liferea.sf.net"));
}

void on_topics_activate(GtkMenuItem *menuitem, gpointer user_data) {
	gchar *filename = g_strdup_printf("file://" PACKAGE_DATA_DIR "/" PACKAGE "/doc/html/%s", _("topics_en.html"));
	ui_tabs_new(filename, _("Help Topics"), TRUE);
	g_free(filename);
}


void on_quick_reference_activate(GtkMenuItem *menuitem, gpointer user_data) {
	gchar *filename = g_strdup_printf("file://" PACKAGE_DATA_DIR "/" PACKAGE "/doc/html/%s", _("reference_en.html"));
	ui_tabs_new(filename, _("Quick Reference"), TRUE);
	g_free(filename);
}

void on_faq_activate(GtkMenuItem *menuitem, gpointer user_data) {
	gchar *filename = g_strdup_printf("file://" PACKAGE_DATA_DIR "/" PACKAGE "/doc/html/%s", _("faq_en.html"));
	ui_tabs_new(filename, _("FAQ"), TRUE);
	g_free(filename);
}
