/**
 * @file bloglines_source-cb.c Bloglines feed list provider
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
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <libxml/uri.h>
#include <glade/glade.h>

#include "fl_sources/bloglines_source.h"
#include "fl_sources/bloglines_source-cb.h"
#include "fl_sources/opml_source.h"
#include "ui/ui_dialog.h"

static void
on_bloglines_source_selected (GtkDialog *dialog,
                              gint response_id,
                              gpointer user_data) 
{
	nodePtr		node, parent = (nodePtr) user_data;
	gchar		*url;
	xmlURIPtr 	uri;

	if (response_id == GTK_RESPONSE_OK) {
		uri = xmlParseURI ("http://rpc.bloglines.com/listsubs");
		uri->user = g_strdup_printf ("%s:%s", gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET (dialog), "userEntry"))),
		                             gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET (dialog), "passwordEntry"))));
		url = xmlSaveUri (uri);

		node = node_new ();
		node_set_title (node, "Bloglines");
		node_source_new (node, bloglines_source_get_type (), url);
		node->source->updateOptions->username = g_strdup (gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "userEntry"))));
		node->source->updateOptions->password = g_strdup (gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "passwordEntry"))));
		opml_source_setup (parent, node);
		opml_source_update (node);
		
		g_free (url);
		xmlFreeURI (uri);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
on_bloglines_source_dialog_destroy (GtkDialog *dialog,
                                    gpointer user_data) 
{
	g_object_unref (user_data);
}

void
ui_bloglines_source_get_account_info (nodePtr parent)
{
	GtkWidget	*dialog;
	GladeXML	*xml;
	
	xml = glade_xml_new ( PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE "bloglines_source.glade", NULL, NULL);
	dialog = glade_xml_get_widget (xml, "bloglines_source_dialog");
	
	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (on_bloglines_source_selected), 
			  (gpointer) parent);
	g_signal_connect (G_OBJECT (dialog), "destroy",
	                  G_CALLBACK (on_bloglines_source_dialog_destroy),
			  (gpointer) xml);
}
