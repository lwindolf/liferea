/**
 * @file ui_tabs.c browser tabs
 *
 * Copyright (C) 2004-2007 Lars Lindner <lars.lindner@gmail.com>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <gdk/gdkkeysyms.h>

#include "common.h"
#include "itemlist.h"
#include "ui/ui_htmlview.h"
#include "ui/ui_itemlist.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_shell.h"
#include "ui/ui_tabs.h"

extern GtkWidget	*mainwindow;

/** structure holding all URLs visited in a tab */
typedef struct tabHistory {
	GList		*locations;	/** list of all visited URLs */
	GList		*current;	/** pointer into locations */
} tabHistory;

/** structure holding all states of a tab */
typedef struct tabInfo {
	GtkWidget	*widget;	/** the tab widget */
	GtkWidget	*forward;	/** the forward button */
	GtkWidget	*back;		/** the back button */
	LifereaHtmlView	*htmlview;	/** the tabs HTML view widget */
	GtkWidget	*urlentry;	/** the tabs URL entry widget */
	tabHistory	*history;	/** the tabs history */
} tabInfo;

/* tab history handling */

static tabHistory *
ui_tabs_history_new (void)
{	
	return (tabHistory *)g_new0 (tabHistory, 1);
}

static void
ui_tabs_history_free (tabHistory *history)
{
	GList	*iter;

	g_return_if_fail (NULL != history);
	iter = history->locations;
	while (iter) {
		g_free (iter->data);
		iter = g_list_next (iter);
	}
	g_list_free (history->locations);
	g_free (history);
}

static gchar *
ui_tabs_history_forward (tabInfo *tab)
{
	GList	*url = tab->history->current;

	url = g_list_next (url);
	tab->history->current = url;

	gtk_widget_set_sensitive (tab->forward, (NULL != g_list_next (url)));
	gtk_widget_set_sensitive (tab->back, TRUE);

	return url->data;
}

static gchar *
ui_tabs_history_back (tabInfo *tab)
{
	GList	*url = tab->history->current;

	url = g_list_previous (url);
	tab->history->current = url;

	gtk_widget_set_sensitive (tab->back, (NULL != g_list_previous (url)));
	gtk_widget_set_sensitive (tab->forward, TRUE);

	return url->data;
}

static void
ui_tabs_history_add_location (tabInfo *tab, const gchar *url)
{
	GList 	*iter;

	/* Do not add the same URL twice... */
	if (tab->history->current &&
	   g_str_equal (tab->history->current->data, url))
		return;

	/* If current URL is not at the end of the list,
	   truncate the rest of the list */
	if (tab->history->locations) {
		while (1) {
			iter = g_list_last (tab->history->locations);
			if (!iter)
				break;
			if (iter == tab->history->current)
				break;
			g_free (iter->data);
			tab->history->locations = g_list_remove (tab->history->locations, iter->data);
		}
	}

	gtk_widget_set_sensitive (tab->back, (NULL != tab->history->locations));
	gtk_widget_set_sensitive (tab->forward, FALSE);

	tab->history->locations = g_list_append (tab->history->locations, g_strdup (url));
	tab->history->current = g_list_last (tab->history->locations);
}

/* tab handling */

static gboolean
on_tab_url_entry_activate (GtkWidget *widget, gpointer user_data)
{
	tabInfo	*tab = (tabInfo *)user_data;
	gchar	*url;

	url = (gchar *)gtk_entry_get_text (GTK_ENTRY (widget));
	liferea_htmlview_launch_URL (tab->htmlview, url, UI_HTMLVIEW_LAUNCH_INTERNAL);

	return TRUE;
}

static void
on_tab_close_clicked (GtkWidget *widget, gpointer user_data)
{
	ui_tabs_close_tab (widget);
}

static void
on_tab_switched (GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, gpointer user_data)
{
	/* needed because switching does sometimes returns to the tree 
	   view with a very disturbing horizontal scrolling state */
	if (0 == page_num)
		ui_itemlist_scroll_left ();
}

static void
on_tab_history_back (GtkWidget *widget, gpointer user_data)
{
	tabInfo		*tab = (tabInfo *)user_data;
	gchar		*url;

	url = ui_tabs_history_back (tab),
	liferea_htmlview_launch_URL (tab->htmlview, url, UI_HTMLVIEW_LAUNCH_INTERNAL);
	gtk_entry_set_text (GTK_ENTRY (tab->urlentry), url);
}

static void
on_tab_history_forward (GtkWidget *widget, gpointer user_data)
{
	tabInfo		*tab = (tabInfo *)user_data;
	gchar		*url;

	url = ui_tabs_history_forward (tab),
	liferea_htmlview_launch_URL (tab->htmlview, url, UI_HTMLVIEW_LAUNCH_INTERNAL);
	gtk_entry_set_text (GTK_ENTRY (tab->urlentry), url);
}

static gboolean
on_tab_key_press (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	guint modifiers;

	modifiers = gtk_accelerator_get_default_mod_mask ();
	if ((event->keyval == GDK_w) && ((event->state & modifiers) == GDK_CONTROL_MASK)) {
		ui_tabs_close_tab ((GtkWidget *)data);
		return TRUE;
	}
	
	return FALSE;
}

void
ui_tabs_init (void)
{
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK(liferea_shell_lookup ("browsertabs")), FALSE);
	g_signal_connect ((gpointer)liferea_shell_lookup ("browsertabs"), "switch-page", G_CALLBACK (on_tab_switched), NULL);
}

static gchar *
ui_tabs_condense_text (const gchar* title)
{
	int i;
	gchar *tmp, *tmp2;

	/* make nice short title */
	if (title) {
		if (!strncmp (title, "http://",7))
			title += strlen ("http://");
		tmp2 = tmp = g_strstrip (g_strdup (title));
		if (*tmp == '\0') {
			g_free (tmp);
			tmp = tmp2 = g_strdup (_("New tab"));
		}
	} else {
		tmp2 = tmp = g_strdup (_("New tab"));
	}
	
	if (g_utf8_strlen (tmp, -1) > 20) {
		for (i = 0; i < 20; i++)
			tmp2 = g_utf8_find_next_char (tmp2, NULL);
		*tmp2 = 0;
		tmp2 = g_strdup_printf ("%s...", tmp);
		g_free (tmp);
		tmp = tmp2;
	}
	return tmp;
}

LifereaHtmlView *
ui_tabs_new (const gchar *url, const gchar *title, gboolean activate)
{
	GtkWidget 	*widget, *label, *vbox, *toolbar, *htmlframe, *image;
	LifereaHtmlView	*htmlview;
	tabInfo		*tab;
	gchar		*tmp;
	int		i;

	tmp = ui_tabs_condense_text (title != NULL ? title : _("New tab"));
	label = gtk_label_new (tmp);
	gtk_widget_show (label);
	g_free (tmp);
	
	/* create widgets */
	vbox = gtk_vbox_new (FALSE, 0);
	htmlview = liferea_htmlview_new (TRUE);
	toolbar = gtk_hbox_new (FALSE, 6);

	tab = g_new0 (tabInfo, 1);
	tab->widget = vbox;
	tab->htmlview = htmlview;
	tab->history = ui_tabs_history_new ();
	
	g_object_set_data (G_OBJECT (vbox), "tabInfo", tab);

	widget = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_NONE);
	image = gtk_image_new_from_stock ("gtk-go-back", GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image);
	gtk_container_add (GTK_CONTAINER (widget), image);
	gtk_box_pack_start (GTK_BOX (toolbar), widget, FALSE, FALSE, 0);
	g_signal_connect ((gpointer)widget, "clicked", G_CALLBACK (on_tab_history_back), (gpointer)tab);
	gtk_widget_set_sensitive(widget, FALSE);
	tab->back = widget;

	widget = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON(widget), GTK_RELIEF_NONE);
	image = gtk_image_new_from_stock ("gtk-go-forward", GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image);
	gtk_container_add (GTK_CONTAINER (widget), image);
	gtk_box_pack_start (GTK_BOX (toolbar), widget, FALSE, FALSE, 0);
	g_signal_connect ((gpointer)widget, "clicked", G_CALLBACK (on_tab_history_forward), (gpointer)tab);
	gtk_widget_set_sensitive (widget, FALSE);
	tab->forward = widget;

	widget = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (widget), url ? url : "");
	g_object_set_data (G_OBJECT (vbox), "url_entry", widget);
	
	gtk_box_pack_start (GTK_BOX (toolbar), widget, TRUE, TRUE, 0);
	g_signal_connect ((gpointer)widget, "activate", G_CALLBACK (on_tab_url_entry_activate), (gpointer)tab);
	tab->urlentry = widget;

	widget = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON(widget), GTK_RELIEF_NONE);
	image = gtk_image_new_from_stock ("gtk-close", GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image);
	gtk_container_add (GTK_CONTAINER (widget), image);
	gtk_box_pack_end (GTK_BOX (toolbar), widget, FALSE, FALSE, 0);
	g_signal_connect ((gpointer)widget, "clicked", G_CALLBACK (on_tab_close_clicked), (gpointer)tab);
	
	htmlframe = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (htmlframe), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (htmlframe), liferea_htmlview_get_widget (htmlview));
	
	gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (vbox), htmlframe, TRUE, TRUE, 0);
	gtk_widget_show_all (vbox);
	
	i = gtk_notebook_append_page (GTK_NOTEBOOK (liferea_shell_lookup ("browsertabs")), vbox, label);
	g_signal_connect (gtk_notebook_get_nth_page (GTK_NOTEBOOK (liferea_shell_lookup ("browsertabs")), i), 
	                  "key-press-event", G_CALLBACK (on_tab_key_press), (gpointer) widget);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (liferea_shell_lookup ("browsertabs")), TRUE);
	
	if (activate && (i != -1))
		gtk_notebook_set_current_page (GTK_NOTEBOOK (liferea_shell_lookup ("browsertabs")), i);
	 
	if (url) {
		ui_tabs_history_add_location (tab, (gchar *)url);
		liferea_htmlview_launch_URL (htmlview, (gchar *)url, UI_HTMLVIEW_LAUNCH_INTERNAL);
	}
	return htmlview;
}

void
ui_tabs_show_headlines (void)
{
	gtk_notebook_set_current_page (GTK_NOTEBOOK (liferea_shell_lookup ("browsertabs")), 0);
}

/** This method is used to find the tab infos for a given
    child widget (usually the HTML rendering widget) of 
    an open tab. */
static tabInfo *
ui_tabs_find_notebook_child (GtkWidget *parent)
{
	GtkWidget 	*child;
	tabInfo		*tab = NULL;

	if (parent) {
		do {
			child = parent;
			parent = gtk_widget_get_parent (child);
			if (!parent)
				break;
			if (tab = g_object_get_data(G_OBJECT(parent), "tabInfo"))
				break;
		} while (!GTK_IS_NOTEBOOK (parent));
	}

	return tab;
}

void
ui_tabs_close_tab (GtkWidget *child)
{
	tabInfo		*tab;
	int		n;

	if (NULL == (tab = ui_tabs_find_notebook_child(child)))
		return;
	
	n = gtk_notebook_get_current_page(GTK_NOTEBOOK(liferea_shell_lookup("browsertabs")));
	gtk_notebook_remove_page(GTK_NOTEBOOK(liferea_shell_lookup("browsertabs")), n);

	ui_tabs_history_free (tab->history);
	g_free (tab);	
	
	/* check if all tabs are closed */
	if(1 == gtk_notebook_get_n_pages(GTK_NOTEBOOK(liferea_shell_lookup("browsertabs"))))
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(liferea_shell_lookup("browsertabs")), FALSE);
}

void
ui_tabs_set_location (GtkWidget *child, const gchar *uri)
{
	tabInfo		*tab;
	
	if (NULL == (tab = ui_tabs_find_notebook_child (child)))
		return;
	
	ui_tabs_history_add_location (tab, uri);

	gtk_entry_set_text (GTK_ENTRY (tab->urlentry), uri);
}

void
ui_tabs_set_title (GtkWidget *child, const gchar *title)
{
	tabInfo		*tab;
	gchar		*text;
	GtkWidget	*label;
	
	if (NULL == (tab = ui_tabs_find_notebook_child (child)))
		return;
	
	text = ui_tabs_condense_text (title != NULL ? title : _("New tab"));
	label = gtk_label_new (text);
	gtk_widget_show (label);
	gtk_notebook_set_tab_label (GTK_NOTEBOOK (liferea_shell_lookup ("browsertabs")),
	                            tab->widget, label);
	g_free (text);
}

LifereaHtmlView *
ui_tabs_get_active_htmlview (void)
{
	tabInfo		*tab;
	gint		current;
	
	current = gtk_notebook_get_current_page (GTK_NOTEBOOK (liferea_shell_lookup ("browsertabs")));
	if (0 == current)
		return ui_mainwindow_get_active_htmlview ();
		
	tab = g_object_get_data (G_OBJECT (gtk_notebook_get_nth_page (GTK_NOTEBOOK (liferea_shell_lookup ("browsertabs")), current)), "tabInfo");
	return tab->htmlview;
}

void
on_popup_open_link_in_tab_selected (gpointer url, guint callback_action, GtkWidget *widget)
{
	ui_tabs_new ((gchar *)url, (gchar *)url, FALSE);
}
