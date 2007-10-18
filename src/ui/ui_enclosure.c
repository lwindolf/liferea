/**
 * @file enclosure-list-view.c enclosures/podcast handling GUI
 *
 * Copyright (C) 2005-2007 Lars Lindner <lars.lindner@gmail.com>
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
#include <string.h>

#include "common.h"
#include "conf.h"
#include "debug.h"
#include "enclosure.h"
#include "ui/ui_dialog.h"
#include "ui/ui_popup.h"
#include "ui/ui_prefs.h"
#include "ui/ui_enclosure.h"

// FIXME: rewrite to enclosure list view class

void
ui_enclosure_new_popup (const gchar *url)
{	
	gtk_menu_popup (ui_popup_make_enclosure_menu (url), NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
}

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
	gchar		*tmp, *url;
	gboolean	new = FALSE;
	encTypePtr	etp;
	
	if (response_id == GTK_RESPONSE_OK) {
		etp = g_object_get_data (G_OBJECT (dialog), "type");
		tmp = g_object_get_data (G_OBJECT (dialog), "typestr");
		url = g_object_get_data (G_OBJECT (dialog), "url");

		if (!etp) {
			new = TRUE;
			etp = g_new0 (struct encType, 1);
			if (!strchr(tmp, '/'))
				etp->extension = tmp;
			else
				etp->mime = tmp;
		} else {
			g_free (etp->cmd);
		}
		etp->cmd = g_strdup (gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET (dialog), "enc_cmd_entry"))));
		etp->permanent = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET (dialog), "enc_always_btn")));
		if (new)
			enclosure_mime_type_add (etp);

		/* now we have ensured an existing type configuration and
		   can launch the URL for which we configured the type */
		if (url)
			on_popup_open_enclosure (g_strdup_printf ("%s%s%s", url, etp->mime?",":"", etp->mime?etp->mime:""), 0, NULL);
	}
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

/* either type or url and typestr are optional */
static void
ui_enclosure_add (encTypePtr type, gchar *url, gchar *typestr)
{
	GtkWidget	*dialog;
	gchar		*tmp;
	
	dialog = liferea_dialog_new (NULL, "enchandlerdialog");
	
	if (type) {
		typestr = type->mime?type->mime:type->extension;
		gtk_entry_set_text (GTK_ENTRY (liferea_dialog_lookup (dialog, "enc_cmd_entry")), type->cmd);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "enc_always_btn")), TRUE);
	}
	
	if (!strchr(typestr, '/')) 
		tmp = g_strdup_printf (_("<b>File Extension .%s</b>"), typestr);
	else
		tmp = g_strdup_printf (_("<b>%s</b>"), typestr);
	gtk_label_set_markup_with_mnemonic (GTK_LABEL (liferea_dialog_lookup (dialog, "enc_type_label")), tmp);
	g_free (tmp);

	g_object_set_data (G_OBJECT(dialog), "typestr", typestr);
	g_object_set_data (G_OBJECT(dialog), "url", url);
	g_object_set_data (G_OBJECT(dialog), "type", type);
	g_signal_connect (G_OBJECT(dialog), "response", G_CALLBACK(on_adddialog_response), type);
	g_signal_connect (G_OBJECT(liferea_dialog_lookup(dialog, "enc_cmd_select_btn")), "clicked", G_CALLBACK(on_selectcmd_pressed), dialog);
	gtk_widget_show (dialog);
	
}

void
on_popup_open_enclosure (gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	gchar		*typestr, *url = (gchar *)callback_data;
	gboolean	found = FALSE, mime = FALSE;
	encTypePtr	etp = NULL;
	GSList		*iter;
	
	/* When opening enclosures we need the type to determine
	   the configured launch command. The format of the enclosure
	   info: <url>[,<mime type] */	
	typestr = strrchr(url, ',');
	if (typestr) {
		*typestr = 0;
		typestr = g_strdup (typestr++);
		mime = TRUE;
	}
	
	/* without a type we use the file extension */
	if (!mime) {
		typestr = strrchr (url, '.');
		if (typestr)
			typestr = g_strdup (typestr + 1);
	}
	
	/* if we have no such thing we map to "data" */
	if (!typestr)
		typestr = g_strdup ("data");

	debug3 (DEBUG_CACHE, "url:%s, type:%s mime:%s", url, typestr, mime?"TRUE":"FALSE");
		
	/* search for type configuration */
	iter = (GSList *)enclosure_mime_types_get ();
	while (iter) {
		etp = (encTypePtr)(iter->data);
		if ((NULL != ((TRUE == mime)?etp->mime:etp->extension)) &&
		    (0 == strcmp(typestr, (TRUE == mime)?etp->mime:etp->extension))) {
		   	found = TRUE;
		   	break;
		}
		iter = g_slist_next (iter);
	}
	
	if (TRUE == found)
		enclosure_save_as_file (etp, url, NULL);
	else
		ui_enclosure_add (NULL, url, g_strdup (typestr));
		
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
	gchar	*filename = (gchar *)callback_data;

	filename = strrchr ((char *)callback_data, '/');
	if (filename)
		filename++; /* Skip the slash to find the filename */
	else
		filename = callback_data;
		
	ui_choose_file (_("Choose File"), GTK_STOCK_SAVE_AS, TRUE, on_encsave_clicked, NULL, filename, callback_data);
}

void
ui_enclosure_change_type (encTypePtr type)
{
	ui_enclosure_add (type, NULL, NULL);
}
