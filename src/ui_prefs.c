/*
   program preferences

   Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>

#include "conf.h"
#include "interface.h"
#include "support.h"
#include "ui_prefs.h"
#include "ui_mainwindow.h"
#include "ui_tray.h"
#include "htmlview.h"
#include "callbacks.h"

extern GSList *availableBrowserModules;

static GtkWidget *prefdialog = NULL;

void on_browsermodule_changed(GtkObject *object, gchar *libname);

/*------------------------------------------------------------------------------*/
/* preferences dialog callbacks 						*/
/*------------------------------------------------------------------------------*/

void on_prefbtn_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*widget, *entry;
	GtkAdjustment	*itemCount;
	GSList		*list;
	gchar		*widgetname, *proxyport, *libname;
	gboolean	enabled;
	int		tmp, i;
	
	if(NULL == prefdialog || !G_IS_OBJECT(prefdialog))
		prefdialog = create_prefdialog ();		
	
	g_assert(NULL != prefdialog);

	/* ================= panel 1 "feed handling" ==================== */
	
	tmp = getNumericConfValue(GNOME_BROWSER_ENABLED);
	if((tmp > 2) || (tmp < 1)) 
		tmp = 1;	/* correct configuration if necessary */
		
	entry = lookup_widget(prefdialog, "browsercmd");
	gtk_entry_set_text(GTK_ENTRY(entry), getStringConfValue(BROWSER_COMMAND));
	gtk_widget_set_sensitive(GTK_WIDGET(entry), tmp==2);

	/* Set fields in the radio widgets so that they know their option # and the pref dialog */
	for(i = 1; i <= 2; i++) {
		widgetname = g_strdup_printf("%s%d", "browserradiobtn", i);
		widget = lookup_widget(prefdialog, widgetname);
		gtk_object_set_data(GTK_OBJECT(widget), "option_number", GINT_TO_POINTER(i));
		gtk_object_set_data(GTK_OBJECT(widget), "entry", entry);
		g_free(widgetname);
	}

	widgetname = g_strdup_printf("%s%d", "browserradiobtn", tmp);
	widget = lookup_widget(prefdialog, widgetname);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
	g_free(widgetname);		

	/* Time format */
	tmp = getNumericConfValue(TIME_FORMAT_MODE);
	if((tmp > 3) || (tmp < 1)) 
		tmp = 1;	/* correct configuration if necessary */

	entry = lookup_widget(prefdialog, "timeformatentry");
	gtk_entry_set_text(GTK_ENTRY(entry), getStringConfValue(TIME_FORMAT));
	gtk_widget_set_sensitive(GTK_WIDGET(entry), tmp==3);


	/* Set fields in the radio widgets so that they know their option # and the pref dialog */
	for(i = 1; i <= 3; i++) {
		widgetname = g_strdup_printf("%s%d", "timeradiobtn", i);
		widget = lookup_widget(prefdialog, widgetname);
		gtk_object_set_data(GTK_OBJECT(widget), "option_number", GINT_TO_POINTER(i));
		gtk_object_set_data(GTK_OBJECT(widget), "entry", entry);
		g_free(widgetname);
	}

	widgetname = g_strdup_printf("%s%d", "timeradiobtn", tmp);
	widget = lookup_widget(prefdialog, widgetname);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
	g_free(widgetname);

	widget = lookup_widget(prefdialog, "updatealloptionbtn");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), getBooleanConfValue(UPDATE_ON_STARTUP));
	
	widget = lookup_widget(prefdialog, "helpoptionbtn");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), getBooleanConfValue(DISABLE_HELPFEEDS));
	
	widget = lookup_widget(prefdialog, "itemCountBtn");
	itemCount = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(widget));
	gtk_adjustment_set_value(itemCount, getNumericConfValue(DEFAULT_MAX_ITEMS));

	/* ================== panel 2 "notification settings" ================ */
	
	widget = lookup_widget(prefdialog, "trayiconoptionbtn");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), getBooleanConfValue(SHOW_TRAY_ICON));

	/* menu / tool bar settings */	
	for(i = 1; i <= 3; i++) {
		/* Set fields in the radio widgets so that they know their option # and the pref dialog */
		widgetname = g_strdup_printf("%s%d", "menuradiobtn", i);
		widget = lookup_widget(prefdialog, widgetname);
		gtk_object_set_data(GTK_OBJECT(widget), "option_number", GINT_TO_POINTER(i));
		gtk_object_set_data(GTK_OBJECT(widget), "entry", entry);
		g_free(widgetname);
	}

	/* select currently active menu option */
	tmp = 1;
	if(getBooleanConfValue(DISABLE_TOOLBAR)) tmp = 2;
	if(getBooleanConfValue(DISABLE_MENUBAR)) tmp = 3;

	widgetname = g_strdup_printf("%s%d", "menuradiobtn", tmp);
	widget = lookup_widget(prefdialog, widgetname);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
	g_free(widgetname);
	
	/* set up the browser module option menu */
	tmp = i = 0;
	widget = gtk_menu_new();
	list = availableBrowserModules;
	libname = getStringConfValue(BROWSER_MODULE);
	while(NULL != list) {
		g_assert(NULL != list->data);
		entry = gtk_menu_item_new_with_label(((struct browserModule *)list->data)->description);
		gtk_widget_show(entry);
		gtk_container_add(GTK_CONTAINER(widget), entry);
		gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(on_browsermodule_changed), ((struct browserModule *)list->data)->libname);
		if(0 == strcmp(libname, ((struct browserModule *)list->data)->libname))
			tmp = i;
		i++;
		list = g_slist_next(list);
	}
	gtk_menu_set_active(GTK_MENU(widget), tmp);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(lookup_widget(prefdialog, "htmlviewoptionmenu")), widget);
	
	/* ================= panel 3 "proxy settings" ======================== */
	
	enabled = getBooleanConfValue(USE_PROXY);
	widget = lookup_widget(prefdialog, "enableproxybtn");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), enabled);
	
	entry = lookup_widget(prefdialog, "proxyhostentry");
	gtk_entry_set_text(GTK_ENTRY(entry), getStringConfValue(PROXY_HOST));
	gtk_widget_set_sensitive(GTK_WIDGET(entry), enabled);
	
	entry = lookup_widget(prefdialog, "proxyportentry");
	proxyport = g_strdup_printf("%d", getNumericConfValue(PROXY_PORT));
	gtk_entry_set_text(GTK_ENTRY(entry), proxyport);
	g_free(proxyport);
	gtk_widget_set_sensitive(GTK_WIDGET(entry), enabled);	
	
	
	gtk_widget_show(prefdialog);
}

/*------------------------------------------------------------------------------*/
/* preference callbacks 							*/
/*------------------------------------------------------------------------------*/
void on_updatealloptionbtn_clicked(GtkButton *button, gpointer user_data) {
	setBooleanConfValue(UPDATE_ON_STARTUP, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)));
}


void on_trayiconoptionbtn_clicked(GtkButton *button, gpointer user_data) {
	setBooleanConfValue(SHOW_TRAY_ICON, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)));
	updateTrayIcon();
}


void on_browserselection_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget *editbox = gtk_object_get_data(GTK_OBJECT(button), "entry");
	int active_button = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(button),"option_number"));

	setNumericConfValue(GNOME_BROWSER_ENABLED, active_button);
	gtk_widget_set_sensitive(GTK_WIDGET(editbox), active_button == 2);
}


void on_browsercmd_changed(GtkEditable *editable, gpointer user_data) {
	setStringConfValue(BROWSER_COMMAND, gtk_editable_get_chars(editable,0,-1));
}

void on_browsermodule_changed(GtkObject *object, gchar *libname) {
	setStringConfValue(BROWSER_MODULE, libname);
}


void on_timeformatselection_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget *editbox = gtk_object_get_data(GTK_OBJECT(button), "entry");
	int active_button = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(button),"option_number"));

	setNumericConfValue(TIME_FORMAT_MODE, active_button);
	gtk_widget_set_sensitive(GTK_WIDGET(editbox), active_button == 3);

	// FIXME there is a better way to do this.... Same goes for the next function
	ui_redraw_widget("mainwindow");
}

void on_timeformatentry_changed(GtkEditable *editable, gpointer user_data) {
	setStringConfValue(TIME_FORMAT, gtk_editable_get_chars(editable,0,-1));
	ui_redraw_widget("mainwindow");
}

void on_itemCountBtn_value_changed(GtkSpinButton *spinbutton, gpointer user_data) {
	GtkAdjustment	*itemCount;
	itemCount = gtk_spin_button_get_adjustment(spinbutton);
	setNumericConfValue(DEFAULT_MAX_ITEMS, gtk_adjustment_get_value(itemCount));
}

void on_menuselection_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*editbox;
	gint		active_button;
	
	editbox = gtk_object_get_data(GTK_OBJECT(button), "entry");
	active_button = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(button), "option_number"));

	switch(active_button) {
		case 1:
			setBooleanConfValue(DISABLE_MENUBAR, FALSE);
			setBooleanConfValue(DISABLE_TOOLBAR, FALSE);
			break;
		case 2:
			setBooleanConfValue(DISABLE_MENUBAR, FALSE);
			setBooleanConfValue(DISABLE_TOOLBAR, TRUE);
			break;
		case 3:
			setBooleanConfValue(DISABLE_MENUBAR, TRUE);
			setBooleanConfValue(DISABLE_TOOLBAR, FALSE);
			break;
		default:
			break;
	}
	
	ui_mainwindow_update_menubar();
	ui_mainwindow_update_toolbar();

	// FIXME there is a better way to do this.... 
	ui_redraw_widget("mainwindow");
}


void on_helpoptionbtn_clicked(GtkButton *button, gpointer user_data) {
	setBooleanConfValue(DISABLE_HELPFEEDS, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)));
}


void on_enableproxybtn_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*entry;
	gboolean	enabled;

	enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
	setBooleanConfValue(USE_PROXY, enabled);
	
	if(NULL != (entry = lookup_widget(prefdialog, "proxyhostentry")))
		gtk_widget_set_sensitive(GTK_WIDGET(entry), enabled);
	if(NULL != (entry = lookup_widget(prefdialog, "proxyportentry")))
		gtk_widget_set_sensitive(GTK_WIDGET(entry), enabled);

	loadConfig();
	ui_redraw_widget("mainwindow");
}


void on_proxyhostentry_changed(GtkEditable *editable, gpointer user_data) {

	setStringConfValue(PROXY_HOST, gtk_editable_get_chars(editable,0,-1));
	loadConfig();
	ui_redraw_widget("mainwindow");
}


void on_proxyportentry_changed(GtkEditable *editable, gpointer user_data) {

	setNumericConfValue(PROXY_PORT, atoi(gtk_editable_get_chars(editable,0,-1)));
	loadConfig();
	ui_redraw_widget("mainwindow");
}
