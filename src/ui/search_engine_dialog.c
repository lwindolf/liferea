/**
 * @file search_engine_dialog.c  Search engine subscription dialog
 *
 * Copyright (C) 2007-2008 Lars Lindner <lars.lindner@gmail.com>
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

#include "ui/search_engine_dialog.h"

#include "common.h"
#include "feed.h"
#include "feedlist.h"
#include "ui/liferea_dialog.h"

static void search_engine_dialog_class_init	(SearchEngineDialogClass *klass);
static void search_engine_dialog_init		(SearchEngineDialog *ld);

#define SEARCH_ENGINE_DIALOG_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), SEARCH_ENGINE_DIALOG_TYPE, SearchEngineDialogPrivate))

struct SearchEngineDialogPrivate {
	GtkWidget	*dialog;	/**< the dialog widget */
	GtkWidget	*query;		/**< entry widget for the search query */
	GtkWidget	*resultCount;	/**< adjustment widget for search result count limit */
	
	const gchar	*uriFmt;	/**< URI format of the search engine feed to be created */
	gboolean	limitSupported;	/**< TRUE if search result count limit is supported in uriFmt */
};

static GObjectClass *parent_class = NULL;

GType
search_engine_dialog_get_type (void) 
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) 
	{
		static const GTypeInfo our_info = 
		{
			sizeof (SearchEngineDialogClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) search_engine_dialog_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (SearchEngineDialog),
			0, /* n_preallocs */
			(GInstanceInitFunc) search_engine_dialog_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "SearchEngineDialog",
					       &our_info, 0);
	}

	return type;
}

static void
search_engine_dialog_finalize (GObject *object)
{
	SearchEngineDialog *sed = SEARCH_ENGINE_DIALOG (object);
	
	gtk_widget_destroy (sed->priv->dialog);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
search_engine_dialog_class_init (SearchEngineDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = search_engine_dialog_finalize;

	g_type_class_add_private (object_class, sizeof(SearchEngineDialogPrivate));
}

static void
search_engine_dialog_init (SearchEngineDialog *sed)
{
	sed->priv = SEARCH_ENGINE_DIALOG_GET_PRIVATE (sed);
}

static void
on_search_engine_dialog_response (GtkDialog *dialog, gint responseId, gpointer user_data)
{
	SearchEngineDialog	*sed = (SearchEngineDialog *)user_data;
	GtkAdjustment		*resultCountAdjust;
	gchar			*searchtext, *searchUri;
	
	if (GTK_RESPONSE_OK == responseId) {
		resultCountAdjust = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (sed->priv->resultCount));
		searchtext = g_uri_escape_string (gtk_entry_get_text (GTK_ENTRY (sed->priv->query)), NULL, TRUE);
		if (sed->priv->limitSupported)
			searchUri = g_strdup_printf (sed->priv->uriFmt, searchtext, (int)gtk_adjustment_get_value (resultCountAdjust));
		else
			searchUri = g_strdup_printf (sed->priv->uriFmt, searchtext);

		feedlist_add_subscription (searchUri, 
					   NULL, 
					   NULL,
		                           FEED_REQ_RESET_TITLE |
					   FEED_REQ_PRIORITY_HIGH);
		g_free (searchUri);
		g_free (searchtext);
	}
	
	g_object_unref (sed);
}

SearchEngineDialog *
search_engine_dialog_new (const gchar *uriFmt, gboolean limitSupported)
{
	SearchEngineDialog	*sed;

	sed = SEARCH_ENGINE_DIALOG (g_object_new (SEARCH_ENGINE_DIALOG_TYPE, NULL));
	sed->priv->dialog = liferea_dialog_new (NULL, "searchenginedialog");
	sed->priv->query = liferea_dialog_lookup (sed->priv->dialog, "searchkeywords");
	sed->priv->resultCount = liferea_dialog_lookup (sed->priv->dialog, "resultcount");
	sed->priv->limitSupported = limitSupported;
	sed->priv->uriFmt = uriFmt;

	gtk_window_set_focus (GTK_WINDOW (sed->priv->dialog), sed->priv->query);
	gtk_entry_set_text (GTK_ENTRY (sed->priv->query), "");
	gtk_widget_set_sensitive (sed->priv->resultCount, limitSupported);
	
	g_signal_connect (G_OBJECT (sed->priv->dialog), "response", G_CALLBACK(on_search_engine_dialog_response), sed);
	
	gtk_widget_show_all (sed->priv->dialog);
	
	return sed;
}
