/**
 * @file ui_prefs.c program preferences
 *
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004-2007 Lars Lindner <lars.lindner@gmail.com>
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

#include "common.h"
#include "conf.h"
#include "favicon.h"
#include "feedlist.h"
#include "itemlist.h"
#include "social.h"
#include "ui/ui_dialog.h"
#include "ui/ui_enclosure.h"
#include "ui/ui_itemlist.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_prefs.h"
#include "ui/ui_tray.h"
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

static void on_browser_changed(GtkOptionMenu *optionmenu, gpointer user_data);
static void on_browser_place_changed(GtkOptionMenu *optionmenu, gpointer user_data);
static void on_updateallfavicons_clicked(GtkButton *button, gpointer user_data);
static void on_enableproxybtn_clicked (GtkButton *button, gpointer user_data);
static void on_enc_download_tool_changed(GtkEditable *editable, gpointer user_data);
static void on_socialsite_changed(GtkOptionMenu *optionmenu, gpointer user_data);
static void on_toolbar_style_changed(GtkComboBox *widget, gpointer user_data);

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

static struct browser browsers[] = {
	{
		"gnome", N_("GNOME Default Browser"), "gnome-open %s", 
		NULL, NULL,
		NULL, NULL,
		NULL, NULL,
		FALSE
	},
	{
		/* tested with SeaMonkey 1.0.6 */
		"mozilla", "Mozilla", "mozilla %s",
		NULL, "mozilla -remote openURL(%s)",
		NULL, "mozillax -remote 'openURL(%s,new-window)'",
		NULL, "mozilla -remote 'openURL(%s,new-tab)'",
		FALSE
	},
	{
		/* tested with Firefox 1.5 and 2.0 */
		"firefox", "Firefox","firefox \"%s\"",
		NULL, "firefox -a firefox -remote \"openURL(%s)\"",
		NULL, "firefox -a firefox -remote 'openURL(%s,new-window)'",
		NULL, "firefox -a firefox -remote 'openURL(%s,new-tab)'",
		TRUE
	},
	{
		/* tested with Netscape 4.76 */
		"netscape", "Netscape", "netscape \"%s\"",
		NULL, "netscape -remote \"openURL(%s)\"",
		NULL, "netscape -remote \"openURL(%s,new-window)\"",
		NULL, NULL,
		TRUE
	},
	{
		"opera", "Opera","opera \"%s\"",
		"opera \"%s\"", "opera -remote \"openURL(%s)\"",
		"opera -newwindow \"%s\"", NULL,
		"opera -newpage \"%s\"", NULL,
		FALSE
	},
	{
		"epiphany", "Epiphany","epiphany \"%s\"",
		NULL, NULL,
		"epiphany \"%s\"", NULL,
		"epiphany -n \"%s\"", NULL,
		FALSE
	},
	{
		"konqueror", "Konqueror", "kfmclient openURL \"%s\"",
		NULL, NULL,
		NULL, NULL,
		NULL, NULL,
		FALSE
	},
	{	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, FALSE }
};

static gchar * gui_toolbar_style[] = { "", "both", "both-horiz", "icons", "text", NULL };

gchar * prefs_get_browser_command(struct browser *browser, gboolean remote, gboolean fallback) {
	gchar	*cmd = NULL;
	gchar	*libname;
	gint	place = getNumericConfValue(BROWSER_PLACE);

	/* check for manual browser command */
	libname = getStringConfValue(BROWSER_ID);
	if(g_str_equal(libname, "manual")) {
		/* retrieve user defined command... */
		cmd = getStringConfValue(BROWSER_COMMAND);
	} else {
		/* non manual browser definitions... */
		if(browser) {
			if(remote) {
				switch(place) {
					case 1:
						cmd = browser->existingwinremote;
						break;
					case 2:
						cmd = browser->newwinremote;
						break;
					case 3:
						cmd = browser->newtabremote;
						break;
				}
			} else {
				switch(place) {
					case 1:
						cmd = browser->existingwin;
						break;
					case 2:
						cmd = browser->newwin;
						break;
					case 3:
						cmd = browser->newtab;
						break;
				}
			}

			if(fallback && !cmd)	/* Default when no special mode defined */
				cmd = browser->defaultplace;
		}

		if(fallback && !cmd)	/* Last fallback: first browser default */
			cmd = browsers[0].defaultplace;
	}
	g_free(libname);
		
	return cmd?g_strdup(cmd):NULL;
}

struct browser * prefs_get_browser(void) {
	gchar		*libname;
	struct browser	*browser = NULL;
	
	libname = getStringConfValue(BROWSER_ID);
	if(!g_str_equal(libname, "manual")) {
		struct browser *iter;
		for(iter = browsers; iter->id != NULL; iter++) {
			if(g_str_equal(libname, iter->id))
				browser = iter;
		}
	}
	g_free(libname);

	return browser;
}

const gchar * prefs_get_download_cmd(void) {
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
	GtkWidget		*widget, *entry, *menu, *combo;
	GtkAdjustment		*itemCount;
	GtkTreeIter		*treeiter;
	GtkTreeStore		*treestore;
	GtkTreeViewColumn 	*column;
	GSList			*list;
	gchar			*widgetname, *proxyport;
	gchar			*configuredBrowser, *name;
	gboolean		enabled, enabled2;
	int			tmp, i;
	static int		manual;
	struct browser			*iter;
	struct enclosure_download_tool	*edtool;
	
	if (!prefdialog) {
		prefdialog = liferea_dialog_new (NULL, "prefdialog");
		g_signal_connect (G_OBJECT (prefdialog), "destroy", G_CALLBACK (ui_pref_destroyed_cb), NULL);

		/* Set up browser selection popup */
		menu = gtk_menu_new();
		for(i=0, iter = browsers; iter->id != NULL; iter++, i++) {
			entry = gtk_menu_item_new_with_label(_(iter->display));
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

		gtk_option_menu_set_menu(GTK_OPTION_MENU(liferea_dialog_lookup(prefdialog, "browserpopup")), menu);

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

		gtk_option_menu_set_menu(GTK_OPTION_MENU(liferea_dialog_lookup(prefdialog, "browserlocpopup")), menu);

		/* Create the toolbar style combo */
		combo = liferea_dialog_lookup(prefdialog, "toolbarcombo");

		/* ================== panel 1 "feeds" ==================== */

		gtk_combo_box_set_active (GTK_COMBO_BOX (liferea_dialog_lookup (prefdialog, "startupActionCombo")), 
		                          getNumericConfValue (STARTUP_FEED_ACTION));

		widget = liferea_dialog_lookup(prefdialog, "itemCountBtn");
		itemCount = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(widget));
		gtk_adjustment_set_value(itemCount, getNumericConfValue(DEFAULT_MAX_ITEMS));

		/* set default update interval spin button and unit combo box */
		widget = liferea_dialog_lookup(prefdialog, "refreshIntervalUnitComboBox");
		tmp = getNumericConfValue(DEFAULT_UPDATE_INTERVAL);
		if (tmp % 1440 == 0) {		/* days */
			gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 2);
			tmp /= 1440;
		} else if (tmp % 60 == 0) {	/* hours */
			gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 1);
			tmp /= 60;
		} else {			/* minutes */
			gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 0);
		}
		widget = liferea_dialog_lookup(prefdialog,"refreshIntervalSpinButton");
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), tmp);

		/* ================== panel 2 "folders" ==================== */

		g_signal_connect(GTK_OBJECT(liferea_dialog_lookup(prefdialog, "updateAllFavicons")), "clicked", G_CALLBACK(on_updateallfavicons_clicked), NULL);	
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(prefdialog, "folderdisplaybtn")), (0 == getNumericConfValue(FOLDER_DISPLAY_MODE)?FALSE:TRUE));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(prefdialog, "hidereadbtn")), (0 == getBooleanConfValue(FOLDER_DISPLAY_HIDE_READ)?FALSE:TRUE));

		/* ================== panel 3 "headlines" ==================== */

		gtk_combo_box_set_active (GTK_COMBO_BOX (liferea_dialog_lookup(prefdialog, "skimKeyCombo")),
		                          getNumericConfValue(BROWSE_KEY_SETTING));
					  
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
		gtk_option_menu_set_menu(GTK_OPTION_MENU(liferea_dialog_lookup(prefdialog, "socialpopup")), menu);
		gtk_option_menu_set_history(GTK_OPTION_MENU(liferea_dialog_lookup(prefdialog, "socialpopup")), tmp);

		/* ================== panel 4 "browser" ==================== */

		/* set the inside browsing flag */
		widget = liferea_dialog_lookup(prefdialog, "browseinwindow");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), getBooleanConfValue(BROWSE_INSIDE_APPLICATION));

		/* set the javascript-disabled flag */
		widget = liferea_dialog_lookup(prefdialog, "disablejavascript");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), getBooleanConfValue(DISABLE_JAVASCRIPT));

		tmp = 0;
		configuredBrowser = getStringConfValue(BROWSER_ID);

		if(!strcmp(configuredBrowser, "manual"))
			tmp = manual;
		else
			for(i=0, iter = browsers; iter->id != NULL; iter++, i++)
				if(!strcmp(configuredBrowser, iter->id))
					tmp = i;

		gtk_option_menu_set_history(GTK_OPTION_MENU(liferea_dialog_lookup(prefdialog, "browserpopup")), tmp);
		g_free(configuredBrowser);

		gtk_option_menu_set_history(GTK_OPTION_MENU(liferea_dialog_lookup(prefdialog, "browserlocpopup")), getNumericConfValue(BROWSER_PLACE));

		entry = liferea_dialog_lookup(prefdialog, "browsercmd");
		gtk_entry_set_text(GTK_ENTRY(entry), getStringConfValue(BROWSER_COMMAND));
		gtk_widget_set_sensitive(GTK_WIDGET(entry), tmp==manual);
		gtk_widget_set_sensitive(liferea_dialog_lookup(prefdialog, "manuallabel"), tmp==manual);	

		/* ================== panel 4 "GUI" ================ */

		widget = liferea_dialog_lookup(prefdialog, "popupwindowsoptionbtn");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), getBooleanConfValue(SHOW_POPUP_WINDOWS));
		
		widget = liferea_dialog_lookup(prefdialog, "trayiconoptionbtn");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), getBooleanConfValue(SHOW_TRAY_ICON));

		widget = liferea_dialog_lookup(prefdialog, "newcountintraybtn");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), getBooleanConfValue(SHOW_NEW_COUNT_IN_TRAY));
		gtk_widget_set_sensitive(liferea_dialog_lookup(prefdialog, "newcountintraybtn"), getBooleanConfValue(SHOW_TRAY_ICON));

		widget = liferea_dialog_lookup(prefdialog, "minimizetotraybtn");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), getBooleanConfValue(DONT_MINIMIZE_TO_TRAY));
		gtk_widget_set_sensitive(liferea_dialog_lookup(prefdialog, "minimizetotraybtn"), getBooleanConfValue(SHOW_TRAY_ICON));

		/* menu / tool bar settings */	
		for(i = 1; i <= 3; i++) {
			/* Set fields in the radio widgets so that they know their option # and the pref dialog */
			widgetname = g_strdup_printf("%s%d", "menuradiobtn", i);
			widget = liferea_dialog_lookup(prefdialog, widgetname);
			gtk_object_set_data(GTK_OBJECT(widget), "option_number", GINT_TO_POINTER(i));
			g_free(widgetname);
		}

		/* select currently active menu option */
		tmp = 1;
		if(getBooleanConfValue(DISABLE_TOOLBAR)) tmp = 2;
		if(getBooleanConfValue(DISABLE_MENUBAR)) tmp = 3;

		widgetname = g_strdup_printf("%s%d", "menuradiobtn", tmp);
		widget = liferea_dialog_lookup(prefdialog, widgetname);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
		g_free(widgetname);

		/* select currently active toolbar style option */
		name = getStringConfValue(TOOLBAR_STYLE);

		i = 0;
		for (i = 0; gui_toolbar_style[i] != NULL; ++i) {
			if (strcmp(name, gui_toolbar_style[i]) == 0)
				break;
		}
		g_free (name);

		// Invalid key value. Revert to default
		if (gui_toolbar_style[i] == NULL)
			i = 0;

		gtk_combo_box_set_active (GTK_COMBO_BOX (liferea_dialog_lookup (prefdialog, "toolbarCombo")), i);

		/* ================= panel 5 "proxy" ======================== */
		enabled = getBooleanConfValue(USE_PROXY);
		widget = liferea_dialog_lookup(prefdialog, "enableproxybtn");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), enabled);

		entry = liferea_dialog_lookup(prefdialog, "proxyhostentry");
		gtk_entry_set_text(GTK_ENTRY(entry), getStringConfValue(PROXY_HOST));

		entry = liferea_dialog_lookup(prefdialog, "proxyportentry");
		proxyport = g_strdup_printf("%d", getNumericConfValue(PROXY_PORT));
		gtk_entry_set_text(GTK_ENTRY(entry), proxyport);
		g_free(proxyport);


		/* Authentication */
		enabled2 = getBooleanConfValue(PROXY_USEAUTH);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(prefdialog, "useProxyAuth")), enabled2);
		gtk_entry_set_text(GTK_ENTRY(liferea_dialog_lookup(prefdialog, "proxyuserentry")), getStringConfValue(PROXY_USER));
		gtk_entry_set_text(GTK_ENTRY(liferea_dialog_lookup(prefdialog, "proxypasswordentry")), getStringConfValue(PROXY_PASSWD));

		gtk_widget_set_sensitive(GTK_WIDGET(liferea_dialog_lookup(prefdialog, "proxybox")), enabled);
		if (enabled)
			gtk_widget_set_sensitive(GTK_WIDGET(liferea_dialog_lookup(prefdialog, "proxyauthbox")), enabled2);

		gtk_signal_connect(GTK_OBJECT(liferea_dialog_lookup(prefdialog, "enableproxybtn")), "clicked", G_CALLBACK(on_enableproxybtn_clicked), NULL);
		gtk_signal_connect(GTK_OBJECT(liferea_dialog_lookup(prefdialog, "useProxyAuth")), "clicked", G_CALLBACK(on_enableproxybtn_clicked), NULL);

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
		gtk_option_menu_set_menu(GTK_OPTION_MENU(liferea_dialog_lookup(prefdialog, "enc_download_tool_option_btn")), menu);

		/* set enclosure download path entry */	
		gtk_entry_set_text(GTK_ENTRY(liferea_dialog_lookup(prefdialog, "save_download_entry")), getStringConfValue(ENCLOSURE_DOWNLOAD_PATH));

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

		widget = liferea_dialog_lookup(prefdialog, "enc_actions_view");
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
	nodePtr	displayedNode;

	gboolean enabled = gtk_toggle_button_get_active(togglebutton);
	setBooleanConfValue(FOLDER_DISPLAY_HIDE_READ, enabled);
	displayedNode = itemlist_get_displayed_node();
	if(displayedNode)
		itemlist_load(displayedNode);
}

void on_trayiconoptionbtn_clicked(GtkButton *button, gpointer user_data) {

	gboolean enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
	setBooleanConfValue(SHOW_TRAY_ICON, enabled);
	gtk_widget_set_sensitive(liferea_dialog_lookup(prefdialog, "newcountintraybtn"), enabled);
	gtk_widget_set_sensitive(liferea_dialog_lookup(prefdialog, "minimizetotraybtn"), enabled);
}

void on_popupwindowsoptionbtn_clicked(GtkButton *button, gpointer user_data) {

	gboolean enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
	setBooleanConfValue(SHOW_POPUP_WINDOWS, enabled);
	notification_enable(getBooleanConfValue(SHOW_POPUP_WINDOWS));
	gtk_widget_set_sensitive(liferea_dialog_lookup(prefdialog, "placement_options"), enabled);
}

void
on_startupActionCombo_changed(GtkComboBox *combo, gpointer user_data)
{
	setNumericConfValue(STARTUP_FEED_ACTION, gtk_combo_box_get_active (GTK_COMBO_BOX (liferea_dialog_lookup (prefdialog, "startupActionCombo"))));
}

void on_browsercmd_changed(GtkEditable *editable, gpointer user_data) {

	setStringConfValue(BROWSER_COMMAND, gtk_editable_get_chars(editable,0,-1));
}

static void on_browser_changed(GtkOptionMenu *optionmenu, gpointer user_data) {
	gint num = GPOINTER_TO_INT(user_data);

	gtk_widget_set_sensitive(liferea_dialog_lookup(prefdialog, "browsercmd"), browsers[num].id == NULL);	
	gtk_widget_set_sensitive(liferea_dialog_lookup(prefdialog, "manuallabel"), browsers[num].id == NULL);	

	if (browsers[num].id == NULL)
		setStringConfValue(BROWSER_ID, "manual");
	else
		setStringConfValue(BROWSER_ID, browsers[num].id);
}

static void on_browser_place_changed(GtkOptionMenu *optionmenu, gpointer user_data) {
	int num = GPOINTER_TO_INT(user_data);
	
	setNumericConfValue(BROWSER_PLACE, num);
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

void on_toolbarCombo_changed(GtkComboBox *widget, gpointer user_data) {
	gchar *style;
	gint value = gtk_combo_box_get_active(widget);
	setStringConfValue(TOOLBAR_STYLE, gui_toolbar_style[value]);

	style = conf_get_toolbar_style();
	ui_mainwindow_set_toolbar_style(style);
	g_free(style);
}

void on_itemCountBtn_value_changed(GtkSpinButton *spinbutton, gpointer user_data) {
	GtkAdjustment	*itemCount;
	
	itemCount = gtk_spin_button_get_adjustment(spinbutton);
	setNumericConfValue(DEFAULT_MAX_ITEMS, gtk_adjustment_get_value(itemCount));
}

void on_default_update_interval_value_changed(GtkSpinButton *spinbutton, gpointer user_data) {
	gint		updateInterval;
	GtkWidget	*widget;
	gint		intervalUnit;

	widget = liferea_dialog_lookup(prefdialog, "refreshIntervalUnitComboBox");
	intervalUnit = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	updateInterval = gtk_spin_button_get_value_as_int(spinbutton);

	if (intervalUnit == 1) updateInterval *= 60;		/* hours */
	else if (intervalUnit == 2) updateInterval *= 1440;	/* days */

	setNumericConfValue(DEFAULT_UPDATE_INTERVAL, updateInterval);
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

static void on_updateallfavicons_clicked(GtkButton *button, gpointer user_data) {

	feedlist_foreach(node_update_favicon);
}
 
static void on_enableproxybtn_clicked(GtkButton *button, gpointer user_data) {
	gboolean	enabled;
	
	enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(prefdialog, "useProxyAuth")));
	gtk_widget_set_sensitive(GTK_WIDGET(liferea_dialog_lookup(prefdialog, "proxyauthbox")), enabled);
	setBooleanConfValue(PROXY_USEAUTH, enabled);
	
	enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(prefdialog, "enableproxybtn")));
	gtk_widget_set_sensitive(GTK_WIDGET(liferea_dialog_lookup(prefdialog, "proxybox")), enabled);
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

void
on_skimKeyCombo_changed(GtkComboBox *combo, gpointer user_data) 
{
	setNumericConfValue (BROWSE_KEY_SETTING, gtk_combo_box_get_active (GTK_COMBO_BOX (liferea_dialog_lookup (prefdialog, "skimKeyCombo"))));
}

static void on_enc_download_tool_changed(GtkEditable *editable, gpointer user_data) {

	setNumericConfValue(ENCLOSURE_DOWNLOAD_TOOL, GPOINTER_TO_INT(user_data));
}

void on_enc_action_change_btn_clicked(GtkButton *button, gpointer user_data) {
	GtkTreeModel		*model;
	GtkTreeSelection	*selection;
	GtkTreeIter		iter;
	gpointer		type;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(liferea_dialog_lookup(prefdialog, "enc_actions_view")));
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

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(liferea_dialog_lookup(prefdialog, "enc_actions_view")));
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
		gtk_entry_set_text(GTK_ENTRY(liferea_dialog_lookup(dialog, "save_download_entry")), utfname);
		setStringConfValue(ENCLOSURE_DOWNLOAD_PATH, utfname);
	}
	
	g_free(utfname);
}

void on_save_download_select_btn_clicked(GtkButton *button, gpointer user_data) {
	const gchar *path = gtk_editable_get_chars(GTK_EDITABLE(liferea_dialog_lookup(prefdialog, "save_download_entry")), 0, -1);
	
	ui_choose_directory(_("Choose download directory"), GTK_STOCK_OPEN, on_save_download_finished, path, prefdialog);
}

void on_newcountintraybtn_clicked(GtkButton *button, gpointer user_data) {

	setBooleanConfValue(SHOW_NEW_COUNT_IN_TRAY, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)));
}

void on_minimizetotraybtn_clicked(GtkButton *button, gpointer user_data) {

	setBooleanConfValue(DONT_MINIMIZE_TO_TRAY, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)));
}
