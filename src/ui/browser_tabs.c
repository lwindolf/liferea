/**
 * @file browser_tabs.c  internal browsing using multiple tabs
 *
 * Copyright (C) 2004-2008 Lars Lindner <lars.lindner@gmail.com>
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
#include "itemlist.h"
#include "ui/liferea_shell.h"
#include "ui/ui_itemlist.h"

/* single tab history handling */

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

static tabHistory *
browser_tab_history_new (void)
{	
	return (tabHistory *)g_new0 (tabHistory, 1);
}

static void
browser_tab_history_free (tabHistory *history)
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
browser_tab_history_forward (tabInfo *tab)
{
	GList	*url = tab->history->current;

	url = g_list_next (url);
	tab->history->current = url;

	gtk_widget_set_sensitive (tab->forward, (NULL != g_list_next (url)));
	gtk_widget_set_sensitive (tab->back, TRUE);

	return url->data;
}

static gchar *
browser_tab_history_back (tabInfo *tab)
{
	GList	*url = tab->history->current;

	url = g_list_previous (url);
	tab->history->current = url;

	gtk_widget_set_sensitive (tab->back, (NULL != g_list_previous (url)));
	gtk_widget_set_sensitive (tab->forward, TRUE);

	return url->data;
}

static void
browser_tab_history_add_location (tabInfo *tab, const gchar *url)
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

/* single tab callbacks */

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

	url = browser_tab_history_back (tab),
	liferea_htmlview_launch_URL (tab->htmlview, url, UI_HTMLVIEW_LAUNCH_INTERNAL);
	gtk_entry_set_text (GTK_ENTRY (tab->urlentry), url);
}

static void
on_tab_history_forward (GtkWidget *widget, gpointer user_data)
{
	tabInfo		*tab = (tabInfo *)user_data;
	gchar		*url;

	url = browser_tab_history_forward (tab),
	liferea_htmlview_launch_URL (tab->htmlview, url, UI_HTMLVIEW_LAUNCH_INTERNAL);
	gtk_entry_set_text (GTK_ENTRY (tab->urlentry), url);
}

static void browser_tabs_close_tab (tabInfo *tab);

static gboolean
on_tab_key_press (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	guint modifiers;

	modifiers = gtk_accelerator_get_default_mod_mask ();
	if ((event->keyval == GDK_w) && ((event->state & modifiers) == GDK_CONTROL_MASK)) {
		browser_tabs_close_tab ((tabInfo *)data);
		return TRUE;
	}
	
	return FALSE;
}

/* browser tabs object */

static void browser_tabs_class_init	(BrowserTabsClass *klass);
static void browser_tabs_init		(BrowserTabs *ls);

#define BROWSER_TABS_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), BROWSER_TABS_TYPE, BrowserTabsPrivate))

struct BrowserTabsPrivate {
	GtkNotebook	*notebook;
};

static GObjectClass *parent_class = NULL;
static BrowserTabs *tabs = NULL;

GType
browser_tabs_get_type (void) 
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) 
	{
		static const GTypeInfo our_info = 
		{
			sizeof (BrowserTabsClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) browser_tabs_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (BrowserTabs),
			0, /* n_preallocs */
			(GInstanceInitFunc) browser_tabs_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "BrowserTabs",
					       &our_info, 0);
	}

	return type;
}

static void
browser_tabs_finalize (GObject *object)
{
	BrowserTabs *ls = BROWSER_TABS (object);
	
	// FIXME: free tabInfo structures!
	gtk_widget_destroy (GTK_WIDGET (tabs->priv->notebook));
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
browser_tabs_class_init (BrowserTabsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = browser_tabs_finalize;

	g_type_class_add_private (object_class, sizeof(BrowserTabsPrivate));
}

static void
browser_tabs_init (BrowserTabs *bt)
{
	/* globally accessible singleton */
	g_assert (NULL == tabs);
	tabs = bt;
	
	tabs->priv = BROWSER_TABS_GET_PRIVATE (tabs);
}

BrowserTabs *
browser_tabs_create (GtkNotebook *notebook)
{
	g_object_new (BROWSER_TABS_TYPE, NULL);
	
	tabs->priv->notebook = notebook;
	
	gtk_notebook_set_show_tabs (tabs->priv->notebook, FALSE);
	g_signal_connect ((gpointer)tabs->priv->notebook, "switch-page", G_CALLBACK (on_tab_switched), NULL);
	
	return tabs;
}

static gchar *
browser_tabs_condense_text (const gchar* title)
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

/* HTML view signal handlers */

static void
on_htmlview_location_changed (gpointer object, gchar *uri, gpointer user_data)
{
	tabInfo	*tab = (tabInfo *)user_data;
	
	browser_tab_history_add_location (tab, uri);
	gtk_entry_set_text (GTK_ENTRY (tab->urlentry), uri);
}

static void
on_htmlview_title_changed (gpointer object, gchar *title, gpointer user_data)
{
	tabInfo		*tab = (tabInfo *)user_data;
	gchar		*text;
	GtkWidget	*label;
	
	text = browser_tabs_condense_text (title?title:_("New tab"));
	label = gtk_label_new (text);
	gtk_widget_show (label);
	gtk_notebook_set_tab_label (tabs->priv->notebook, tab->widget, label);
	g_free (text);
}

static void
on_htmlview_open_tab (gpointer object, gchar *url, gpointer user_data)
{
	browser_tabs_add_new (url, url, FALSE);
}

static void
on_htmlview_close_tab (gpointer object, gpointer user_data)
{
	browser_tabs_close_tab((tabInfo *)user_data);
}

static void
browser_tabs_close_tab (tabInfo *tab)
{	
	int		n;

	n = gtk_notebook_get_current_page (tabs->priv->notebook);
	gtk_notebook_remove_page (tabs->priv->notebook, n);

	browser_tab_history_free (tab->history);
	g_free (tab);	
	
	/* check if all tabs are closed */
	if (1 == gtk_notebook_get_n_pages (tabs->priv->notebook))
		gtk_notebook_set_show_tabs (tabs->priv->notebook, FALSE);
}

/* single tab creation */

LifereaHtmlView *
browser_tabs_add_new (const gchar *url, const gchar *title, gboolean activate)
{
	GtkWidget 	*widget, *label, *toolbar, *htmlframe, *image;
	tabInfo		*tab;
	gchar		*tmp;
	int		i;

	tab = g_new0 (tabInfo, 1);
	tab->widget = gtk_vbox_new (FALSE, 0);
	tab->htmlview = liferea_htmlview_new (TRUE);
	tab->history = browser_tab_history_new ();

	g_object_set_data (G_OBJECT (tab->widget), "tabInfo", tab);	

	g_signal_connect (tab->htmlview, "title-changed", G_CALLBACK (on_htmlview_title_changed), tab);
	g_signal_connect (tab->htmlview, "location-changed", G_CALLBACK (on_htmlview_location_changed), tab);
	g_signal_connect (tab->htmlview, "open-tab", G_CALLBACK (on_htmlview_open_tab), tab);
	g_signal_connect (tab->htmlview, "close-tab", G_CALLBACK (on_htmlview_close_tab), tab);
	
	/* create tab widgets */

	tmp = browser_tabs_condense_text (title?title:_("New tab"));
	label = gtk_label_new (tmp);
	gtk_widget_show (label);
	g_free (tmp);
	
	toolbar = gtk_hbox_new (FALSE, 6);
	
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
	gtk_entry_set_text (GTK_ENTRY (widget), url?url:"");
	g_object_set_data (G_OBJECT (tab->widget), "url_entry", widget);
	
	gtk_box_pack_start (GTK_BOX (toolbar), widget, TRUE, TRUE, 0);
	g_signal_connect ((gpointer)widget, "activate", G_CALLBACK (on_tab_url_entry_activate), (gpointer)tab);
	tab->urlentry = widget;

	widget = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON(widget), GTK_RELIEF_NONE);
	image = gtk_image_new_from_stock ("gtk-close", GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image);
	gtk_container_add (GTK_CONTAINER (widget), image);
	gtk_box_pack_end (GTK_BOX (toolbar), widget, FALSE, FALSE, 0);
	g_signal_connect ((gpointer)widget, "clicked", G_CALLBACK (on_htmlview_close_tab), (gpointer)tab);
	
	htmlframe = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (htmlframe), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (htmlframe), liferea_htmlview_get_widget (tab->htmlview));
	
	gtk_box_pack_start (GTK_BOX (tab->widget), toolbar, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (tab->widget), htmlframe, TRUE, TRUE, 0);
	gtk_widget_show_all (tab->widget);
	
	i = gtk_notebook_append_page (tabs->priv->notebook, tab->widget, label);
	g_signal_connect (gtk_notebook_get_nth_page (tabs->priv->notebook, i), 
	                  "key-press-event", G_CALLBACK (on_tab_key_press), (gpointer)tab);
	gtk_notebook_set_show_tabs (tabs->priv->notebook, TRUE);
	
	if (activate && (i != -1))
		gtk_notebook_set_current_page (tabs->priv->notebook, i);
	 
	if (url) {
		browser_tab_history_add_location (tab, (gchar *)url);
		liferea_htmlview_launch_URL (tab->htmlview, (gchar *)url, UI_HTMLVIEW_LAUNCH_INTERNAL);
	}
	return tab->htmlview;
}

void
browser_tabs_show_headlines (void)
{
	gtk_notebook_set_current_page (tabs->priv->notebook, 0);
}

LifereaHtmlView *
browser_tabs_get_active_htmlview (void)
{
	tabInfo		*tab;
	gint		current;
	
	current = gtk_notebook_get_current_page (tabs->priv->notebook);
	if (0 == current)
		return NULL;	/* never return the first page widget (because it is the item view) */
		
	tab = g_object_get_data (G_OBJECT (gtk_notebook_get_nth_page (tabs->priv->notebook, current)), "tabInfo");
	return tab->htmlview;
}
