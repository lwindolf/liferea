/**
 * @file enclosure-list-view.c enclosures/podcast handling GUI
 *
 * Copyright (C) 2005-2008 Lars Lindner <lars.lindner@gmail.com>
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

#include "enclosure_list_view.h"

#include <string.h>

#include "common.h"
#include "conf.h"
#include "debug.h"
#include "enclosure.h"
#include "item.h"
#include "metadata.h"
#include "ui/liferea_dialog.h"
#include "ui/ui_popup.h"
#include "ui/ui_prefs.h"

/* enclosure list view implementation */

enum {
	ES_NAME_STR,
	ES_MIME_STR,
	ES_DOWNLOADED,
	ES_SIZE,
	ES_SIZE_STR,
	ES_PTR,
	ES_LEN
};

static void enclosure_list_view_class_init	(EnclosureListViewClass *klass);
static void enclosure_list_view_init		(EnclosureListView *ld);

#define ENCLOSURE_LIST_VIEW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), ENCLOSURE_LIST_VIEW_TYPE, EnclosureListViewPrivate))

struct EnclosureListViewPrivate {
	GSList		*enclosures;		/**< list of currently presented enclosures */

	GtkWidget	*container;		/**< container the list is embedded in */
	GtkWidget	*treeview;
	GtkTreeStore	*treestore;
};

static GObjectClass *parent_class = NULL;

GType
enclosure_list_view_get_type (void) 
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) 
	{
		static const GTypeInfo our_info = 
		{
			sizeof (EnclosureListViewClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) enclosure_list_view_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (EnclosureListView),
			0, /* n_preallocs */
			(GInstanceInitFunc) enclosure_list_view_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "EnclosureListView",
					       &our_info, 0);
	}

	return type;
}

static void
enclosure_list_view_finalize (GObject *object)
{
	EnclosureListView *ls = ENCLOSURE_LIST_VIEW (object);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
enclosure_list_view_destroy_cb (GtkWidget *widget, EnclosureListView *elv)
{
	g_object_unref (elv);
}

static void
enclosure_list_view_class_init (EnclosureListViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = enclosure_list_view_finalize;

	g_type_class_add_private (object_class, sizeof(EnclosureListViewPrivate));
}

static void
enclosure_list_view_init (EnclosureListView *elv)
{
	elv->priv = ENCLOSURE_LIST_VIEW_GET_PRIVATE (elv);
}

static gboolean
on_enclosure_list_button_press (GtkWidget *treeview, GdkEventButton *event, gpointer user_data)
{
	GdkEventButton		*eb = (GdkEventButton *)event;
	GtkTreePath		*path;
	GtkTreeIter		iter;
	EnclosureListView 	*elv = (EnclosureListView *)user_data;
	
	if ((event->type != GDK_BUTTON_PRESS) || (3 != eb->button))
		return FALSE;

	/* avoid handling header clicks */
	if (event->window != gtk_tree_view_get_bin_window (GTK_TREE_VIEW (treeview)))
		return FALSE;

	if (!gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview), (gint)event->x, (gint)event->y, &path, NULL, NULL, NULL))
		return FALSE;

	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (elv->priv->treestore), &iter, path)) {
		enclosurePtr enclosure;
		
		gtk_tree_model_get (GTK_TREE_MODEL (elv->priv->treestore), &iter, ES_PTR, &enclosure, -1);
		gtk_menu_popup (ui_popup_make_enclosure_menu (enclosure), NULL, NULL, NULL, NULL, eb->button, eb->time);
	}
	
	return TRUE;
}

static gboolean
on_enclosure_list_activate (GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
	enclosurePtr	enclosure;
	GtkTreeIter	iter;
	GtkTreeModel	*model;
		
	gtk_tree_selection_get_selected (gtk_tree_view_get_selection (treeview), &model, &iter);
	gtk_tree_model_get (model, &iter, ES_PTR, &enclosure, -1);
	on_popup_open_enclosure (enclosure, 0, NULL);

	return TRUE;
}

EnclosureListView *
enclosure_list_view_new () 
{
	EnclosureListView	*elv;
	GtkCellRenderer		*renderer;
	GtkTreeViewColumn 	*column;
	GtkTreeSelection	*select;
	GtkWidget		*widget;
		
	elv = ENCLOSURE_LIST_VIEW (g_object_new (ENCLOSURE_LIST_VIEW_TYPE, NULL));
	elv->priv->container = gtk_expander_new (_("Attachments"));	
	
	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (elv->priv->container), widget);

	elv->priv->treeview = gtk_tree_view_new ();
	gtk_container_add (GTK_CONTAINER (widget), elv->priv->treeview);
	gtk_widget_show (elv->priv->treeview);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (elv->priv->treeview), TRUE);
	
	elv->priv->treestore = gtk_tree_store_new (ES_LEN,
	                                           G_TYPE_STRING,	/* ES_NAME_STR */
						   G_TYPE_STRING,	/* ES_MIME_STR */
						   G_TYPE_BOOLEAN,	/* ES_DOWNLOADED */
						   G_TYPE_ULONG,	/* ES_SIZE */
						   G_TYPE_STRING,	/* ES_SIZE_STRING */
						   G_TYPE_POINTER	/* ES_PTR */
	                                           );
	gtk_tree_view_set_model (GTK_TREE_VIEW (elv->priv->treeview), GTK_TREE_MODEL(elv->priv->treestore));

	/* explicitely no translation for invisible column headers... */
	
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Size", renderer, 
	                                                   "text", ES_SIZE_STR,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (elv->priv->treeview), column);
		
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("URL", renderer, 
	                                                   "text", ES_NAME_STR,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (elv->priv->treeview), column);
	gtk_tree_view_column_set_sort_column_id (column, ES_NAME_STR);
	gtk_tree_view_column_set_expand (column, TRUE);
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("MIME", renderer, 
	                                                   "text", ES_MIME_STR,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (elv->priv->treeview), column);
	
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (elv->priv->treeview), FALSE);

	g_signal_connect ((gpointer)elv->priv->treeview, "button_press_event",
	                  G_CALLBACK (on_enclosure_list_button_press), (gpointer)elv);
			  
	g_signal_connect ((gpointer)elv->priv->treeview, "row-activated",
	                  G_CALLBACK (on_enclosure_list_activate), (gpointer)elv);

	g_signal_connect_object (elv->priv->container, "destroy", G_CALLBACK (enclosure_list_view_destroy_cb), elv, 0);

	return elv;
}

GtkWidget *
enclosure_list_view_get_widget (EnclosureListView *elv)
{
	return elv->priv->container;
}

void
enclosure_list_view_load (EnclosureListView *elv, itemPtr item)
{
	GSList		*list;
	guint		len;

	/* cleanup old content */
	gtk_tree_store_clear (elv->priv->treestore);
	list = elv->priv->enclosures;
	while (list) {
		enclosure_free ((enclosurePtr)list->data);
		list = g_slist_next (list);
	}
	g_slist_free (elv->priv->enclosures);
	elv->priv->enclosures = NULL;	

	list = metadata_list_get_values (item->metadata, "enclosure");
	
	/* decide visibility of the list */
	list = metadata_list_get_values (item->metadata, "enclosure");
	len = g_slist_length (list);
	if (len == 0) {
		enclosure_list_view_hide (elv);
		return;
	}	
	
	gtk_widget_show_all (elv->priv->container);

	/* update list title */
	gchar *text = g_strdup_printf (ngettext("%d attachment", "%d attachments", len), len);
	gtk_expander_set_label (GTK_EXPANDER (elv->priv->container), text);
	g_free (text);

	/* load list into tree view */	
	while (list) {
		gchar *sizeStr;
		enclosurePtr enclosure;
		GtkTreeIter iter;
		
		enclosure = enclosure_from_string (list->data);
		if (enclosure) {
			guint size = enclosure->size;
			
			/* The following literals are the enclosure list size units */
			gchar *unit = _(" Bytes");
			if (size > 1024) {
				size /= 1024;
				unit = _("kB");
			}
			if (size > 1024) {
				size /= 1024;
				unit = _("MB");
			}
			if (size > 1024) {
				size /= 1024;
				unit = _("GB");
			}			
			
			/* The following literal is the format string for enclosure sizes (number + unit string) */
			if (size > 0)
				sizeStr = g_strdup_printf (_("%d%s"), size, unit);
			else
				sizeStr = g_strdup ("");
			
			gtk_tree_store_append (elv->priv->treestore, &iter, NULL);
			gtk_tree_store_set (elv->priv->treestore, &iter, 
			                    ES_NAME_STR, enclosure->url,
					    ES_MIME_STR, enclosure->mime?enclosure->mime:"",
			                    ES_DOWNLOADED, enclosure->downloaded,
					    ES_SIZE, enclosure->size,
					    ES_SIZE_STR, sizeStr,
					    ES_PTR, enclosure,
					    -1);
			g_free (sizeStr);

			elv->priv->enclosures = g_slist_append (elv->priv->enclosures, enclosure);
		}
		
		list = list->next;
	}
}

void
enclosure_list_view_hide (EnclosureListView *elv)
{
	if (!elv)
		return;
	
	gtk_widget_hide (GTK_WIDGET (elv->priv->container));
}

/* callback for preferences and enclosure type handling */

static void
on_selectcmdok_clicked (const gchar *filename, gpointer user_data)
{
	GtkWidget	*dialog = (GtkWidget *)user_data;
	gchar		*utfname;

	if (!filename)
		return;

	utfname = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
	if (utfname) {
		gtk_entry_set_text (GTK_ENTRY (liferea_dialog_lookup (dialog, "enc_cmd_entry")), utfname);	
		g_free (utfname);
	}
}

static void
on_selectcmd_pressed (GtkButton *button, gpointer user_data)
{
	GtkWidget	*dialog = (GtkWidget *)user_data;
	const gchar	*utfname;
	gchar 		*name;

	utfname = gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (dialog,"enc_cmd_entry")));
	name = g_filename_from_utf8 (utfname, -1, NULL, NULL, NULL);
	if (name) {
		ui_choose_file (_("Choose File"), GTK_STOCK_OPEN, FALSE, on_selectcmdok_clicked, name, NULL, dialog);
		g_free (name);
	}
}

/* dialog used for both changing and adding new definitions */
static void
on_adddialog_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
	gchar		*typestr;
	gboolean	new = FALSE;
	enclosurePtr	enclosure;
	encTypePtr	etp;

	if (response_id == GTK_RESPONSE_OK) {
		etp = g_object_get_data (G_OBJECT (dialog), "type");
		typestr = g_object_get_data (G_OBJECT (dialog), "typestr");
		enclosure = g_object_get_data (G_OBJECT (dialog), "enclosure");

		if (!etp) {
			new = TRUE;
			etp = g_new0 (struct encType, 1);
			if (!strchr (typestr, '/'))
				etp->extension = g_strdup (typestr);
			else
				etp->mime = g_strdup (typestr);
		} else {
			g_free (etp->cmd);
		}
		etp->cmd = g_strdup (gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET (dialog), "enc_cmd_entry"))));
		etp->permanent = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET (dialog), "enc_always_btn")));
		etp->remote = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET (dialog), "enc_remote_open_btn")));
		if (new)
			enclosure_mime_type_add (etp);

		/* now we have ensured an existing type configuration and
		   can launch the URL for which we configured the type */
		if (enclosure)
			on_popup_open_enclosure (enclosure, 0, NULL);
			
		g_free (typestr);
	}
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

/* either type or url and typestr are optional */
static void
ui_enclosure_type_setup (encTypePtr type, enclosurePtr enclosure, gchar *typestr)
{
	GtkWidget	*dialog;
	gchar		*tmp;

	dialog = liferea_dialog_new (NULL, "enchandlerdialog");

	if (type) {
		typestr = type->mime?type->mime:type->extension;
		gtk_entry_set_text (GTK_ENTRY (liferea_dialog_lookup (dialog, "enc_cmd_entry")), type->cmd);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "enc_always_btn")), TRUE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET(dialog), "enc_remote_open_btn")), type->remote);
	}

	if (!strchr(typestr, '/')) 
		tmp = g_strdup_printf (_("<b>File Extension .%s</b>"), typestr);
	else
		tmp = g_strdup_printf (_("<b>%s</b>"), typestr);
	gtk_label_set_markup_with_mnemonic (GTK_LABEL (liferea_dialog_lookup (dialog, "enc_type_label")), tmp);
	g_free (tmp);

	g_object_set_data (G_OBJECT(dialog), "typestr", g_strdup (typestr));
	g_object_set_data (G_OBJECT(dialog), "enclosure", enclosure);
	g_object_set_data (G_OBJECT(dialog), "type", type);
	g_signal_connect (G_OBJECT(dialog), "response", G_CALLBACK(on_adddialog_response), type);
	g_signal_connect (G_OBJECT(liferea_dialog_lookup(dialog, "enc_cmd_select_btn")), "clicked", G_CALLBACK(on_selectcmd_pressed), dialog);
	gtk_widget_show (dialog);

}

void
on_popup_open_enclosure (gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	gchar		*typestr;
	enclosurePtr	enclosure = (enclosurePtr)callback_data;
	gboolean	found = FALSE;
	encTypePtr	etp = NULL;
	GSList		*iter;

	/* 1.) Always try to determine the file extension... */
	
	/* FIXME: improve this to match only '.' not followed by '/' chars */
	typestr = strrchr (enclosure->url, '.');
	if (typestr)
		typestr = g_strdup (typestr + 1);
			
	/* if we found no extension we map to dummy type "data" */
	if (!typestr)
		typestr = g_strdup ("data");

	debug2 (DEBUG_CACHE, "url:%s, mime:%s", enclosure->url, enclosure->mime);
	
	/* FIXME: improve following check to first try to match
	   MIME types and if no match was found to check for
	   file extensions afterwards... */
	   
	/* 2.) Search for type configuration based on MIME or file extension... */
	iter = (GSList *)enclosure_mime_types_get ();
	while (iter) {
		etp = (encTypePtr)(iter->data);
		if (enclosure->mime && etp->mime) {
			/* match know MIME types */
			if (!strcmp(enclosure->mime, etp->mime)) {
				found = TRUE;
				break;
			}
		} else {
			/* match known file extensions */
			if (!strcmp(typestr, etp->extension)) {
				found = TRUE;
				break;
			}
		}
		iter = g_slist_next (iter);
	}
	
	if (found)
		enclosure_save_as_file (etp, enclosure->url, NULL);
	else
		ui_enclosure_type_setup (NULL, enclosure, typestr);
		
	g_free (typestr);
}

static void
on_encsave_clicked (const gchar *filename, gpointer user_data)
{
	gchar		*url = (gchar *)user_data;
	gchar		*utfname;
	
	if (!filename)
		return;

	utfname = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
	enclosure_save_as_file (NULL, url, utfname);
	g_free (utfname);
}

void
on_popup_save_enclosure (gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	enclosurePtr	enclosure = (enclosurePtr)callback_data;
	gchar		*filename;

	filename = strrchr (enclosure->url, '/');
	if (filename)
		filename++; /* Skip the slash to find the filename */
	else
		filename = enclosure->url;
		
	ui_choose_file (_("Choose File"), GTK_STOCK_SAVE_AS, TRUE, on_encsave_clicked, NULL, filename, enclosure->url);
}

void
ui_enclosure_change_type (encTypePtr type)
{
	ui_enclosure_type_setup (type, NULL, NULL);
}

void
on_popup_copy_enclosure (gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	enclosurePtr enclosure = (enclosurePtr)callback_data;
	
        gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_PRIMARY), enclosure->url, -1);
        gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), enclosure->url, -1);
}
