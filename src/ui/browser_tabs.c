/*
 * @file browser_tabs.c  internal browsing using multiple tabs
 *
 * Copyright (C) 2004-2018 Lars Windolf <lars.windolf@gmx.de>
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
#include "ui/item_list_view.h"
#include "ui/gedit-close-button.h"

/* All widget elements and state of a tab */
typedef struct _tabInfo tabInfo;
struct _tabInfo {
	GtkWidget	*label;		/*<< the tab label */
	GtkWidget	*widget;	/*<< the embedded child widget */
	LifereaHtmlView	*htmlview;	/*<< the tabs HTML view widget */
};

/**
  * tab_info_copy: (skip)
  *
  * Creates a copy of @tabInfo
  */
static gpointer tab_info_copy (gpointer orig)
{
	tabInfo *a = orig;
	tabInfo *b;
	b = g_new0 (tabInfo, 1);
	b->label = a->label;
	b->widget = a->widget;
	b->htmlview = a->htmlview;
	// Shall we?
	g_object_ref(b->label);
	g_object_ref(b->widget);
	g_object_ref(b->htmlview);
	return b;
}

/**
  * tab_info_free: (skip)
  *
  * free @tabInfo
  */
static void tab_info_free (gpointer orig)
{
	tabInfo *a = orig;
	g_object_unref(a->label);
	g_object_unref(a->widget);
	g_object_unref(a->htmlview);
	g_free(orig);
}

G_DEFINE_BOXED_TYPE (tabInfo, tab_info, tab_info_copy, tab_info_free)

// gslist type for gproperty
//https://git.gnome.org/browse/gobject-introspection/tree/tests/gimarshallingtests.c
static GType
gi_marshalling_boxed_gslist_get_type (void)
{
    static GType type = 0;

    if (type == 0) {
        type = g_boxed_type_register_static ("GIMarshallingBoxedGSList",
                (GBoxedCopyFunc) g_slist_copy,
                (GBoxedFreeFunc) g_slist_free);
    }

    return type;
}

/* tab callbacks */

static void browser_tabs_close_tab (tabInfo *tab);

static gboolean
on_tab_key_press (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	guint modifiers;

	modifiers = gtk_accelerator_get_default_mod_mask ();
	if ((event->keyval == GDK_KEY_w)
	    && ((event->state & modifiers) == GDK_CONTROL_MASK)) {
		browser_tabs_close_tab ((tabInfo *)data);
		return TRUE;
	}

	return FALSE;
}

/* browser tabs object */

enum {
	PROP_NONE,
	PROP_NOTEBOOK,
	PROP_HEAD_LINES,
	PROP_TAB_INFO_LIST

};

struct _BrowserTabs {
	GObject		parentInstance;

	GtkNotebook	*notebook;

	GtkWidget	*headlines;	/*<< widget of the headlines tab */

	GSList		*list;		/*<< tabInfo structures for all tabs */
};

static BrowserTabs *tabs = NULL;

G_DEFINE_TYPE (BrowserTabs, browser_tabs, G_TYPE_OBJECT);

/* Removes tab info structure */
static void
browser_tabs_remove_tab (tabInfo *tab)
{
	tabs->list = g_slist_remove (tabs->list, tab);
	g_object_unref (tab->htmlview);
	g_free (tab);
}

static void
browser_tabs_finalize (GObject *object)
{
	BrowserTabs	*bt = BROWSER_TABS (object);
	GSList		*iter = bt->list;

	while (iter) {
		browser_tabs_remove_tab (iter->data);
		iter = g_slist_next (iter);
	}
}

static void
browser_tabs_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	BrowserTabs	*bt = BROWSER_TABS (object);

	switch (prop_id) {
		case PROP_NOTEBOOK:
			g_value_set_object (value, bt->notebook);
			break;
		case PROP_HEAD_LINES:
			g_value_set_object (value, bt->headlines);
			break;
		case PROP_TAB_INFO_LIST:
			g_value_set_boxed (value, bt->list);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
browser_tabs_class_init (BrowserTabsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = browser_tabs_get_property;
	object_class->finalize = browser_tabs_finalize;

	/* BrowserTabs:notebook: */
	g_object_class_install_property (
			object_class,
			PROP_NOTEBOOK,
			g_param_spec_object (
				"notebook",
				"GtkNotebook",
				"GtkNotebook object",
				GTK_TYPE_NOTEBOOK,
				G_PARAM_READABLE));

	/* BrowserTabs:headlines: */
	g_object_class_install_property (
			object_class,
			PROP_HEAD_LINES,
			g_param_spec_object (
				"head-lines",
				"GtkWidget",
				"GtkWidget object",
				GTK_TYPE_WIDGET,
				G_PARAM_READABLE));

	/**
	 * BrowserTabs:tab-info-list: (type GSList(tabInfo)) (transfer none):
	 */
	g_object_class_install_property (
			object_class,
			PROP_TAB_INFO_LIST,
			g_param_spec_boxed(
				"tab-info-list",
				"SList of browser tab info",
				"A GList of tab info containing htmlviews",
				gi_marshalling_boxed_gslist_get_type(),
				G_PARAM_READABLE));
}

static void
browser_tabs_init (BrowserTabs *bt)
{
	/* globally accessible singleton */
	g_assert (NULL == tabs);
	tabs = bt;
}

BrowserTabs *
browser_tabs_create (GtkNotebook *notebook)
{
	g_object_new (BROWSER_TABS_TYPE, NULL);

	tabs->notebook = notebook;
	tabs->headlines = gtk_notebook_get_nth_page (notebook, 0);

	gtk_notebook_set_show_tabs (tabs->notebook, FALSE);

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
on_htmlview_title_changed (gpointer object, gchar *title, gpointer user_data)
{
	tabInfo		*tab = (tabInfo *)user_data;

	gtk_label_set_text (GTK_LABEL(tab->label), create_label_text (title));
}

static void
on_htmlview_close_tab (gpointer object, gpointer user_data)
{
	browser_tabs_close_tab((tabInfo *)user_data);
}

/* Close tab and removes tab info structure */
static void
browser_tabs_close_tab (tabInfo *tab)
{
	int	n = 0;
	GList	*iter, *list;

	/* Find the tab index that needs to be closed */
	iter = list = gtk_container_get_children (GTK_CONTAINER (tabs->notebook));
	while (iter) {
		if (tab->widget == GTK_WIDGET (iter->data))
			break;
		n++;
		iter = g_list_next (iter);
	}
	g_list_free (list);

	if (iter) {
		gtk_notebook_remove_page (tabs->notebook, n);
		browser_tabs_remove_tab (tab);
	}

	/* check if all tabs are closed */
	if (1 == gtk_notebook_get_n_pages (tabs->notebook))
		gtk_notebook_set_show_tabs (tabs->notebook, FALSE);
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
	GtkWidget 	*close_button, *labelBox;
	tabInfo		*tab;
	int		i;

	tab = g_new0 (tabInfo, 1);
	tab->htmlview = liferea_htmlview_new (TRUE /* internal browsing */);
	tab->widget = liferea_htmlview_get_widget (tab->htmlview);
	tabs->list = g_slist_append (tabs->list, tab);

	g_object_set_data (G_OBJECT (tab->widget), "tabInfo", tab);

	g_signal_connect (tab->htmlview, "title-changed", G_CALLBACK (on_htmlview_title_changed), tab);
	g_signal_connect (tab->htmlview, "statusbar-changed", G_CALLBACK (on_htmlview_status_message), NULL);

	/* create tab widgets */

	tab->label = gtk_label_new (create_label_text (title));
	gtk_label_set_ellipsize (GTK_LABEL (tab->label), PANGO_ELLIPSIZE_END);
	gtk_label_set_width_chars (GTK_LABEL (tab->label), 17);

	labelBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start (GTK_BOX (labelBox), tab->label, FALSE, FALSE, 0);

	close_button = gedit_close_button_new ();
	gtk_box_pack_end (GTK_BOX (labelBox), close_button, FALSE, FALSE, 0);
	g_signal_connect ((gpointer)close_button, "clicked", G_CALLBACK (on_htmlview_close_tab), (gpointer)tab);

	gtk_widget_show_all (labelBox);

	i = gtk_notebook_append_page (tabs->notebook, tab->widget, labelBox);
	g_signal_connect (gtk_notebook_get_nth_page (tabs->notebook, i),
	                  "key-press-event", G_CALLBACK (on_tab_key_press), (gpointer)tab);
	gtk_notebook_set_show_tabs (tabs->notebook, TRUE);
	gtk_notebook_set_tab_reorderable (tabs->notebook, tab->widget, TRUE);

	if (activate && (i != -1))
		gtk_notebook_set_current_page (tabs->notebook, i);

	if (url)
		liferea_htmlview_launch_URL_internal (tab->htmlview, (gchar *)url);

	return tab->htmlview;
}

void
browser_tabs_show_headlines (void)
{
	gtk_notebook_set_current_page (tabs->notebook, gtk_notebook_page_num (tabs->notebook, tabs->headlines));
}

LifereaHtmlView *
browser_tabs_get_active_htmlview (void)
{
	tabInfo		*tab;
	gint		current;

	current = gtk_notebook_get_current_page (tabs->notebook);
	if (0 == current)
		return NULL;	/* never return the first page widget (because it is the item view) */

	tab = g_object_get_data (G_OBJECT (gtk_notebook_get_nth_page (tabs->notebook, current)), "tabInfo");
	return tab->htmlview;
}

void
browser_tabs_do_zoom (gint zoom)
{
	liferea_htmlview_do_zoom (browser_tabs_get_active_htmlview (), zoom);
}
