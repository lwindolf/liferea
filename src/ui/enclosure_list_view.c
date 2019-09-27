/**
 * @file enclosure-list-view.c enclosures/podcast handling GUI
 *
 * Copyright (C) 2005-2019 Lars Windolf <lars.windolf@gmx.de>
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
#include "ui/media_player.h"
#include "ui/popup_menu.h"
#include "ui/ui_common.h"

/* enclosure list view implementation */

enum {
	ES_NAME_STR,
	ES_MIME_STR,
	ES_DOWNLOADED,
	ES_SIZE,
	ES_SIZE_STR,
	ES_SERIALIZED,
	ES_LEN
};

struct _EnclosureListView {
	GObject			parentInstance;

	GSList			*enclosures;		/**< list of currently presented enclosures */

	GtkWidget		*container;		/**< container the list is embedded in */
	GtkWidget		*expander;		/**< expander that shows/hides the list */
	GtkWidget		*treeview;
	GtkTreeStore	*treestore;
};

G_DEFINE_TYPE (EnclosureListView, enclosure_list_view, G_TYPE_OBJECT);

static void
enclosure_list_view_finalize (GObject *object)
{
	g_slist_free_full (ENCLOSURE_LIST_VIEW (object)->enclosures, (GDestroyNotify)enclosure_free);
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

	object_class->finalize = enclosure_list_view_finalize;
}

static void
enclosure_list_view_init (EnclosureListView *elv)
{
}

static enclosurePtr
enclosure_list_view_get_selected_enclosure (EnclosureListView *elv, GtkTreeIter *iter)
{
	gchar		*str;
	enclosurePtr	enclosure;

	gtk_tree_model_get (GTK_TREE_MODEL (elv->treestore), iter, ES_SERIALIZED, &str, -1);
	enclosure = enclosure_from_string (str);
	g_free (str);

	return enclosure;
}

static gboolean
on_enclosure_list_button_press (GtkWidget *treeview, GdkEvent *event, gpointer user_data)
{
	GdkEventButton		*eb = (GdkEventButton *)event;
	GtkTreePath		*path;
	GtkTreeIter		iter;
	EnclosureListView 	*elv = (EnclosureListView *)user_data;

	if ((event->type != GDK_BUTTON_PRESS) || (3 != eb->button))
		return FALSE;

	/* avoid handling header clicks */
	if (eb->window != gtk_tree_view_get_bin_window (GTK_TREE_VIEW (treeview)))
		return FALSE;

	if (!gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview), (gint)eb->x, (gint)eb->y, &path, NULL, NULL, NULL))
		return FALSE;

	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (elv->treestore), &iter, path))
		ui_popup_enclosure_menu (enclosure_list_view_get_selected_enclosure (elv, &iter), event);

	return TRUE;
}

static gboolean
on_enclosure_list_popup_menu (GtkWidget *widget, gpointer user_data)
{
	GtkTreeView		*treeview = GTK_TREE_VIEW (widget);
	GtkTreeModel		*model;
	GtkTreeIter		iter;
	EnclosureListView 	*elv = (EnclosureListView *)user_data;

	if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (treeview), &model, &iter)) {
		ui_popup_enclosure_menu (enclosure_list_view_get_selected_enclosure (elv, &iter), NULL);
		return TRUE;
	}

	return FALSE;
}

static gboolean
on_enclosure_list_activate (GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
	GtkTreeIter		iter;
	GtkTreeModel		*model;
	EnclosureListView 	*elv = (EnclosureListView *)user_data;

	if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (treeview), &model, &iter)) {
		on_popup_open_enclosure (enclosure_list_view_get_selected_enclosure (elv, &iter));
		return TRUE;
	}

	return FALSE;
}

EnclosureListView *
enclosure_list_view_new ()
{
	EnclosureListView	*elv;
	GtkCellRenderer		*renderer;
	GtkTreeViewColumn 	*column;
	GtkWidget		*widget;

	elv = ENCLOSURE_LIST_VIEW (g_object_new (ENCLOSURE_LIST_VIEW_TYPE, NULL));

	/* Use a vbox to allow media player insertion */
	elv->container = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_name (GTK_WIDGET (elv->container), "enclosureview");

	elv->expander = gtk_expander_new (_("Attachments"));
	gtk_box_pack_end (GTK_BOX (elv->container), elv->expander, TRUE, TRUE, 0);

	widget = gtk_scrolled_window_new (NULL, NULL);
	/* FIXME: Setting a fixed size is not nice, but a workaround for the
	   enclosure list view being hidden as 1px size in Ubuntu */
	gtk_widget_set_size_request (widget, -1, 75);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (elv->expander), widget);

	elv->treeview = gtk_tree_view_new ();
	gtk_container_add (GTK_CONTAINER (widget), elv->treeview);
	gtk_widget_show (elv->treeview);

	elv->treestore = gtk_tree_store_new (ES_LEN,
	                                           G_TYPE_STRING,	/* ES_NAME_STR */
						   G_TYPE_STRING,	/* ES_MIME_STR */
						   G_TYPE_BOOLEAN,	/* ES_DOWNLOADED */
						   G_TYPE_ULONG,	/* ES_SIZE */
						   G_TYPE_STRING,	/* ES_SIZE_STRING */
						   G_TYPE_STRING	/* ES_SERIALIZED */
	                                           );
	gtk_tree_view_set_model (GTK_TREE_VIEW (elv->treeview), GTK_TREE_MODEL(elv->treestore));

	/* explicitely no translation for invisible column headers... */

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Size", renderer,
	                                                   "text", ES_SIZE_STR,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (elv->treeview), column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("URL", renderer,
	                                                   "text", ES_NAME_STR,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (elv->treeview), column);
	gtk_tree_view_column_set_sort_column_id (column, ES_NAME_STR);
	gtk_tree_view_column_set_expand (column, TRUE);
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("MIME", renderer,
	                                                   "text", ES_MIME_STR,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (elv->treeview), column);

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (elv->treeview), FALSE);

	g_signal_connect (G_OBJECT (elv->treeview), "button_press_event", G_CALLBACK (on_enclosure_list_button_press), (gpointer)elv);
	g_signal_connect (G_OBJECT (elv->treeview), "row-activated", G_CALLBACK (on_enclosure_list_activate), (gpointer)elv);
	g_signal_connect (G_OBJECT (elv->treeview), "popup_menu", G_CALLBACK (on_enclosure_list_popup_menu), (gpointer)elv);

	g_signal_connect_object (elv->container, "destroy", G_CALLBACK (enclosure_list_view_destroy_cb), elv, 0);

	return elv;
}

GtkWidget *
enclosure_list_view_get_widget (EnclosureListView *elv)
{
	return elv->container;
}

void
enclosure_list_view_load (EnclosureListView *elv, itemPtr item)
{
	GSList		*list, *filteredList;
	guint		len;

	/* Ugly workaround to prevent race on startup when item is selected
	   but enclosure list view not yet initialized. */
	if (!elv)
		return;

	/* cleanup old content */
	gtk_tree_store_clear (elv->treestore);
	g_slist_free_full (elv->enclosures, (GDestroyNotify)enclosure_free);
	elv->enclosures = NULL;

	/* load list into tree view */
	filteredList = NULL;
	list = metadata_list_get_values (item->metadata, "enclosure");
	while (list) {
		enclosurePtr enclosure = enclosure_from_string (list->data);
		if (enclosure) {
			GtkTreeIter	iter;
			gchar		*sizeStr;
			guint		size = enclosure->size;

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

			gtk_tree_store_append (elv->treestore, &iter, NULL);
			gtk_tree_store_set (elv->treestore, &iter,
				            ES_NAME_STR, enclosure->url,
					    ES_MIME_STR, enclosure->mime?enclosure->mime:"",
				            ES_DOWNLOADED, enclosure->downloaded,
					    ES_SIZE, enclosure->size,
					    ES_SIZE_STR, sizeStr,
					    ES_SERIALIZED, list->data,
					    -1);
			g_free (sizeStr);

			elv->enclosures = g_slist_append (elv->enclosures, enclosure);

			// Filter unwanted MIME types (we only want audio/* and video/*)
			if (enclosure->mime &&
                            (g_str_has_prefix (enclosure->mime, "video/") ||
			    (g_str_has_prefix (enclosure->mime, "audio/")))) {
				filteredList = g_slist_append (filteredList, list->data);
			}
		}

		list = g_slist_next (list);
	}

	/* decide visibility of the list */
	len = g_slist_length (elv->enclosures);
	if (len == 0) {
		enclosure_list_view_hide (elv);
		return;
	}

	gtk_widget_show_all (elv->container);

	/* update list title */
	gchar *text = g_strdup_printf (ngettext("%d attachment", "%d attachments", len), len);
	gtk_expander_set_label (GTK_EXPANDER (elv->expander), text);
	g_free (text);

	/* Load the optional media player plugin */
	if (g_slist_length (filteredList) > 0) {
		liferea_media_player_load (elv->container, filteredList);
	}
}

void
enclosure_list_view_select (EnclosureListView *elv, guint position)
{
	GtkTreeIter iter;

	if (!gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (elv->treestore), &iter, NULL, position))
		return;

	gtk_tree_selection_select_iter (gtk_tree_view_get_selection (GTK_TREE_VIEW (elv->treeview)), &iter);
}

void
enclosure_list_view_select_next (EnclosureListView *elv)
{
	GtkTreeIter selected_iter;
	GtkTreeSelection *selection;
	GtkTreeModel *model;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (elv->treeview));

	if (gtk_tree_selection_get_selected (selection, &model, &selected_iter) &&
	    gtk_tree_model_iter_next (model, &selected_iter))
		gtk_tree_selection_select_iter (selection, &selected_iter);
	else
		enclosure_list_view_select (elv, 0);
}

void
enclosure_list_view_open_next (EnclosureListView *elv)
{
	GtkTreeIter selected_iter;

	enclosure_list_view_select_next (elv);

	if (gtk_tree_selection_get_selected (
	      gtk_tree_view_get_selection (GTK_TREE_VIEW (elv->treeview)), NULL, &selected_iter)) {
		enclosurePtr enclosure;
		enclosure = enclosure_list_view_get_selected_enclosure (elv, &selected_iter);
		on_popup_open_enclosure ((gpointer) enclosure);
	}
}

void
enclosure_list_view_hide (EnclosureListView *elv)
{
	if (!elv)
		return;

	gtk_widget_hide (GTK_WIDGET (elv->container));
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
		ui_choose_file (_("Choose File"), "gtk-open", FALSE, on_selectcmdok_clicked, name, NULL, NULL, NULL, dialog);
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
		if (new)
			enclosure_mime_type_add (etp);
		else
			enclosure_mime_types_save ();

		/* now we have ensured an existing type configuration and
		   can launch the URL for which we configured the type */
		if (enclosure)
			on_popup_open_enclosure (enclosure);

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

	dialog = liferea_dialog_new ("enclosure_handler");
	if (type) {
		typestr = type->mime?type->mime:type->extension;
		gtk_entry_set_text (GTK_ENTRY (liferea_dialog_lookup (dialog, "enc_cmd_entry")), type->cmd);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "enc_always_btn")), TRUE);
	}

	if (!strchr(typestr, '/'))
		tmp = g_strdup_printf (_("File Extension .%s"), typestr);
	else
		tmp = g_strdup_printf ("%s", typestr);
	gtk_label_set_text (GTK_LABEL (liferea_dialog_lookup (dialog, "enc_type_label")), tmp);
	g_free (tmp);

	g_object_set_data (G_OBJECT(dialog), "typestr", g_strdup (typestr));
	g_object_set_data (G_OBJECT(dialog), "enclosure", enclosure);
	g_object_set_data (G_OBJECT(dialog), "type", type);
	g_signal_connect (G_OBJECT(dialog), "response", G_CALLBACK(on_adddialog_response), type);
	g_signal_connect (G_OBJECT(liferea_dialog_lookup(dialog, "enc_cmd_select_btn")), "clicked", G_CALLBACK(on_selectcmd_pressed), dialog);
	gtk_widget_show (dialog);

}

void
on_popup_open_enclosure (gpointer callback_data)
{
	gchar		*typestr, *tmp = NULL;
	enclosurePtr	enclosure = (enclosurePtr)callback_data;
	encTypePtr	etp_tmp = NULL, etp_found = NULL;
	GSList		*iter;

	/* 1.) Always try to determine the file extension... */

	/* find extension by looking for last '.' */
	typestr = strrchr (enclosure->url, '.');
	if (typestr)
		typestr = tmp = g_strdup (typestr + 1);

	/* handle case where there is a slash after the '.' */
	if (typestr && strrchr (typestr, '/'))
		typestr = strrchr (typestr, '/');

	/* handle case where there is no '.' at all */
	if (!typestr && strrchr (enclosure->url, '/'))
		typestr = strrchr (enclosure->url, '/');

	/* if we found no extension we map to dummy type "data" */
	if (!typestr)
		typestr = tmp = g_strdup ("data");

	/* strip GET parameters from typestr */
	g_strdelimit (typestr, "?", 0);

	debug2 (DEBUG_CACHE, "url:%s, mime:%s", enclosure->url, enclosure->mime);

	/* 2.) Search for type configuration based on MIME or file extension... */
	iter = (GSList *)enclosure_mime_types_get ();
	while (iter) {
		etp_tmp = (encTypePtr)(iter->data);
		if (enclosure->mime && etp_tmp->mime) {
			/* match know MIME types and stop looking if found */
			if (!strcmp(enclosure->mime, etp_tmp->mime)) {
				etp_found = etp_tmp;
				break;
			}
		} else if (etp_tmp->extension) {
			/* match known file extensions and keep looking for matching MIME type */
			if (!strcmp(typestr, etp_tmp->extension)) {
				etp_found = etp_tmp;
			}
		}
		iter = g_slist_next (iter);
	}

	if (etp_found) {
		enclosure_download (etp_found, enclosure->url, TRUE);
	} else {
		if (enclosure->mime)
			ui_enclosure_type_setup (NULL, enclosure, enclosure->mime);
		else
			ui_enclosure_type_setup (NULL, enclosure, typestr);
	}

	g_free (tmp);
}

void
on_popup_save_enclosure (gpointer callback_data)
{
	enclosurePtr	enclosure = (enclosurePtr)callback_data;

	enclosure_download (NULL, enclosure->url, TRUE);
}

void
ui_enclosure_change_type (encTypePtr type)
{
	ui_enclosure_type_setup (type, NULL, NULL);
}

void
on_popup_copy_enclosure (gpointer callback_data)
{
	enclosurePtr enclosure = (enclosurePtr)callback_data;

        gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_PRIMARY), enclosure->url, -1);
        gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), enclosure->url, -1);
}
