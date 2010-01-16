/**
 * @file ui_shell.c  UI layout handling
 *
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2007-2010 Lars Lindner <lars.lindner@gmail.com>
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
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>		/* strcmp() and strsep() */

#include "browser.h"
#include "common.h"
#include "conf.h"
#include "debug.h"
#include "export.h"
#include "feedlist.h"
#include "htmlview.h"
#include "itemlist.h"
#include "net_monitor.h"
#include "ui/attention_profile_dialog.h"
#include "ui/browser_tabs.h"
#include "ui/feed_list_view.h"
#include "ui/itemview.h"
#include "ui/item_list_view.h"
#include "ui/liferea_dialog.h"
#include "ui/search_dialog.h"
#include "ui/ui_common.h"
#include "ui/ui_prefs.h"
#include "ui/ui_script.h"
#include "ui/ui_search.h"
#include "ui/ui_session.h"
#include "ui/ui_tray.h"
#include "ui/ui_update.h"

/* all used icons (FIXME: evil) */
GdkPixbuf *icons[MAX_ICONS];

static void liferea_shell_class_init	(LifereaShellClass *klass);
static void liferea_shell_init		(LifereaShell *ls);

#define LIFEREA_SHELL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), LIFEREA_SHELL_TYPE, LifereaShellPrivate))

struct LifereaShellPrivate {
	GtkBuilder	*xml;

	GtkWindow	*window;		/**< Liferea main window */
	GtkWidget	*menubar;
	GtkWidget	*toolbar;
	GtkTreeView	*feedlistView;
	
	GtkStatusbar	*statusbar;		/**< main window status bar */
	gboolean	statusbarLocked;	/**< flag locking important message on status bar */
	guint		statusbarLockTimer;	/**< timer id for status bar lock reset timer */

	GtkWidget	*statusbar_feedsinfo;
	GtkActionGroup	*generalActions;
	GtkActionGroup	*addActions;		/**< all types of "New" options */
	GtkActionGroup	*feedActions;		/**< update and mark read */
	GtkActionGroup	*readWriteActions;	/**< node remove and properties, node itemset items remove */
	GtkActionGroup	*itemActions;		/**< item state toggline, single item remove */
	
	FeedList	*feedlist;
	ItemView	*itemview;
	BrowserTabs	*tabs;
};

static GObjectClass *parent_class = NULL;
static LifereaShell *shell = NULL;

GType
liferea_shell_get_type (void) 
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) 
	{
		static const GTypeInfo our_info = 
		{
			sizeof (LifereaShellClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) liferea_shell_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (LifereaShell),
			0, /* n_preallocs */
			(GInstanceInitFunc) liferea_shell_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "LifereaShell",
					       &our_info, 0);
	}

	return type;
}

static void
liferea_shell_finalize (GObject *object)
{
	LifereaShell *ls = LIFEREA_SHELL (object);
	
	g_object_unref (ls->priv->xml);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
liferea_shell_class_init (LifereaShellClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = liferea_shell_finalize;

	g_type_class_add_private (object_class, sizeof(LifereaShellPrivate));
}

GtkWidget *
liferea_shell_lookup (const gchar *name)
{
	g_return_val_if_fail (shell != NULL, NULL);
	g_return_val_if_fail (shell->priv != NULL, NULL);

	return GTK_WIDGET (gtk_builder_get_object (shell->priv->xml, name));
}

static void
liferea_shell_init (LifereaShell *ls)
{
	/* FIXME: we should only need to load mainwindow, but GtkBuilder won't
	 * load the other required objects, so we need to do it manually...
	 * See fixme in liferea_dialog_new()
	 * http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=508585
	 */
	gchar *objs[] = { "adjustment6", "mainwindow", NULL };
	/* globally accessible singleton */
	g_assert (NULL == shell);
	shell = ls;
	
	shell->priv = LIFEREA_SHELL_GET_PRIVATE (ls);
	shell->priv->xml = gtk_builder_new ();
	gtk_builder_add_objects_from_file (shell->priv->xml, PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "liferea.ui", objs, NULL);
	gtk_builder_connect_signals (shell->priv->xml, NULL);
}

/**
 * Restore the window position from the values saved into gconf. Note
 * that this does not display/present/show the mainwindow.
 */
static void
liferea_shell_restore_position (void)
{
	/* load window position */
	int x, y, w, h;
	gboolean last_window_maximized;

	conf_get_int_value (LAST_WINDOW_X, &x);
	conf_get_int_value (LAST_WINDOW_Y, &y);

	conf_get_int_value (LAST_WINDOW_WIDTH, &w);
	conf_get_int_value (LAST_WINDOW_HEIGHT, &h);
	
	debug4 (DEBUG_GUI, "Retrieved saved setting: size %dx%d position %d:%d", w, h, x, y);
	
	/* Restore position only if the width and height were saved */
	if (w != 0 && h != 0) {
	
		if (x >= gdk_screen_width ())
			x = gdk_screen_width () - 100;
		else if (x + w < 0)
			x  = 100;

		if (y >= gdk_screen_height ())
			y = gdk_screen_height () - 100;
		else if (y + w < 0)
			y  = 100;
			
		debug4 (DEBUG_GUI, "Restoring to size %dx%d position %d:%d", w, h, x, y);

		gtk_window_move (GTK_WINDOW (shell->priv->window), x, y);

		/* load window size */
		gtk_window_resize (GTK_WINDOW (shell->priv->window), w, h);
	}

	conf_get_bool_value (LAST_WINDOW_MAXIMIZED, &last_window_maximized);

	if (last_window_maximized)
		gtk_window_maximize (GTK_WINDOW (shell->priv->window));
	else
		gtk_window_unmaximize (GTK_WINDOW (shell->priv->window));

}

void
liferea_shell_save_position (void)
{
	GtkWidget	*pane;
	gint		x, y, w, h;
	gboolean	last_window_maximized;

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
	if (!gtk_widget_get_visible (GTK_WIDGET (shell->priv->window)))
		return;

	conf_get_bool_value (LAST_WINDOW_MAXIMIZED, &last_window_maximized);

	if (last_window_maximized)
		return;

	gtk_window_get_position (shell->priv->window, &x, &y);
	gtk_window_get_size (shell->priv->window, &w, &h);

	if (x+w<0 || y+h<0 ||
	    x > gdk_screen_width () ||
	    y > gdk_screen_height ())
		return;

	debug4 (DEBUG_GUI, "Saving window size and position: %dx%d %d:%d", w, h, x, y);

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
		gtk_toolbar_set_style (GTK_TOOLBAR (shell->priv->toolbar), GTK_TOOLBAR_ICONS);
	else if (!strcmp (toolbar_style, "text"))
		gtk_toolbar_set_style (GTK_TOOLBAR (shell->priv->toolbar), GTK_TOOLBAR_TEXT);
	else if (!strcmp (toolbar_style, "both"))
		gtk_toolbar_set_style (GTK_TOOLBAR (shell->priv->toolbar), GTK_TOOLBAR_BOTH);
	else if (!strcmp (toolbar_style, "both_horiz") || !strcmp (toolbar_style, "both-horiz") )
		gtk_toolbar_set_style (GTK_TOOLBAR (shell->priv->toolbar), GTK_TOOLBAR_BOTH_HORIZ);
	else /* default to icons */
		gtk_toolbar_set_style (GTK_TOOLBAR (shell->priv->toolbar), GTK_TOOLBAR_ICONS);
}

void
liferea_shell_update_toolbar (void)
{
	gboolean disable_toolbar;

	conf_get_bool_value (DISABLE_TOOLBAR, &disable_toolbar);

	if (disable_toolbar)
		gtk_widget_hide (shell->priv->toolbar);
	else
		gtk_widget_show (shell->priv->toolbar);
}

void
liferea_shell_update_update_menu (gboolean enabled)
{
	gtk_action_set_sensitive (gtk_action_group_get_action (shell->priv->feedActions, "UpdateSelected"),	enabled);
}

void
liferea_shell_update_feed_menu (gboolean add, gboolean enabled, gboolean readWrite)
{
	gtk_action_group_set_sensitive (shell->priv->addActions, add);
	gtk_action_group_set_sensitive (shell->priv->feedActions, enabled);
	gtk_action_group_set_sensitive (shell->priv->readWriteActions, readWrite);
}

void
liferea_shell_update_item_menu (gboolean enabled)
{
	gtk_action_group_set_sensitive (shell->priv->itemActions, enabled);
}

void
liferea_shell_update_allitems_actions (gboolean isNotEmpty, gboolean isRead)
{
	gtk_action_set_sensitive (gtk_action_group_get_action (shell->priv->generalActions, "RemoveAllItems"), isNotEmpty);
	gtk_action_set_sensitive (gtk_action_group_get_action (shell->priv->feedActions, "MarkFeedAsRead"), isRead);
}

void
liferea_shell_update_unread_stats (void)
{
	gint	new_items, unread_items;
	gchar	*msg, *tmp;

	if (!shell->priv)
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

	gtk_label_set_text (GTK_LABEL (shell->priv->statusbar_feedsinfo), tmp);
	g_free (tmp);
	g_free (msg);
}

/*
   Do to the unsuitable GtkStatusBar stack handling which doesn't
   allow to keep messages on top of the stack for some time without
   overwriting them with newly arriving messages we need some extra
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
	shell->priv->statusbarLocked = FALSE;
	
	return FALSE;
}

static gboolean
liferea_shell_set_status_bar_important_cb (gpointer user_data)
{
	gchar		*text = (gchar *)user_data;
	guint		id;
	GtkStatusbar	*statusbar;
	
	statusbar = GTK_STATUSBAR (shell->priv->statusbar);
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

	statusbar = GTK_STATUSBAR (shell->priv->statusbar);
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
	
	if (shell->priv->statusbarLocked)
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

	shell->priv->statusbarLocked = FALSE;
	if (shell->priv->statusbarLockTimer) {
		g_source_remove (shell->priv->statusbarLockTimer);
		shell->priv->statusbarLockTimer = 0;
	}
	
	/* URL hover messages are reset with an empty string, so 
	   we must locking the status bar on empty strings! */
	if (!g_str_equal (text, "")) {
		/* Realize 5s locking for important messages... */
		shell->priv->statusbarLocked = TRUE;
		shell->priv->statusbarLockTimer = g_timeout_add_seconds (5, liferea_shell_unlock_status_bar_cb, NULL);
	}
	
	g_idle_add ((GSourceFunc)liferea_shell_set_status_bar_important_cb, (gpointer)text);
}

static void
liferea_shell_do_zoom (gboolean in)
{
	/* We must apply the zoom either to the item view
	   or to an open tab, depending on the browser tabs
	   GtkNotebook page that is active... */
	if (!browser_tabs_get_active_htmlview ())
		itemview_do_zoom (in);
	else
		browser_tabs_do_zoom (in);
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
	gboolean dont_maximize_to_tray;

	conf_get_bool_value (DONT_MINIMIZE_TO_TRAY, &dont_maximize_to_tray);

	if ((ui_tray_get_count() == 0) || dont_maximize_to_tray) {
		liferea_shutdown ();
		return TRUE;
	}
		
	liferea_shell_save_position ();
	gtk_widget_hide (GTK_WIDGET (shell->priv->window));
	
	return TRUE;
}

static gboolean
on_window_state_event (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	if (event->type == GDK_WINDOW_STATE) {
		GdkWindowState changed = ((GdkEventWindowState*)event)->changed_mask, state = ((GdkEventWindowState*)event)->new_window_state;

		if (changed == GDK_WINDOW_STATE_MAXIMIZED && !(state & GDK_WINDOW_STATE_WITHDRAWN)) {
			if (state & GDK_WINDOW_STATE_MAXIMIZED)
				conf_set_bool_value (LAST_WINDOW_MAXIMIZED, TRUE);
			else
				conf_set_bool_value (LAST_WINDOW_MAXIMIZED, FALSE);
		}
		if (state & GDK_WINDOW_STATE_ICONIFIED)
			session_set_cmd (NULL, MAINWINDOW_ICONIFIED);
		else if(state & GDK_WINDOW_STATE_WITHDRAWN)
			session_set_cmd (NULL, MAINWINDOW_HIDDEN);
		else
			session_set_cmd (NULL, MAINWINDOW_SHOWN);
	}
	return FALSE;
}

static gboolean
on_key_press_event (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	gboolean	modifier_matches = FALSE;
	guint		default_modifiers;
	const gchar	*type;
	GtkWidget	*focusw;
	gint		browse_key_setting;

	if (event->type == GDK_KEY_PRESS) {
		default_modifiers = gtk_accelerator_get_default_mod_mask ();

		/* handle [<modifier>+]<Space> headline skimming hotkey */
		switch (event->keyval) {
			case GDK_space:
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
						return FALSE;
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
		
		/* some <Ctrl> hotkeys that overrule the HTML view */
		if ((event->state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK) {
			switch (event->keyval) {
				case GDK_KP_Add:
				case GDK_equal:
				case GDK_plus:
					liferea_shell_do_zoom (TRUE);
					return TRUE;
					break;
				case GDK_KP_Subtract:
				case GDK_minus:
					liferea_shell_do_zoom (FALSE);
					return TRUE;
					break;
			}
		}

		/* prevent usage of navigation keys in entries */
		focusw = gtk_window_get_focus (GTK_WINDOW (widget));
		if (!focusw || GTK_IS_ENTRY (focusw))
			return FALSE;

		/* prevent usage of navigation keys in HTML view */
		type = g_type_name (GTK_WIDGET_TYPE (focusw));
		if (type && (!strcmp (type, "WebKitWebView")))
			return FALSE;
		
		/* check for treeview navigation */
		if (0 == (event->state & default_modifiers)) {
			switch (event->keyval) {
				case GDK_KP_Delete:
				case GDK_Delete:
					if (focusw == GTK_WIDGET (shell->priv->feedlistView))
						return FALSE;	/* to be handled in feed_list_view_key_press_cb() */
						
					on_remove_item_activate (NULL, NULL);
					return TRUE;
					break;
				case GDK_n: 
					on_next_unread_item_activate (NULL, NULL);
					return TRUE;
					break;
				case GDK_f:
					itemview_move_cursor (1);
					return TRUE;
					break;
				case GDK_b:
					itemview_move_cursor (-1);
					return TRUE;
					break;
				case GDK_u:
					ui_common_treeview_move_cursor (shell->priv->feedlistView, -1);
					itemview_move_cursor_to_first ();
					return TRUE;
					break;
				case GDK_d:
					ui_common_treeview_move_cursor (shell->priv->feedlistView, 1);
					itemview_move_cursor_to_first ();
					return TRUE;
					break;
			}
		}
	}
	
	return FALSE;
}

static gboolean
liferea_shell_save_accels (gpointer data)
{
	gchar *accels_file = NULL;

	accels_file = g_build_filename (common_get_cache_path (), "accels", NULL);
	gtk_accel_map_save (accels_file);
	g_free (accels_file);
	return FALSE;
}

static void
on_accel_change (GtkAccelMap *object, gchar *accel_path,
		guint accel_key, GdkModifierType accel_mode,
		gpointer user_data)
{
	g_idle_add (liferea_shell_save_accels, NULL);
}

static void
on_searchbtn_clicked (GtkButton *button, gpointer user_data)
{
	simple_search_dialog_open ();
}

void
on_onlinebtn_clicked (GtkButton *button, gpointer user_data)
{
	network_monitor_set_online (!network_monitor_is_online ());
}

static void
on_work_offline_activate (GtkToggleAction *menuitem, gpointer user_data)
{
	network_monitor_set_online (!gtk_toggle_action_get_active (menuitem));
}

static void
on_about_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *dialog;

	dialog = liferea_dialog_new (NULL, "aboutdialog");
	gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (dialog), VERSION);
	g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_hide), NULL);
	gtk_widget_show (dialog);
}

static void
on_topics_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	gchar *filename = g_strdup_printf ("file://" PACKAGE_DATA_DIR "/" PACKAGE "/doc/html/%s", _("topics_en.html"));
	browser_tabs_add_new (filename, _("Help Topics"), TRUE);
	g_free (filename);
}

static void
on_quick_reference_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	gchar *filename = g_strdup_printf ("file://" PACKAGE_DATA_DIR "/" PACKAGE "/doc/html/%s", _("reference_en.html"));
	browser_tabs_add_new (filename, _("Quick Reference"), TRUE);
	g_free (filename);
}

static void
on_faq_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	gchar *filename = g_strdup_printf ("file://" PACKAGE_DATA_DIR "/" PACKAGE "/doc/html/%s", _("faq_en.html"));
	browser_tabs_add_new (filename, _("FAQ"), TRUE);
	g_free (filename);
}

static void
on_menu_attention_profile (GtkMenuItem *menuitem, gpointer user_data)
{
	attention_profile_dialog_open (attention_profile_get ());
}

static void
on_menu_quit (GtkMenuItem *menuitem, gpointer user_data)
{
	liferea_shutdown ();
}

static void
on_menu_zoomin_selected (gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	liferea_shell_do_zoom (TRUE);
}

static void
on_menu_zoomout_selected (gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	liferea_shell_do_zoom (FALSE);
}

static void
on_menu_import (gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	import_OPML_file ();
}

static void
on_menu_export (gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	export_OPML_file ();
}

static void
liferea_shell_set_online_icon (gboolean online)
{
	GtkWidget	*widget;

	widget = liferea_shell_lookup ("onlineimage");

	if (online) {
		liferea_shell_set_status_bar (_("Liferea is now online"));
		gtk_image_set_from_pixbuf (GTK_IMAGE (widget), icons[ICON_ONLINE]);
		atk_object_set_name (gtk_widget_get_accessible (widget), _("Work Offline"));		
	} else {
		liferea_shell_set_status_bar (_("Liferea is now offline"));
		gtk_image_set_from_pixbuf (GTK_IMAGE (widget), icons[ICON_OFFLINE]);
		atk_object_set_name (gtk_widget_get_accessible (widget), _("Work Online"));	
	}
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (gtk_action_group_get_action (shell->priv->generalActions, "ToggleOfflineMode")), !online);
}

static void
liferea_shell_online_status_changed (NetworkMonitor *nm, gboolean online, gpointer userdata)
{
	liferea_shell_set_online_icon (online);
	ui_tray_update ();
}

/* methods to receive URLs which were dropped anywhere in the main window */
static void
liferea_shell_URL_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *data, guint info, guint time_received)
{
	gchar		*tmp1, *tmp2, *freeme;
	GtkWidget	*mainwindow;
	GtkTreeView	*treeview;
	GtkTreeModel	*model;
	GtkTreePath	*path;
	GtkTreeIter	iter;
	nodePtr		node;
	gint		tx, ty;
	
	g_return_if_fail (gtk_selection_data_get_data (data) != NULL);
		
	mainwindow = GTK_WIDGET (liferea_shell_lookup ("mainwindow"));
	treeview = GTK_TREE_VIEW (liferea_shell_lookup ("feedlist"));
	model = gtk_tree_view_get_model (treeview);

	/* x and y are relative to the main window, make them relative to the treeview */
	g_assert (gtk_widget_translate_coordinates (mainwindow, GTK_WIDGET (treeview), x, y, &tx, &ty));

	if ((gtk_selection_data_get_length (data) >= 0) && (gtk_selection_data_get_format (data) == 8)) {
		/* extra handling to accept multiple drops */
		freeme = tmp1 = g_strdup (gtk_selection_data_get_data (data));
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

void
liferea_shell_setup_URL_receiver (GtkWidget *widget)
{
	GtkTargetEntry target_table[] = {
		{ "STRING",     		0, 0 },
		{ "text/plain", 		0, 0 },
		{ "text/uri-list",		0, 1 },
		{ "_NETSCAPE_URL",		0, 1 },
		{ "application/x-rootwin-drop", 0, 2 }
	};

	/* doesn't work with GTK_DEST_DEFAULT_DROP... */
	gtk_drag_dest_set (widget, GTK_DEST_DEFAULT_ALL,
	                   target_table, sizeof (target_table)/sizeof (target_table[0]),
	                   GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
		       
	g_signal_connect (G_OBJECT (widget), "drag_data_received",
	                  G_CALLBACK (liferea_shell_URL_received), NULL);
}

static const GtkActionEntry ui_mainwindow_action_entries[] = {
	{"SubscriptionsMenu", NULL, N_("_Subscriptions")},
	{"UpdateAll", "gtk-refresh", N_("Update _All"), "<control>A", N_("Updates all subscriptions."),
	 G_CALLBACK(on_menu_update_all)},
	{"MarkAllFeedsAsRead", "gtk-apply", N_("Mark All As _Read"), NULL, N_("Marks read every item of every subscription."),
	 G_CALLBACK(on_menu_allfeedsread)},
	{"ImportFeedList", "gtk-open", N_("_Import Feed List..."), NULL, N_("Imports an OPML feed list."), G_CALLBACK(on_menu_import)},
	{"ExportFeedList", "gtk-save-as", N_("_Export Feed List..."), NULL, N_("Exports the feed list as OPML."), G_CALLBACK(on_menu_export)},
	{"Quit",GTK_STOCK_QUIT, N_("_Quit"), "<control>Q", NULL, G_CALLBACK(on_menu_quit)},

	{"FeedMenu", NULL, N_("_Feed")},
	{"RemoveAllItems", "gtk-delete", N_("Remove _All Items"), NULL, N_("Removes all items of the currently selected feed."),
	 G_CALLBACK(on_remove_items_activate)},

	{"ItemMenu", NULL, N_("_Item")},
	{"NextUnreadItem", GTK_STOCK_GO_FORWARD, N_("_Next Unread Item"), "<control>N", N_("Jumps to the next unread item. If necessary selects the next feed with unread items."),
	 G_CALLBACK(on_next_unread_item_activate)},

	{"ViewMenu", NULL, N_("_View")},
	{"ZoomIn", "gtk-zoom-in", N_("_Increase Text Size"), "<control>plus", N_("Increases the text size of the item view."),
	 G_CALLBACK(on_menu_zoomin_selected)},
	{"ZoomOut", "gtk-zoom-out", N_("_Decrease Text Size"), "<control>minus", N_("Decreases the text size of the item view."),
	 G_CALLBACK(on_menu_zoomout_selected)},

	{"ToolsMenu", NULL, N_("_Tools")},
	{"ShowUpdateMonitor", NULL, N_("_Update Monitor"), NULL, N_("Show a list of all feeds currently in the update queue"),
	 G_CALLBACK(on_menu_show_update_monitor)},
	{"ShowScriptManager", NULL, N_("_Script Manager"), NULL, N_("Allows to configure and edit LUA hook scripts"),
	 G_CALLBACK(on_menu_show_script_manager)},
	{"ShowAttentionProfile", NULL, N_("Attention Profile"), NULL, N_("Presents statistics on your most read categories"),
	 G_CALLBACK(on_menu_attention_profile)},
	{"ShowPreferences", GTK_STOCK_PREFERENCES, N_("_Preferences"), NULL, N_("Edit Preferences."),
	 G_CALLBACK(on_prefbtn_clicked)},

	{"SearchMenu", NULL, N_("S_earch")},
	{"SearchFeeds", "gtk-find", N_("Search All Feeds..."), "<control>F", N_("Show the search dialog."), G_CALLBACK(on_searchbtn_clicked)},
	{"CreateEngineSearch", NULL, N_("Search With ...")},

	{"HelpMenu", NULL, N_("_Help")},
	{"ShowHelpContents", "gtk-help", N_("_Contents"), "F1", N_("View help for this application."), G_CALLBACK(on_topics_activate)},
	{"ShowHelpQuickReference", NULL, N_("_Quick Reference"), NULL, N_("View a list of all Liferea shortcuts."),
	 G_CALLBACK(on_quick_reference_activate)},
	{"ShowHelpFAQ", NULL, N_("_FAQ"), NULL, N_("View the FAQ for this application."), G_CALLBACK(on_faq_activate)},
	{"ShowAbout", "gtk-about", N_("_About"), NULL, N_("Shows an about dialog."), G_CALLBACK(on_about_activate)}
};

static const GtkRadioActionEntry ui_mainwindow_view_radio_entries[] = {
	{"NormalView", NULL, N_("_Normal View"), NULL, N_("Set view mode to mail client mode."),
	 0},
	{"WideView", NULL, N_("_Wide View"), NULL, N_("Set view mode to use three vertical panes."),
	 1},
	{"CombinedView", NULL, N_("_Combined View"), NULL, N_("Set view mode to two pane mode."),
	 2}
};

static GtkToggleActionEntry ui_mainwindow_feedlist_toggle_entries[] = {
	{"ReducedFeedList", NULL, N_("_Reduced Feed List"), NULL, N_("Hide feeds with no unread items."),
	G_CALLBACK(on_feedlist_reduced_activate)}
};

static const GtkActionEntry ui_mainwindow_add_action_entries[] = {
	{"NewSubscription", "gtk-add", N_("_New Subscription..."), NULL, N_("Adds a subscription to the feed list."),
	 G_CALLBACK(on_menu_feed_new)},
	{"NewFolder", NULL, N_("New _Folder..."), NULL, N_("Adds a folder to the feed list."), G_CALLBACK(on_menu_folder_new)},
	{"NewVFolder", NULL, N_("New S_earch Folder..."), NULL, N_("Adds a new search folder to the feed list."), G_CALLBACK(on_new_vfolder_activate)},
	{"NewSource", NULL, N_("New _Source..."), NULL, N_("Adds a new feed list source."), G_CALLBACK(on_new_plugin_activate)},
	{"NewNewsBin", NULL, N_("New _News Bin..."), NULL, N_("Adds a new news bin."), G_CALLBACK(on_new_newsbin_activate)}
};

static const GtkActionEntry ui_mainwindow_feed_action_entries[] = {
	{"MarkFeedAsRead", "gtk-apply", N_("_Mark Items Read"), "<control>R", N_("Marks all items of the selected feed list node / in the item list as read."), 
	 G_CALLBACK(on_menu_allread)},
	{"UpdateSelected", "gtk-refresh", N_("_Update"), NULL, N_("Updates the selected subscription or all subscriptions of the selected folder."),
	 G_CALLBACK(on_menu_update)}
};

static const GtkActionEntry ui_mainwindow_read_write_action_entries[] = {
	{"Properties", "gtk-properties", N_("_Properties"), NULL, N_("Opens the property dialog for the selected subscription."), G_CALLBACK(on_menu_properties)},
	{"DeleteSelected", "gtk-delete", N_("_Remove"), NULL, N_("Removes the selected subscription."), G_CALLBACK(on_menu_delete)}
};

static const GtkActionEntry ui_mainwindow_item_action_entries[] = {
	{"ToggleItemReadStatus", "gtk-apply", N_("Toggle _Read Status"), "<control>U", N_("Toggles the read status of the selected item."),
	 G_CALLBACK(on_toggle_unread_status)},
	{"ToggleItemFlag", NULL, N_("Toggle Item _Flag"), "<control>T", N_("Toggles the flag status of the selected item."),
	 G_CALLBACK(on_toggle_item_flag)},
	{"RemoveSelectedItem", "gtk-delete", N_("R_emove"), NULL, N_("Removes the selected item."),
	 G_CALLBACK(on_remove_item_activate)},
	{"LaunchItemInBrowser", NULL, N_("_Launch In Browser"), NULL, N_("Launches the item's link in the configured browser."),
	 G_CALLBACK(on_popup_launchitem_selected)}
};

static const GtkToggleActionEntry ui_mainwindow_action_toggle_entries[] = {
	{"ToggleOfflineMode", NULL, N_("_Work Offline"), NULL, N_("This option allows you to disable subscription updating."),
	 G_CALLBACK(on_work_offline_activate)}
};

static const char *ui_mainwindow_ui_desc =
"<ui>"
"  <menubar name='MainwindowMenubar'>"
"    <menu action='SubscriptionsMenu'>"
"      <menuitem action='UpdateAll'/>"
"      <menuitem action='MarkAllFeedsAsRead'/>"
"      <separator/>"
"      <menuitem action='NewSubscription'/>"
"      <menuitem action='NewFolder'/>"
"      <menuitem action='NewVFolder'/>"
"      <menuitem action='NewSource'/>"
"      <menuitem action='NewNewsBin'/>"
"      <separator/>"
"      <menuitem action='ImportFeedList'/>"
"      <menuitem action='ExportFeedList'/>"
"      <separator/>"
"      <menuitem action='ToggleOfflineMode'/>"
"      <separator/>"
"      <menuitem action='Quit'/>"
"    </menu>"
"    <menu action='FeedMenu'>"
"      <menuitem action='UpdateSelected'/>"
"      <menuitem action='MarkFeedAsRead'/>"
"      <separator/>"
"      <menuitem action='RemoveAllItems'/>"
"      <menuitem action='DeleteSelected'/>"
"      <separator/>"
"      <menuitem action='Properties'/>"
"    </menu>"
"    <menu action='ItemMenu'>"
"      <menuitem action='NextUnreadItem'/>"
"      <separator/>"
"      <menuitem action='ToggleItemReadStatus'/>"
"      <menuitem action='ToggleItemFlag'/>"
"      <menuitem action='RemoveSelectedItem'/>"
"      <separator/>"
"      <menuitem action='LaunchItemInBrowser'/>"
"    </menu>"
"    <menu action='ViewMenu'>"
"      <menuitem action='ZoomIn'/>"
"      <menuitem action='ZoomOut'/>"
"      <separator/>"
"      <menuitem action='NormalView'/>"
"      <menuitem action='WideView'/>"
"      <menuitem action='CombinedView'/>"
"      <separator/>"
"      <menuitem action='ReducedFeedList'/>"
"    </menu>"
"    <menu action='ToolsMenu'>"
"      <menuitem action='ShowUpdateMonitor'/>"
"      <menuitem action='ShowScriptManager'/>"
"      <menuitem action='ShowAttentionProfile'/>"
"      <menuitem action='ShowPreferences'/>"
"    </menu>"
"    <menu action='SearchMenu'>"
"      <menuitem action='SearchFeeds'/>"
"      <menu action='CreateEngineSearch'/>"
"    </menu>"
"    <menu action='HelpMenu'>"
"      <menuitem action='ShowHelpContents'/>"
"      <menuitem action='ShowHelpQuickReference'/>"
"      <menuitem action='ShowHelpFAQ'/>"
"      <separator/>"
"      <menuitem action='ShowAbout'/>"
"    </menu>"
"  </menubar>"
"  <toolbar action='maintoolbar'>"
"    <separator/>"
"    <toolitem name='newFeedButton' action='NewSubscription'/>"
"    <toolitem name='nextUnreadButton' action='NextUnreadItem'/>"
"    <toolitem name='MarkAsReadButton' action='MarkFeedAsRead'/>"
"    <toolitem name='UpdateAllButton' action='UpdateAll'/>"
"    <toolitem name='SearchButton' action='SearchFeeds'/>"
"    <separator/>"
"  </toolbar>"
"</ui>";

static GdkPixbuf *
liferea_shell_get_theme_icon (GtkIconTheme *icon_theme, const gchar *name, gint size)
{
	GError *error = NULL;
	GdkPixbuf *pixbuf;

	pixbuf = gtk_icon_theme_load_icon (icon_theme,
	                                   name, /* icon name */
	                                   size, /* size */
	                                   0,  /* flags */
	                                   &error);
	if (!pixbuf) {
		debug1(DEBUG_GUI, "Couldn't load icon: %s", error->message);
		g_error_free (error);
	}
	return pixbuf;
}

static void
liferea_shell_restore_state (void)
{
	gchar *toolbar_style, *accels_file;
	gint last_vpane_pos, last_hpane_pos, last_wpane_pos;
	
	debug0 (DEBUG_GUI, "Setting toolbar style");
	
	toolbar_style = conf_get_toolbar_style ();	
	liferea_shell_set_toolbar_style (toolbar_style);
	g_free (toolbar_style);

	debug0 (DEBUG_GUI, "Loading accelerators");
	
	accels_file = g_build_filename (common_get_cache_path(), "accels", NULL);
	gtk_accel_map_load (accels_file);
	g_free (accels_file);	

	debug0 (DEBUG_GUI, "Restoring window position");
	
	liferea_shell_restore_position ();

	debug0 (DEBUG_GUI, "Loading pane proportions");
		
	conf_get_int_value (LAST_VPANE_POS, &last_vpane_pos);
	if (last_vpane_pos)
		gtk_paned_set_position (GTK_PANED (liferea_shell_lookup ("leftpane")), last_vpane_pos);
	conf_get_int_value (LAST_HPANE_POS, &last_hpane_pos);
	if (last_hpane_pos)
		gtk_paned_set_position (GTK_PANED (liferea_shell_lookup ("normalViewPane")), last_hpane_pos);
	conf_get_int_value (LAST_WPANE_POS, &last_wpane_pos);
	if (last_wpane_pos)
		gtk_paned_set_position (GTK_PANED (liferea_shell_lookup ("wideViewPane")), last_wpane_pos);
}

void
liferea_shell_create (int initialState)
{
	GtkUIManager	*ui_manager;
	GtkAccelGroup	*accel_group;
	GError		*error = NULL;	
	GtkWidget	*widget;
	int		i;
	GString		*buffer;
	GtkIconTheme	*icon_theme;
	gboolean	show_tray_icon, toggle;
	
	debug_enter ("liferea_shell_create");

	/* 1.) object setup */

	g_object_new (LIFEREA_SHELL_TYPE, NULL);

	shell->priv->window = GTK_WINDOW (liferea_shell_lookup ("mainwindow"));
	
	/* 2.) menu creation */
	
	debug0 (DEBUG_GUI, "Setting up menues");

	/* Prepare some toggle button states */	
	conf_get_bool_value (REDUCED_FEEDLIST, &toggle);
	ui_mainwindow_feedlist_toggle_entries[0].is_active = toggle;

	ui_manager = gtk_ui_manager_new ();

	shell->priv->generalActions = gtk_action_group_new ("GeneralActions");
	gtk_action_group_set_translation_domain (shell->priv->generalActions, PACKAGE);
	gtk_action_group_add_actions (shell->priv->generalActions, ui_mainwindow_action_entries, G_N_ELEMENTS (ui_mainwindow_action_entries), shell->priv);
	gtk_action_group_add_toggle_actions (shell->priv->generalActions, ui_mainwindow_action_toggle_entries, G_N_ELEMENTS (ui_mainwindow_action_toggle_entries), shell->priv);
	gtk_action_group_add_radio_actions (shell->priv->generalActions, ui_mainwindow_view_radio_entries, G_N_ELEMENTS (ui_mainwindow_view_radio_entries), itemlist_get_view_mode (), (GCallback)on_view_activate, (gpointer)TRUE);
	gtk_action_group_add_toggle_actions (shell->priv->generalActions, ui_mainwindow_feedlist_toggle_entries, G_N_ELEMENTS (ui_mainwindow_feedlist_toggle_entries), shell->priv);
	gtk_ui_manager_insert_action_group (ui_manager, shell->priv->generalActions, 0);

	shell->priv->addActions = gtk_action_group_new ("AddActions");
	gtk_action_group_set_translation_domain (shell->priv->addActions, PACKAGE);
	gtk_action_group_add_actions (shell->priv->addActions, ui_mainwindow_add_action_entries, G_N_ELEMENTS (ui_mainwindow_add_action_entries), shell->priv);
	gtk_ui_manager_insert_action_group (ui_manager, shell->priv->addActions, 0);

	shell->priv->feedActions = gtk_action_group_new ("FeedActions");
	gtk_action_group_set_translation_domain (shell->priv->feedActions, PACKAGE);
	gtk_action_group_add_actions (shell->priv->feedActions, ui_mainwindow_feed_action_entries, G_N_ELEMENTS (ui_mainwindow_feed_action_entries), shell->priv);
	gtk_ui_manager_insert_action_group (ui_manager, shell->priv->feedActions, 0);

	shell->priv->readWriteActions = gtk_action_group_new("ReadWriteActions");
	gtk_action_group_set_translation_domain (shell->priv->readWriteActions, PACKAGE);
	gtk_action_group_add_actions (shell->priv->readWriteActions, ui_mainwindow_read_write_action_entries, G_N_ELEMENTS (ui_mainwindow_read_write_action_entries), shell->priv);
	gtk_ui_manager_insert_action_group (ui_manager, shell->priv->readWriteActions, 0);

	shell->priv->itemActions = gtk_action_group_new ("ItemActions");
	gtk_action_group_set_translation_domain (shell->priv->itemActions, PACKAGE);
	gtk_action_group_add_actions (shell->priv->itemActions, ui_mainwindow_item_action_entries, G_N_ELEMENTS (ui_mainwindow_item_action_entries), shell->priv);
	gtk_ui_manager_insert_action_group (ui_manager, shell->priv->itemActions, 0);

	accel_group = gtk_ui_manager_get_accel_group (ui_manager);
	gtk_window_add_accel_group (GTK_WINDOW (shell->priv->window), accel_group);
	g_object_unref (accel_group);

	g_signal_connect (gtk_accel_map_get (), "changed", G_CALLBACK (on_accel_change), NULL);

	if (!gtk_ui_manager_add_ui_from_string (ui_manager, ui_mainwindow_ui_desc, -1, &error))
		g_error ("building menus failed: %s", error->message);

	ui_search_engines_setup_menu (ui_manager);

	shell->priv->menubar = gtk_ui_manager_get_widget (ui_manager, "/MainwindowMenubar");
	shell->priv->toolbar = gtk_ui_manager_get_widget (ui_manager, "/maintoolbar");

	/* what a pain, why is there no markup for this option? */
	g_object_set (G_OBJECT (gtk_ui_manager_get_widget (ui_manager, "/maintoolbar/newFeedButton")), "is_important", TRUE, NULL);
	g_object_set (G_OBJECT (gtk_ui_manager_get_widget (ui_manager, "/maintoolbar/nextUnreadButton")), "is_important", TRUE, NULL);
	g_object_set (G_OBJECT (gtk_ui_manager_get_widget (ui_manager, "/maintoolbar/MarkAsReadButton")), "is_important", TRUE, NULL);
	g_object_set (G_OBJECT (gtk_ui_manager_get_widget (ui_manager, "/maintoolbar/UpdateAllButton")), "is_important", TRUE, NULL);
	g_object_set (G_OBJECT (gtk_ui_manager_get_widget (ui_manager, "/maintoolbar/SearchButton")), "is_important", TRUE, NULL);

	/* 2.) setup containers */
	
	debug0 (DEBUG_GUI, "Setting up widget containers");

	gtk_box_pack_start (GTK_BOX (liferea_shell_lookup ("vbox1")), shell->priv->toolbar, FALSE, FALSE, 0);
	gtk_box_reorder_child (GTK_BOX (liferea_shell_lookup ("vbox1")), shell->priv->toolbar, 0);
	gtk_box_pack_start (GTK_BOX (liferea_shell_lookup ("vbox1")), shell->priv->menubar, FALSE, FALSE, 0);
	gtk_box_reorder_child (GTK_BOX (liferea_shell_lookup ("vbox1")), shell->priv->menubar, 0);

	gtk_widget_show_all(GTK_WIDGET(shell->priv->toolbar));

	g_signal_connect ((gpointer) liferea_shell_lookup ("itemtabs"), "key_press_event",
	                  G_CALLBACK (on_key_press_event_null_cb), NULL);

	g_signal_connect ((gpointer) liferea_shell_lookup ("itemtabs"), "key_release_event",
	                  G_CALLBACK (on_key_press_event_null_cb), NULL);
	
	g_signal_connect ((gpointer) liferea_shell_lookup ("itemtabs"), "scroll_event",
	                  G_CALLBACK (on_notebook_scroll_event_null_cb), NULL);
	
	g_signal_connect (G_OBJECT (shell->priv->window), "delete_event", G_CALLBACK(on_close), shell->priv);
	g_signal_connect (G_OBJECT (shell->priv->window), "window_state_event", G_CALLBACK(on_window_state_event), shell->priv);
	g_signal_connect (G_OBJECT (shell->priv->window), "key_press_event", G_CALLBACK(on_key_press_event), shell->priv);
	
	/* 3.) setup status bar */
	
	debug0 (DEBUG_GUI, "Setting up status bar");
	
	shell->priv->statusbar = GTK_STATUSBAR (liferea_shell_lookup ("statusbar"));
	shell->priv->statusbarLocked = FALSE;
	shell->priv->statusbarLockTimer = 0;
	shell->priv->statusbar_feedsinfo = gtk_label_new("");
	gtk_widget_show(shell->priv->statusbar_feedsinfo);
	gtk_box_pack_start (GTK_BOX (shell->priv->statusbar), shell->priv->statusbar_feedsinfo, FALSE, FALSE, 5);
	
	/* 4.) setup tabs */
	
	debug0 (DEBUG_GUI, "Setting up tabbed browsing");	
	shell->priv->tabs = browser_tabs_create (GTK_NOTEBOOK (liferea_shell_lookup ("browsertabs")));
	
	/* 5.) setup feed list */

	debug0 (DEBUG_GUI, "Setting up feed list");
	shell->priv->feedlistView = GTK_TREE_VIEW (liferea_shell_lookup ("feedlist"));
	feed_list_view_init (shell->priv->feedlistView);

	/* 6.) setup menu sensivity */
	
	debug0 (DEBUG_GUI, "Initialising menues");
		
	/* On start, no item or feed is selected, so Item menu should be insensitive: */
	liferea_shell_update_item_menu (FALSE);

	/* necessary to prevent selection signals when filling the feed list
	   and setting the 2/3 pane mode view */
	gtk_widget_set_sensitive (GTK_WIDGET (shell->priv->feedlistView), FALSE);
	
	/* 7.) setup item view */
	
	debug0 (DEBUG_GUI, "Setting up item view");

	shell->priv->itemview = itemview_create (GTK_WIDGET (shell->priv->window));
	
	/* 8.) load icons as required */
	
	debug0 (DEBUG_GUI, "Loading icons");
	
	ui_common_add_pixmap_directory (PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps");
	
	/* first try to load icons from theme */
	static const gchar *iconThemeNames[] = {
		NULL,			/* ICON_READ */
		NULL,			/* ICON_UNREAD */
		"emblem-important",	/* ICON_FLAG */
		NULL,			/* ICON_AVAILABLE */
		NULL,			/* ICON_AVAILABLE_OFFLINE */
		NULL,			/* ICON_UNAVAILABLE */
		NULL,			/* ICON_DEFAULT */
		"folder",		/* ICON_FOLDER */
		"folder-saved-search",	/* ICON_VFOLDER */
		NULL,			/* ICON_NEWSBIN */
		NULL,			/* ICON_EMPTY */
		NULL,			/* ICON_EMPTY_OFFLINE */
		"gtk-connect",		/* ICON_ONLINE */
		"gtk-disconnect",	/* ICON_OFFLINE */
		NULL,			/* ICON_UPDATED */
		"mail-attachment",	/* ICON_ENCLOSURE */
		NULL
	};

	icon_theme = gtk_icon_theme_get_default ();
	for (i = 0; i < MAX_ICONS; i++)
		if (iconThemeNames[i])
			icons[i] = liferea_shell_get_theme_icon (icon_theme, iconThemeNames[i], 16);

	/* and then load own default icons */
	static const gchar *iconNames[] = {
		"read.xpm",		/* ICON_READ */
		"unread.png",		/* ICON_UNREAD */
		"flag.png",		/* ICON_FLAG */
		"available.png",	/* ICON_AVAILABLE */
		"available_offline.png",	/* ICON_AVAILABLE_OFFLINE */
		NULL,			/* ICON_UNAVAILABLE */
		"default.png",		/* ICON_DEFAULT */
		"directory.png",	/* ICON_FOLDER */
		"vfolder.png",		/* ICON_VFOLDER */
		"newsbin.png",		/* ICON_NEWSBIN */
		"empty.png",		/* ICON_EMPTY */
		"empty_offline.png",	/* ICON_EMPTY_OFFLINE */
		"online.png",		/* ICON_ONLINE */
		"offline.png",		/* ICON_OFFLINE */
		"edit.png",		/* ICON_UPDATED */
		"attachment.png",	/* ICON_ENCLOSURE */
		NULL
	};

	for (i = 0; i < MAX_ICONS; i++)
		if (!icons[i])
			icons[i] = ui_common_create_pixbuf (iconNames[i]);

	/* set up icons that are build from stock */
	widget = gtk_button_new ();
	icons[ICON_UNAVAILABLE] = gtk_widget_render_icon (widget, GTK_STOCK_DIALOG_ERROR, GTK_ICON_SIZE_MENU, "");
	gtk_widget_destroy (widget);
	
	liferea_shell_update_toolbar ();
	
	liferea_shell_setup_URL_receiver (GTK_WIDGET (shell->priv->window));	/* setup URL dropping support */

	shell->priv->feedlist = feedlist_create ();

	conf_get_bool_value (SHOW_TRAY_ICON, &show_tray_icon);

	ui_tray_enable (show_tray_icon);		/* init tray icon */

	liferea_shell_restore_state ();
	
	if (initialState == MAINWINDOW_ICONIFIED || 
	    (initialState == MAINWINDOW_HIDDEN && ui_tray_get_count () == 0)) {
		gtk_window_iconify (shell->priv->window);
		gtk_widget_show (GTK_WIDGET (shell->priv->window));
	} else if (initialState == MAINWINDOW_SHOWN) {
		gtk_widget_show (GTK_WIDGET (shell->priv->window));
	} else {
		/* Needed so that the window structure can be
		   accessed... otherwise will GTK warning when window is
		   shown by clicking on notification icon. */
		// gtk_widget_realize (GTK_WIDGET (shell->priv->window));
		// Does not work with gtkmozembed...
		
		gtk_widget_show (GTK_WIDGET (shell->priv->window));
		gtk_widget_hide (GTK_WIDGET (shell->priv->window));
	}

	gtk_widget_set_sensitive (GTK_WIDGET (shell->priv->feedlistView), TRUE);
	
	/* 9. Create welcome text */

	/* force two pane mode */
	/*   For some reason, this causes the first item to be selected and then
	     unselected... strange. */
	feed_list_view_select (NULL);
	
	itemview_set_layout (NODE_VIEW_MODE_COMBINED);
	
	buffer = g_string_new (NULL);
	htmlview_start_output (buffer, NULL, TRUE, FALSE);
	g_string_append (buffer,   "<div style=\"padding:8px\">"
				   "<table class=\"headmeta\" style=\"border:solid 1px #aaa;font-size:120%\" border=\"0\" cellspacing=\"0\" cellpadding=\"5px\"><tr><td>"
				   // Display application icon
				   "<img src=\""
				   PACKAGE_DATA_DIR G_DIR_SEPARATOR_S "icons" G_DIR_SEPARATOR_S
				   "hicolor" G_DIR_SEPARATOR_S "48x48" G_DIR_SEPARATOR_S "apps"
				   G_DIR_SEPARATOR_S "liferea.png\" />"
				   "</td><td><h3>");
	g_string_append (buffer,   _("Liferea - Linux Feed Reader"));
	g_string_append (buffer,   "</h3></td></tr><tr><td colspan=\"2\">");
	g_string_append (buffer,   _("<p>Welcome to <b>Liferea</b>, a desktop news aggregator for online news "
				   "feeds.</p>"
				   "<p>The left pane contains the list of your subscriptions. To add a "
				   "subscription select Subscription -&gt; New Subscription. To browse the headlines "
				   "of a feed select it in the feed list and the headlines will be loaded "
				   "into the right pane.</p>"));
	g_string_append (buffer,   "</td></tr></table>");

	g_string_append (buffer,   "</div>");

	htmlview_finish_output (buffer);
	itemview_display_info (buffer->str);
	g_string_free (buffer, TRUE);
	
	/* 10. Connect network monitoring and set icon*/
	
	g_signal_connect (network_monitor_get (), "online-status-changed",
	                  G_CALLBACK (liferea_shell_online_status_changed), shell);

	liferea_shell_set_online_icon (network_monitor_is_online ());

	debug_exit ("liferea_shell_create");
}

void
liferea_shell_destroy (void)
{
	feed_list_view_select (NULL);
	liferea_shell_save_position ();
	ui_tray_enable (FALSE);
	g_object_unref (shell->priv->tabs);
	g_object_unref (shell->priv->feedlist);
	g_object_unref (shell->priv->itemview);

	gtk_widget_destroy (GTK_WIDGET (shell->priv->window));

	g_object_unref (shell);
}

void
liferea_shell_present (void)
{
	GtkWidget *mainwindow = GTK_WIDGET (shell->priv->window);
	
	if ((gdk_window_get_state (gtk_widget_get_window (mainwindow)) & GDK_WINDOW_STATE_ICONIFIED) || !gtk_widget_get_visible (mainwindow))
		liferea_shell_restore_position ();

	gtk_window_present (shell->priv->window);
}

void
liferea_shell_toggle_visibility (void)
{
	GtkWidget *mainwindow = GTK_WIDGET (shell->priv->window);
	
	if ((gdk_window_get_state (gtk_widget_get_window (mainwindow)) & GDK_WINDOW_STATE_ICONIFIED) ||
	    !gtk_widget_get_visible (mainwindow)) {
		liferea_shell_restore_position ();
		gtk_window_present (shell->priv->window);
	} else {
		liferea_shell_save_position ();
		gtk_widget_hide (mainwindow);
	}
}

GtkWidget *
liferea_shell_get_window (void)
{
	return GTK_WIDGET (shell->priv->window);
}

void liferea_shell_set_view_mode (nodeViewType newMode)
{
	GtkRadioAction       *action;

	action = GTK_RADIO_ACTION (gtk_action_group_get_action (shell->priv->generalActions, "NormalView"));
	gtk_radio_action_set_current_value (action, newMode);
}
