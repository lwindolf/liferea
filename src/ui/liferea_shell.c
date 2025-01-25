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
#include "ui/feed_list_view.h"
#include "ui/icons.h"
#include "ui/itemview.h"
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
	GActionGroup	*htmlviewActions;

	LifereaPluginsEngine *plugins;

	GtkWindow	*window;			/*<< Liferea main window */
	GtkEventController *keypress;
	GtkWidget	*headerbar;
	GtkTreeView	*feedlistView;

	GtkStatusbar	*statusbar;		/*<< main window status bar */
	gboolean	statusbarLocked;	/*<< flag locking important message on status bar */
	guint		statusbarLockTimer;	/*<< timer id for status bar lock reset timer */
	guint		resizeTimer;		/*<< timer id for resize callback */

	ItemList	*itemlist;
	FeedList	*feedlist;
	ItemView	*itemview;
	BrowserTabs	*tabs;
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

LifereaShell *
liferea_shell_get_instance (void)
{
	return shell;
}

void liferea_shell_save_layout (void);

static void
liferea_shell_finalize (GObject *object)
{
	LifereaShell *ls = LIFEREA_SHELL (object);

	g_object_unref (ls->plugins);

	liferea_shell_save_layout ();
	g_object_unref (ls->tabs);
	g_object_unref (ls->feedlist);
	g_object_unref (ls->itemview);

	gtk_window_destroy (ls->window);

	g_object_unref (ls->settings);
	g_object_unref (ls->keypress);
	g_object_unref (ls->xml);

	g_object_unref (ls->shellActions);
	g_object_unref (ls->feedlistActions);
	g_object_unref (ls->itemlistActions);
	g_object_unref (ls->htmlviewActions);

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
	shell->xml = gtk_builder_new_from_resource ("/org/gnome/liferea/ui/mainwindow.ui");
}

void
liferea_shell_save_layout (void)
{
	gint		x, y, w, h;
	GtkWidget	*pane;

	g_assert (shell);

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

	/*monitor = gdk_display_get_monitor_at_window (gtk_widget_get_display (GTK_WIDGET(shell->window)), gdk_window);
	gdk_monitor_get_workarea (monitor, &work_area);

	if (x+w<0 || y+h<0 ||
	    x > work_area.width ||
	    y > work_area.height)
		return;*/

	// FIXME: GTK4 https://developer.gnome.org/documentation/tutorials/save-state.html
	/* save window size */
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

	conf_get_int_value (LAST_WINDOW_X, &x);
	conf_get_int_value (LAST_WINDOW_Y, &y);

	conf_get_int_value (LAST_WINDOW_WIDTH, &w);
	conf_get_int_value (LAST_WINDOW_HEIGHT, &h);

	debug (DEBUG_GUI, "Retrieved saved setting: size %dx%d position %d:%d", w, h, x, y);

	/* Restore position only if the width and height were saved */
	if (w != 0 && h != 0) {
		g_warning ("FIXME GTK4 restore window dimensions");
	}

	/*conf_get_bool_value (LAST_WINDOW_MAXIMIZED, &last_window_maximized);

	if (last_window_maximized)
		gtk_window_maximize (GTK_WINDOW (shell->window));
	else
		gtk_window_unmaximize (GTK_WINDOW (shell->window));
	*/
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

	// FIXME: GTK4
	//gtk_label_set_text (GTK_LABEL (shell->statusbar_feedsinfo), tmp);
	g_free (tmp);
	g_free (msg);
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

static gboolean
on_key_pressed_event_null_cb (GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer data)
{
	return FALSE;
}
/* FIXME: GTK4
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
*/
	/* ignore scroll events from the content of the page */
/*	if (!originator || gtk_widget_is_ancestor (originator, child))
		return FALSE;

	return TRUE;
}*/

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
//	GdkWindow	*gdk_window;
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
// FIXME: GTK4
	/* a) set leftpane to 1/3rd of window size if too large */

	/* b) set normalViewPane to 50% container height if too large */

	/* c) set wideViewPane to 50% container width if too large */

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
on_key_pressed_event (GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer data)
{
	gboolean	modifier_matches = FALSE;
	guint		default_modifiers;
	const gchar	*type = NULL;
	GtkWidget	*focusw = NULL;
	gint		browse_key_setting;

	default_modifiers = gtk_accelerator_get_default_mod_mask ();

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
					focusw = gtk_window_get_focus (shell->window);
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
				itemview_scroll ();
				return TRUE;
			}
			break;
	}

	/* prevent usage of navigation keys in entries */
	focusw = gtk_window_get_focus (shell->window);
	if (!focusw || GTK_IS_ENTRY (focusw))
		return FALSE;

	/* prevent usage of navigation keys in HTML view */
	type = g_type_name (G_OBJECT_TYPE (focusw));
	if (type && (g_str_equal (type, "LifereaWebView")))
		return FALSE;

	/* check for treeview navigation */
	if (0 == (state & default_modifiers)) {
		switch (keyval) {
			// FIXME: GTK4 migration
			/*case GDK_KEY_KP_Delete:
			case GDK_KEY_Delete:
				if (focusw == GTK_WIDGET (shell->feedlistView))
					return FALSE;	// to be handled in feed_list_view_key_press_cb() 

				on_action_remove_item (NULL, NULL, NULL);
				return TRUE;
				break;
			case GDK_KEY_n:
				on_next_unread_item_activate (NULL, NULL, NULL);
				return TRUE;
				break;*/
			case GDK_KEY_f:
				itemview_move_cursor (1);
				return TRUE;
				break;
			case GDK_KEY_b:
				itemview_move_cursor (-1);
				return TRUE;
				break;
			case GDK_KEY_u:
				ui_common_treeview_move_cursor (shell->feedlistView, -1);
				itemview_move_cursor_to_first ();
				return TRUE;
				break;
			case GDK_KEY_d:
				ui_common_treeview_move_cursor (shell->feedlistView, 1);
				itemview_move_cursor_to_first ();
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

static gboolean
liferea_shell_restore_layout (gpointer user_data)
{
//	GdkWindow	*gdk_window;
	GtkAllocation	allocation;
	gint		last_vpane_pos, last_hpane_pos, last_wpane_pos;

	liferea_shell_restore_position ();

	/* This only works after the window has been restored, so we do it last. */
	conf_get_int_value (LAST_VPANE_POS, &last_vpane_pos);
	conf_get_int_value (LAST_HPANE_POS, &last_hpane_pos);
	conf_get_int_value (LAST_WPANE_POS, &last_wpane_pos);

g_warning("FIXME: GTK4 restore pane position sanity checks");	
	/* Sanity check pane sizes for too large values */
	/* a) set leftpane to 1/3rd of window size if too large */
	//gdk_window = gtk_widget_get_window (GTK_WIDGET (shell->window));
	//if (gdk_window_get_width (gdk_window) * 95 / 100 <= last_vpane_pos || 0 == last_vpane_pos)
	//	last_vpane_pos = gdk_window_get_width (gdk_window) / 3;
	
	/* b) set normalViewPane to 50% container height if too large */
	//gtk_widget_get_allocation (GTK_WIDGET (liferea_shell_lookup ("normalViewPane")), &allocation);
	//if ((allocation.height * 95 / 100 <= last_hpane_pos) || 0 == last_hpane_pos)
	//	last_hpane_pos = allocation.height / 2;
	
	/* c) set wideViewPane to 50% container width if too large */
	//gtk_widget_get_allocation (GTK_WIDGET (liferea_shell_lookup ("wideViewPane")), &allocation);
	//if ((allocation.width * 95 / 100 <= last_wpane_pos) || 0 == last_wpane_pos)
//		last_wpane_pos = allocation.width / 2;

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
	gint		resultState;

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
			gtk_widget_set_visible (GTK_WIDGET (shell->window), FALSE);
			break;
		case MAINWINDOW_SHOWN:
		default:
			/* Safe default is always to show window */
			debug (DEBUG_GUI, "Restoring window state 'shown'");
			gtk_widget_set_visible (GTK_WIDGET (shell->window), TRUE);
	}
	
	conf_get_bool_value (LAST_WINDOW_MAXIMIZED, &last_window_maximized);
	if (!last_window_maximized) {
		gtk_paned_set_end_child (GTK_PANED (liferea_shell_lookup ("normalViewPane")), liferea_shell_lookup ("normalViewItems"));
		gtk_paned_set_end_child (GTK_PANED (liferea_shell_lookup ("wideViewPane")), liferea_shell_lookup ("wideViewItems"));
	}
	
	/* Need to run asynchronous otherwise pane widget allocation is reported
	   wrong, maybe it is running to early as we realize only above. Also
	   only like this window position restore works properly. */
	g_idle_add (liferea_shell_restore_layout, NULL);
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
static const gchar * liferea_accels_launch_item_in_external_browser[] = {"<Control>d", NULL};

void
liferea_shell_create (GtkApplication *app, const gchar *overrideWindowState, gint pluginsDisabled)
{
	GMenuModel	*menubar_model;
	gchar		*id;
	gint		mode;
	FeedListView	*feedListView;

	g_object_new (LIFEREA_SHELL_TYPE, NULL);
	g_assert (shell);

	shell->window = GTK_WINDOW (liferea_shell_lookup ("mainwindow"));

	gtk_window_set_application (GTK_WINDOW (shell->window), app);

	/* 1.) setup plugin engine including mandatory base plugins that (for example the feed list or auth) might depend on */
	debug (DEBUG_GUI, "Register mandatory plugins");
	shell->plugins = liferea_plugins_engine_get ();

	/* 2.) Setup item list */
	shell->itemlist = itemlist_create ();

	/* 3.) Headerbar and menu creation */
	debug (DEBUG_GUI, "Setting up header bar");
	gtk_builder_add_from_file (shell->xml, PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "liferea_header.ui", NULL);
	shell->headerbar = GTK_WIDGET (gtk_builder_get_object (shell->xml, "headerbar"));

	gtk_builder_add_from_file (shell->xml, PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "liferea_menu.ui", NULL);
	menubar_model = G_MENU_MODEL (gtk_builder_get_object (shell->xml, "menubar"));
	gtk_application_set_menubar (app, menubar_model);

        // FIXME: GTK4 liferea_shell_update_history_actions ();

	/* Add accelerators for shell */
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
	gtk_application_set_accels_for_action (app, "app.launch-item-in-external-browser", liferea_accels_launch_item_in_external_browser);

	/* 4.) setup status bar */
	debug (DEBUG_GUI, "Setting up status bar");
	shell->statusbar = GTK_STATUSBAR (liferea_shell_lookup ("statusbar"));
	shell->statusbarLocked = FALSE;
	shell->statusbarLockTimer = 0;
	// FIXME: GTK4 eventbox replacement
	//shell->statusbar_feedsinfo_evbox = gtk_event_box_new ();
	//shell->statusbar_feedsinfo = gtk_label_new("");
	//gtk_container_add (GTK_CONTAINER (shell->statusbar_feedsinfo_evbox), shell->statusbar_feedsinfo);
	// FIXME: GTK4 5px statusbar padding
	//gtk_box_append (GTK_BOX (shell->statusbar), shell->statusbar_feedsinfo_evbox);
	//g_signal_connect (G_OBJECT (shell->statusbar_feedsinfo_evbox), "button_release_event", G_CALLBACK (on_next_unread_item_activate), NULL);

	/* 5.) setup tabs */
	debug (DEBUG_GUI, "Setting up tabbed browsing");
	shell->tabs = browser_tabs_create (GTK_NOTEBOOK (liferea_shell_lookup ("browsertabs")));

	/* 6.) setup feed and item list widgets */
	debug (DEBUG_GUI, "Setting up feed list");
	shell->feedlistView = GTK_TREE_VIEW (liferea_shell_lookup ("feedlist"));
	feedListView = feed_list_view_create (shell->feedlistView);
	/* necessary to prevent selection signals when filling the feed list */
	gtk_widget_set_sensitive (GTK_WIDGET (shell->feedlistView), FALSE);

	debug (DEBUG_GUI, "Setting up item view");
	shell->itemview = itemview_create (GTK_WIDGET (shell->window));

        /* 8.) load icons as required */
        debug (DEBUG_GUI, "Loading icons");
        icons_load ();

	/* 9.) update and restore all menu elements */
	liferea_shell_setup_URL_receiver ();
	liferea_shell_restore_state (overrideWindowState);

	/* 10.) Set up feed list model */
	shell->feedlist = feedlist_create (feedListView);

	gtk_widget_set_sensitive (GTK_WIDGET (shell->feedlistView), TRUE);

	/* 11.) Restore latest layout and selection */
	conf_get_int_value (DEFAULT_VIEW_MODE, &mode);
	itemview_set_layout (mode);

	// FIXME: Move to feed list code
	if (conf_get_str_value (LAST_NODE_SELECTED, &id)) {
		feed_list_view_select (node_from_id (id));
		g_free (id);
	}

	/* 12. Setup shell window signals, only after all widgets are ready */
	g_signal_connect (shell->feedlist, "new-items",  G_CALLBACK (liferea_shell_update_unread_stats), shell->feedlist);

	g_signal_connect (shell->keypress, "key_pressed", G_CALLBACK (on_key_pressed_event_null_cb), NULL);
	g_signal_connect (shell->keypress, "key_released", G_CALLBACK (on_key_pressed_event_null_cb), NULL);
	gtk_widget_add_controller (liferea_shell_lookup ("itemtabs"), shell->keypress);

	// FIXME GTK4
	//g_signal_connect ((gpointer) liferea_shell_lookup ("itemtabs"), "scroll_event", G_CALLBACK (on_notebook_scroll_event_null_cb), NULL);

	g_signal_connect (G_OBJECT (shell->window), "delete_event", G_CALLBACK(on_close), NULL);
	g_signal_connect (G_OBJECT (shell->window), "configure_event", G_CALLBACK(on_configure_event), shell);
	g_signal_connect (shell->keypress, "key_pressed", G_CALLBACK(on_key_pressed_event), shell);
	gtk_widget_add_controller (GTK_WIDGET (shell->window), shell->keypress);

	/* 13. Setup plugins */
	if (!pluginsDisabled) {
		debug (DEBUG_GUI, "Register shell plugins");
		liferea_plugins_engine_register_shell_plugins (shell);
	}

	/* 14. Rebuild search folders if needed */
	if (searchFolderRebuild)
		vfolder_foreach (vfolder_rebuild);

	/* 15. setup actions */
	shell->shellActions = shell_actions_create ();
	shell->feedlistActions = node_actions_create ();
	shell->itemlistActions = item_actions_create ();
	shell->htmlviewActions = link_actions_create ();

}

void
liferea_shell_destroy (void)
{
	g_object_unref (shell);
}

void liferea_shell_show_window (void)
{
	GtkWidget *mainwindow = GTK_WIDGET (shell->window);

	if (!gtk_widget_get_visible (GTK_WIDGET (mainwindow)))
		liferea_shell_restore_position ();
	// FIXME: how to do deiconify in GTK4?
	//gtk_window_deiconify (GTK_WINDOW (mainwindow));
	gtk_window_present (shell->window);
}

void
liferea_shell_toggle_visibility (void)
{
	GtkWidget *mainwindow = GTK_WIDGET (shell->window);

	if (!gtk_widget_get_visible (mainwindow)) {
		liferea_shell_show_window ();
	} else {
		liferea_shell_save_layout ();
		gtk_widget_set_visible (mainwindow, FALSE);
	}
}

GtkWidget *
liferea_shell_get_window (void)
{
	return GTK_WIDGET (shell->window);
}
