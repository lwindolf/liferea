/**
 * @file browser_tabs.c  internal browsing using multiple tabs
 *
 * Copyright (C) 2004-2012 Lars Windolf <lars.lindner@gmail.com>
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

#include "browser_history.h"
#include "common.h"
#include "itemlist.h"
#include "ui/ui_common.h"
#include "ui/liferea_shell.h"
#include "ui/item_list_view.h"

/** All widget elements and state of a tab */
typedef struct tabInfo {
	GtkWidget	*widget;	/**< the tab widget */
	GtkWidget	*label;		/**< the tab label */
	GtkWidget	*forward;	/**< the forward button */
	GtkWidget	*back;		/**< the back button */
	LifereaHtmlView	*htmlview;	/**< the tabs HTML view widget */
	GtkWidget	*urlentry;	/**< the tabs URL entry widget */
	browserHistory	*history;	/**< the tabs browser history */
} tabInfo;

/* single tab callbacks */

static gboolean
on_tab_url_entry_activate (GtkWidget *widget, gpointer user_data)
{
	tabInfo	*tab = (tabInfo *)user_data;
	gchar	*url;

	url = (gchar *)gtk_entry_get_text (GTK_ENTRY (widget));
	liferea_htmlview_launch_URL_internal (tab->htmlview, url);

	return TRUE;
}

static void
on_tab_history_back (GtkWidget *widget, gpointer user_data)
{
	tabInfo		*tab = (tabInfo *)user_data;
	gchar		*url;

	url = browser_history_back (tab->history);

	gtk_widget_set_sensitive (tab->forward, browser_history_can_go_forward (tab->history));
	gtk_widget_set_sensitive (tab->back,    browser_history_can_go_back (tab->history));

	liferea_htmlview_launch_URL_internal (tab->htmlview, url);
	gtk_entry_set_text (GTK_ENTRY (tab->urlentry), url);
}

static void
on_tab_history_forward (GtkWidget *widget, gpointer user_data)
{
	tabInfo		*tab = (tabInfo *)user_data;
	gchar		*url;

	url = browser_history_forward (tab->history);

	gtk_widget_set_sensitive (tab->forward, browser_history_can_go_forward (tab->history));
	gtk_widget_set_sensitive (tab->back,    browser_history_can_go_back (tab->history));

	liferea_htmlview_launch_URL_internal (tab->htmlview, url);
	gtk_entry_set_text (GTK_ENTRY (tab->urlentry), url);
}

static void browser_tabs_close_tab (tabInfo *tab);

static gboolean
on_tab_key_press (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	guint modifiers;

	modifiers = gtk_accelerator_get_default_mod_mask ();
	if ((event->keyval == GDK_KEY(w))
	    && ((event->state & modifiers) == GDK_CONTROL_MASK)) {
		browser_tabs_close_tab ((tabInfo *)data);
		return TRUE;
	}
	
	return FALSE;
}

/* browser tabs object */

#define BROWSER_TABS_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), BROWSER_TABS_TYPE, BrowserTabsPrivate))

struct BrowserTabsPrivate {
	GtkNotebook	*notebook;

	GtkWidget	*headlines;	/**< widget of the headlines tab */
	
	GSList		*list;		/**< tabInfo structures for all tabs */
};

static GObjectClass *parent_class = NULL;
static BrowserTabs *tabs = NULL;

G_DEFINE_TYPE (BrowserTabs, browser_tabs, G_TYPE_OBJECT);

/** Removes tab info structure */
static void
browser_tabs_remove_tab (tabInfo *tab)
{
	tabs->priv->list = g_slist_remove (tabs->priv->list, tab);
	browser_history_free (tab->history);
	g_object_unref (tab->htmlview);
	g_free (tab);
}

static void
browser_tabs_finalize (GObject *object)
{
	BrowserTabs	*bt = BROWSER_TABS (object);
	GSList		*iter = bt->priv->list;

	while (iter) {
		browser_tabs_remove_tab (iter->data);
		iter = g_slist_next (iter);
	}
	
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
	tabs->priv->headlines = gtk_notebook_get_nth_page (notebook, 0);

	gtk_notebook_set_show_tabs (tabs->priv->notebook, FALSE);
	
	return tabs;
}

/* HTML view signal handlers */

static void
on_htmlview_location_changed (gpointer object, gchar *uri, gpointer user_data)
{
	tabInfo	*tab = (tabInfo *)user_data;
	
	browser_history_add_location (tab->history, uri);

	gtk_widget_set_sensitive (tab->forward, browser_history_can_go_forward (tab->history));
	gtk_widget_set_sensitive (tab->back,    browser_history_can_go_back (tab->history));

	gtk_entry_set_text (GTK_ENTRY (tab->urlentry), uri);
}

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
on_htmlview_title_changed (gpointer object, gchar *title, gpointer user_data)
{
	tabInfo		*tab = (tabInfo *)user_data;
	GtkWidget	*label;

	gtk_label_set_text (GTK_LABEL(tab->label), create_label_text (title));
}

static void
on_htmlview_close_tab (gpointer object, gpointer user_data)
{
	browser_tabs_close_tab((tabInfo *)user_data);
}

/** Close tab and removes tab info structure */
static void
browser_tabs_close_tab (tabInfo *tab)
{	
	int	n = 0;
	GList	*iter, *list;

	/* Find the tab index that needs to be closed */
	iter = list = gtk_container_get_children (GTK_CONTAINER (tabs->priv->notebook));
	while (iter) {
		if (tab->widget == GTK_WIDGET (iter->data))
			break;
		n++;
		iter = g_list_next (iter);
	}
	g_list_free (list);

	if (iter) {
		gtk_notebook_remove_page (tabs->priv->notebook, n);
		browser_tabs_remove_tab (tab);
	}
		
	/* check if all tabs are closed */
	if (1 == gtk_notebook_get_n_pages (tabs->priv->notebook))
		gtk_notebook_set_show_tabs (tabs->priv->notebook, FALSE);
}

static void
on_htmlview_status_message (gpointer obj, gchar *url)
{
	liferea_shell_set_important_status_bar ("%s", url);
}

/* single tab creation */

LifereaHtmlView *
browser_tabs_add_new (const gchar *url, const gchar *title, gboolean activate)
{
	GtkWidget 	*widget, *labelBox, *toolbar, *htmlframe, *image;
	tabInfo		*tab;
	int		i;

	tab = g_new0 (tabInfo, 1);
	tab->widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	tab->htmlview = liferea_htmlview_new (TRUE);
	tab->history = browser_history_new ();
	tabs->priv->list = g_slist_append (tabs->priv->list, tab);

	g_object_set_data (G_OBJECT (tab->widget), "tabInfo", tab);	

	g_signal_connect (tab->htmlview, "title-changed", G_CALLBACK (on_htmlview_title_changed), tab);
	g_signal_connect (tab->htmlview, "location-changed", G_CALLBACK (on_htmlview_location_changed), tab);
	g_signal_connect (tab->htmlview, "statusbar-changed", G_CALLBACK (on_htmlview_status_message), NULL);
	
	/* create tab widgets */

	tab->label = gtk_label_new (create_label_text (title));
	gtk_label_set_ellipsize (GTK_LABEL (tab->label), PANGO_ELLIPSIZE_END);
	gtk_label_set_width_chars (GTK_LABEL (tab->label), 17);

	labelBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start (GTK_BOX (labelBox), tab->label, FALSE, FALSE, 0);

	widget = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON(widget), GTK_RELIEF_NONE);
	image = gtk_image_new_from_stock ("gtk-close", GTK_ICON_SIZE_MENU);
	gtk_widget_show (image);
	gtk_container_add (GTK_CONTAINER (widget), image);
	gtk_box_pack_end (GTK_BOX (labelBox), widget, FALSE, FALSE, 0);
	g_signal_connect ((gpointer)widget, "clicked", G_CALLBACK (on_htmlview_close_tab), (gpointer)tab);

	gtk_widget_show_all (labelBox);
	
	toolbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	
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
	
	htmlframe = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (htmlframe), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (htmlframe), liferea_htmlview_get_widget (tab->htmlview));
	
	gtk_box_pack_start (GTK_BOX (tab->widget), toolbar, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (tab->widget), htmlframe, TRUE, TRUE, 0);
	gtk_widget_show_all (tab->widget);
	
	i = gtk_notebook_append_page (tabs->priv->notebook, tab->widget, labelBox);
	g_signal_connect (gtk_notebook_get_nth_page (tabs->priv->notebook, i), 
	                  "key-press-event", G_CALLBACK (on_tab_key_press), (gpointer)tab);
	gtk_notebook_set_show_tabs (tabs->priv->notebook, TRUE);
	gtk_notebook_set_tab_reorderable (tabs->priv->notebook, tab->widget, TRUE);	
		
	if (activate && (i != -1))
		gtk_notebook_set_current_page (tabs->priv->notebook, i);
	 
	if (url) {
		browser_history_add_location (tab->history, (gchar *)url);

		gtk_widget_set_sensitive (tab->forward, browser_history_can_go_forward (tab->history));
		gtk_widget_set_sensitive (tab->back,    browser_history_can_go_back (tab->history));

		liferea_htmlview_launch_URL_internal (tab->htmlview, (gchar *)url);
	}
	return tab->htmlview;
}

void
browser_tabs_show_headlines (void)
{
	gtk_notebook_set_current_page (tabs->priv->notebook, gtk_notebook_page_num (tabs->priv->notebook, tabs->priv->headlines));
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

void
browser_tabs_do_zoom (gboolean in)
{
	liferea_htmlview_do_zoom (browser_tabs_get_active_htmlview (), in);
}
