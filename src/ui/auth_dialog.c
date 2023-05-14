/**
 * @file auth_dialog.c  authentication dialog
 *
 * Copyright (C) 2007-2018  Lars Windolf <lars.windolf@gmx.de>
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

#include "ui/auth_dialog.h"

#include <libxml/uri.h>

#include "common.h"
#include "debug.h"
#include "ui/liferea_dialog.h"

struct _AuthDialog {
	GObject parentInstance;

	subscriptionPtr subscription;

	GtkWidget *dialog;
	GtkWidget *username;
	GtkWidget *password;

	gint flags;
};

G_DEFINE_TYPE (AuthDialog, auth_dialog, G_TYPE_OBJECT);

static void
auth_dialog_finalize (GObject *object)
{
	AuthDialog *ad = AUTH_DIALOG (object);

	if (ad->subscription != NULL)
		ad->subscription->activeAuth = FALSE;

	gtk_widget_destroy (ad->dialog);
}

static void
auth_dialog_class_init (AuthDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = auth_dialog_finalize;
}

static void
on_authdialog_response (GtkDialog *dialog,
                        gint response_id,
			gpointer user_data)
{
	AuthDialog *ad = AUTH_DIALOG (user_data);

	if (response_id == GTK_RESPONSE_OK) {
		subscription_set_auth_info (ad->subscription,
		                            gtk_entry_get_text (GTK_ENTRY (ad->username)),
		                            gtk_entry_get_text (GTK_ENTRY (ad->password)));
		subscription_update (ad->subscription, ad->flags);
	}

	g_object_unref (ad);
}

static void
auth_dialog_load (AuthDialog *ad,
                  subscriptionPtr subscription,
                  gint flags)
{
	gchar			*promptStr;
	gchar			*source = NULL;
	xmlURIPtr		uri;

	subscription->activeAuth = TRUE;

	ad->subscription = subscription;
	ad->flags = flags;

	uri = xmlParseURI (subscription_get_source (ad->subscription));

	if (uri) {
		if (uri->user) {
			gchar *user = uri->user;
			gchar *pass = strstr (user, ":");
			if(pass) {
				pass[0] = '\0';
				pass++;
				gtk_entry_set_text (GTK_ENTRY (ad->password), pass);
			}
			gtk_entry_set_text (GTK_ENTRY (ad->username), user);
			xmlFree (uri->user);
			uri->user = NULL;
		}
		xmlFree (uri->user);
		uri->user = NULL;
		source = (gchar *) xmlSaveUri (uri);
		xmlFreeURI (uri);
	}

	promptStr = g_strdup_printf ( _("Enter the username and password for \"%s\" (%s):"),
	                             node_get_title (ad->subscription->node),
	                             source?source:_("Unknown source"));
	gtk_label_set_text (GTK_LABEL (liferea_dialog_lookup (ad->dialog, "prompt")), promptStr);
	g_free (promptStr);
	if (source)
		xmlFree (source);
}

static void
auth_dialog_init (AuthDialog *ad)
{
	ad->dialog = liferea_dialog_new ("auth");
	ad->username = liferea_dialog_lookup (ad->dialog, "usernameEntry");
	ad->password = liferea_dialog_lookup (ad->dialog, "passwordEntry");

	g_signal_connect (G_OBJECT (ad->dialog), "response", G_CALLBACK (on_authdialog_response), ad);

	gtk_widget_show_all (ad->dialog);
}


AuthDialog *
auth_dialog_new (subscriptionPtr subscription, gint flags)
{
	AuthDialog *ad;

	if (subscription->activeAuth) {
		debug (DEBUG_UPDATE, "Missing/wrong authentication. Skipping, as a dialog is already active.");
		return NULL;
	}

	ad = AUTH_DIALOG (g_object_new (AUTH_DIALOG_TYPE, NULL));
	auth_dialog_load(ad, subscription, flags);
	return ad;
}
