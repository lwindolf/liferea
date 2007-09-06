/**
 * @file bloglines_source.c Bloglines feed list source support
 * 
 * Copyright (C) 2006-2007 Lars Lindner <lars.lindner@gmail.com>
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

#include <glib.h>
#include <gtk/gtk.h>
#include <unistd.h>

#include "conf.h"
#include "subscription.h"
#include "ui/ui_dialog.h"
#include "fl_sources/bloglines_source.h"
#include "fl_sources/node_source.h"
#include "fl_sources/opml_source.h"

static void bloglines_source_init (void) { }

static void bloglines_source_deinit (void) { }

/* GUI callbacks */

static void
on_bloglines_source_selected (GtkDialog *dialog,
                              gint response_id,
                              gpointer user_data) 
{
	nodePtr		node, parent = (nodePtr) user_data;
	subscriptionPtr	subscription;

	if (response_id == GTK_RESPONSE_OK) {
		subscription = subscription_new ("http://rpc.bloglines.com/listsubs", NULL, NULL);
		subscription->updateOptions->username = g_strdup (gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "userEntry"))));
		subscription->updateOptions->password = g_strdup (gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "passwordEntry"))));

		node = node_new ();
		node_set_title (node, "Bloglines");
		node_source_new (node, bloglines_source_get_type ());
		opml_source_setup (parent, node);
		node_set_subscription (node, subscription);
		opml_source_update (node);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
on_bloglines_source_dialog_destroy (GtkDialog *dialog,
                                    gpointer user_data) 
{
	g_object_unref (user_data);
}

static void
ui_bloglines_source_get_account_info (nodePtr parent)
{
	GtkWidget	*dialog;
	
	dialog = liferea_dialog_new ( PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "bloglines_source.glade", "bloglines_source_dialog");
	
	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (on_bloglines_source_selected), 
			  (gpointer) parent);
}

/* node source type definition */

static struct nodeSourceType nst = {
	NODE_SOURCE_TYPE_API_VERSION,
	"fl_bloglines",
	N_("Bloglines"),
	N_("Integrate the feed list of your Bloglines account. Liferea will "
	   "present your Bloglines subscription as a read-only subtree in the feed list."),
	NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION,
	bloglines_source_init,
	bloglines_source_deinit,
	ui_bloglines_source_get_account_info,
	opml_source_remove,
	opml_source_import,
	opml_source_export,
	opml_source_get_feedlist,
	opml_source_update,
	opml_source_auto_update,
	NULL	/* free */
};

nodeSourceTypePtr
bloglines_source_get_type(void)
{
	return &nst;
}
