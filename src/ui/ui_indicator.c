/*
 * @file ui_indicator.c  libindicate support
 *
 * Copyright (C) 2010-2011 Maia Kozheva <sikon@ubuntu.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include "ui_indicator.h"

#ifdef HAVE_LIBINDICATE

#include <gtk/gtk.h>
#include <libindicate/server.h>
#include <libindicate/indicator.h>
#include <libindicate-gtk/indicator.h>
#include <libindicate/interests.h>
#include "feedlist.h"
#include "feed_list_view.h"
#include "liferea_shell.h"
#include "ui_tray.h"
#include "vfolder.h"

/* The maximum number of feeds to display in the indicator menu. */
#define MAX_INDICATORS      6
/* Whether Liferea should set the indicator menu to attention
   status whenever new feed items are downloaded. Since news feeds
   do not typically require the user's urgent attention, unlike
   mail and IM messages, this is set to false by default. */
#define SET_DRAW_ATTENTION  FALSE

static struct indicator_priv {
	IndicateServer *server;
	gboolean visible;
	GPtrArray *indicators;
} *indicator_priv = NULL;

/*
 The desktop file to initialize the indicator menu with. Resolves to
 a string like "/usr/share/applications/liferea.desktop".
*/ 
static const char *DESKTOP_FILE = PACKAGE_DATA_DIR G_DIR_SEPARATOR_S "applications" G_DIR_SEPARATOR_S "liferea.desktop";

static void
remove_all_indicators () {
	g_ptr_array_set_size (indicator_priv->indicators, 0);
}

/*
 Called when the main "Liferea" entry in the indicator menu is clicked.
*/ 
static void
on_indicator_server_clicked (IndicateServer *server, gchar *type, gpointer user_data)
{
	liferea_shell_present ();
	remove_all_indicators ();
}

/*
 Called when the indicator container applet is shown.
*/ 
static void
on_indicator_interest_added (IndicateServer *server, guint interest, gpointer user_data)
{
	if (interest != INDICATE_INTEREST_SERVER_SIGNAL)
		return;

	indicator_priv->visible = TRUE;
	ui_tray_update ();
}

/*
 Called when the indicator container applet is hidden.
*/ 
static void
on_indicator_interest_removed (IndicateServer *server, guint interest, gpointer user_data)
{
	if (interest != INDICATE_INTEREST_SERVER_SIGNAL)
		return;

	indicator_priv->visible = FALSE;
	ui_tray_update ();
}

/*
 Called when the indicator menu entry for a specific feed
 is clicked, meaning Liferea should switch to that feed.
*/ 
static void
on_indicator_clicked (IndicateIndicator *indicator, guint timestamp, gpointer user_data)
{
	feed_list_view_select ((nodePtr) user_data);
	liferea_shell_present ();
	remove_all_indicators ();
}

static void
destroy_indicator (gpointer indicator)
{
	if (indicator_priv->server == NULL || indicator == NULL) 
		return;
	
	indicate_server_remove_indicator (indicator_priv->server, INDICATE_INDICATOR(indicator));
	g_object_unref (G_OBJECT (indicator));
}

void
ui_indicator_init ()
{
	if (indicator_priv != NULL)
		return;
	
	indicator_priv = g_new0 (struct indicator_priv, 1);
	indicator_priv->visible = FALSE;
	indicator_priv->indicators = g_ptr_array_new_with_free_func (destroy_indicator);
	
	indicator_priv->server = indicate_server_ref_default();
	indicate_server_set_type (indicator_priv->server, "message.im");
	indicate_server_set_desktop_file (indicator_priv->server, DESKTOP_FILE);
	
	g_signal_connect (G_OBJECT (indicator_priv->server), "server-display", G_CALLBACK (on_indicator_server_clicked), NULL);
	g_signal_connect (G_OBJECT (indicator_priv->server), "interest-added", G_CALLBACK (on_indicator_interest_added), NULL);
	g_signal_connect (G_OBJECT (indicator_priv->server), "interest-removed", G_CALLBACK (on_indicator_interest_removed), NULL);
	
	indicate_server_show (indicator_priv->server);
	ui_indicator_update ();
}

void
ui_indicator_destroy ()
{
	if (indicator_priv == NULL)
		return;
	
	remove_all_indicators ();
	g_object_unref (indicator_priv->server);
	indicator_priv->server = NULL;
	g_ptr_array_free (indicator_priv->indicators, TRUE);
	g_free (indicator_priv);
	indicator_priv = NULL;
}

static void
add_node_indicator (nodePtr node)
{
	IndicateIndicator *indicator;
	GdkPixbuf *pixbuf;
	gchar count[10];
	
	if (indicator_priv->indicators->len >= MAX_INDICATORS)
		return;

	if (IS_VFOLDER(node) || g_slist_length (node->children) > 0) {
		/* Not a feed - walk children and do nothing more */
		node_foreach_child (node, add_node_indicator);
		return;
	}
	
	/* Skip feeds with no unread items */
	if (node->unreadCount == 0)
		return;
	
	indicator = indicate_indicator_new_with_server (indicator_priv->server);
	g_signal_connect (indicator, "user-display", G_CALLBACK (on_indicator_clicked), node);

	/* load favicon */
	pixbuf = gdk_pixbuf_new_from_file (node->iconFile, NULL);

	/* display favicon */
	indicate_gtk_indicator_set_property_icon (indicator, "icon", pixbuf);
	g_object_unref (pixbuf);

	sprintf (count, "%u", node->unreadCount);
	indicate_indicator_set_property (indicator, "name", node->title);
	indicate_indicator_set_property (indicator, "count", count);
#if SET_DRAW_ATTENTION
	indicate_indicator_set_property_bool (indicator, "draw-attention", TRUE);
#endif
	g_ptr_array_add (indicator_priv->indicators, indicator);
}

void
ui_indicator_update ()
{
	guint index;

	/* Do not update indicators if the user is interacting with the main window */
	if (!indicator_priv || gtk_window_is_active (GTK_WINDOW (liferea_shell_get_window ())))
		return;

	/* Remove all previous indicators from the menu */
	remove_all_indicators ();
	/* ...then walk the tree and add an indicator for each unread feed */
	feedlist_foreach (add_node_indicator);

	/* revert order of items */
	for (index = indicator_priv->indicators->len; index > 0; index--)
		indicate_indicator_show (g_ptr_array_index (indicator_priv->indicators, index - 1));	
}

gboolean
ui_indicator_is_visible ()
{
	return indicator_priv && indicator_priv->visible;
}

#else

/*
 If Liferea is compiled without libindicate support, all indicator
 support functions do nothing. The application behaves as if there
 is no indicator applet present.
*/

void ui_indicator_init () {}
void ui_indicator_destroy () {}
void ui_indicator_update () {}
gboolean ui_indicator_is_visible () { return FALSE; }

#endif  /* HAVE_LIBINDICATE */
