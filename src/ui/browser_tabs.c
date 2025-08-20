/*
 * @file browser_tabs.c  internal browsing using multiple tabs
 *
 * Copyright (C) 2004-2025 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2006 Nathan Conrad <conrad@bungled.net>
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

#include "ui/browser_tabs.h"

#include <string.h>
#include <gdk/gdkkeysyms.h>

#include "common.h"
#include "ui/liferea_shell.h"

enum {
	PROP_NONE,
	PROP_NOTEBOOK
};

struct _BrowserTabs {
	GObject		parentInstance;

	GtkNotebook		*notebook;
};

G_DEFINE_TYPE (BrowserTabs, browser_tabs, G_TYPE_OBJECT);

// singleton
static BrowserTabs *tabs = NULL;

static void
browser_tabs_class_init (BrowserTabsClass *klass)
{
}

static void
browser_tabs_init (BrowserTabs *bt)
{
}

BrowserTabs *
browser_tabs_create (GtkNotebook *notebook)
{
	g_assert (notebook != NULL);

	tabs = g_object_new (BROWSER_TABS_TYPE, NULL);
	tabs->notebook = notebook;
	gtk_notebook_set_show_tabs (notebook, FALSE);

	return tabs;
}

/* HTML view signal handlers */

static const gchar *
remove_string_prefix (const gchar *string, const gchar *prefix)
{
	int	len;

	len = strlen (prefix);

	if (!strncmp (string, prefix, len))
		string += len;

	return string;
}

static const gchar *
create_label_text (const gchar *title)
{
	const gchar 	*tmp;

	tmp = (title && *title) ? title : _("Untitled");

	tmp = remove_string_prefix (tmp, "http://");
	tmp = remove_string_prefix (tmp, "https://");

	return tmp;
}

static void
on_htmlview_title_changed (gpointer obj, gchar *title, gpointer user_data)
{
	gtk_label_set_text (GTK_LABEL (user_data), create_label_text (title));
}

/* Find tab by child widget and close it */
static void
browser_tabs_close_tab (GtkWidget *widget)
{
	gtk_notebook_remove_page (tabs->notebook, gtk_notebook_page_num (tabs->notebook, widget));

	/* check if all tabs are closed, in this case hide tab header to save vertical space */
	if (1 == gtk_notebook_get_n_pages (tabs->notebook))
		gtk_notebook_set_show_tabs (tabs->notebook, FALSE);
}

static gboolean
on_tab_key_press (GtkEventControllerKey *controller,
		guint keyval,
		guint keycode,
		GdkModifierType state,
		gpointer user_data)
{
	guint modifiers;

	modifiers = gtk_accelerator_get_default_mod_mask ();
	if ((keyval == GDK_KEY_w) &&
	    ((state & modifiers) == GDK_CONTROL_MASK)) {
		browser_tabs_close_tab (user_data);
		return TRUE;
	}

	return FALSE;
}

static void
on_browser_tab_close (gpointer object, gpointer user_data)
{
	browser_tabs_close_tab(user_data);
}

static void
on_htmlview_status_message (gpointer obj, gchar *url)
{
	liferea_shell_set_important_status_bar ("%s", url);
}

LifereaBrowser *
browser_tabs_add_new (const gchar *url, const gchar *title, gboolean activate)
{
	GtkWidget 	*close_button, *labelBox, *widget, *label;
	GtkEventController *controller;
	LifereaBrowser	*htmlview;
	int		i;

	htmlview = liferea_browser_new (TRUE /* internal browsing */);
	widget = liferea_browser_get_widget (htmlview);
	controller = gtk_event_controller_key_new ();

	/* create tab widgets */

	label = gtk_label_new (create_label_text (title));
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_label_set_width_chars (GTK_LABEL (label), 17);

	labelBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_append (GTK_BOX (labelBox), label);

	close_button = gtk_button_new ();
	gtk_button_set_icon_name (GTK_BUTTON (close_button), "window-close-symbolic");
	gtk_button_set_has_frame (GTK_BUTTON (close_button), FALSE);
	gtk_widget_add_css_class (close_button, "flat");
	gtk_widget_set_size_request (close_button, 16, 16);
	gtk_box_append (GTK_BOX (labelBox), close_button);

	/* connect stuff */

	g_signal_connect (htmlview, "title-changed", G_CALLBACK (on_htmlview_title_changed), label);
	g_signal_connect (htmlview, "statusbar-changed", G_CALLBACK (on_htmlview_status_message), NULL);

	g_signal_connect ((gpointer)close_button, "clicked", G_CALLBACK (on_browser_tab_close), NULL);
	g_object_set_data_full (G_OBJECT (close_button), "controller", controller, (GDestroyNotify)g_object_unref);
	g_object_set_data_full (G_OBJECT (close_button), "htmlview", htmlview, (GDestroyNotify)g_object_unref);

	i = gtk_notebook_append_page (tabs->notebook, widget, labelBox);
	g_signal_connect (controller, "key-pressed", G_CALLBACK (on_tab_key_press), widget);
	gtk_widget_add_controller (widget, controller);
	gtk_notebook_set_show_tabs (tabs->notebook, TRUE);
	gtk_notebook_set_tab_reorderable (tabs->notebook, widget, TRUE);

	if (activate && (i != -1))
		gtk_notebook_set_current_page (tabs->notebook, i);

	if (url)
		liferea_browser_launch_URL_internal (htmlview, (gchar *)url);

	return htmlview;
}

void
browser_tabs_show_headlines (void)
{
	gtk_notebook_set_current_page (tabs->notebook, 0);
}

LifereaBrowser *
browser_tabs_get_active_htmlview (void)
{
	gint current = gtk_notebook_get_current_page (tabs->notebook);
	GtkWidget *child = gtk_notebook_get_nth_page (tabs->notebook, current);
	
	if (0 != current)
		return LIFEREA_BROWSER (g_object_get_data (G_OBJECT (child), "htmlview"));
	
	gpointer htmlview;
	g_object_get (G_OBJECT (liferea_shell_get_instance()), "htmlview", &htmlview, NULL);
	return LIFEREA_BROWSER (htmlview);
}