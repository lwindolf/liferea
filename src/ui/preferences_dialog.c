/**
 * @file preferences_dialog.c Liferea preferences
 *
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004-2023 Lars Windolf <lars.windolf@gmx.de>
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

#include <webkit2/webkit2.h>

#include "common.h"
#include "conf.h"
#include "enclosure.h"
#include "favicon.h"
#include "feedlist.h"
#include "folder.h"
#include "itemlist.h"
#include "social.h"
#include "ui/enclosure_list_view.h"
#include "ui/item_list_view.h"
#include "ui/liferea_dialog.h"
#include "ui/liferea_shell.h"
#include "ui/ui_common.h"
#include "ui/itemview.h"

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

static PreferencesDialog *prefdialog = NULL;

/** download tool commands need to take an URI as %s */
static const gchar * enclosure_download_commands[] = {
	"steadyflow add %s",
	"dbus-send --session --print-reply --dest=org.gnome.gwget.ApplicationService /org/gnome/gwget/Gwget org.gnome.gwget.Application.OpenURI string:%s uint32:0",
	"kget %s",
	"uget-gtk %s",
	"transmission-gtk %s",
	"aria2c %s"
};

/** order must match enclosure_download_commands[] */
static const gchar *enclosure_download_tool_options[] = { "Steadyflow", "gwget", "KGet", "uGet", "Transmission (Gtk)", "aria2", NULL };

/** GConf representation of toolbar styles */
static const gchar * gui_toolbar_style_values[] = { "", "both", "both-horiz", "icons", "text", NULL };

static const gchar * gui_toolbar_style_options[] = {
	N_("GNOME default"),
	N_("Text below icons"),
	N_("Text beside icons"),
	N_("Icons only"),
	N_("Text only"),
	NULL
};

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

gchar *
prefs_get_download_command (void)
{
	gboolean 	use_custom_command;
	gchar 		*result = NULL;

	conf_get_bool_value (DOWNLOAD_USE_CUSTOM_COMMAND, &use_custom_command);
	if (use_custom_command)
		conf_get_str_value (DOWNLOAD_CUSTOM_COMMAND, &result);
	else {
		gint	enclosure_download_tool;
		conf_get_int_value (DOWNLOAD_TOOL, &enclosure_download_tool);
		/* FIXME: array boundary check */
		result = g_strdup (enclosure_download_commands[enclosure_download_tool]);
	}
	return result;
}

/* Preference dialog class */

static void
preferences_dialog_finalize (GObject *object)
{
	PreferencesDialog *pd = PREFERENCES_DIALOG (object);

	gtk_widget_destroy (pd->dialog);
	prefdialog = NULL;
}

static void
preferences_dialog_class_init (PreferencesDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = preferences_dialog_finalize;
}

/* Preference callbacks */

void
on_folderdisplaybtn_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean enabled = gtk_toggle_button_get_active(togglebutton);
	conf_set_int_value(FOLDER_DISPLAY_MODE, (TRUE == enabled)?1:0);
}

/**
 * The "Hide read items" button has been clicked. Here we change the
 * preference and, if the selected node is a folder, we reload the
 * itemlist. The item selection is lost by this.
 */
void
on_folderhidereadbtn_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
	nodePtr		displayedNode;
	gboolean	enabled;

	displayedNode = itemlist_get_displayed_node ();

	enabled = gtk_toggle_button_get_active (togglebutton);
	conf_set_bool_value (FOLDER_DISPLAY_HIDE_READ, enabled);

	if (displayedNode && IS_FOLDER (displayedNode)) {
		itemlist_unload ();
		itemlist_load (displayedNode);

		/* Note: For simplicity when toggling this preference we
		   accept that the current item selection is lost. */
	}
}

void
on_startupactionbtn_toggled (GtkButton *button, gpointer user_data)
{
	gboolean enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	conf_set_int_value (STARTUP_FEED_ACTION, enabled?0:1);
}

void
on_browsercmd_changed (GtkEditable *editable, gpointer user_data)
{
	conf_set_str_value (BROWSER_COMMAND, gtk_editable_get_chars (editable,0,-1));
}

static void
on_browser_changed (GtkComboBox *optionmenu, gpointer user_data)
{
	GtkTreeIter		iter;
	gint			num = -1;

	if (gtk_combo_box_get_active_iter (optionmenu, &iter)) {
		gtk_tree_model_get (gtk_combo_box_get_model (optionmenu), &iter, 1, &num, -1);

		gtk_widget_set_sensitive (liferea_dialog_lookup (prefdialog->dialog, "browsercmd"), num != 0);
		gtk_widget_set_sensitive (liferea_dialog_lookup (prefdialog->dialog, "manuallabel"), num != 0);
		gtk_widget_set_sensitive (liferea_dialog_lookup (prefdialog->dialog, "urlhintlabel"), num != 0);

		if (!num)
			conf_set_str_value (BROWSER_ID, "default");
		else
			conf_set_str_value (BROWSER_ID, "manual");
	}
}

void
on_openlinksinsidebtn_clicked (GtkToggleButton *button, gpointer user_data)
{
	conf_set_bool_value (BROWSE_INSIDE_APPLICATION, gtk_toggle_button_get_active (button));
}

void
on_disablejavascript_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
	conf_set_bool_value (DISABLE_JAVASCRIPT, gtk_toggle_button_get_active (togglebutton));
}

void
on_enableplugins_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
	conf_set_bool_value (ENABLE_PLUGINS, gtk_toggle_button_get_active (togglebutton));
}

static void
on_socialsite_changed (GtkComboBox *optionmenu, gpointer user_data)
{
	GtkTreeIter iter;
	if (gtk_combo_box_get_active_iter (optionmenu, &iter)) {
		gchar * site;
		gtk_tree_model_get (gtk_combo_box_get_model (optionmenu), &iter, 0, &site, -1);
		social_set_bookmark_site (site);
	}
}

static void
on_gui_toolbar_style_changed (gpointer user_data)
{
	gchar *style;
	gint value = gtk_combo_box_get_active (GTK_COMBO_BOX (user_data));
	conf_set_str_value (TOOLBAR_STYLE, gui_toolbar_style_values[value]);

	style = conf_get_toolbar_style ();
	liferea_shell_set_toolbar_style (style);
	g_free (style);
}

void
on_itemCountBtn_value_changed (GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkAdjustment	*itemCount;

	itemCount = gtk_spin_button_get_adjustment (spinbutton);
	conf_set_int_value (DEFAULT_MAX_ITEMS, gtk_adjustment_get_value (itemCount));
}

void
on_default_update_interval_value_changed (GtkSpinButton *spinbutton, gpointer user_data)
{
	gint			updateInterval, intervalUnit;
	GtkWidget		*unitWidget, *valueWidget;

	unitWidget = liferea_dialog_lookup (prefdialog->dialog, "globalRefreshIntervalUnitComboBox");
	valueWidget = liferea_dialog_lookup (prefdialog->dialog, "globalRefreshIntervalSpinButton");
	intervalUnit = gtk_combo_box_get_active (GTK_COMBO_BOX (unitWidget));
	updateInterval = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (valueWidget));

	if (intervalUnit == 1)
		updateInterval *= 60;		/* hours */
	else if (intervalUnit == 2)
		updateInterval *= 1440;		/* days */

	conf_set_int_value (DEFAULT_UPDATE_INTERVAL, updateInterval);
}

static void
on_default_update_interval_unit_changed (gpointer user_data)
{
	on_default_update_interval_value_changed (NULL, prefdialog);
}

static void
on_updateallfavicons_clicked (GtkButton *button, gpointer user_data)
{
	feedlist_foreach (node_update_favicon);
}

static void
on_proxyAutoDetect_clicked (GtkButton *button, gpointer user_data)
{
	conf_set_int_value (PROXY_DETECT_MODE, 0);
	gtk_widget_set_sensitive (GTK_WIDGET (liferea_dialog_lookup (prefdialog->dialog, "proxybox")), FALSE);
}

static void
on_noProxy_clicked (GtkButton *button, gpointer user_data)
{
	conf_set_int_value (PROXY_DETECT_MODE, 1);
	gtk_widget_set_sensitive (GTK_WIDGET (liferea_dialog_lookup (prefdialog->dialog, "proxybox")), FALSE);
}

static void
on_manualProxy_clicked (GtkButton *button, gpointer user_data)
{
	conf_set_int_value (PROXY_DETECT_MODE, 2);
	gtk_widget_set_sensitive (GTK_WIDGET (liferea_dialog_lookup (prefdialog->dialog, "proxybox")), TRUE);
}

void
on_useProxyAuth_toggled (GtkToggleButton *button, gpointer user_data)
{
	gboolean enabled = gtk_toggle_button_get_active (button);

	gtk_widget_set_sensitive (GTK_WIDGET (liferea_dialog_lookup (prefdialog->dialog, "proxyauthtable")), enabled);
	conf_set_bool_value (PROXY_USEAUTH, enabled);
}

static void
on_proxyhostentry_changed (GtkEditable *editable, gpointer user_data)
{
	conf_set_str_value (PROXY_HOST, gtk_editable_get_chars (editable,0,-1));
}

static void
on_proxyportentry_changed (GtkEditable *editable, gpointer user_data)
{
	conf_set_int_value (PROXY_PORT, atoi (gtk_editable_get_chars (editable,0,-1)));
}

static void
on_proxyusernameentry_changed (GtkEditable *editable, gpointer user_data)
{
	conf_set_str_value (PROXY_USER, gtk_editable_get_chars (editable,0,-1));
}

static void
on_proxypasswordentry_changed (GtkEditable *editable, gpointer user_data)
{
	conf_set_str_value (PROXY_PASSWD, gtk_editable_get_chars (editable,0,-1));
}

static void
on_skim_key_changed (gpointer user_data)
{
	conf_set_int_value (BROWSE_KEY_SETTING, gtk_combo_box_get_active (GTK_COMBO_BOX (user_data)));
}

static void
on_default_view_mode_changed (gpointer user_data)
{
	gint 	mode = gtk_combo_box_get_active (GTK_COMBO_BOX (user_data));
	
	conf_set_int_value (DEFAULT_VIEW_MODE, mode);
	itemview_set_layout (mode);
}

void
on_deferdeletemode_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean	enabled;

	enabled = gtk_toggle_button_get_active (togglebutton);
	conf_set_bool_value (DEFER_DELETE_MODE, enabled);
}

void
on_enclosure_download_predefined_toggled (GtkToggleButton *button, gpointer user_data)
{
	gboolean is_active = gtk_toggle_button_get_active (button);

	gtk_widget_set_sensitive (liferea_dialog_lookup (prefdialog->dialog, "customDownloadEntry"), !is_active);
	gtk_widget_set_sensitive (liferea_dialog_lookup (prefdialog->dialog, "downloadToolCombo"), is_active);
	conf_set_bool_value (DOWNLOAD_USE_CUSTOM_COMMAND, !is_active);
}

static void
on_enclosure_download_tool_changed (gpointer user_data)
{
	conf_set_int_value (DOWNLOAD_TOOL, gtk_combo_box_get_active (GTK_COMBO_BOX (user_data)));
}

void
on_enclosure_download_custom_command_changed (GtkEditable *entry, gpointer user_data)
{
	conf_set_str_value (DOWNLOAD_CUSTOM_COMMAND, gtk_entry_get_text (GTK_ENTRY (entry)));
}

void
on_enc_action_change_btn_clicked (GtkButton *button, gpointer user_data)
{
	GtkTreeModel		*model;
	GtkTreeSelection	*selection;
	GtkTreeIter		iter;
	gpointer		type;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (liferea_dialog_lookup (prefdialog->dialog, "enc_action_view")));
	if(gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (model, &iter, FTS_PTR, &type, -1);
		ui_enclosure_change_type (type);
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
		                    FTS_CMD, ((encTypePtr)type)->cmd, -1);
	}
}

void
on_enc_action_remove_btn_clicked (GtkButton *button, gpointer user_data)
{
	GtkTreeModel		*model;
	GtkTreeSelection	*selection;
	GtkTreeIter		iter;
	gpointer		type;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (liferea_dialog_lookup (prefdialog->dialog, "enc_action_view")));
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (model, &iter, FTS_PTR, &type, -1);
		gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
		enclosure_mime_type_remove (type);
	}
}

void
on_hidetoolbar_toggled (GtkToggleButton *button, gpointer user_data)
{
	conf_set_bool_value (DISABLE_TOOLBAR, gtk_toggle_button_get_active (button));
	liferea_shell_update_toolbar ();
}

void
on_readermodebtn_toggled (GtkToggleButton *button, gpointer user_data)
{
	conf_set_bool_value (ENABLE_READER_MODE, gtk_toggle_button_get_active (button));
}

void
on_donottrackbtn_toggled (GtkToggleButton *button, gpointer user_data)
{
	conf_set_bool_value (DO_NOT_TRACK, gtk_toggle_button_get_active (button));
}

void
on_itpbtn_toggled (GtkToggleButton *button, gpointer user_data)
{
	conf_set_bool_value (ENABLE_ITP, gtk_toggle_button_get_active (button));
}

static void
preferences_dialog_destroy_cb (GtkWidget *widget, PreferencesDialog *pd)
{
	g_object_unref (pd);
}

void
preferences_dialog_init (PreferencesDialog *pd)
{
	GtkWidget		*widget, *entry;
	GtkComboBox		*combo;
	GtkListStore		*store;
	GtkTreeIter		treeiter;
	GtkAdjustment		*itemCount;
	GtkTreeStore		*treestore;
	GtkTreeViewColumn 	*column;
	GSList			*list;
	gchar			*proxyport;
	gchar			*configuredBrowser, *name;
	gboolean		enabled;
	gint			tmp, i, iSetting, proxy_port;
	gboolean		bSetting, manualBrowser;
	gchar			*proxy_host, *proxy_user, *proxy_passwd;
	gchar			*browser_command;
	gchar 			*custom_download_command;

	prefdialog = pd;
	pd->dialog = liferea_dialog_new ("prefs");

	/* Set up browser selection popup */
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
	gtk_list_store_append (store, &treeiter);
	gtk_list_store_set (store, &treeiter, 0, _("Default Browser"), 1, 0, -1);
	gtk_list_store_append (store, &treeiter);
	gtk_list_store_set (store, &treeiter, 0, _("Manual"), 1, 1, -1);

	combo = GTK_COMBO_BOX (liferea_dialog_lookup (pd->dialog, "browserpopup"));
	gtk_combo_box_set_model (combo, GTK_TREE_MODEL (store));
	ui_common_setup_combo_text (combo, 0);
	g_signal_connect(G_OBJECT(combo), "changed", G_CALLBACK(on_browser_changed), pd);

	/* ================== panel 1 "feeds" ==================== */

	/* check box for feed startup update */
	conf_get_int_value (STARTUP_FEED_ACTION, &iSetting);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (pd->dialog, "startupactionbtn")), (iSetting == 0));

	/* cache size setting */
	widget = liferea_dialog_lookup (pd->dialog, "itemCountBtn");
	itemCount = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget));
	conf_get_int_value (DEFAULT_MAX_ITEMS, &iSetting);
	gtk_adjustment_set_value (itemCount, iSetting);

	/* set default update interval spin button and unit combo box */
	ui_common_setup_combo_menu (liferea_dialog_lookup (pd->dialog, "globalRefreshIntervalUnitComboBox"),
	                            default_update_interval_unit_options,
	                            G_CALLBACK (on_default_update_interval_unit_changed),
				    -1);

	widget = liferea_dialog_lookup (pd->dialog, "globalRefreshIntervalUnitComboBox");
	conf_get_int_value (DEFAULT_UPDATE_INTERVAL, &tmp);
	if (tmp % 1440 == 0) {		/* days */
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 2);
		tmp /= 1440;
	} else if (tmp % 60 == 0) {	/* hours */
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 1);
		tmp /= 60;
	} else {			/* minutes */
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
	}
	widget = liferea_dialog_lookup (pd->dialog,"globalRefreshIntervalSpinButton");
	gtk_spin_button_set_range (GTK_SPIN_BUTTON (widget), 0, 1000000000);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), tmp);
	g_signal_connect (G_OBJECT (widget), "changed", G_CALLBACK (on_default_update_interval_value_changed), pd);

	/* ================== panel 2 "folders" ==================== */

	g_signal_connect (G_OBJECT (liferea_dialog_lookup (pd->dialog, "updateAllFavicons")), "clicked", G_CALLBACK(on_updateallfavicons_clicked), pd);

	conf_get_int_value (FOLDER_DISPLAY_MODE, &iSetting);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (liferea_dialog_lookup (pd->dialog, "folderdisplaybtn")), iSetting?TRUE:FALSE);
	conf_get_bool_value (FOLDER_DISPLAY_HIDE_READ, &bSetting);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (liferea_dialog_lookup (pd->dialog, "hidereadbtn")), bSetting?TRUE:FALSE);

	/* ================== panel 3 "headlines" ==================== */

	conf_get_int_value (BROWSE_KEY_SETTING, &iSetting);
	ui_common_setup_combo_menu (liferea_dialog_lookup (pd->dialog, "skimKeyCombo"),
	                            browser_skim_key_options,
	                            G_CALLBACK (on_skim_key_changed),
	                            iSetting);

	conf_get_int_value (DEFAULT_VIEW_MODE, &iSetting);
	ui_common_setup_combo_menu (liferea_dialog_lookup (pd->dialog, "defaultViewModeCombo"),
	                            default_view_mode_options,
	                            G_CALLBACK (on_default_view_mode_changed),
	                            iSetting);

	conf_get_bool_value (DEFER_DELETE_MODE, &bSetting);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (liferea_dialog_lookup (pd->dialog, "deferdeletebtn")), bSetting?TRUE:FALSE);

	/* Setup social bookmarking list */
	i = 0;
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
	gtk_combo_box_set_active (combo, tmp);

	/* ================== panel 4 "browser" ==================== */

	/* set the inside browsing flag */
	widget = liferea_dialog_lookup(pd->dialog, "browseinwindow");
	conf_get_bool_value(BROWSE_INSIDE_APPLICATION, &bSetting);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), bSetting);

	/* set the javascript-disabled flag */
	widget = liferea_dialog_lookup(pd->dialog, "disablejavascript");
	conf_get_bool_value(DISABLE_JAVASCRIPT, &bSetting);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), bSetting);

	/* set the enable Plugins flag */
	widget = liferea_dialog_lookup(pd->dialog, "enableplugins");
	conf_get_bool_value(ENABLE_PLUGINS, &bSetting);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), bSetting);

	conf_get_str_value (BROWSER_ID, &configuredBrowser);
	manualBrowser = !strcmp (configuredBrowser, "manual");
	g_free (configuredBrowser);

	gtk_combo_box_set_active (GTK_COMBO_BOX (liferea_dialog_lookup (pd->dialog, "browserpopup")), manualBrowser);

	conf_get_str_value (BROWSER_COMMAND, &browser_command);
	entry = liferea_dialog_lookup (pd->dialog, "browsercmd");
	gtk_entry_set_text (GTK_ENTRY(entry), browser_command);
	g_free (browser_command);

	gtk_widget_set_sensitive (GTK_WIDGET (entry), manualBrowser);
	gtk_widget_set_sensitive (liferea_dialog_lookup (pd->dialog, "manuallabel"), manualBrowser);
	gtk_widget_set_sensitive (liferea_dialog_lookup (pd->dialog, "urlhintlabel"), manualBrowser);

	/* ================== panel 4 "GUI" ================ */

	/* tool bar settings */
	widget = liferea_dialog_lookup (pd->dialog, "hidetoolbarbtn");
	conf_get_bool_value(DISABLE_TOOLBAR, &bSetting);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), bSetting);

	/* select currently active toolbar style option */
	conf_get_str_value (TOOLBAR_STYLE, &name);
	for (i = 0; gui_toolbar_style_values[i] != NULL; ++i) {
		if (strcmp (name, gui_toolbar_style_values[i]) == 0)
			break;
	}
	g_free (name);

	/* On invalid key value: revert to default */
	if (gui_toolbar_style_values[i] == NULL)
		i = 0;

	/* create toolbar style menu */
	ui_common_setup_combo_menu (liferea_dialog_lookup (pd->dialog, "toolbarCombo"),
	                            gui_toolbar_style_options,
	                            G_CALLBACK (on_gui_toolbar_style_changed),
	                            i);

	conf_bind (CONFIRM_MARK_ALL_READ, liferea_dialog_lookup (pd->dialog, "confirmMarkAllReadButton"), "active", G_SETTINGS_BIND_DEFAULT);

	/* ================= panel 5 "proxy" ======================== */

#if WEBKIT_CHECK_VERSION (2, 15, 3)
	gtk_widget_destroy (GTK_WIDGET (liferea_dialog_lookup (pd->dialog, "proxyDisabledInfobar")));
	conf_get_str_value (PROXY_HOST, &proxy_host);
	gtk_entry_set_text (GTK_ENTRY (liferea_dialog_lookup (pd->dialog, "proxyhostentry")), proxy_host);
	g_free (proxy_host);

	conf_get_int_value (PROXY_PORT, &proxy_port);
	proxyport = g_strdup_printf ("%d", proxy_port);
	gtk_entry_set_text (GTK_ENTRY (liferea_dialog_lookup (pd->dialog, "proxyportentry")), proxyport);
	g_free (proxyport);

	conf_get_bool_value (PROXY_USEAUTH, &enabled);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (pd->dialog, "useProxyAuth")), enabled);

	conf_get_str_value (PROXY_USER, &proxy_user);
	gtk_entry_set_text (GTK_ENTRY (liferea_dialog_lookup (pd->dialog, "proxyusernameentry")), proxy_user);
	g_free (proxy_user);

	conf_get_str_value (PROXY_PASSWD, &proxy_passwd);
	gtk_entry_set_text (GTK_ENTRY (liferea_dialog_lookup (pd->dialog, "proxypasswordentry")), proxy_passwd);
	g_free (proxy_passwd);

	gtk_widget_set_sensitive (GTK_WIDGET (liferea_dialog_lookup(pd->dialog, "proxyauthtable")), enabled);

	conf_get_int_value (PROXY_DETECT_MODE, &i);
	switch (i) {
		default:
		case 0: /* proxy auto detect */
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (pd->dialog, "proxyAutoDetectRadio")), TRUE);
			enabled = FALSE;
			break;
		case 1: /* no proxy */
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (pd->dialog, "noProxyRadio")), TRUE);
			enabled = FALSE;
			break;
		case 2: /* manual proxy */
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (pd->dialog, "manualProxyRadio")), TRUE);
			enabled = TRUE;
			break;
	}
	gtk_widget_set_sensitive (GTK_WIDGET (liferea_dialog_lookup (pd->dialog, "proxybox")), enabled);
	g_signal_connect (G_OBJECT (liferea_dialog_lookup (pd->dialog, "proxyAutoDetectRadio")), "clicked", G_CALLBACK (on_proxyAutoDetect_clicked), pd);
	g_signal_connect (G_OBJECT (liferea_dialog_lookup (pd->dialog, "noProxyRadio")), "clicked", G_CALLBACK (on_noProxy_clicked), pd);
	g_signal_connect (G_OBJECT (liferea_dialog_lookup (pd->dialog, "manualProxyRadio")), "clicked", G_CALLBACK (on_manualProxy_clicked), pd);
	g_signal_connect (G_OBJECT (liferea_dialog_lookup (pd->dialog, "proxyhostentry")), "changed", G_CALLBACK (on_proxyhostentry_changed), pd);
	g_signal_connect (G_OBJECT (liferea_dialog_lookup (pd->dialog, "proxyportentry")), "changed", G_CALLBACK (on_proxyportentry_changed), pd);
	g_signal_connect (G_OBJECT (liferea_dialog_lookup (pd->dialog, "proxyusernameentry")), "changed", G_CALLBACK (on_proxyusernameentry_changed), pd);
	g_signal_connect (G_OBJECT (liferea_dialog_lookup (pd->dialog, "proxypasswordentry")), "changed", G_CALLBACK (on_proxypasswordentry_changed), pd);
#else
	gtk_widget_show (GTK_WIDGET (liferea_dialog_lookup (pd->dialog, "proxyDisabledInfobar")));
	gtk_widget_set_sensitive (GTK_WIDGET (liferea_dialog_lookup (pd->dialog, "proxybox")), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (liferea_dialog_lookup (pd->dialog, "proxyAutoDetectRadio")), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (liferea_dialog_lookup (pd->dialog, "noProxyRadio")), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (liferea_dialog_lookup (pd->dialog, "manualProxyRadio")), FALSE);
#endif

	/* ================= panel 6 "Privacy" ======================== */

	widget = liferea_dialog_lookup (pd->dialog, "readermodebtn");
	conf_get_bool_value (ENABLE_READER_MODE, &bSetting);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), bSetting);

	widget = liferea_dialog_lookup (pd->dialog, "donottrackbtn");
	conf_get_bool_value (DO_NOT_TRACK, &bSetting);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), bSetting);

#if WEBKIT_CHECK_VERSION (2, 30, 0)
	gtk_widget_destroy (GTK_WIDGET (liferea_dialog_lookup (pd->dialog, "itpInfoBar")));
	widget = liferea_dialog_lookup (pd->dialog, "itpbtn");
	conf_get_bool_value (ENABLE_ITP, &bSetting);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), bSetting);
#else
	gtk_widget_set_sensitive (GTK_WIDGET (liferea_dialog_lookup (pd->dialog, "itpbtn")), FALSE);
	gtk_widget_show (GTK_WIDGET (liferea_dialog_lookup (pd->dialog, "itpInfoBar")));
#endif
	/* ================= panel 7 "Enclosures" ======================== */

	/* menu for download tool */
	conf_get_int_value (DOWNLOAD_TOOL, &iSetting);
	ui_common_setup_combo_menu (liferea_dialog_lookup (pd->dialog, "downloadToolCombo"),
	                            enclosure_download_tool_options,
	                            G_CALLBACK (on_enclosure_download_tool_changed),
	                            iSetting);

	/* restore toggle */
	conf_get_bool_value (DOWNLOAD_USE_CUSTOM_COMMAND, &bSetting);
	widget = liferea_dialog_lookup (pd->dialog, "customDownload");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), bSetting);
	gtk_widget_set_sensitive (liferea_dialog_lookup (pd->dialog, "customDownloadEntry"), bSetting);
	gtk_widget_set_sensitive (liferea_dialog_lookup (pd->dialog, "downloadToolCombo"), !bSetting);

	/* restore custom command */
	conf_get_str_value (DOWNLOAD_CUSTOM_COMMAND, &custom_download_command);
	widget = liferea_dialog_lookup (pd->dialog, "customDownloadEntry");
	gtk_entry_set_text (GTK_ENTRY (widget), custom_download_command);
	g_free (custom_download_command);

	/* set up list of configured enclosure types */
	treestore = gtk_tree_store_new (FTS_LEN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
	list = (GSList *)enclosure_mime_types_get ();
	while (list) {
		GtkTreeIter *newIter = g_new0 (GtkTreeIter, 1);
		gtk_tree_store_append (treestore, newIter, NULL);
		gtk_tree_store_set (treestore, newIter,
	                	    FTS_TYPE, (NULL != ((encTypePtr)(list->data))->mime)?((encTypePtr)(list->data))->mime:((encTypePtr)(list->data))->extension,
	                	    FTS_CMD, ((encTypePtr)(list->data))->cmd,
	                	    FTS_PTR, list->data,
				    -1);
		list = g_slist_next (list);
	}

	widget = liferea_dialog_lookup (pd->dialog, "enc_action_view");
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget), GTK_TREE_MODEL (treestore));

	column = gtk_tree_view_column_new_with_attributes (_("Type"), gtk_cell_renderer_text_new (), "text", FTS_TYPE, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);
	gtk_tree_view_column_set_sort_column_id (column, FTS_TYPE);
	column = gtk_tree_view_column_new_with_attributes (_("Program"), gtk_cell_renderer_text_new (), "text", FTS_CMD, NULL);
	gtk_tree_view_column_set_sort_column_id (column, FTS_CMD);
	gtk_tree_view_append_column (GTK_TREE_VIEW(widget), column);

	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW(widget)), GTK_SELECTION_SINGLE);

	g_signal_connect_object (pd->dialog, "destroy", G_CALLBACK (preferences_dialog_destroy_cb), pd, 0);

	gtk_widget_show_all (pd->dialog);

	gtk_window_present (GTK_WINDOW (pd->dialog));
}

void
preferences_dialog_open (void)
{
	if (prefdialog) {
		gtk_widget_show (prefdialog->dialog);
		return;
	}

	g_object_new (PREFERENCES_DIALOG_TYPE, NULL);
}
