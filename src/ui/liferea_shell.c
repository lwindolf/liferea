/*
 * @file liferea_shell.c  UI layout handling
 *
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2007-2023 Lars Windolf <lars.windolf@gmx.de>
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
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <libpeas/peas-extension-set.h>

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
#include "newsbin.h"
#include "plugins_engine.h"
#include "render.h"
#include "social.h"
#include "vfolder.h"
#include "fl_sources/node_source.h"
#include "ui/browser_tabs.h"
#include "ui/feed_list_view.h"
#include "ui/icons.h"
#include "ui/itemview.h"
#include "ui/item_list_view.h"
#include "ui/liferea_dialog.h"
#include "ui/liferea_shell_activatable.h"
#include "ui/preferences_dialog.h"
#include "ui/search_dialog.h"
#include "ui/ui_common.h"
#include "ui/ui_update.h"

extern gboolean searchFolderRebuild; /* db.c */

struct _LifereaShell {
	GObject	parent_instance;

	GtkBuilder	*xml;

	GtkWindow	*window;			/*<< Liferea main window */
	GtkWidget	*toolbar;
	GtkTreeView	*feedlistViewWidget;

	GtkStatusbar	*statusbar;		/*<< main window status bar */
	gboolean	statusbarLocked;	/*<< flag locking important message on status bar */
	guint		statusbarLockTimer;	/*<< timer id for status bar lock reset timer */
	guint		resizeTimer;		/*<< timer id for resize callback */

	GtkWidget	*statusbar_feedsinfo;
	GtkWidget	*statusbar_feedsinfo_evbox;
	GActionGroup	*generalActions;
	GActionGroup	*addActions;		/*<< all types of "New" options */
	GActionGroup	*feedActions;		/*<< update and mark read */
	GActionGroup	*readWriteActions;	/*<< node remove and properties, node itemset items remove */
	GActionGroup	*itemActions;		/*<< item state toggline, single item remove */

	ItemList	*itemlist;
	FeedList	*feedlist;
	ItemView	*itemview;
	BrowserTabs	*tabs;

	PeasExtensionSet *extensions;		/*<< Plugin management */

	gboolean	fullscreen;				/*<< track fullscreen */
};

enum {
	PROP_NONE,
	PROP_FEED_LIST,
	PROP_ITEM_LIST,
	PROP_ITEM_VIEW,
	PROP_BROWSER_TABS,
	PROP_BUILDER
};

static LifereaShell *shell = NULL;

G_DEFINE_TYPE (LifereaShell, liferea_shell, G_TYPE_OBJECT);

static void
liferea_shell_finalize (GObject *object)
{
	LifereaShell *ls = LIFEREA_SHELL (object);

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
	        case PROP_ITEM_VIEW:
				g_value_set_object (value, shell->itemview);
				break;
	        case PROP_BROWSER_TABS:
				g_value_set_object (value, shell->tabs);
				break;
	        case PROP_BUILDER:
				g_value_set_object (value, shell->xml);
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
	object_class->finalize = liferea_shell_finalize;

	/* LifereaShell:feed-list: */
	g_object_class_install_property (object_class,
		                         PROP_FEED_LIST,
		                         g_param_spec_object ("feed-list",
		                                              "LifereaFeedList",
		                                              "LifereaFeedList object",
		                                              FEED_LIST_TYPE,
		                                              G_PARAM_READABLE));

	/* LifereaShell:item-list: */
	g_object_class_install_property (object_class,
		                         PROP_ITEM_LIST,
		                         g_param_spec_object ("item-list",
		                                              "LifereaItemList",
		                                              "LifereaItemList object",
		                                              ITEMLIST_TYPE,
		                                              G_PARAM_READABLE));

	/* LifereaShell:item-view: */
	g_object_class_install_property (object_class,
		                         PROP_ITEM_VIEW,
		                         g_param_spec_object ("item-view",
		                                              "LifereaItemView",
		                                              "LifereaItemView object",
		                                              ITEM_VIEW_TYPE,
		                                              G_PARAM_READABLE));

	/* LifereaShell:browser-tabs: */
	g_object_class_install_property (object_class,
		                         PROP_BROWSER_TABS,
		                         g_param_spec_object ("browser-tabs",
		                                              "LifereaBrowserTabs",
		                                              "LifereaBrowserTabs object",
		                                              BROWSER_TABS_TYPE,
		                                              G_PARAM_READABLE));

	/* LifereaShell:builder: */
	g_object_class_install_property (object_class,
		                         PROP_BUILDER,
		                         g_param_spec_object ("builder",
		                                              "GtkBuilder",
		                                              "Liferea user interfaces definitions",
		                                              GTK_TYPE_BUILDER,
		                                              G_PARAM_READABLE));
}

GtkWidget *
liferea_shell_lookup (const gchar *name)
{
	g_return_val_if_fail (shell != NULL, NULL);

	return GTK_WIDGET (gtk_builder_get_object (shell->xml, name));
}

static void
liferea_shell_init (LifereaShell *ls)
{
	/* globally accessible singleton */
	g_assert (NULL == shell);
	shell = ls;
	shell->xml = gtk_builder_new_from_file (PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "mainwindow.ui");
	if (!shell->xml)
		g_error ("Loading " PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "mainwindow.ui failed");

	gtk_builder_connect_signals (shell->xml, NULL);
}

/*
 * Restore the window position from the values saved into gconf. Note
 * that this does not display/present/show the mainwindow.
 */
static void
liferea_shell_restore_position (void)
{
	/* load window position */
	int x, y, w, h;
	gboolean last_window_maximized;
	GdkWindow *gdk_window;
	GdkMonitor *monitor;
	GdkRectangle work_area;

	conf_get_int_value (LAST_WINDOW_X, &x);
	conf_get_int_value (LAST_WINDOW_Y, &y);

	conf_get_int_value (LAST_WINDOW_WIDTH, &w);
	conf_get_int_value (LAST_WINDOW_HEIGHT, &h);

	debug (DEBUG_GUI, "Retrieved saved setting: size %dx%d position %d:%d", w, h, x, y);

	/* Restore position only if the width and height were saved */
	if (w != 0 && h != 0) {
		gdk_window = gtk_widget_get_window (GTK_WIDGET (shell->window));
		monitor = gdk_display_get_monitor_at_window (gtk_widget_get_display (GTK_WIDGET(shell->window)), gdk_window);
		gdk_monitor_get_workarea (monitor, &work_area);

		if (x >= work_area.width)
			x = work_area.width - 100;
		else if (x + w < 0)
			x = 100;

		if (y >= work_area.height)
			y = work_area.height - 100;
		else if (y + w < 0)
			y = 100;

		debug (DEBUG_GUI, "Restoring to size %dx%d position %d:%d", w, h, x, y);

		gtk_window_move (GTK_WINDOW (shell->window), x, y);

		/* load window size */
		gtk_window_resize (GTK_WINDOW (shell->window), w, h);
	}

	conf_get_bool_value (LAST_WINDOW_MAXIMIZED, &last_window_maximized);

	if (last_window_maximized)
		gtk_window_maximize (GTK_WINDOW (shell->window));
	else
		gtk_window_unmaximize (GTK_WINDOW (shell->window));

}

void
liferea_shell_save_position (void)
{
	GtkWidget		*pane;
	gint			x, y, w, h;
	gboolean		last_window_maximized;
	GdkWindow 		*gdk_window;
	GdkMonitor 		*monitor;
	GdkRectangle		work_area;

	g_assert(shell);

	/* save pane proportions */
	pane = liferea_shell_lookup ("leftpane");
	if (pane) {
		x = gtk_paned_get_position (GTK_PANED (pane));
		conf_set_int_value (LAST_VPANE_POS, x);
	}

	pane = liferea_shell_lookup ("normalViewPane");
	if (pane) {
		y = gtk_paned_get_position (GTK_PANED (pane));
		conf_set_int_value (LAST_HPANE_POS, y);
	}

	pane = liferea_shell_lookup ("wideViewPane");
	if (pane) {
		y = gtk_paned_get_position (GTK_PANED (pane));
		conf_set_int_value (LAST_WPANE_POS, y);
	}

	/* The following needs to be skipped when the window is not visible */
	if (!gtk_widget_get_visible (GTK_WIDGET (shell->window)))
		return;

	conf_get_bool_value (LAST_WINDOW_MAXIMIZED, &last_window_maximized);

	if (last_window_maximized)
		return;

	gtk_window_get_position (shell->window, &x, &y);
	gtk_window_get_size (shell->window, &w, &h);

	gdk_window = gtk_widget_get_window (GTK_WIDGET (shell->window));
	monitor = gdk_display_get_monitor_at_window (gtk_widget_get_display (GTK_WIDGET(shell->window)), gdk_window);
	gdk_monitor_get_workarea (monitor, &work_area);

	if (x+w<0 || y+h<0 ||
	    x > work_area.width ||
	    y > work_area.height)
		return;

	debug (DEBUG_GUI, "Saving window size and position: %dx%d %d:%d", w, h, x, y);

	/* save window position */
	conf_set_int_value (LAST_WINDOW_X, x);
	conf_set_int_value (LAST_WINDOW_Y, y);

	/* save window size */
	conf_set_int_value (LAST_WINDOW_WIDTH, w);
	conf_set_int_value (LAST_WINDOW_HEIGHT, h);
}

void
liferea_shell_set_toolbar_style (const gchar *toolbar_style)
{
	if (!toolbar_style) /* default to icons */
		gtk_toolbar_set_style (GTK_TOOLBAR (shell->toolbar), GTK_TOOLBAR_ICONS);
	else if (g_str_equal (toolbar_style, "text"))
		gtk_toolbar_set_style (GTK_TOOLBAR (shell->toolbar), GTK_TOOLBAR_TEXT);
	else if (g_str_equal (toolbar_style, "both"))
		gtk_toolbar_set_style (GTK_TOOLBAR (shell->toolbar), GTK_TOOLBAR_BOTH);
	else if (g_str_equal (toolbar_style, "both_horiz") || g_str_equal (toolbar_style, "both-horiz") )
		gtk_toolbar_set_style (GTK_TOOLBAR (shell->toolbar), GTK_TOOLBAR_BOTH_HORIZ);
	else /* default to icons */
		gtk_toolbar_set_style (GTK_TOOLBAR (shell->toolbar), GTK_TOOLBAR_ICONS);
}

void
liferea_shell_update_toolbar (void)
{
	gboolean disable_toolbar;

	conf_get_bool_value (DISABLE_TOOLBAR, &disable_toolbar);

	if (disable_toolbar)
		gtk_widget_hide (shell->toolbar);
	else
		gtk_widget_show (shell->toolbar);
}

static void
liferea_shell_update_update_menu (gboolean enabled)
{
	g_simple_action_set_enabled (G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (shell->feedActions), "update-selected")), enabled);
}

static void
liferea_shell_update_feed_menu (gboolean add, gboolean enabled, gboolean readWrite)
{
	ui_common_simple_action_group_set_enabled (shell->addActions, add);
	ui_common_simple_action_group_set_enabled (shell->feedActions, enabled);
	ui_common_simple_action_group_set_enabled (shell->readWriteActions, readWrite);
}

void
liferea_shell_update_item_menu (gboolean enabled)
{
	ui_common_simple_action_group_set_enabled (shell->itemActions, enabled);
}

static void
liferea_shell_update_allitems_actions (gboolean isNotEmpty, gboolean isRead)
{
	g_simple_action_set_enabled (G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (shell->generalActions), "remove-selected-feed-items")), isNotEmpty);
	g_simple_action_set_enabled (G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (shell->feedActions), "mark-selected-feed-as-read")), isRead);
}

void
liferea_shell_update_history_actions (void)
{
	g_simple_action_set_enabled (G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (shell->generalActions), "prev-read-item")), item_history_has_previous ());
	g_simple_action_set_enabled (G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (shell->generalActions), "next-read-item")), item_history_has_next ());
}

static void
liferea_shell_update_unread_stats (gpointer user_data)
{
	gint	new_items, unread_items;
	gchar	*msg, *tmp;

	if (!shell)
		return;

	new_items = feedlist_get_new_item_count ();
	unread_items = feedlist_get_unread_item_count ();

	if (new_items != 0)
		msg = g_strdup_printf (ngettext (" (%d new)", " (%d new)", new_items), new_items);
	else
		msg = g_strdup ("");

	if (unread_items != 0)
		tmp = g_strdup_printf (ngettext ("%d unread%s", "%d unread%s", unread_items), unread_items, msg);
	else
		tmp = g_strdup ("");

	gtk_label_set_text (GTK_LABEL (shell->statusbar_feedsinfo), tmp);
	g_free (tmp);
	g_free (msg);
}

static void
liferea_shell_update_node_actions (gpointer obj, gchar *unusedNodeId, gpointer data)
{
	/* We need to use the selected node here, as for search folders
	   if we'd rely on the parent node of the changed item we would
	   enable the wrong menu options */
	nodePtr	node = feedlist_get_selected ();

	if (!node) {
		liferea_shell_update_feed_menu (TRUE, FALSE, FALSE);
		liferea_shell_update_allitems_actions (FALSE, FALSE);
		liferea_shell_update_update_menu (FALSE);
		return;
	}

	gboolean allowModify = (NODE_SOURCE_TYPE (node->source->root)->capabilities & NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST);
	liferea_shell_update_feed_menu (allowModify, TRUE, allowModify);
	liferea_shell_update_update_menu ((NODE_TYPE (node)->capabilities & NODE_CAPABILITY_UPDATE) ||
	                                  (NODE_TYPE (node)->capabilities & NODE_CAPABILITY_UPDATE_CHILDS));

	// Needs to be last as liferea_shell_update_update_menu() default enables actions
	if (IS_FEED (node))
		liferea_shell_update_allitems_actions (0 != node->itemCount, 0 != node->unreadCount);
	else
		liferea_shell_update_allitems_actions (FALSE, 0 != node->unreadCount);
}

/*
   Due to the unsuitable GtkStatusBar stack handling, which doesn't
   allow to keep messages on top of the stack for some time without
   overwriting them with newly arriving messages, we need some extra
   handling here.

   Liferea knows two types of status messages:

     -> low prio messages (e.g. updating status messages)
     -> high prio messages (caused by user interaction, e.g. link hovering)

   The ideas is to keep the high prio messages always visible no matter
   what low prio messages arrive. To solve this we define the status bar
   stack is always a stack of two messages at most. At the bottom of the
   stack is always the latest low prio message and on top of the stack
   is the latest high prio message (or none at all).

   To enforce this using GtkStatusBar we use a lock to avoid adding
   low prio messages on top of high priority ones. This lock is valid
   for at most 5s which should be enough to read the high priority
   message. Afterwards new low priority messages will overrule the
   out-dated high priority message.
 */

static gboolean
liferea_shell_unlock_status_bar_cb (gpointer user_data)
{
	shell->statusbarLocked = FALSE;
	shell->statusbarLockTimer = 0;

	return FALSE;
}

static gboolean
liferea_shell_set_status_bar_important_cb (gpointer user_data)
{
	gchar		*text = (gchar *)user_data;
	guint		id;
	GtkStatusbar	*statusbar;

	statusbar = GTK_STATUSBAR (shell->statusbar);
	id = gtk_statusbar_get_context_id (statusbar, "important");
	gtk_statusbar_pop (statusbar, id);
	gtk_statusbar_push (statusbar, id, text);
	g_free(text);

	return FALSE;
}

static gboolean
liferea_shell_set_status_bar_default_cb (gpointer user_data)
{
	gchar		*text = (gchar *)user_data;
	guint		id;
	GtkStatusbar	*statusbar;

	statusbar = GTK_STATUSBAR (shell->statusbar);
	id = gtk_statusbar_get_context_id (statusbar, "default");
	gtk_statusbar_pop (statusbar, id);
	gtk_statusbar_push (statusbar, id, text);
	g_free(text);

	return FALSE;
}

void
liferea_shell_set_status_bar (const char *format, ...)
{
	va_list		args;
	gchar		*text;

	if (shell->statusbarLocked)
		return;

	g_return_if_fail (format != NULL);

	va_start (args, format);
	text = g_strdup_vprintf (format, args);
	va_end (args);

	g_idle_add ((GSourceFunc)liferea_shell_set_status_bar_default_cb, (gpointer)text);
}

void
liferea_shell_set_important_status_bar (const char *format, ...)
{
	va_list		args;
	gchar		*text;

	g_return_if_fail (format != NULL);

	va_start (args, format);
	text = g_strdup_vprintf (format, args);
	va_end (args);

	shell->statusbarLocked = FALSE;
	if (shell->statusbarLockTimer) {
		g_source_remove (shell->statusbarLockTimer);
		shell->statusbarLockTimer = 0;
	}

	/* URL hover messages are reset with an empty string, so
	   we must locking the status bar on empty strings! */
	if (!g_str_equal (text, "")) {
		/* Realize 5s locking for important messages... */
		shell->statusbarLocked = TRUE;
		shell->statusbarLockTimer = g_timeout_add_seconds (5, liferea_shell_unlock_status_bar_cb, NULL);
	}

	g_idle_add ((GSourceFunc)liferea_shell_set_status_bar_important_cb, (gpointer)text);
}

/* For zoom in : zoom = 1, for zoom out : zoom= -1, for reset : zoom = 0 */
static void
liferea_shell_do_zoom (gint zoom)
{
	/* We must apply the zoom either to the item view
	   or to an open tab, depending on the browser tabs
	   GtkNotebook page that is active... */
	if (!browser_tabs_get_active_htmlview ())
		itemview_do_zoom (zoom);
	else
		browser_tabs_do_zoom (zoom);
}

static gboolean
on_key_press_event_null_cb (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	return FALSE;
}

static gboolean
on_notebook_scroll_event_null_cb (GtkWidget *widget, GdkEventScroll *event)
{
	GtkNotebook *notebook = GTK_NOTEBOOK (widget);

	GtkWidget* child;
	GtkWidget* originator;

	if (!gtk_notebook_get_current_page (notebook))
		return FALSE;

	child = gtk_notebook_get_nth_page (notebook, gtk_notebook_get_current_page (notebook));
	originator = gtk_get_event_widget ((GdkEvent *)event);

	/* ignore scroll events from the content of the page */
	if (!originator || gtk_widget_is_ancestor (originator, child))
		return FALSE;

	return TRUE;
}

static gboolean
on_close (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	liferea_application_shutdown ();
	return TRUE;
}

static gboolean
on_window_resize_cb (gpointer user_data)
{
	gint 		vpane_pos = 0, hpane_pos = 0, wpane_pos = 0;
	GtkWidget	*pane;
	GdkWindow	*gdk_window;
	GtkAllocation	allocation;

	shell->resizeTimer = 0;

	/* If we are in auto layout mode we ask the itemview to calculate it again */
	if (NODE_VIEW_MODE_AUTO == itemview_get_layout ())
		itemview_set_layout (NODE_VIEW_MODE_AUTO);

	/* Sanity check pane sizes for 0 values after layout switch
	   this is necessary when switching wide mode to maximized window */

	/* get pane proportions */
	pane = liferea_shell_lookup ("leftpane");
	if (!pane)
		return FALSE;
	vpane_pos = gtk_paned_get_position (GTK_PANED (pane));

	pane = liferea_shell_lookup ("normalViewPane");
	if (!pane)
		return FALSE;
	hpane_pos = gtk_paned_get_position (GTK_PANED (pane));

	pane = liferea_shell_lookup ("wideViewPane");
	if (!pane)
		return FALSE;
	wpane_pos = gtk_paned_get_position (GTK_PANED (pane));

	// The following code partially duplicates liferea_shell_restore_panes()

	/* a) set leftpane to 1/3rd of window size if too large */
	gdk_window = gtk_widget_get_window (GTK_WIDGET (shell->window));
	if (gdk_window_get_width (gdk_window) * 95 / 100 <= vpane_pos || 0 == vpane_pos) {
		debug(DEBUG_GUI, "Fixing leftpane position");
		gtk_paned_set_position (GTK_PANED (liferea_shell_lookup ("leftpane")), gdk_window_get_width (gdk_window) / 3);
	}

	/* b) set normalViewPane to 50% container height if too large */
	gtk_widget_get_allocation (GTK_WIDGET (liferea_shell_lookup ("normalViewPane")), &allocation);
	if ((allocation.height * 95 / 100 <= hpane_pos) || 0 == hpane_pos) {
		debug(DEBUG_GUI, "Fixing normalViewPane position");
		gtk_paned_set_position (GTK_PANED (liferea_shell_lookup ("normalViewPane")), allocation.height / 2);
	}

	/* c) set wideViewPane to 50% container width if too large */
	gtk_widget_get_allocation (GTK_WIDGET (liferea_shell_lookup ("wideViewPane")), &allocation);
	if ((allocation.width * 95 / 100 <= wpane_pos) || 0 == wpane_pos) {
		debug(DEBUG_GUI, "Fixing wideViewPane position");
		gtk_paned_set_position (GTK_PANED (liferea_shell_lookup ("wideViewPane")), allocation.width / 2);
	}

	return FALSE;
}

static gboolean
on_configure_event (GtkWidget *window, GdkEvent *event, gpointer user_data)
{
	LifereaShell *shell = LIFEREA_SHELL (user_data);

	if (shell->resizeTimer)
		g_source_remove (shell->resizeTimer);
	shell->resizeTimer = g_timeout_add (250, on_window_resize_cb, shell);

	return FALSE;
}

static gboolean
on_window_state_event (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	if(!shell)
		return FALSE;

	if (event->type == GDK_WINDOW_STATE) {
		GdkWindowState changed = ((GdkEventWindowState*)event)->changed_mask, state = ((GdkEventWindowState*)event)->new_window_state;

		if (changed == GDK_WINDOW_STATE_MAXIMIZED && !(state & GDK_WINDOW_STATE_WITHDRAWN)) {
			if (state & GDK_WINDOW_STATE_MAXIMIZED) {
				conf_set_bool_value (LAST_WINDOW_MAXIMIZED, TRUE);
			} else {
				conf_set_bool_value (LAST_WINDOW_MAXIMIZED, FALSE);
				gtk_container_child_set (GTK_CONTAINER (liferea_shell_lookup ("normalViewPane")), liferea_shell_lookup ("normalViewItems"),
					"resize", TRUE, NULL);
				gtk_container_child_set (GTK_CONTAINER (liferea_shell_lookup ("wideViewPane")), liferea_shell_lookup ("wideViewItems"),
					"resize", TRUE, NULL);
			}
		}
		if (state & GDK_WINDOW_STATE_ICONIFIED)
			conf_set_int_value (LAST_WINDOW_STATE, MAINWINDOW_ICONIFIED);
		else if(state & GDK_WINDOW_STATE_WITHDRAWN)
			conf_set_int_value (LAST_WINDOW_STATE, MAINWINDOW_HIDDEN);
		else
			conf_set_int_value (LAST_WINDOW_STATE, MAINWINDOW_SHOWN);
	}

	if ((event->window_state.new_window_state & GDK_WINDOW_STATE_FULLSCREEN) == 0)
		shell->fullscreen = TRUE;
	else
		shell->fullscreen = FALSE;

	return FALSE;
}

static gboolean
on_key_press_event (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	gboolean	modifier_matches = FALSE;
	guint		default_modifiers;
	const gchar	*type = NULL;
	GtkWidget	*focusw = NULL;
	gint		browse_key_setting;

	if (event->type == GDK_KEY_PRESS) {
		default_modifiers = gtk_accelerator_get_default_mod_mask ();

		/* handle [<modifier>+]<Space> headline skimming hotkey */
		switch (event->keyval) {
			case GDK_KEY_space:
				conf_get_int_value (BROWSE_KEY_SETTING, &browse_key_setting);
				switch (browse_key_setting) {
					default:
					case 0:
						modifier_matches = ((event->state & default_modifiers) == 0);
						/* Hack to make space handled in the module. This is necessary
						   because the HTML widget code must be able to catch spaces
						   for input fields.

						   By ignoring the space here it will be passed to the HTML
						   widget which in turn will pass it back if it is not eaten by
						   any input field currently focussed. */

						/* pass through space keys only if HTML widget has the focus */
						focusw = gtk_window_get_focus (GTK_WINDOW (widget));
						if (focusw)
							type = g_type_name (G_OBJECT_TYPE (focusw));
						if (type && (g_str_equal (type, "LifereaWebView")))
							return FALSE;
						break;
					case 1:
						modifier_matches = ((event->state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK);
						break;
					case 2:
						modifier_matches = ((event->state & GDK_MOD1_MASK) == GDK_MOD1_MASK);
						break;
				}

				if (modifier_matches) {
					itemview_scroll ();
					return TRUE;
				}
				break;
		}

		/* prevent usage of navigation keys in entries */
		focusw = gtk_window_get_focus (GTK_WINDOW (widget));
		if (!focusw || GTK_IS_ENTRY (focusw))
			return FALSE;

		/* prevent usage of navigation keys in HTML view */
		type = g_type_name (G_OBJECT_TYPE (focusw));
		if (type && (g_str_equal (type, "LifereaWebView")))
			return FALSE;

		/* check for treeview navigation */
		if (0 == (event->state & default_modifiers)) {
			switch (event->keyval) {
				case GDK_KEY_KP_Delete:
				case GDK_KEY_Delete:
					if (focusw == GTK_WIDGET (shell->feedlistViewWidget))
						return FALSE;	/* to be handled in feed_list_view_key_press_cb() */

					on_action_remove_item (NULL, NULL, NULL);
					return TRUE;
					break;
				case GDK_KEY_n:
					on_next_unread_item_activate (NULL, NULL, NULL);
					return TRUE;
					break;
				case GDK_KEY_f:
					itemview_move_cursor (1);
					return TRUE;
					break;
				case GDK_KEY_b:
					itemview_move_cursor (-1);
					return TRUE;
					break;
				case GDK_KEY_u:
					ui_common_treeview_move_cursor (shell->feedlistViewWidget, -1);
					itemview_move_cursor_to_first ();
					return TRUE;
					break;
				case GDK_KEY_d:
					ui_common_treeview_move_cursor (shell->feedlistViewWidget, 1);
					itemview_move_cursor_to_first ();
					return TRUE;
					break;
			}
		}
	}

	return FALSE;
}

static void
on_prefbtn_clicked (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	preferences_dialog_open ();
}

static void
on_searchbtn_clicked (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	simple_search_dialog_open ();
}

static void
on_about_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GtkWidget *dialog;

	dialog = liferea_dialog_new ("about");
	gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (dialog), VERSION);
	g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_hide), NULL);
	gtk_widget_show (dialog);
}

static void
liferea_shell_add_html_tab (const gchar *file, const gchar *name)
{
	gchar *filepattern = g_strdup_printf (PACKAGE_DATA_DIR "/" PACKAGE "/doc/html/%s", file);
	gchar *filename = common_get_localized_filename (filepattern);
	gchar *fileuri = g_strdup_printf ("file://%s", filename);

	browser_tabs_add_new (fileuri, name, TRUE);

	g_free (filepattern);
	g_free (filename);
	g_free (fileuri);
}

static void
on_topics_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	liferea_shell_add_html_tab ("topics_%s.html", _("Help Topics"));
}

static void
on_quick_reference_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	liferea_shell_add_html_tab ("reference_%s.html", _("Quick Reference"));
}

static void
on_faq_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	liferea_shell_add_html_tab ("faq_%s.html", _("FAQ"));
}

static void
on_menu_quit (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	liferea_application_shutdown ();
}

static void
on_menu_fullscreen_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	shell->fullscreen == TRUE ?
		gtk_window_fullscreen(shell->window) :
		gtk_window_unfullscreen (shell->window);
	g_simple_action_set_state (action, g_variant_new_boolean (shell->fullscreen));
}

static void
on_action_zoomin_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	liferea_shell_do_zoom (1);
}

static void
on_action_zoomout_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	liferea_shell_do_zoom (-1);
}

static void
on_action_zoomreset_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	liferea_shell_do_zoom (0);
}

static void
on_menu_import (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	import_OPML_file ();
}

static void
on_menu_export (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	export_OPML_file ();
}

/* methods to receive URLs which were dropped anywhere in the main window */
static void
liferea_shell_URL_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *data, guint info, guint time_received)
{
	gchar		*tmp1, *tmp2, *freeme;
	GtkWidget	*mainwindow;
	GtkAllocation	alloc;
	GtkTreeView	*treeview;
	GtkTreeModel	*model;
	GtkTreePath	*path;
	GtkTreeIter	iter;
	nodePtr		node;
	gint		tx, ty;

	g_return_if_fail (gtk_selection_data_get_data (data) != NULL);

	mainwindow = GTK_WIDGET (shell->window);
	treeview = GTK_TREE_VIEW (shell->feedlistViewWidget);
	model = gtk_tree_view_get_model (treeview);

	/* x and y are relative to the main window, make them relative to the treeview */
	gtk_widget_translate_coordinates (mainwindow, GTK_WIDGET (treeview), x, y, &tx, &ty);

	/* Allow link drops only over feed list widget. This is to avoid
	   the frequent accidental text drops in the HTML view. */

	gtk_widget_get_allocation(GTK_WIDGET(treeview), &alloc);

	if((x > alloc.x+alloc.width) || (x < alloc.x) ||
	   (y > alloc.y+alloc.height) || (y < alloc.y)) {
		gtk_drag_finish (context, FALSE, FALSE, time_received);
		return;
	}

	if ((gtk_selection_data_get_length (data) >= 0) && (gtk_selection_data_get_format (data) == 8)) {
		/* extra handling to accept multiple drops */
		freeme = tmp1 = g_strdup ((gchar *) gtk_selection_data_get_data (data));
		while ((tmp2 = strsep (&tmp1, "\n\r"))) {
			if (strlen (tmp2)) {
				/* if the drop is over a node, select it so that feedlist_add_subscription()
				 * adds it in the correct folder */
				if (gtk_tree_view_get_dest_row_at_pos (treeview, tx, ty, &path, NULL)) {
					if (gtk_tree_model_get_iter (model, &iter, path)) {
						gtk_tree_model_get (model, &iter, FS_PTR, &node, -1);
						/* if node is NULL, feed_list_view_select() will unselect the tv */
						feed_list_view_select (node);
					}
					gtk_tree_path_free (path);
				}
				feedlist_add_subscription (g_strdup (tmp2), NULL, NULL,
				                           FEED_REQ_PRIORITY_HIGH);
			}
		}
		g_free (freeme);
		gtk_drag_finish (context, TRUE, FALSE, time_received);
	} else {
		gtk_drag_finish (context, FALSE, FALSE, time_received);
	}
}

static void
liferea_shell_setup_URL_receiver (void)
{
	GtkWidget *mainwindow;
	GtkTargetEntry target_table[] = {
		{ "STRING",     		GTK_TARGET_OTHER_WIDGET, 0 },
		{ "text/plain", 		GTK_TARGET_OTHER_WIDGET, 0 },
		{ "text/uri-list",		GTK_TARGET_OTHER_WIDGET, 1 },
		{ "_NETSCAPE_URL",		GTK_TARGET_OTHER_APP, 1 },
		{ "application/x-rootwin-drop", GTK_TARGET_OTHER_APP, 2 }
	};

	mainwindow = GTK_WIDGET (shell->window);

	/* doesn't work with GTK_DEST_DEFAULT_DROP... */
	gtk_drag_dest_set (mainwindow, GTK_DEST_DEFAULT_ALL,
	                   target_table, sizeof (target_table)/sizeof (target_table[0]),
	                   GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);

	g_signal_connect (G_OBJECT (mainwindow), "drag_data_received",
	                  G_CALLBACK (liferea_shell_URL_received), NULL);
}

static void
on_action_open_enclosure (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	LifereaShell *shell = LIFEREA_SHELL (user_data);
	itemview_open_next_enclosure (shell->itemview);
}

static const GActionEntry liferea_shell_gaction_entries[] = {
	{"update-all", on_menu_update_all, NULL, NULL, NULL},
	{"mark-all-feeds-read", on_action_mark_all_read, NULL, NULL, NULL},
	{"import-feed-list", on_menu_import, NULL, NULL, NULL},
	{"export-feed-list", on_menu_export, NULL, NULL, NULL},
	{"quit", on_menu_quit, NULL, NULL, NULL},
	{"remove-selected-feed-items", on_remove_items_activate, NULL, NULL, NULL},
	{"prev-read-item", on_prev_read_item_activate, NULL, NULL, NULL},
	{"next-read-item", on_next_read_item_activate, NULL, NULL, NULL},
	{"next-unread-item", on_next_unread_item_activate, NULL, NULL, NULL},
	{"zoom-in", on_action_zoomin_activate, NULL, NULL, NULL},
	{"zoom-out", on_action_zoomout_activate, NULL, NULL, NULL},
	{"zoom-reset", on_action_zoomreset_activate, NULL, NULL, NULL},
	{"show-update-monitor", on_menu_show_update_monitor, NULL, NULL, NULL},
	{"show-preferences", on_prefbtn_clicked, NULL, NULL, NULL},
	{"search-feeds", on_searchbtn_clicked, NULL, NULL, NULL},
	{"show-help-contents", on_topics_activate, NULL, NULL, NULL},
	{"show-help-quick-reference", on_quick_reference_activate, NULL, NULL, NULL},
	{"show-help-faq", on_faq_activate, NULL, NULL, NULL},
	{"show-about", on_about_activate, NULL, NULL, NULL},

	/* Parameter type must be NULL for toggle. */
	{"fullscreen", NULL, NULL, "@b false", on_menu_fullscreen_activate},
	{"feedlist-view-mode", NULL, "s", "@s 'normal'", on_feedlist_view_mode_activate},

	{"toggle-item-read-status", on_toggle_unread_status, "t", NULL, NULL},
	{"toggle-item-flag", on_toggle_item_flag, "t", NULL, NULL},
	{"remove-item", on_action_remove_item, "t", NULL, NULL},
	{"launch-item-in-tab", on_action_launch_item_in_tab, "t", NULL, NULL},
	{"launch-item-in-browser", on_action_launch_item_in_browser, "t", NULL, NULL},
	{"launch-item-in-external-browser", on_action_launch_item_in_external_browser, "t", NULL, NULL},
	{"open-item-enclosure", on_action_open_enclosure, "t", NULL, NULL},
};

static const GActionEntry liferea_shell_add_gaction_entries[] = {
	{"new-subscription", on_menu_feed_new, NULL, NULL, NULL},
	{"new-folder", on_menu_folder_new, NULL, NULL, NULL},
	{"new-vfolder", on_new_vfolder_activate, NULL, NULL, NULL},
	{"new-source", on_new_plugin_activate, NULL, NULL, NULL},
	{"new-newsbin", on_new_newsbin_activate, NULL, NULL, NULL}
};

static const GActionEntry liferea_shell_feed_gaction_entries[] = {
	{"mark-selected-feed-as-read", on_action_mark_all_read, NULL, NULL, NULL},
	{"update-selected", on_menu_update, NULL, NULL, NULL}
};

static const GActionEntry liferea_shell_read_write_gaction_entries[] = {
	{"selected-node-properties", on_menu_properties, NULL, NULL, NULL},
	{"delete-selected", on_menu_delete, NULL, NULL, NULL}
};

static const GActionEntry liferea_shell_item_gaction_entries[] = {
	{"toggle-selected-item-read-status", on_toggle_unread_status, NULL, NULL, NULL},
	{"toggle-selected-item-flag", on_toggle_item_flag, NULL, NULL, NULL},
	{"remove-selected-item", on_action_remove_item, NULL, NULL, NULL},
	{"launch-selected-item-in-tab", on_action_launch_item_in_tab, NULL, NULL, NULL},
	{"launch-selected-item-in-browser", on_action_launch_item_in_browser, NULL, NULL, NULL},
	{"launch-selected-item-in-external-browser", on_action_launch_item_in_external_browser, NULL, NULL, NULL},
	{"open-selected-item-enclosure", on_action_open_enclosure, NULL, NULL, NULL}
};

static void
on_action_open_link_in_browser (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemview_launch_URL (g_variant_get_string (parameter, NULL), TRUE /* use internal browser */);
}

static void
on_action_open_link_in_external_browser (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	browser_launch_URL_external (g_variant_get_string (parameter, NULL));
}

static void
on_action_open_link_in_tab (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	browser_tabs_add_new (g_variant_get_string (parameter, NULL), g_variant_get_string (parameter, NULL), FALSE);
}

static void
on_action_social_bookmark_link (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	gchar *social_url, *link, *title;

	g_variant_get (parameter, "(ss)", &link, &title);
	social_url = social_get_bookmark_url (link, title);
	(void)browser_tabs_add_new (social_url, social_url, TRUE);
	g_free (social_url);
}

static void
on_action_copy_link_to_clipboard (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GtkClipboard	*clipboard;
	gchar		*link = (gchar *) common_uri_sanitize (BAD_CAST g_variant_get_string (parameter, NULL));

	clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);
	gtk_clipboard_set_text (clipboard, link, -1);

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text (clipboard, link, -1);

	g_free (link);

}

static void
email_the_author(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemPtr item = NULL;

	if (parameter)
		item = item_load (g_variant_get_uint64 (parameter));
	else
		item = itemlist_get_selected ();

	if(item) {
		const gchar *author, *subject;
		GError		*error = NULL;
		gchar 		*argv[5];

		author = item_get_author(item);
		subject = item_get_title (item);

		g_assert (author != NULL);

		argv[0] = g_strdup("xdg-email");
		argv[1] = g_strdup_printf ("mailto:%s", author);
		argv[2] = g_strdup("--subject");
		argv[3] = g_strdup_printf ("%s", subject);
		argv[4] = NULL;

		g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error);

		if (error && (0 != error->code)) {
			debug (DEBUG_GUI, "Email command failed: %s : %s", argv[0], error->message);
			liferea_shell_set_important_status_bar (_("Email command failed: %s"), error->message);
			g_error_free (error);
		} else {
			liferea_shell_set_status_bar (_("Starting: \"%s\""), argv[0]);
		}

		g_free(argv[0]);
		g_free(argv[1]);
		g_free(argv[2]);
		g_free(argv[3]);
		item_unload(item);
	}
}

static const GActionEntry liferea_shell_link_gaction_entries[] = {
	{"open-link-in-tab", on_action_open_link_in_tab, "s", NULL, NULL},
	{"open-link-in-browser", on_action_open_link_in_browser, "s", NULL, NULL},
	{"open-link-in-external-browser", on_action_open_link_in_external_browser, "s", NULL, NULL},
	/* The parameters are link, then title. */
	{"social-bookmark-link", on_action_social_bookmark_link, "(ss)", NULL, NULL},
	{"copy-link-to-clipboard", on_action_copy_link_to_clipboard, "s", NULL, NULL},
	{"email-the-author", email_the_author, "t", NULL, NULL}
};

static gboolean
liferea_shell_restore_layout (gpointer user_data)
{
	GdkWindow	*gdk_window;
	GtkAllocation	allocation;
	gint		last_vpane_pos, last_hpane_pos, last_wpane_pos;

	liferea_shell_restore_position ();

	/* This only works after the window has been restored, so we do it last. */
	conf_get_int_value (LAST_VPANE_POS, &last_vpane_pos);
	conf_get_int_value (LAST_HPANE_POS, &last_hpane_pos);
	conf_get_int_value (LAST_WPANE_POS, &last_wpane_pos);
	
	/* Sanity check pane sizes for too large values */
	/* a) set leftpane to 1/3rd of window size if too large */
	gdk_window = gtk_widget_get_window (GTK_WIDGET (shell->window));
	if (gdk_window_get_width (gdk_window) * 95 / 100 <= last_vpane_pos || 0 == last_vpane_pos)
		last_vpane_pos = gdk_window_get_width (gdk_window) / 3;
	
	/* b) set normalViewPane to 50% container height if too large */
	gtk_widget_get_allocation (GTK_WIDGET (liferea_shell_lookup ("normalViewPane")), &allocation);
	if ((allocation.height * 95 / 100 <= last_hpane_pos) || 0 == last_hpane_pos)
		last_hpane_pos = allocation.height / 2;
	
	/* c) set wideViewPane to 50% container width if too large */
	gtk_widget_get_allocation (GTK_WIDGET (liferea_shell_lookup ("wideViewPane")), &allocation);
	if ((allocation.width * 95 / 100 <= last_wpane_pos) || 0 == last_wpane_pos)
		last_wpane_pos = allocation.width / 2;

	debug (DEBUG_GUI, "Restoring pane proportions (left:%d normal:%d wide:%d)", last_vpane_pos, last_hpane_pos, last_wpane_pos);

	gtk_paned_set_position (GTK_PANED (liferea_shell_lookup ("leftpane")), last_vpane_pos);
	gtk_paned_set_position (GTK_PANED (liferea_shell_lookup ("normalViewPane")), last_hpane_pos);
	gtk_paned_set_position (GTK_PANED (liferea_shell_lookup ("wideViewPane")), last_wpane_pos);
	
	return FALSE;
}

static void
liferea_shell_restore_state (const gchar *overrideWindowState)
{
	gboolean 	last_window_maximized;
	gchar		*toolbar_style;
	gint		resultState;

	debug (DEBUG_GUI, "Setting toolbar style");

	toolbar_style = conf_get_toolbar_style ();
	liferea_shell_set_toolbar_style (toolbar_style);
	g_free (toolbar_style);

	debug (DEBUG_GUI, "Restoring window position");
	/* Realize needed so that the window structure can be
	   accessed... otherwise we get a GTK warning when window is
	   shown by clicking on notification icon or when theme
	   colors are fetched. */
	gtk_widget_realize (GTK_WIDGET (shell->window));

	/* Apply horrible window state parameter logic:
	   -> overrideWindowState provides optional command line flags passed by
	      user or the session manager (prio 1)
	   -> lastState provides last shutdown preference (prio 2)
	 */

	/* Initialize with last saved state */
	conf_get_int_value (LAST_WINDOW_STATE, &resultState);

	debug (DEBUG_GUI, "Previous window state indicators: dconf=%d, CLI switch=%s", resultState, overrideWindowState);

	/* Override with command line options */
	if (!g_strcmp0 (overrideWindowState, "hidden"))
		resultState = MAINWINDOW_HIDDEN;
	if (!g_strcmp0 (overrideWindowState, "shown"))
		resultState = MAINWINDOW_SHOWN;

	/* And set the window to the resulting state */
	switch (resultState) {
		case MAINWINDOW_HIDDEN:
			debug (DEBUG_GUI, "Restoring window state 'hidden (to tray)'");
			gtk_widget_hide (GTK_WIDGET (shell->window));
			break;
		case MAINWINDOW_SHOWN:
		default:
			/* Safe default is always to show window */
			debug (DEBUG_GUI, "Restoring window state 'shown'");
			gtk_widget_show (GTK_WIDGET (shell->window));
	}
	
	conf_get_bool_value (LAST_WINDOW_MAXIMIZED, &last_window_maximized);
	if (!last_window_maximized) {
		gtk_container_child_set (GTK_CONTAINER (liferea_shell_lookup ("normalViewPane")), liferea_shell_lookup ("normalViewItems"),
			"resize", TRUE, NULL);
		gtk_container_child_set (GTK_CONTAINER (liferea_shell_lookup ("wideViewPane")), liferea_shell_lookup ("wideViewItems"),
			"resize", TRUE, NULL);
	}
	
	/* Need to run asynchronous otherwise pane widget allocation is reported
	   wrong, maybe it is running to early as we realize only above. Also
	   only like this window position restore works properly. */
	g_idle_add (liferea_shell_restore_layout, NULL);
}

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
static const gchar * liferea_accels_show_help_contents[] = {"F1", NULL};
static const gchar * liferea_accels_open_selected_item_enclosure[] = {"<Control>o", NULL};
static const gchar * liferea_accels_launch_item_in_external_browser[] = {"<Control>d", NULL};

void
liferea_shell_create (GtkApplication *app, const gchar *overrideWindowState, gint pluginsDisabled)
{
	GMenuModel		*menubar_model;
	gboolean		toggle;
	gchar			*id;
	gint			mode;
	FeedListView	*feedListView;


	g_object_new (LIFEREA_SHELL_TYPE, NULL);
	g_assert(shell);

	shell->window = GTK_WINDOW (liferea_shell_lookup ("mainwindow"));

	gtk_window_set_application (GTK_WINDOW (shell->window), app);

	/* Add GActions to application */
	shell->generalActions = G_ACTION_GROUP (g_simple_action_group_new ());
	g_action_map_add_action_entries (G_ACTION_MAP(shell->generalActions), liferea_shell_gaction_entries, G_N_ELEMENTS (liferea_shell_gaction_entries), NULL);
	ui_common_add_action_group_to_map (shell->generalActions, G_ACTION_MAP (app));

	shell->addActions = G_ACTION_GROUP (g_simple_action_group_new ());
	g_action_map_add_action_entries (G_ACTION_MAP(shell->addActions), liferea_shell_add_gaction_entries, G_N_ELEMENTS (liferea_shell_add_gaction_entries), NULL);
	ui_common_add_action_group_to_map (shell->addActions, G_ACTION_MAP (app));

	shell->feedActions = G_ACTION_GROUP (g_simple_action_group_new ());
	g_action_map_add_action_entries (G_ACTION_MAP(shell->feedActions), liferea_shell_feed_gaction_entries, G_N_ELEMENTS (liferea_shell_feed_gaction_entries), NULL);
	ui_common_add_action_group_to_map (shell->feedActions, G_ACTION_MAP (app));

	shell->itemActions = G_ACTION_GROUP (g_simple_action_group_new ());
	g_action_map_add_action_entries (G_ACTION_MAP(shell->itemActions), liferea_shell_item_gaction_entries, G_N_ELEMENTS (liferea_shell_item_gaction_entries), shell);
	ui_common_add_action_group_to_map (shell->itemActions, G_ACTION_MAP (app));

	shell->readWriteActions = G_ACTION_GROUP (g_simple_action_group_new ());
	g_action_map_add_action_entries (G_ACTION_MAP(shell->readWriteActions), liferea_shell_read_write_gaction_entries, G_N_ELEMENTS (liferea_shell_read_write_gaction_entries), NULL);
	ui_common_add_action_group_to_map (shell->readWriteActions, G_ACTION_MAP (app));

	g_action_map_add_action_entries (G_ACTION_MAP(app), liferea_shell_link_gaction_entries, G_N_ELEMENTS (liferea_shell_link_gaction_entries), NULL);

	/* 1.) menu creation */

	debug (DEBUG_GUI, "Setting up menus");

	shell->itemlist = itemlist_create ();

	/* Prepare some toggle button states */

	conf_get_enum_value (FEEDLIST_VIEW_MODE, &mode);
	g_simple_action_set_state (
		G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (app), "feedlist-view-mode")),
		g_variant_new_string (feed_list_view_mode_value_to_string (mode)));

	/* Menu creation */
	gtk_builder_add_from_file (shell->xml, PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "liferea_menu.ui", NULL);
	menubar_model = G_MENU_MODEL (gtk_builder_get_object (shell->xml, "menubar"));
	gtk_application_set_menubar (app, menubar_model);

	/* Add accelerators */
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
	gtk_application_set_accels_for_action (app, "app.show-help-contents", liferea_accels_show_help_contents);
	gtk_application_set_accels_for_action (app, "app.open-selected-item-enclosure", liferea_accels_open_selected_item_enclosure);
	gtk_application_set_accels_for_action (app, "app.launch-item-in-external-browser", liferea_accels_launch_item_in_external_browser);

	/* Toolbar */
	gtk_builder_add_from_file (shell->xml, PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "liferea_toolbar.ui", NULL);

	shell->toolbar = GTK_WIDGET (gtk_builder_get_object (shell->xml, "maintoolbar"));

	/* 2.) setup containers */

	debug (DEBUG_GUI, "Setting up widget containers");

	gtk_grid_attach_next_to (GTK_GRID (liferea_shell_lookup ("vbox1")), shell->toolbar, NULL, GTK_POS_TOP, 1,1);

	gtk_widget_show_all(GTK_WIDGET(shell->toolbar));
	render_init_theme_colors (GTK_WIDGET (shell->window));
	g_signal_connect (G_OBJECT (shell->window), "style-updated", G_CALLBACK(liferea_shell_rebuild_css), NULL);

	/* 3.) setup status bar */

	debug (DEBUG_GUI, "Setting up status bar");

	shell->statusbar = GTK_STATUSBAR (liferea_shell_lookup ("statusbar"));
	shell->statusbarLocked = FALSE;
	shell->statusbarLockTimer = 0;
	shell->statusbar_feedsinfo_evbox = gtk_event_box_new ();
	shell->statusbar_feedsinfo = gtk_label_new("");
	gtk_container_add (GTK_CONTAINER (shell->statusbar_feedsinfo_evbox), shell->statusbar_feedsinfo);
	gtk_widget_show_all (shell->statusbar_feedsinfo_evbox);
	gtk_box_pack_start (GTK_BOX (shell->statusbar), shell->statusbar_feedsinfo_evbox, FALSE, FALSE, 5);
	g_signal_connect (G_OBJECT (shell->statusbar_feedsinfo_evbox), "button_release_event", G_CALLBACK (on_next_unread_item_activate), NULL);

	/* 4.) setup tabs */

	debug (DEBUG_GUI, "Setting up tabbed browsing");
	shell->tabs = browser_tabs_create (GTK_NOTEBOOK (liferea_shell_lookup ("browsertabs")));

	/* 5.) setup feed list */

	debug (DEBUG_GUI, "Setting up feed list");
	shell->feedlistViewWidget = GTK_TREE_VIEW (liferea_shell_lookup ("feedlist"));
	feedListView = feed_list_view_create (shell->feedlistViewWidget);

	/* 6.) setup menu sensivity */

	debug (DEBUG_GUI, "Initialising menus");

	/* On start, no item or feed is selected, so menus should be insensitive */
	liferea_shell_update_item_menu (FALSE);

	/* necessary to prevent selection signals when filling the feed list
	   and setting the 2/3 pane mode view */
	gtk_widget_set_sensitive (GTK_WIDGET (shell->feedlistViewWidget), FALSE);

	/* 7.) setup item view */

	debug (DEBUG_GUI, "Setting up item view");

	shell->itemview = itemview_create (GTK_WIDGET (shell->window));

        /* 8.) load icons as required */

        debug (DEBUG_GUI, "Loading icons");

        icons_load ();

	/* 9.) update and restore all menu elements */
	liferea_shell_update_toolbar ();
	liferea_shell_update_history_actions ();
	liferea_shell_setup_URL_receiver ();
	liferea_shell_restore_state (overrideWindowState);

	gtk_widget_set_sensitive (GTK_WIDGET (shell->feedlistViewWidget), TRUE);

	/* 10.) Set up feed list */
	shell->feedlist = feedlist_create (feedListView);

	/* 11.) Restore latest layout and selection */
	conf_get_int_value (DEFAULT_VIEW_MODE, &mode);
	itemview_set_layout (mode);

	// FIXME: Move to feed list code
	if (conf_get_str_value (LAST_NODE_SELECTED, &id)) {
		feed_list_view_select (node_from_id (id));
		g_free (id);
	}

	/* 12. Setup shell window signals, only after all widgets are ready */
	g_signal_connect (shell->feedlist, "new-items",
	                  G_CALLBACK (liferea_shell_update_unread_stats), shell->feedlist);
	g_signal_connect (shell->feedlist, "items-updated",
	                  G_CALLBACK (liferea_shell_update_node_actions), NULL);
	g_signal_connect (shell->itemlist, "item-updated",
	                  G_CALLBACK (liferea_shell_update_node_actions), NULL);
	g_signal_connect (feedListView, "selection-changed",
	                  G_CALLBACK (liferea_shell_update_node_actions), NULL);

	g_signal_connect ((gpointer) liferea_shell_lookup ("itemtabs"), "key_press_event",
	                  G_CALLBACK (on_key_press_event_null_cb), NULL);

	g_signal_connect ((gpointer) liferea_shell_lookup ("itemtabs"), "key_release_event",
	                  G_CALLBACK (on_key_press_event_null_cb), NULL);

	g_signal_connect ((gpointer) liferea_shell_lookup ("itemtabs"), "scroll_event",
	                  G_CALLBACK (on_notebook_scroll_event_null_cb), NULL);

	g_signal_connect (G_OBJECT (shell->window), "delete_event", G_CALLBACK(on_close), NULL);
	g_signal_connect (G_OBJECT (shell->window), "window_state_event", G_CALLBACK(on_window_state_event), shell);
	g_signal_connect (G_OBJECT (shell->window), "configure_event", G_CALLBACK(on_configure_event), shell);
	g_signal_connect (G_OBJECT (shell->window), "key_press_event", G_CALLBACK(on_key_press_event), shell);

	/* 13. Setup shell plugins */
	if(0 == pluginsDisabled) {
		shell->extensions = peas_extension_set_new (PEAS_ENGINE (liferea_plugins_engine_get_default ()),
				                     LIFEREA_TYPE_SHELL_ACTIVATABLE, "shell", shell, NULL);

		liferea_plugins_engine_set_default_signals (shell->extensions, shell);
	}

	/* 14. Rebuild search folders if needed */
	if (searchFolderRebuild)
		vfolder_foreach (vfolder_rebuild);

}

void
liferea_shell_destroy (void)
{
	liferea_shell_save_position ();
	g_object_unref (shell->extensions);
	g_object_unref (shell->tabs);
	g_object_unref (shell->feedlist);
	g_object_unref (shell->itemview);

	g_signal_handlers_block_by_func (shell, G_CALLBACK (on_window_state_event), shell);
	gtk_widget_destroy (GTK_WIDGET (shell->window));

	g_object_unref (shell);
}

static gboolean
liferea_shell_window_is_on_other_desktop(GdkWindow *gdkwindow)
{
#ifdef GDK_WINDOWING_X11
	return GDK_IS_X11_DISPLAY (gdk_window_get_display (gdkwindow)) &&
	    (gdk_x11_window_get_desktop (gdkwindow) !=
	     gdk_x11_screen_get_current_desktop (gdk_window_get_screen (gdkwindow)));
#else
	return FALSE;
#endif
}

static void
liferea_shell_window_move_to_current_desktop(GdkWindow *gdkwindow)
{
#ifdef GDK_WINDOWING_X11
	if (GDK_IS_X11_DISPLAY (gdk_window_get_display (gdkwindow)))
	    gdk_x11_window_move_to_current_desktop (gdkwindow);
#endif
}

void liferea_shell_show_window (void)
{
	GtkWidget *mainwindow = GTK_WIDGET (shell->window);
	GdkWindow *gdkwindow = gtk_widget_get_window (mainwindow);

	liferea_shell_window_move_to_current_desktop (gdkwindow);
	if (!gtk_widget_get_visible (GTK_WIDGET (mainwindow)))
		liferea_shell_restore_position ();
	gtk_window_deiconify (GTK_WINDOW (mainwindow));
	gtk_window_present (shell->window);
}

void
liferea_shell_toggle_visibility (void)
{
	GtkWidget *mainwindow = GTK_WIDGET (shell->window);
	GdkWindow *gdkwindow = gtk_widget_get_window (mainwindow);

	if (liferea_shell_window_is_on_other_desktop (gdkwindow) ||
	    !gtk_widget_get_visible (mainwindow)) {
		liferea_shell_show_window ();
	} else {
		liferea_shell_save_position ();
		gtk_widget_hide (mainwindow);
	}
}

GtkWidget *
liferea_shell_get_window (void)
{
	return GTK_WIDGET (shell->window);
}

void
liferea_shell_rebuild_css (void)
{
	render_init_theme_colors (GTK_WIDGET (shell->window));
	itemview_style_update ();
}

