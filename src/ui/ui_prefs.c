/**
 * @file ui_prefs.c program preferences
 *
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004-2006 Lars Lindner <lars.lindner@gmx.net>
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

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "conf.h"
#include "interface.h"
#include "support.h"
#include "callbacks.h"
#include "favicon.h"
#include "itemlist.h"
#include "social.h"
#include "ui_mainwindow.h"
#include "ui_itemlist.h"
#include "ui_prefs.h"
#include "ui_mainwindow.h"
#include "ui_enclosure.h"
#include "ui_tray.h"
#include "ui_enclosure.h"
#include "notification/notif_plugin.h"

/* file type tree store column ids */
enum fts_columns {
	FTS_TYPE,	/* file type name */
	FTS_CMD,	/* file cmd name */
	FTS_PTR,	/* pointer to config entry */
	FTS_LEN
};

extern GSList *htmlviewPlugins;
extern GSList *socialBookmarkSites;

static GtkWidget *prefdialog = NULL;

static void on_browsermodule_changed(GtkObject *object, gchar *libname);
static void on_browser_changed(GtkOptionMenu *optionmenu, gpointer user_data);
static void on_browser_place_changed(GtkOptionMenu *optionmenu, gpointer user_data);
static void on_startup_feed_handler_changed(GtkEditable *editable, gpointer user_data);
static void on_updateallfavicons_clicked(GtkButton *button, gpointer user_data);
static void on_enableproxybtn_clicked (GtkButton *button, gpointer user_data);
static void on_enc_download_tool_changed(GtkEditable *editable, gpointer user_data);
static void on_socialsite_changed(GtkOptionMenu *optionmenu, gpointer user_data);

struct enclosure_download_tool {
	gchar	*name;
	gchar	*cmd;
};

/* tool commands need to take an absolute file path as first %s and an URL as second %s */
struct enclosure_download_tool enclosure_download_tools[] = {
	{ "wget",	"wget -q -O %s %s" },
	{ "curl",	"curl -s -o %s %s" },
	{ NULL,		NULL}
};

struct browser {
	gchar *id; /**< Unique ID used in storing the prefs */
	gchar *display; /**< Name to display in the prefs */
	gchar *defaultplace; /**< Default command.... Use %s to specify URL. This command is called in the background. */
	gchar *existingwin;
	gchar *existingwinremote;
	gchar *newwin;
	gchar *newwinremote;
	gchar *newtab;
	gchar *newtabremote;
};

struct browser browsers[] = {
	{"gnome", "Gnome Default Browser", "gnome-open %s", NULL, NULL,
	 NULL, NULL,
	 NULL, NULL},
	{"mozilla", "Mozilla", "mozilla %s",
	 NULL, "mozilla -remote openURL(%s)",
	 NULL, "mozillax -remote 'openURL(%s,new-window)'",
	 NULL, "mozilla -remote 'openURL(%s,new-tab)'"},
	{"firefox", "Firefox","firefox \"%s\"",
	 NULL, "firefox -a firefox -remote \"openURL(%s)\"",
	 NULL, "firefox -a firefox -remote 'openURL(%s,new-window)'",
	 NULL, "firefox -a firefox -remote 'openURL(%s,new-tab)'"},
	{"netscape", "Netscape", "netscape \"%s\"",
	 NULL, "netscape -remote \"openURL(%s)\"",
	 NULL, "netscape -remote \"openURL(%s,new-window)\"",
	 NULL, NULL},
	{"opera", "Opera","opera \"%s\"",
	 "opera \"%s\"", "opera -remote \"openURL(%s)\"",
	 "opera -newwindow \"%s\"", NULL,
	 "opera -newpage \"%s\"", NULL},
	{"epiphany", "Epiphany","epiphany \"%s\"",
	 NULL, NULL,
	 "epiphany \"%s\"", NULL,
	 "epiphany -n \"%s\"", NULL},
	{"konqueror", "Konqueror", "kfmclient openURL \"%s\"",
		 NULL, NULL,
		 NULL, NULL,
		 NULL, NULL},
	{NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL}
};

gchar * prefs_get_browser_remotecmd(void) {
	gchar *ret = NULL;
	gchar *libname;
	gint place = getNumericConfValue(BROWSER_PLACE);

	libname = getStringConfValue(BROWSER_ID);
	if (!strcmp(libname, "manual")) {
		ret = NULL;
	} else {
		struct browser *iter;
		for (iter = browsers; iter->id != NULL; iter++) {
			if(!strcmp(libname, iter->id)) {
				
				switch (place) {
				case 1:
					ret = g_strdup(iter->existingwinremote);
					break;
				case 2:
					ret = g_strdup(iter->newwinremote);
					break;
				case 3:
					ret = g_strdup(iter->newtabremote);
					break;
				}
			}
		}
	}
	g_free(libname);
	return ret;
}

gchar * prefs_get_browser_cmd(void) {
	gchar *ret = NULL;
	gchar *libname;
	gint place = getNumericConfValue(BROWSER_PLACE);
	
	libname = getStringConfValue(BROWSER_ID);
	if (!strcmp(libname, "manual")) {
		ret = g_strdup(getStringConfValue(BROWSER_COMMAND));
	} else {
		struct browser *iter;
		for (iter = browsers; iter->id != NULL; iter++) {
			if(!strcmp(libname, iter->id)) {
				
				switch (place) {
				case 1:
					ret = g_strdup(iter->existingwin);
					break;
				case 2:
					ret = g_strdup(iter->newwin);
					break;
				case 3:
					ret = g_strdup(iter->newtab);
					break;
				}
				if (ret == NULL) /* Default when no special mode defined */
					ret = g_strdup(iter->defaultplace);
			}
		}
	}
	g_free(libname);
	if (ret == NULL)
		ret = g_strdup(browsers[0].defaultplace);
	return ret;
}

gchar * prefs_get_download_cmd(void) {
	struct enclosure_download_tool	*edt = enclosure_download_tools;

	edt += getNumericConfValue(ENCLOSURE_DOWNLOAD_TOOL);
	/* FIXME: array boundary check */
	return edt->cmd;
}

/*------------------------------------------------------------------------------*/
/* preferences dialog callbacks 						*/
/*------------------------------------------------------------------------------*/

static void ui_pref_destroyed_cb(GtkWidget *widget, void *data) {

	prefdialog = NULL;
}

void on_prefbtn_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget		*widget, *entry, *menu;
	GtkAdjustment		*itemCount;
	GtkTreeIter		*treeiter;
	GtkTreeStore		*treestore;
	GtkTreeViewColumn 	*column;
	GSList			*list;
	gchar			*widgetname, *proxyport;
	gchar			*configuredPluginName;
	gchar			*configuredBrowser, *name;
	gboolean		enabled, enabled2;
	int			tmp, i;
	static int		manual;
	struct browser			*iter;
	struct enclosure_download_tool	*edtool;
	
	if(NULL == prefdialog) {
		prefdialog = create_prefdialog();
		gtk_window_set_transient_for(GTK_WINDOW(prefdialog), GTK_WINDOW(mainwindow));
		g_signal_connect(G_OBJECT(prefdialog), "destroy", G_CALLBACK(ui_pref_destroyed_cb), NULL);

		/* Set up browser selection popup */
		menu = gtk_menu_new();
		for(i=0, iter = browsers; iter->id != NULL; iter++, i++) {
			entry = gtk_menu_item_new_with_label(iter->display);
			gtk_widget_show(entry);
			gtk_container_add(GTK_CONTAINER(menu), entry);
			gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(on_browser_changed), GINT_TO_POINTER(i));
		}
		manual = i;
		/* This allows the user to choose their own browser by typing in the command. */
		entry = gtk_menu_item_new_with_label(_("Manual"));
		gtk_widget_show(entry);
		gtk_container_add(GTK_CONTAINER(menu), entry);
		gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(on_browser_changed), GINT_TO_POINTER(i));

		gtk_option_menu_set_menu(GTK_OPTION_MENU(lookup_widget(prefdialog, "browserpopup")), menu);

		/* Create location menu */
		menu = gtk_menu_new();

		entry = gtk_menu_item_new_with_label(_("Browser default"));
		gtk_widget_show(entry);
		gtk_container_add(GTK_CONTAINER(menu), entry);
		gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(on_browser_place_changed), GINT_TO_POINTER(0));

		entry = gtk_menu_item_new_with_label(_("Existing window"));
		gtk_widget_show(entry);
		gtk_container_add(GTK_CONTAINER(menu), entry);
		gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(on_browser_place_changed), GINT_TO_POINTER(1));

		entry = gtk_menu_item_new_with_label(_("New window"));
		gtk_widget_show(entry);
		gtk_container_add(GTK_CONTAINER(menu), entry);
		gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(on_browser_place_changed), GINT_TO_POINTER(2));

		entry = gtk_menu_item_new_with_label(_("New tab"));
		gtk_widget_show(entry);
		gtk_container_add(GTK_CONTAINER(menu), entry);
		gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(on_browser_place_changed), GINT_TO_POINTER(3));

		gtk_option_menu_set_menu(GTK_OPTION_MENU(lookup_widget(prefdialog, "browserlocpopup")), menu);

		/* Create the startup feed handling menu */
		menu = gtk_menu_new();

		entry = gtk_menu_item_new_with_label(_("Update only feeds scheduled for updates"));
		gtk_widget_show(entry);
		gtk_container_add(GTK_CONTAINER(menu), entry);
		gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(on_startup_feed_handler_changed), GINT_TO_POINTER(0));

		entry = gtk_menu_item_new_with_label(_("Update all feeds"));
		gtk_widget_show(entry);
		gtk_container_add(GTK_CONTAINER(menu), entry);
		gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(on_startup_feed_handler_changed), GINT_TO_POINTER(1));

		entry = gtk_menu_item_new_with_label(_("Reset feed update timers (Update no feeds)"));
		gtk_widget_show(entry);
		gtk_container_add(GTK_CONTAINER(menu), entry);
		gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(on_startup_feed_handler_changed), GINT_TO_POINTER(2));

		gtk_option_menu_set_menu(GTK_OPTION_MENU(lookup_widget(prefdialog, "startupfeedhandler")), menu);

		/* ================== panel 1 "feeds" ==================== */

		tmp = getNumericConfValue(STARTUP_FEED_ACTION);
		gtk_option_menu_set_history(GTK_OPTION_MENU(lookup_widget(prefdialog, "startupfeedhandler")), tmp);

		widget = lookup_widget(prefdialog, "itemCountBtn");
		itemCount = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(widget));
		gtk_adjustment_set_value(itemCount, getNumericConfValue(DEFAULT_MAX_ITEMS));

		/* Set fields in the radio widgets so that they know their option # and the pref dialog */
		for(i = 1; i <= 2; i++) {
			widgetname = g_strdup_printf("%s%d", "feedsinmemorybtn", i);
			widget = lookup_widget(prefdialog, widgetname);
			gtk_object_set_data(GTK_OBJECT(widget), "option_number", GINT_TO_POINTER(i));
			g_free(widgetname);
		}

		/* set default update interval spin button */
		widget = lookup_widget(prefdialog,"refreshIntervalSpinButton");
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), getNumericConfValue(DEFAULT_UPDATE_INTERVAL));

		/* select currently active menu option */
		tmp = 1;
		if(getBooleanConfValue(KEEP_FEEDS_IN_MEMORY)) tmp = 2;

		widgetname = g_strdup_printf("%s%d", "feedsinmemorybtn", tmp);
		widget = lookup_widget(prefdialog, widgetname);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
		g_free(widgetname);

		/* ================== panel 2 "folders" ==================== */

		g_signal_connect(GTK_OBJECT(lookup_widget(prefdialog, "updateAllFavicons")), "clicked", G_CALLBACK(on_updateallfavicons_clicked), NULL);	
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(prefdialog, "folderdisplaybtn")), (0 == getNumericConfValue(FOLDER_DISPLAY_MODE)?FALSE:TRUE));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(prefdialog, "hidereadbtn")), (0 == getBooleanConfValue(FOLDER_DISPLAY_HIDE_READ)?FALSE:TRUE));

		/* ================== panel 3 "headlines" ==================== */

		/* select current browse key menu entry */
		switch(getNumericConfValue(BROWSE_KEY_SETTING)) {
			case 0:
				tmp = 1;
				break;
			default:
			case 1:
				tmp = 0;
				break;
			case 2:
				tmp = 2;
				break;

		}
		widget = lookup_widget(prefdialog, "browsekeyoptionmenu");
		gtk_option_menu_set_history(GTK_OPTION_MENU(widget), tmp);
		
		/* Setup social bookmarking list */
		i = 0;
		name = getStringConfValue(SOCIAL_BM_SITE);
		menu = gtk_menu_new();
		list = socialBookmarkSites;
		while(list) {
			socialBookmarkSitePtr siter = list->data;
			if(name && !strcmp(siter->name, name))
				tmp = i;
			entry = gtk_menu_item_new_with_label(siter->name);
			gtk_widget_show(entry);
			gtk_container_add(GTK_CONTAINER(menu), entry);
			gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(on_socialsite_changed), (gpointer)siter->name);
			list = g_slist_next(list);
			i++;
		}
		gtk_option_menu_set_menu(GTK_OPTION_MENU(lookup_widget(prefdialog, "socialpopup")), menu);
		gtk_option_menu_set_history(GTK_OPTION_MENU(lookup_widget(prefdialog, "socialpopup")), tmp);

		/* ================== panel 4 "browser" ==================== */

		/* set up the internal browser module option menu */
		tmp = i = 0;
		widget = gtk_menu_new();
		list = htmlviewPlugins;
		configuredPluginName = getStringConfValue(BROWSER_MODULE);
		while(list) {		
			g_assert(NULL != list->data);
			gchar	*pluginName = ((htmlviewPluginPtr)((pluginPtr)list->data)->symbols)->name;
			entry = gtk_menu_item_new_with_label(pluginName);
			gtk_widget_show(entry);
			gtk_container_add(GTK_CONTAINER(widget), entry);
			gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(on_browsermodule_changed), pluginName);
			if(!strcmp(configuredPluginName, pluginName))
				tmp = i;
			i++;
			list = g_slist_next(list);
		}
		gtk_menu_set_active(GTK_MENU(widget), tmp);
		gtk_option_menu_set_menu(GTK_OPTION_MENU(lookup_widget(prefdialog, "htmlviewoptionmenu")), widget);
		g_free(configuredPluginName);

		/* set the inside browsing flag */
		widget = lookup_widget(prefdialog, "browseinwindow");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), getBooleanConfValue(BROWSE_INSIDE_APPLICATION));

		/* set the javascript-disabled flag */
		widget = lookup_widget(prefdialog, "disablejavascript");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), getBooleanConfValue(DISABLE_JAVASCRIPT));

		tmp = 0;
		configuredBrowser = getStringConfValue(BROWSER_ID);

		if(!strcmp(configuredBrowser, "manual"))
			tmp = manual;
		else
			for(i=0, iter = browsers; iter->id != NULL; iter++, i++)
				if(!strcmp(configuredBrowser, iter->id))
					tmp = i;

		gtk_option_menu_set_history(GTK_OPTION_MENU(lookup_widget(prefdialog, "browserpopup")), tmp);
		g_free(configuredBrowser);

		gtk_option_menu_set_history(GTK_OPTION_MENU(lookup_widget(prefdialog, "browserlocpopup")), getNumericConfValue(BROWSER_PLACE));

		entry = lookup_widget(prefdialog, "browsercmd");
		gtk_entry_set_text(GTK_ENTRY(entry), getStringConfValue(BROWSER_COMMAND));
		gtk_widget_set_sensitive(GTK_WIDGET(entry), tmp==manual);
		gtk_widget_set_sensitive(lookup_widget(prefdialog, "manuallabel"), tmp==manual);	

		/* ================== panel 4 "GUI" ================ */

		widget = lookup_widget(prefdialog, "trayiconoptionbtn");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), getBooleanConfValue(SHOW_TRAY_ICON));

		widget = lookup_widget(prefdialog, "popupwindowsoptionbtn");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), getBooleanConfValue(SHOW_POPUP_WINDOWS));
		gtk_widget_set_sensitive(lookup_widget(prefdialog, "placement_options"), getBooleanConfValue(SHOW_POPUP_WINDOWS));

		/* menu / tool bar settings */	
		for(i = 1; i <= 3; i++) {
			/* Set fields in the radio widgets so that they know their option # and the pref dialog */
			widgetname = g_strdup_printf("%s%d", "menuradiobtn", i);
			widget = lookup_widget(prefdialog, widgetname);
			gtk_object_set_data(GTK_OBJECT(widget), "option_number", GINT_TO_POINTER(i));
			g_free(widgetname);
		}

		/* the same for the popup placements settings */	
		for(i = 1; i <= 4; i++) {
			/* Set fields in the radio widgets so that they know their option # and the pref dialog */
			widgetname = g_strdup_printf("popup_placement%d_radiobtn", i);
			widget = lookup_widget(prefdialog, widgetname);
			gtk_object_set_data(GTK_OBJECT(widget), "option_number", GINT_TO_POINTER(i));
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

		/* select currently active popup placement option */	
		tmp = getNumericConfValue(POPUP_PLACEMENT);
		if((tmp < 1) || (tmp > 4))
			tmp = 1;
		widgetname = g_strdup_printf("popup_placement%d_radiobtn", tmp);
		widget = lookup_widget(prefdialog, widgetname);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
		g_free(widgetname);

		/* ================= panel 5 "proxy" ======================== */
		enabled = getBooleanConfValue(USE_PROXY);
		widget = lookup_widget(prefdialog, "enableproxybtn");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), enabled);

		entry = lookup_widget(prefdialog, "proxyhostentry");
		gtk_entry_set_text(GTK_ENTRY(entry), getStringConfValue(PROXY_HOST));

		entry = lookup_widget(prefdialog, "proxyportentry");
		proxyport = g_strdup_printf("%d", getNumericConfValue(PROXY_PORT));
		gtk_entry_set_text(GTK_ENTRY(entry), proxyport);
		g_free(proxyport);


		/* Authentication */
		enabled2 = getBooleanConfValue(PROXY_USEAUTH);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(prefdialog, "useProxyAuth")), enabled2);
		gtk_entry_set_text(GTK_ENTRY(lookup_widget(prefdialog, "proxyuserentry")), getStringConfValue(PROXY_USER));
		gtk_entry_set_text(GTK_ENTRY(lookup_widget(prefdialog, "proxypasswordentry")), getStringConfValue(PROXY_PASSWD));

		gtk_widget_set_sensitive(GTK_WIDGET(lookup_widget(prefdialog, "proxybox")), enabled);
		if (enabled)
			gtk_widget_set_sensitive(GTK_WIDGET(lookup_widget(prefdialog, "proxyauthbox")), enabled2);

		gtk_signal_connect(GTK_OBJECT(lookup_widget(prefdialog, "enableproxybtn")), "clicked", G_CALLBACK(on_enableproxybtn_clicked), NULL);
		gtk_signal_connect(GTK_OBJECT(lookup_widget(prefdialog, "useProxyAuth")), "clicked", G_CALLBACK(on_enableproxybtn_clicked), NULL);

		/* ================= panel 6 "enclosures" ======================== */

		/* menu for download tool */
		menu = gtk_menu_new();
		i = 0; edtool = enclosure_download_tools;
		while(edtool->name != NULL) {		
			entry = gtk_menu_item_new_with_label(edtool->name);
			gtk_widget_show(entry);
			gtk_container_add(GTK_CONTAINER(menu), entry);
			gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(on_enc_download_tool_changed), GINT_TO_POINTER(i));
			edtool++;
			i++;
		}		
		gtk_menu_set_active(GTK_MENU(menu), getNumericConfValue(ENCLOSURE_DOWNLOAD_TOOL));
		gtk_option_menu_set_menu(GTK_OPTION_MENU(lookup_widget(prefdialog, "enc_download_tool_option_btn")), menu);

		/* set enclosure download path entry */	
		gtk_entry_set_text(GTK_ENTRY(lookup_widget(prefdialog, "save_download_entry")), getStringConfValue(ENCLOSURE_DOWNLOAD_PATH));

		/* set up list of configured enclosure types */
		treestore = gtk_tree_store_new(FTS_LEN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
		list = ui_enclosure_get_types();
		while(NULL != list) {
			treeiter = g_new0(GtkTreeIter, 1);
			gtk_tree_store_append(treestore, treeiter, NULL);
			gtk_tree_store_set(treestore, treeiter,
		                	   FTS_TYPE, (NULL != ((encTypePtr)(list->data))->mime)?((encTypePtr)(list->data))->mime:((encTypePtr)(list->data))->extension, 
		                	   FTS_CMD, ((encTypePtr)(list->data))->cmd,
		                	   FTS_PTR, list->data, 
					   -1);
			list = g_slist_next(list);
		}

		widget = lookup_widget(prefdialog, "enc_actions_view");
		gtk_tree_view_set_model(GTK_TREE_VIEW(widget), GTK_TREE_MODEL(treestore));

		column = gtk_tree_view_column_new_with_attributes(_("Type"), gtk_cell_renderer_text_new(), "text", FTS_TYPE, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(widget), column);
		gtk_tree_view_column_set_sort_column_id(column, FTS_TYPE);
		column = gtk_tree_view_column_new_with_attributes(_("Program"), gtk_cell_renderer_text_new(), "text", FTS_CMD, NULL);
		gtk_tree_view_column_set_sort_column_id(column, FTS_CMD);
		gtk_tree_view_append_column(GTK_TREE_VIEW(widget), column);

		gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(widget)), GTK_SELECTION_SINGLE);
	}	
	gtk_window_present(GTK_WINDOW(prefdialog));
}

/*------------------------------------------------------------------------------*/
/* preference callbacks 							*/
/*------------------------------------------------------------------------------*/

void on_folderdisplaybtn_toggled(GtkToggleButton *togglebutton, gpointer user_data) {

	gboolean enabled = gtk_toggle_button_get_active(togglebutton);
	setNumericConfValue(FOLDER_DISPLAY_MODE, (TRUE == enabled)?1:0);
}

void on_folderhidereadbtn_toggled(GtkToggleButton *togglebutton, gpointer user_data) {
	nodePtr	displayed_node;

	gboolean enabled = gtk_toggle_button_get_active(togglebutton);
	setBooleanConfValue(FOLDER_DISPLAY_HIDE_READ, enabled);
	displayed_node = itemlist_get_displayed_node();
	if(displayed_node)
		itemlist_load(displayed_node->itemSet);
}

void on_trayiconoptionbtn_clicked(GtkButton *button, gpointer user_data) {

	gboolean enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
	setBooleanConfValue(SHOW_TRAY_ICON, enabled);
}

void on_popupwindowsoptionbtn_clicked(GtkButton *button, gpointer user_data) {

	gboolean enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
	setBooleanConfValue(SHOW_POPUP_WINDOWS, enabled);
	notification_enable(getBooleanConfValue(SHOW_POPUP_WINDOWS));
	gtk_widget_set_sensitive(lookup_widget(prefdialog, "placement_options"), enabled);
}

static void on_startup_feed_handler_changed(GtkEditable *editable, gpointer user_data) {

	setNumericConfValue(STARTUP_FEED_ACTION, GPOINTER_TO_INT(user_data));
}

void on_browsercmd_changed(GtkEditable *editable, gpointer user_data) {

	setStringConfValue(BROWSER_COMMAND, gtk_editable_get_chars(editable,0,-1));
}

static void on_browser_changed(GtkOptionMenu *optionmenu, gpointer user_data) {
	gint num = GPOINTER_TO_INT(user_data);

	gtk_widget_set_sensitive(lookup_widget(prefdialog, "browsercmd"), browsers[num].id == NULL);	
	gtk_widget_set_sensitive(lookup_widget(prefdialog, "manuallabel"), browsers[num].id == NULL);	

	if (browsers[num].id == NULL)
		setStringConfValue(BROWSER_ID, "manual");
	else
		setStringConfValue(BROWSER_ID, browsers[num].id);
}

static void on_browser_place_changed(GtkOptionMenu *optionmenu, gpointer user_data) {
	int num = GPOINTER_TO_INT(user_data);
	
	setNumericConfValue(BROWSER_PLACE, num);
}

static void on_browsermodule_changed(GtkObject *object, gchar *libname) {
	setStringConfValue(BROWSER_MODULE, libname);
}


void on_openlinksinsidebtn_clicked(GtkToggleButton *button, gpointer user_data) {
	setBooleanConfValue(BROWSE_INSIDE_APPLICATION, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)));
}

void on_disablejavascript_toggled(GtkToggleButton *togglebutton, gpointer user_data) {
	setBooleanConfValue(DISABLE_JAVASCRIPT, gtk_toggle_button_get_active(togglebutton));
}

void on_socialsite_changed(GtkOptionMenu *optionmenu, gpointer user_data) {
	social_set_site((gchar *)user_data);
}

void on_itemCountBtn_value_changed(GtkSpinButton *spinbutton, gpointer user_data) {
	GtkAdjustment	*itemCount;
	
	itemCount = gtk_spin_button_get_adjustment(spinbutton);
	setNumericConfValue(DEFAULT_MAX_ITEMS, gtk_adjustment_get_value(itemCount));
}

void on_default_update_interval_value_changed(GtkSpinButton *spinbutton, gpointer user_data) {
	GtkAdjustment	*updateInterval;
	
	updateInterval = gtk_spin_button_get_adjustment(spinbutton);
	setNumericConfValue(DEFAULT_UPDATE_INTERVAL, gtk_adjustment_get_value(updateInterval));
}

void on_menuselection_clicked(GtkButton *button, gpointer user_data) {
	gint		active_button;
	
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
}

void on_placement_radiobtn_clicked(GtkButton *button, gpointer user_data) {

	setNumericConfValue(POPUP_PLACEMENT, GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(button), "option_number")));
}

static void on_updateallfavicons_clicked(GtkButton *button, gpointer user_data) {

	feedlist_foreach(node_update_favicon);
}
 
static void on_enableproxybtn_clicked(GtkButton *button, gpointer user_data) {
	gboolean	enabled;
	
	enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(prefdialog, "useProxyAuth")));
	gtk_widget_set_sensitive(GTK_WIDGET(lookup_widget(prefdialog, "proxyauthbox")), enabled);
	setBooleanConfValue(PROXY_USEAUTH, enabled);
	
	enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(prefdialog, "enableproxybtn")));
	gtk_widget_set_sensitive(GTK_WIDGET(lookup_widget(prefdialog, "proxybox")), enabled);
	setBooleanConfValue(USE_PROXY, enabled);
}

void on_proxyhostentry_changed(GtkEditable *editable, gpointer user_data) {
	setStringConfValue(PROXY_HOST, gtk_editable_get_chars(editable,0,-1));
}


void on_proxyportentry_changed(GtkEditable *editable, gpointer user_data) {
	setNumericConfValue(PROXY_PORT, atoi(gtk_editable_get_chars(editable,0,-1)));
}

void on_proxyusernameentry_changed(GtkEditable *editable, gpointer user_data) {
	setStringConfValue(PROXY_USER, gtk_editable_get_chars(editable,0,-1));
}

void on_proxypasswordentry_changed(GtkEditable *editable, gpointer user_data) {
	setStringConfValue(PROXY_PASSWD, gtk_editable_get_chars(editable,0,-1));
}

void on_feedsinmemorybtn_clicked(GtkButton *button, gpointer user_data) {
	gint		active_button;
	
	active_button = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(button), "option_number"));
	setBooleanConfValue(KEEP_FEEDS_IN_MEMORY, (active_button == 2));
}

void on_browsekey_space_activate(GtkMenuItem *menuitem, gpointer user_data) {

	setNumericConfValue(BROWSE_KEY_SETTING, 1);
}

void on_browsekey_ctrl_space_activate(GtkMenuItem *menuitem, gpointer user_data) {

	setNumericConfValue(BROWSE_KEY_SETTING, 0);
}

void on_browsekey_alt_space_activate(GtkMenuItem *menuitem, gpointer user_data) {

	setNumericConfValue(BROWSE_KEY_SETTING, 2);
}

static void on_enc_download_tool_changed(GtkEditable *editable, gpointer user_data) {

	setNumericConfValue(ENCLOSURE_DOWNLOAD_TOOL, GPOINTER_TO_INT(user_data));
}

void on_enc_action_change_btn_clicked(GtkButton *button, gpointer user_data) {
	GtkTreeModel		*model;
	GtkTreeSelection	*selection;
	GtkTreeIter		iter;
	gpointer		type;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(lookup_widget(prefdialog, "enc_actions_view")));
	if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
		gtk_tree_model_get(model, &iter, FTS_PTR, &type, -1);
		ui_enclosure_change_type(type);
		gtk_tree_store_set(GTK_TREE_STORE(model), &iter, 
		                   FTS_CMD, ((encTypePtr)type)->cmd, -1);
	}
}

void on_enc_action_remove_btn_clicked(GtkButton *button, gpointer user_data) {
	GtkTreeModel		*model;
	GtkTreeSelection	*selection;
	GtkTreeIter		iter;
	gpointer		type;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(lookup_widget(prefdialog, "enc_actions_view")));
	if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
		gtk_tree_model_get(model, &iter, FTS_PTR, &type, -1);
		gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
		ui_enclosure_remove_type(type);
	}
}

void on_save_download_entry_changed(GtkEditable *editable, gpointer user_data) {

	setStringConfValue(ENCLOSURE_DOWNLOAD_PATH, gtk_editable_get_chars(editable , 0, -1));
}

static void on_save_download_finished(const gchar *filename, gpointer user_data) {
	GtkWidget	*dialog = (GtkWidget *)user_data;
	gchar		*utfname;
	
	if(filename == NULL)
		return;
	
	utfname = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);

	if(utfname != NULL) {
		gtk_entry_set_text(GTK_ENTRY(lookup_widget(dialog, "save_download_entry")), utfname);
		setStringConfValue(ENCLOSURE_DOWNLOAD_PATH, utfname);
	}
	
	g_free(utfname);
}

void on_save_download_select_btn_clicked(GtkButton *button, gpointer user_data) {
	const gchar *path = gtk_editable_get_chars(GTK_EDITABLE(lookup_widget(prefdialog, "save_download_entry")), 0, -1);
	
	ui_choose_directory(_("Choose download directory"), GTK_WINDOW(prefdialog), GTK_STOCK_OPEN, on_save_download_finished, path, prefdialog);
}


