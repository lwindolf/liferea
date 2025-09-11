/*
 * @file liferea_shell.c  UI layout handling
 *
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2007-2025 Lars Windolf <lars.windolf@gmx.de>
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

#include "ui/liferea_shell.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "actions/item_actions.h"
#include "actions/link_actions.h"
#include "actions/node_actions.h"
#include "actions/shell_actions.h"
#include "browser.h"
#include "common.h"
#include "conf.h"
#include "db.h"
#include "debug.h"
#include "export.h"
#include "feedlist.h"
#include "item_history.h"
#include "itemlist.h"
#include "liferea_application.h"
#include "net_monitor.h"
#include "social.h"
#include "node_source.h"
#include "node_providers/newsbin.h"
#include "node_providers/vfolder.h"
#include "plugins/plugins_engine.h"
#include "ui/browser_tabs.h"
#include "ui/content_view.h"
#include "ui/feed_list_view.h"
#include "ui/icons.h"
#include "ui/item_list_view.h"
#include "ui/liferea_dialog.h"
#include "ui/preferences_dialog.h"
#include "ui/search_dialog.h"
#include "ui/ui_common.h"
#include "ui/ui_update.h"

extern gboolean searchFolderRebuild; /* db.c */

struct _LifereaShell {
	GObject	parent_instance;

	GtkBuilder	*xml;
	GSettings	*settings;

	GActionGroup	*shellActions;
	GActionGroup	*feedlistActions;
	GActionGroup	*itemlistActions;

	LifereaPluginsEngine *plugins;

	GtkWindow	*window;		/*<< Liferea main window */

	gboolean	autoLayout;		/*<< TRUE if automatic layout switching is active */
	guint		currentLayoutMode;	/*<< effective layout mode (email or wide) */

	ItemList	*itemlist;
	FeedList	*feedlist;

	FeedListView	*feedListView;
	ItemListView	*itemListView;

	LifereaBrowser	*htmlview;		/*<< the primary browser instance to render node/item info to */
	BrowserTabs	*tabs;
};

enum {
	PROP_NONE,
	PROP_FEED_LIST,
	PROP_ITEM_LIST,
	PROP_HTML_VIEW,
	PROP_BROWSER_TABS,
	PROP_LAYOUT_MODE,
	PROP_BUILDER
};

static LifereaShell *shell = NULL;

G_DEFINE_TYPE (LifereaShell, liferea_shell, G_TYPE_OBJECT);

static void liferea_shell_update_layout (nodeViewType newMode);

LifereaShell *
liferea_shell_get_instance (void)
{
	return shell;
}

static void
liferea_shell_finalize (GObject *object)
{
	LifereaShell *ls = LIFEREA_SHELL (object);

	g_object_unref (ls->plugins);
	g_object_unref (ls->tabs);
	g_object_unref (ls->feedlist);
	g_object_unref (ls->feedListView);
	g_object_unref (ls->itemlist);
	g_object_unref (ls->itemListView);
	g_object_unref (ls->htmlview);

	gtk_window_destroy (ls->window);

	g_object_unref (ls->settings);
	g_object_unref (ls->xml);

	g_object_unref (ls->shellActions);
	g_object_unref (ls->feedlistActions);
	g_object_unref (ls->itemlistActions);

	g_object_unref (ls->xml);
}

static void
liferea_shell_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
        LifereaShell *shell = LIFEREA_SHELL (object);

        switch (prop_id) {
	        case PROP_FEED_LIST:
			g_value_set_object (value, shell->feedlist);
			break;
	        case PROP_ITEM_LIST:
			g_value_set_object (value, shell->itemlist);
			break;
	        case PROP_HTML_VIEW:
			g_value_set_object (value, shell->htmlview);
			break;
	        case PROP_BROWSER_TABS:
			g_value_set_object (value, shell->tabs);
			break;
	        case PROP_BUILDER:
			g_value_set_object (value, shell->xml);
			break;
		case PROP_LAYOUT_MODE:
			g_value_set_int (value, shell->currentLayoutMode);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
        }
}

static void
liferea_shell_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
		case PROP_LAYOUT_MODE:
			liferea_shell_update_layout (g_value_get_int (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
liferea_shell_class_init (LifereaShellClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = liferea_shell_get_property;
	object_class->set_property = liferea_shell_set_property;
	object_class->finalize = liferea_shell_finalize;

	g_object_class_install_property (object_class,
		                         PROP_FEED_LIST,
		                         g_param_spec_object ("feedlist",
		                                              "LifereaFeedList",
		                                              "LifereaFeedList object",
		                                              FEED_LIST_TYPE,
		                                              G_PARAM_READABLE));

	g_object_class_install_property (object_class,
		                         PROP_ITEM_LIST,
		                         g_param_spec_object ("itemlist",
		                                              "LifereaItemList",
		                                              "LifereaItemList object",
		                                              ITEMLIST_TYPE,
		                                              G_PARAM_READABLE));

	g_object_class_install_property (object_class,
		                         PROP_HTML_VIEW,
		                         g_param_spec_object ("htmlview",
		                                              "LifereaBrowser",
		                                              "LifereaBrowser object",
		                                              LIFEREA_BROWSER_TYPE,
		                                              G_PARAM_READABLE));

	g_object_class_install_property (object_class,
		                         PROP_BROWSER_TABS,
		                         g_param_spec_object ("browser-tabs",
		                                              "LifereaBrowserTabs",
		                                              "LifereaBrowserTabs object",
		                                              BROWSER_TABS_TYPE,
		                                              G_PARAM_READABLE));

	g_object_class_install_property (object_class,
		                         PROP_BUILDER,
		                         g_param_spec_object ("builder",
		                                              "GtkBuilder",
		                                              "Liferea user interfaces definitions",
		                                              GTK_TYPE_BUILDER,
		                                              G_PARAM_READABLE));

	g_object_class_install_property (object_class,
		                         PROP_LAYOUT_MODE,
		                         g_param_spec_int ("layout-mode",
		                                             "Layout Mode",
		                                             "Current layout mode",
							     0,
							     G_MAXINT,
		                                             NODE_VIEW_MODE_AUTO,
		                                             G_PARAM_READWRITE));

}

GtkWidget *
liferea_shell_lookup (const gchar *name)
{
	GObject *obj;
	g_return_val_if_fail (shell != NULL, NULL);

	obj = gtk_builder_get_object (shell->xml, name);
	if (obj)
		return GTK_WIDGET (obj);
	else
		return NULL;
}

static void
liferea_shell_init (LifereaShell *ls)
{
	/* globally accessible singleton */
	g_assert (NULL == shell);
	shell = ls;
	shell->xml = gtk_builder_new ();
	gtk_builder_add_from_resource (shell->xml, "/org/gnome/liferea/ui/mainwindow.ui", NULL);
	gtk_builder_add_from_resource (shell->xml, "/org/gnome/liferea/ui/itemlist.ui", NULL);
}

void
liferea_shell_set_status_bar (const char *format, ...)
{
	// FIXME: implement a modern GTK4 solution
}

void
liferea_shell_set_important_status_bar (const char *format, ...)
{
	// FIXME: implement a modern GTK4 solution
}


static void
liferea_shell_update_layout (nodeViewType newMode)
{
	const gchar	*htmlWidgetName, *ilWidgetName;
	Node		*node;
	nodeViewType	effectiveMode;

	browser_tabs_show_headlines ();

	if (NODE_VIEW_MODE_AUTO == newMode) {
		gint	w, h, f;
		f = gtk_widget_get_width (liferea_shell_lookup ("feedlist"));
		w = gtk_widget_get_width (GTK_WIDGET (liferea_shell_get_window ()));
		h = gtk_widget_get_height (GTK_WIDGET (liferea_shell_get_window ()));

		/* we switch layout if window width - feed list width > window heigt */
		effectiveMode = (w - f > h)?NODE_VIEW_MODE_WIDE:NODE_VIEW_MODE_NORMAL;
	} else {
		effectiveMode = newMode;
	}

	if (effectiveMode == shell->currentLayoutMode)
		return;

	shell->autoLayout = (NODE_VIEW_MODE_AUTO == newMode);
	shell->currentLayoutMode = effectiveMode;

	node = itemlist_get_displayed_node ();

	/* Drop items */
	if (node)
		itemlist_unload ();

	debug (DEBUG_GUI, "Setting item list layout mode: %d (auto=%d)", effectiveMode, shell->autoLayout);

	switch (effectiveMode) {
		case NODE_VIEW_MODE_NORMAL:
			htmlWidgetName = "normalViewHtml";
			ilWidgetName = "normalViewItems";
			break;
		case NODE_VIEW_MODE_WIDE:
			htmlWidgetName = "wideViewHtml";
			ilWidgetName = "wideViewItems";
			break;
		default:
			g_warning("fatal: illegal viewing mode!");
			return;
			break;
	}

	gtk_notebook_set_current_page (GTK_NOTEBOOK (liferea_shell_lookup ("itemtabs")), effectiveMode);

	gtk_viewport_set_child (GTK_VIEWPORT (liferea_shell_lookup ("normalViewItems")), NULL);
	gtk_viewport_set_child (GTK_VIEWPORT (liferea_shell_lookup ("wideViewItems")), NULL);
	gtk_viewport_set_child (GTK_VIEWPORT (liferea_shell_lookup ("normalViewHtml")), NULL);
	gtk_viewport_set_child (GTK_VIEWPORT (liferea_shell_lookup ("wideViewHtml")), NULL);

	/* Reparent HTML view */
	g_assert (shell->htmlview);
	gtk_viewport_set_child (GTK_VIEWPORT (liferea_shell_lookup (htmlWidgetName)), liferea_browser_get_widget (shell->htmlview));

	/* Reparent the item list view */
	GtkWidget *container = liferea_shell_lookup ("itemListViewContainer");
	gtk_viewport_set_child (GTK_VIEWPORT (liferea_shell_lookup (ilWidgetName)), container);
	g_object_set (G_OBJECT (shell->itemListView), "wide-view", (NODE_VIEW_MODE_WIDE == effectiveMode), NULL);

	/* Load previously selected node into the new item list GtkTreeView widget */
	if (node)
		itemlist_load (node);

	// FIXME: reselect item (does not work yet because itemlist_load() is async)
}

static guint resizeTimer = 0;

static gboolean
on_auto_update_layout (gpointer user_data)
{
	liferea_shell_update_layout (NODE_VIEW_MODE_AUTO);
	resizeTimer = 0;
	return FALSE;
}

static gboolean
on_window_resize_cb (gpointer user_data)
{
	/* If we are in auto layout mode we ask to calculate it again */
	if (shell->autoLayout) {
		if (resizeTimer)
			g_source_remove (resizeTimer);

		resizeTimer = g_timeout_add(100, (GSourceFunc)on_auto_update_layout, NULL);
	}
	
	return FALSE;
}

static gboolean
on_searchentry_key_pressed (GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer data)
{
	switch (keyval) {
		case GDK_KEY_Escape:
			gtk_widget_set_visible (liferea_shell_lookup ("searchbox"), FALSE);
			return TRUE;
			break;
	}

	return FALSE;
}

static gboolean
on_shell_key_pressed_event (GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer data)
{
	gboolean	modifier_matches = FALSE;
	guint		default_modifiers;
	const gchar	*type = NULL;
	GtkWidget	*focusw = NULL;
	gint		browse_key_setting;

	default_modifiers = gtk_accelerator_get_default_mod_mask ();
	focusw = gtk_window_get_focus (shell->window);

	/* handle [<modifier>+]<Space> headline skimming hotkey */
	switch (keyval) {
		case GDK_KEY_space:
			conf_get_int_value (BROWSE_KEY_SETTING, &browse_key_setting);
			switch (browse_key_setting) {
				default:
				case 0:
					modifier_matches = ((state & default_modifiers) == 0);
					/* Hack to make space handled in the module. This is necessary
						because the HTML widget code must be able to catch spaces
						for input fields.

						By ignoring the space here it will be passed to the HTML
						widget which in turn will pass it back if it is not eaten by
						any input field currently focussed. */

					/* pass through space keys only if HTML widget has the focus */
					
					if (focusw)
						type = g_type_name (G_OBJECT_TYPE (focusw));
					if (type && (g_str_equal (type, "LifereaWebView")))
						return FALSE;
					break;
				case 1:
					modifier_matches = ((state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK);
					break;
				case 2:
					modifier_matches = ((state & GDK_ALT_MASK) == GDK_ALT_MASK);
					break;
			}

			if (modifier_matches) {
				liferea_browser_scroll (shell->htmlview);
				return TRUE;
			}
			break;
	}

	/* prevent usage of navigation keys in GtkEntrys */
	if (!focusw || GTK_IS_ENTRY (focusw))
		return FALSE;

	/* prevent usage of navigation keys in HTML view */
	type = g_type_name (G_OBJECT_TYPE (focusw));
	if (type && (g_str_equal (type, "LifereaWebView")))
		return FALSE;

	/* check for treeview navigation */
	if (0 == (state & default_modifiers)) {
		switch (keyval) {
			case GDK_KEY_KP_Delete:
			case GDK_KEY_Delete:
				if (focusw == liferea_shell_lookup ("feedlist")) {
					gtk_event_controller_key_forward (controller, focusw);
					return FALSE;
				}
				g_action_group_activate_action (G_ACTION_GROUP (shell->itemlistActions), "remove-selected-item", NULL);
				return TRUE;
				break;
			case GDK_KEY_n:
				g_action_group_activate_action (G_ACTION_GROUP (shell->shellActions), "next-unread-item", NULL);
				return TRUE;
				break;
			case GDK_KEY_f:
				item_list_view_move_cursor (shell->itemListView, 1);
				return TRUE;
				break;
			case GDK_KEY_b:
				item_list_view_move_cursor (shell->itemListView, -1);
				return TRUE;
				break;
			case GDK_KEY_u:
				ui_common_treeview_move_cursor (GTK_TREE_VIEW (liferea_shell_lookup ("feedlist")), -1);
				item_list_view_move_cursor_to_first (shell->itemListView);
				return TRUE;
				break;
			case GDK_KEY_d:
				ui_common_treeview_move_cursor (GTK_TREE_VIEW (liferea_shell_lookup ("feedlist")), 1);
				item_list_view_move_cursor_to_first (shell->itemListView);
				return TRUE;
				break;
		}
	}

	return FALSE;
}

static void
liferea_shell_setup_URL_receiver (void)
{
	// FIXME: GTK4
	/*GtkTargetEntry target_table[] = {
		{ "STRING",     		GTK_TARGET_OTHER_WIDGET, 0 },
		{ "text/plain", 		GTK_TARGET_OTHER_WIDGET, 0 },
		{ "text/uri-list",		GTK_TARGET_OTHER_WIDGET, 1 },
		{ "_NETSCAPE_URL",		GTK_TARGET_OTHER_APP, 1 },
		{ "application/x-rootwin-drop", GTK_TARGET_OTHER_APP, 2 }
	};
*/

	/* doesn't work with GTK_DEST_DEFAULT_DROP... */
/*	gtk_drag_dest_set (mainwindow, GTK_DEST_DEFAULT_ALL,
	                   target_table, sizeof (target_table)/sizeof (target_table[0]),
	                   GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);

	g_signal_connect (G_OBJECT (mainwindow), "drag_data_received",
	                  G_CALLBACK (liferea_shell_URL_received), NULL);*/
}

void
liferea_shell_copy_to_clipboard (const gchar *str)
{
	GdkClipboard *primary = gdk_display_get_primary_clipboard (gdk_display_get_default ());
	GdkClipboard *copypaste = gdk_display_get_clipboard (gdk_display_get_default ());

	if (!str)
		return;

	gdk_clipboard_set_text (primary, str);
	gdk_clipboard_set_text (copypaste, str);
}

static void
liferea_shell_restore_layout (void)
{
	GtkWidget	*widget;
	gint		w, h;
	gint		last_vpane_pos, last_hpane_pos, last_wpane_pos;
	nodeViewType	viewMode;

	/* This only works after the window has been restored, so we do it last. */
	conf_get_int_value (LAST_VPANE_POS, &last_vpane_pos);
	conf_get_int_value (LAST_HPANE_POS, &last_hpane_pos);
	conf_get_int_value (LAST_WPANE_POS, &last_wpane_pos);

	/* Sanity check pane sizes for too large values */
	/* a) set leftpane to 1/3rd of window size if too large */
	
	/* In GTK4 get_default_size is effectively given us the size */
	gtk_window_get_default_size (shell->window, &w, &h);
	if (w * 95 / 100 <= last_vpane_pos || 0 == last_vpane_pos)
		last_vpane_pos = w / 3;

	/* b) set normalViewPane to 50% container height if too large */
	widget = GTK_WIDGET (liferea_shell_lookup ("normalViewPane"));
	if ((gtk_widget_get_height (widget) * 95 / 100 <= last_hpane_pos) || 0 == last_hpane_pos)
		last_hpane_pos = gtk_widget_get_height (widget) / 2;

	/* c) set wideViewPane to 50% container width if too large */
	widget = GTK_WIDGET (liferea_shell_lookup ("wideViewPane"));
	if ((gtk_widget_get_width (widget) * 95 / 100 <= last_wpane_pos) || 0 == last_wpane_pos)
		last_wpane_pos = gtk_widget_get_width (widget) / 2;

	debug (DEBUG_GUI, "Restoring pane proportions (left:%d normal:%d wide:%d)", last_vpane_pos, last_hpane_pos, last_wpane_pos);

	gtk_paned_set_position (GTK_PANED (liferea_shell_lookup ("leftpane")), last_vpane_pos);
	gtk_paned_set_position (GTK_PANED (liferea_shell_lookup ("normalViewPane")), last_hpane_pos);
	gtk_paned_set_position (GTK_PANED (liferea_shell_lookup ("wideViewPane")), last_wpane_pos);

	conf_get_int_value (DEFAULT_VIEW_MODE, (gint *)&viewMode);
	liferea_shell_update_layout (viewMode);
}

static void
liferea_shell_add_action_group_to_map (GActionGroup *group, GActionMap *map)
{
	gchar **actions_list = g_action_group_list_actions (group);
	gint i;
	for (i=0;actions_list[i] != NULL;i++) {
		g_action_map_add_action (map, g_action_map_lookup_action (G_ACTION_MAP (group), actions_list [i]));
	}
	g_strfreev (actions_list);
}

GActionGroup *
liferea_shell_add_actions (const GActionEntry *entries, int count)
{
	GtkApplication	*app = gtk_window_get_application (GTK_WINDOW (shell->window));
	GActionGroup	*group = G_ACTION_GROUP (g_simple_action_group_new ());

	g_action_map_add_action_entries (G_ACTION_MAP (group), entries, count, NULL);
	liferea_shell_add_action_group_to_map (group, G_ACTION_MAP (app));	

	return group;
}

itemPtr
liferea_shell_find_next_unread (gulong startId)
{
	itemPtr	result = NULL;

	/* Note: to select in sorting order we need to do it in the ItemListView
	   otherwise we would have to sort the item list here... */

	g_assert (shell->itemListView);

	/* First do a scan from the start position (usually the selected
	   item to the end of the sorted item list) if one is given. */
	if (startId)
		result = item_list_view_find_unread_item (shell->itemListView, startId);

	/* Now perform a wrap around by searching again from the top */
	if (!result)
		result = item_list_view_find_unread_item (shell->itemListView, 0);

	/* Return NULL if not found, or only the selected item is unread */
	if (result && result->id == startId)
		return NULL;

	return result;
}

void
liferea_shell_create (GtkApplication *app, const gchar *overrideWindowState, gint pluginsDisabled)
{
	GtkEventController *keypress;
	gchar		*id;

	g_object_new (LIFEREA_SHELL_TYPE, NULL);
	g_assert (shell);

	/* Import custom CSS */
	GtkCssProvider *css_provider = gtk_css_provider_new();
	gtk_css_provider_load_from_resource (css_provider, "/org/gnome/liferea/ui/gtk-liferea.css");
	gtk_style_context_add_provider_for_display(gdk_display_get_default(),
		GTK_STYLE_PROVIDER(css_provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref(css_provider);

	/* 1.) stuff where order does not matter */
	shell->currentLayoutMode = 10000;	// something invalid
	shell->window = GTK_WINDOW (liferea_shell_lookup ("mainwindow"));
	shell->plugins = liferea_plugins_engine_get ();
	shell->itemlist = ITEMLIST (g_object_new (ITEMLIST_TYPE, NULL));
	shell->feedlist = FEED_LIST (g_object_new (FEED_LIST_TYPE, NULL));
	shell->htmlview = LIFEREA_BROWSER (content_view_create (shell->feedlist, shell->itemlist));
	shell->itemListView = item_list_view_create (shell->feedlist, shell->itemlist);
	gtk_box_append (GTK_BOX (liferea_shell_lookup ("itemListViewContainer")), item_list_view_get_widget (shell->itemListView));

	shell->tabs = browser_tabs_create (GTK_NOTEBOOK (liferea_shell_lookup ("browsertabs")));

	gtk_window_set_application (GTK_WINDOW (shell->window), app);

	/* 2.) Load shell state and bind for persisting */
	liferea_shell_restore_layout ();
	conf_bind (DEFAULT_VIEW_MODE, shell, "layout-mode", G_SETTINGS_BIND_DEFAULT);
	conf_bind (LAST_WINDOW_WIDTH, shell->window, "default-width", G_SETTINGS_BIND_DEFAULT);
	conf_bind (LAST_WINDOW_HEIGHT, shell->window, "default-height", G_SETTINGS_BIND_DEFAULT);
	conf_bind (LAST_WINDOW_MAXIMIZED, shell->window, "maximized", G_SETTINGS_BIND_DEFAULT);
	conf_bind (LAST_VPANE_POS, liferea_shell_lookup ("leftpane"), "position", G_SETTINGS_BIND_DEFAULT);
	conf_bind (LAST_HPANE_POS, liferea_shell_lookup ("normalViewPane"), "position", G_SETTINGS_BIND_DEFAULT);
	conf_bind (LAST_WPANE_POS, liferea_shell_lookup ("wideViewPane"), "position", G_SETTINGS_BIND_DEFAULT);

	/* 3. Add accelerators for shell */
	static const gchar * liferea_accels_update_all[] = {"<Control>u", NULL};
	static const gchar * liferea_accels_quit[] = {"<Control>q", NULL};
	static const gchar * liferea_accels_mark_feed_as_read[] = {"<Control>r", NULL};
	static const gchar * liferea_accels_next_unread_item[] = {"<Control>n", NULL};
	static const gchar * liferea_accels_prev_read_item[] = {"<Control><Shift>n", NULL};
	static const gchar * liferea_accels_toggle_item_read_status[] = {"<Control>m", NULL};
	static const gchar * liferea_accels_toggle_item_flag[] = {"<Control>t", NULL};
	static const gchar * liferea_accels_fullscreen[] = {"F11", NULL};
	static const gchar * liferea_accels_zoom_in[] = {"<Control>plus", "<Control>equal",NULL};
	static const gchar * liferea_accels_zoom_out[] = {"<Control>minus", NULL};
	static const gchar * liferea_accels_zoom_reset[] = {"<Control>0", NULL};
	static const gchar * liferea_accels_search_feeds[] = {"<Control>f", NULL};
	static const gchar * liferea_accels_show_help[] = {"F1", NULL};
	static const gchar * liferea_accels_show_shortcuts[] = {"<Control>question", NULL};
	static const gchar * liferea_accels_launch_item_in_external_browser[] = {"<Control>d", NULL};

	gtk_application_set_accels_for_action (app, "app.update-all", liferea_accels_update_all);
	gtk_application_set_accels_for_action (app, "app.quit", liferea_accels_quit);
	gtk_application_set_accels_for_action (app, "app.mark-selected-feed-as-read", liferea_accels_mark_feed_as_read);
	gtk_application_set_accels_for_action (app, "app.next-unread-item", liferea_accels_next_unread_item);
	gtk_application_set_accels_for_action (app, "app.prev-read-item", liferea_accels_prev_read_item);
	gtk_application_set_accels_for_action (app, "app.toggle-selected-item-read-status", liferea_accels_toggle_item_read_status);
	gtk_application_set_accels_for_action (app, "app.toggle-selected-item-flag", liferea_accels_toggle_item_flag);
	gtk_application_set_accels_for_action (app, "app.fullscreen", liferea_accels_fullscreen);
	gtk_application_set_accels_for_action (app, "app.zoom-in", liferea_accels_zoom_in);
	gtk_application_set_accels_for_action (app, "app.zoom-out", liferea_accels_zoom_out);
	gtk_application_set_accels_for_action (app, "app.zoom-reset", liferea_accels_zoom_reset);
	gtk_application_set_accels_for_action (app, "app.search-feeds", liferea_accels_search_feeds);
	gtk_application_set_accels_for_action (app, "app.show-help-contents", liferea_accels_show_help);
	gtk_application_set_accels_for_action (app, "app.show-shortcuts", liferea_accels_show_shortcuts);
	gtk_application_set_accels_for_action (app, "app.launch-item-in-external-browser", liferea_accels_launch_item_in_external_browser);

	/* 4.) setup feed and item list widgets */
	debug (DEBUG_GUI, "Setting up feed list");
	shell->feedListView = feed_list_view_create (GTK_TREE_VIEW (liferea_shell_lookup ("feedlist")), shell->feedlist);

	/* 5.) update and restore all menu elements */
	liferea_shell_setup_URL_receiver ();

	// 6.) Restore selection (FIXME: Move to feed list code?)
	if (conf_get_str_value (LAST_NODE_SELECTED, &id)) {
		feedlist_set_selected (node_from_id (id));
		g_free (id);
	}

	/* 7. Setup shell window signals, only after all widgets are ready */
	g_signal_connect (shell->window, "notify::default-width", G_CALLBACK (on_window_resize_cb), NULL);
	g_signal_connect (shell->window, "notify::default-height", G_CALLBACK (on_window_resize_cb), NULL);
	g_signal_connect (shell->window, "notify::maximized", G_CALLBACK (on_window_resize_cb), NULL);
	g_signal_connect (shell->window, "notify::fullscreen", G_CALLBACK (on_window_resize_cb), NULL);

	keypress = gtk_event_controller_key_new ();
	gtk_widget_add_controller (GTK_WIDGET (shell->window), keypress);
	g_signal_connect (keypress, "key-pressed", G_CALLBACK (on_shell_key_pressed_event), shell);
		
	keypress = gtk_event_controller_key_new ();
	gtk_widget_add_controller (liferea_shell_lookup ("searchentry"), keypress);
	g_signal_connect (keypress, "key-pressed", G_CALLBACK (on_searchentry_key_pressed), shell);

	/* 8. setup actions */
	shell->shellActions = shell_actions_create (shell);
	shell->feedlistActions = node_actions_create (shell);
	shell->itemlistActions = item_actions_create (shell);

	/* 9. Setup plugins that all LifereaShell child objects already created */
	if (!pluginsDisabled)
		liferea_plugins_engine_register_shell_plugins (shell);

	/* 10. Rebuild search folders if needed */
	if (searchFolderRebuild)
		vfolder_foreach (vfolder_rebuild);

	/* 11. Load feedlist and enable signals for feedListView */
	feed_list_view_set_reduce_mode (FALSE);	// FIXME: this would be better triggered by a GAction init somewhere
	gtk_widget_set_sensitive (liferea_shell_lookup ("feedlist"), TRUE);

	/* 12. Unstable branch only: show current work in progress */
	browser_tabs_add_new ("https://raw.githubusercontent.com/lwindolf/liferea/refs/heads/main/TODO", "Current TODOs", TRUE);
}

void liferea_shell_show_window (void)
{
	gtk_window_present (shell->window);
}

void
liferea_shell_toggle_visibility (void)
{
	GtkWidget *mainwindow = GTK_WIDGET (shell->window);

	if (!gtk_widget_get_visible (mainwindow)) {
		liferea_shell_show_window ();
	} else {
		gtk_widget_set_visible (mainwindow, FALSE);
	}
}

GtkWidget *
liferea_shell_get_window (void)
{
	g_assert (shell);

	return GTK_WIDGET (shell->window);
}