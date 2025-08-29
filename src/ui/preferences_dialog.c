/**
 * @file preferences_dialog.c Liferea preferences
 *
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004-2025 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2009 Hubert Figuiere <hub@figuiere.net>
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

#include "ui/preferences_dialog.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "common.h"
#include "conf.h"
#include "favicon.h"
#include "feedlist.h"
#include "node_providers/folder.h"
#include "itemlist.h"
#include "social.h"
#include "ui/item_list_view.h"
#include "ui/liferea_dialog.h"
#include "ui/liferea_shell.h"
#include "ui/ui_common.h"

/** common private structure for all subscription dialogs */
struct _PreferencesDialog {
	GObject 	parentInstance;

	GtkWidget	*dialog;	/**< the GtkDialog widget */
};

G_DEFINE_TYPE (PreferencesDialog, preferences_dialog, G_TYPE_OBJECT);

/* file type tree store column ids */
enum fts_columns {
	FTS_TYPE,	/* file type name */
	FTS_CMD,	/* file cmd name */
	FTS_PTR,	/* pointer to config entry */
	FTS_LEN
};

extern GSList *bookmarkSites;	/* from social.c */

/* Note: these update interval literal should be kept in sync with the
   ones in ui_subscription.c! */

static const gchar * default_update_interval_unit_options[] = {
	N_("minutes"),
	N_("hours"),
	N_("days"),
	NULL
};

static const gchar * browser_skim_key_options[] = {
	N_("Space"),
	N_("<Ctrl> Space"),
	N_("<Alt> Space"),
	NULL
};

static const gchar * default_view_mode_options[] = {
	N_("Normal View"),
	N_("Wide View"),
	N_("Automatic"),
	NULL
};

static const gchar * browser_id_options[] = {
	N_("Default Browser"),
	N_("Manual"),
	NULL
};

/* Preference dialog class */

static void
preferences_dialog_finalize (GObject *object)
{
	PreferencesDialog *pd = PREFERENCES_DIALOG (object);

	g_object_unref (pd->dialog);
}

static void
preferences_dialog_class_init (PreferencesDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = preferences_dialog_finalize;
}

/* Preference callbacks */

/**
 * The "Hide read items" button has been clicked. Here we change the
 * preference and, if the selected node is a folder, we reload the
 * itemlist. The item selection is lost by this.
 */
void
on_folder_settings_changed (GtkCheckButton *togglebutton, gpointer user_data)
{
	Node		*displayedNode;

	displayedNode = itemlist_get_displayed_node ();
	if (displayedNode && IS_FOLDER (displayedNode)) {
		itemlist_unload ();
		itemlist_load (displayedNode);

		/* Note: For simplicity when toggling this preference we
		   accept that the current item selection is lost. */
	}
}

static void
on_browser_changed (GtkDropDown *dropdown, gpointer user_data)
{
	PreferencesDialog *pd = PREFERENCES_DIALOG (user_data);
	gint		num = gtk_drop_down_get_selected (dropdown);

	gtk_widget_set_sensitive (liferea_dialog_lookup (pd->dialog, "browsercmd"), num != 0);
	gtk_widget_set_sensitive (liferea_dialog_lookup (pd->dialog, "manuallabel"), num != 0);
	gtk_widget_set_sensitive (liferea_dialog_lookup (pd->dialog, "urlhintlabel"), num != 0);
}

/*
static void
on_socialsite_changed (GtkComboBox *optionmenu, gpointer user_data)
{
	GtkTreeIter iter;
	if (gtk_combo_box_get_active_iter (optionmenu, &iter)) {
		gchar * site;
		gtk_tree_model_get (gtk_combo_box_get_model (optionmenu), &iter, 0, &site, -1);
		social_set_bookmark_site (site);
	}
}*/

void
on_itemCountBtn_value_changed (GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkAdjustment	*itemCount;

	itemCount = gtk_spin_button_get_adjustment (spinbutton);
	conf_set_int_value (DEFAULT_MAX_ITEMS, gtk_adjustment_get_value (itemCount));
}

static void
on_default_update_interval_value_changed (GtkSpinButton *spinbutton, gpointer user_data)
{
	PreferencesDialog *pd = PREFERENCES_DIALOG (user_data);
	gint		updateInterval, intervalUnit;
	GtkWidget	*unitWidget, *valueWidget;

	unitWidget = liferea_dialog_lookup (pd->dialog, "globalRefreshIntervalUnit");
	valueWidget = liferea_dialog_lookup (pd->dialog, "globalRefreshIntervalSpinButton");
	intervalUnit = gtk_drop_down_get_selected (GTK_DROP_DOWN (unitWidget));
	updateInterval = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (valueWidget));

	if (intervalUnit == 1)
		updateInterval *= 60;		/* hours */
	else if (intervalUnit == 2)
		updateInterval *= 1440;		/* days */
g_print("updateInterval %d unit %d\n", updateInterval, intervalUnit);
	conf_set_int_value (DEFAULT_UPDATE_INTERVAL, updateInterval);
}

static void
on_updateallfavicons_clicked (GtkButton *button, gpointer user_data)
{
	feedlist_foreach (node_update_favicon);
}

static void
on_proxyAutoDetect_clicked (GtkButton *button, gpointer user_data)
{
	PreferencesDialog *pd = PREFERENCES_DIALOG (user_data);

	conf_set_int_value (PROXY_DETECT_MODE, 0);
	gtk_widget_set_sensitive (GTK_WIDGET (liferea_dialog_lookup (pd->dialog, "proxybox")), FALSE);
}

static void
on_noProxy_clicked (GtkButton *button, gpointer user_data)
{
	PreferencesDialog *pd = PREFERENCES_DIALOG (user_data);

	conf_set_int_value (PROXY_DETECT_MODE, 1);
	gtk_widget_set_sensitive (GTK_WIDGET (liferea_dialog_lookup (pd->dialog, "proxybox")), FALSE);
}

static void
on_drop_down_changed (GtkDropDown *dropdown, guint selected, gpointer user_data)
{
	PreferencesDialog *pd = PREFERENCES_DIALOG (user_data);
	const gchar * settingName;
	g_print("on_drop_down_changed %s selected %d\n", gtk_widget_get_name (GTK_WIDGET (dropdown)), selected);
	if (dropdown == GTK_DROP_DOWN (liferea_dialog_lookup (pd->dialog, "skimKey"))) {
		settingName = BROWSE_KEY_SETTING;
	} else if (dropdown == GTK_DROP_DOWN (liferea_dialog_lookup (pd->dialog, "defaultViewMode"))) {
		settingName = DEFAULT_VIEW_MODE;
	} else if (dropdown == GTK_DROP_DOWN (liferea_dialog_lookup (pd->dialog, "browserpopup"))) {
		settingName = BROWSER_ID;
		// extra handling
		on_browser_changed (dropdown, pd);
	} else if (dropdown == GTK_DROP_DOWN (liferea_dialog_lookup (pd->dialog, "globalRefreshIntervalUnit"))) {
		// special handling and no standard conf_set_int_value()!
		on_default_update_interval_value_changed (NULL, pd);
		return;
	} else {
		g_warning ("preferences dialog: unknown drop down changed");
		return;
	}

	conf_set_int_value (settingName, gtk_drop_down_get_selected (dropdown));
}

/* To map our legacy integer dropdowns to gsettings we have this setup helper */
static void
preferences_dialog_setup_drop_down (PreferencesDialog *pd, const gchar *widget_name, const gchar **options, const gchar *setting_name)
{
	GtkDropDown *dropdown = GTK_DROP_DOWN (liferea_dialog_lookup (pd->dialog, widget_name));
	g_autoptr(GtkStringList) list = gtk_string_list_new (options);

	gint selected;
	conf_get_int_value (setting_name, &selected);
	gtk_drop_down_set_model (dropdown, G_LIST_MODEL (list));
	gtk_drop_down_set_selected (dropdown, (guint)selected);
	g_signal_connect (G_OBJECT (dropdown), "notify::selected", G_CALLBACK (on_drop_down_changed), pd);
}

void
preferences_dialog_init (PreferencesDialog *pd)
{
	GtkWidget		*widget;
	gint			tmp, i;

	pd->dialog = liferea_dialog_new ("prefs");

	/* ================== panel 1 "feeds" ==================== */

	conf_bind (CONFIRM_MARK_ALL_READ, liferea_dialog_lookup (pd->dialog, "confirmMarkAllReadButton"), "active", G_SETTINGS_BIND_DEFAULT);
	conf_bind (DEFAULT_MAX_ITEMS, liferea_dialog_lookup (pd->dialog, "itemCountBtn"), "value", G_SETTINGS_BIND_DEFAULT);

	/* set default update interval spin button and unit drop down */
	preferences_dialog_setup_drop_down (pd, "globalRefreshIntervalUnit", default_update_interval_unit_options, DEFAULT_UPDATE_INTERVAL);

	widget = liferea_dialog_lookup (pd->dialog, "globalRefreshIntervalUnit");
	conf_get_int_value (DEFAULT_UPDATE_INTERVAL, &tmp);
	if (tmp % 1440 == 0) {		/* days */
		gtk_drop_down_set_selected (GTK_DROP_DOWN (widget), 2);
		tmp /= 1440;
	} else if (tmp % 60 == 0) {	/* hours */
		gtk_drop_down_set_selected (GTK_DROP_DOWN (widget), 1);
		tmp /= 60;
	} else {			/* minutes */
		gtk_drop_down_set_selected (GTK_DROP_DOWN (widget), 0);
	}
	widget = liferea_dialog_lookup (pd->dialog,"globalRefreshIntervalSpinButton");
	gtk_spin_button_set_range (GTK_SPIN_BUTTON (widget), 0, 1000000000);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), tmp);
	g_signal_connect (G_OBJECT (widget), "changed", G_CALLBACK (on_default_update_interval_value_changed), pd);

	/* ================== panel 2 "folders" ==================== */

	g_signal_connect (G_OBJECT (liferea_dialog_lookup (pd->dialog, "updateAllFavicons")), "clicked", G_CALLBACK(on_updateallfavicons_clicked), pd);

	conf_bind (FOLDER_DISPLAY_CHILDREN, liferea_dialog_lookup (pd->dialog, "folderdisplaybtn"), "active", G_SETTINGS_BIND_DEFAULT);
	conf_bind (FOLDER_DISPLAY_HIDE_READ, liferea_dialog_lookup (pd->dialog, "hidereadbtn"), "active", G_SETTINGS_BIND_DEFAULT);

	/* ================== panel 3 "headlines" ==================== */

	preferences_dialog_setup_drop_down (pd, "skimKey", browser_skim_key_options, BROWSE_KEY_SETTING);
	preferences_dialog_setup_drop_down (pd, "defaultViewMode", default_view_mode_options, DEFAULT_VIEW_MODE);

	conf_bind (DEFER_DELETE_MODE, liferea_dialog_lookup (pd->dialog, "deferdeletebtn"), "active", G_SETTINGS_BIND_DEFAULT);

	/* Setup social bookmarking list */
	/*i = 0;
	conf_get_str_value (SOCIAL_BM_SITE, &name);
	store = gtk_list_store_new (1, G_TYPE_STRING);
	list = bookmarkSites;
	while (list) {
		socialSitePtr siter = list->data;
		if (name && !strcmp (siter->name, name))
			tmp = i;
		gtk_list_store_append (store, &treeiter);
		gtk_list_store_set (store, &treeiter, 0, siter->name, -1);
		list = g_slist_next (list);
		i++;
	}

	combo = GTK_COMBO_BOX (liferea_dialog_lookup (pd->dialog, "socialpopup"));
	g_signal_connect (G_OBJECT (combo), "changed", G_CALLBACK (on_socialsite_changed), pd);
	gtk_combo_box_set_model (combo, GTK_TREE_MODEL (store));
	ui_common_setup_combo_text (combo, 0);
	gtk_combo_box_set_active (combo, tmp);*/

	/* ================== panel 4 "browser" ==================== */

	conf_bind (BROWSE_INSIDE_APPLICATION, liferea_dialog_lookup (pd->dialog, "browseinwindow"), "active", G_SETTINGS_BIND_DEFAULT);
	conf_bind (DISABLE_JAVASCRIPT, liferea_dialog_lookup (pd->dialog, "disablejavascript"), "active", G_SETTINGS_BIND_DEFAULT);
	conf_bind (BROWSER_COMMAND, liferea_dialog_lookup (pd->dialog, "browsercmd"), "text", G_SETTINGS_BIND_DEFAULT);
	
	preferences_dialog_setup_drop_down (pd, "browserpopup", browser_id_options, BROWSER_ID);
	// call on_browser_changed() to conditionally enable dependant widgets
	on_browser_changed (GTK_DROP_DOWN (liferea_dialog_lookup (pd->dialog, "browserpopup")), pd);

	/* ================== panel 4 "GUI" ================ */

	conf_bind (CONFIRM_MARK_ALL_READ, liferea_dialog_lookup (pd->dialog, "confirmMarkAllReadButton"), "active", G_SETTINGS_BIND_DEFAULT);

	/* ================= panel 5 "proxy" ======================== */

	conf_get_int_value (PROXY_DETECT_MODE, &i);
	switch (i) {
		default:
		case 2: /* manual proxy -> deprecated so fall through to auto detect */
		case 0: /* proxy auto detect */
			gtk_check_button_set_active (GTK_CHECK_BUTTON (liferea_dialog_lookup (pd->dialog, "proxyAutoDetectRadio")), TRUE);
			break;
		case 1: /* no proxy */
			gtk_check_button_set_active (GTK_CHECK_BUTTON (liferea_dialog_lookup (pd->dialog, "noProxyRadio")), TRUE);
			break;
	}
	g_signal_connect (G_OBJECT (liferea_dialog_lookup (pd->dialog, "proxyAutoDetectRadio")), "clicked", G_CALLBACK (on_proxyAutoDetect_clicked), pd);
	g_signal_connect (G_OBJECT (liferea_dialog_lookup (pd->dialog, "noProxyRadio")), "clicked", G_CALLBACK (on_noProxy_clicked), pd);

	/* ================= panel 6 "Privacy" ======================== */

	conf_bind (ENABLE_ITP,   liferea_dialog_lookup (pd->dialog, "itpbtn"), "active", G_SETTINGS_BIND_DEFAULT);
	conf_bind (DO_NOT_TRACK, liferea_dialog_lookup (pd->dialog, "donottrackbtn"), "active", G_SETTINGS_BIND_DEFAULT);
	conf_bind (DO_NOT_SELL,  liferea_dialog_lookup (pd->dialog, "donotsellbtn"), "active", G_SETTINGS_BIND_DEFAULT);

	g_signal_connect_swapped (G_OBJECT (pd->dialog), "response", G_CALLBACK (gtk_window_close), pd->dialog);

	gtk_window_present (GTK_WINDOW (pd->dialog));
}