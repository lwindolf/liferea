/**
 * @file auth_dialog.c  authentication dialog
 *
 * Copyright (C) 2007-2012 Lars Windolf <lars.windolf@gmx.de>
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

#define AUTH_DIALOG_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), AUTH_DIALOG_TYPE, AuthDialogPrivate))

struct AuthDialogPrivate {

	subscriptionPtr subscription;
	
	GtkWidget *dialog;
	GtkWidget *username;
	GtkWidget *password;	
	
	gint flags;
};

static GObjectClass *parent_class = NULL;

G_DEFINE_TYPE (AuthDialog, auth_dialog, G_TYPE_OBJECT);

static void
auth_dialog_finalize (GObject *object)
{
	AuthDialog *ad = AUTH_DIALOG (object);
	
	if (ad->priv->subscription != NULL)
		ad->priv->subscription->activeAuth = FALSE;
	
	gtk_widget_destroy (ad->priv->dialog);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
auth_dialog_class_init (AuthDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = auth_dialog_finalize;

	g_type_class_add_private (object_class, sizeof(AuthDialogPrivate));
}

static void
on_authdialog_response (GtkDialog *dialog,
                        gint response_id,
			gpointer user_data) 
{
	AuthDialog *ad = AUTH_DIALOG (user_data);
	
	if (response_id == GTK_RESPONSE_OK) {	
		subscription_set_auth_info (ad->priv->subscription,
		                            gtk_entry_get_text (GTK_ENTRY (ad->priv->username)),
		                            gtk_entry_get_text (GTK_ENTRY (ad->priv->password)));
		subscription_update (ad->priv->subscription, ad->priv->flags);
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
	
	ad->priv->subscription = subscription;
	ad->priv->flags = flags;
	
	uri = xmlParseURI (BAD_CAST subscription_get_source (ad->priv->subscription));
	
	if (uri) {
		if (uri->user) {
			gchar *user = uri->user;
			gchar *pass = strstr (user, ":");
			if(pass) {
				pass[0] = '\0';
				pass++;
				gtk_entry_set_text (GTK_ENTRY (ad->priv->password), pass);
			}
			gtk_entry_set_text (GTK_ENTRY (ad->priv->username), user);
			xmlFree (uri->user);
			uri->user = NULL;
		}
		xmlFree (uri->user);
		uri->user = NULL;
		source = xmlSaveUri (uri);
		xmlFreeURI (uri);
	}
	
	promptStr = g_strdup_printf ( _("Enter the username and password for \"%s\" (%s):"),
	                             node_get_title (ad->priv->subscription->node),
	                             source?source:_("Unknown source"));
	gtk_label_set_text (GTK_LABEL (liferea_dialog_lookup (ad->priv->dialog, "prompt")), promptStr);
	g_free (promptStr);
	if (source)
		xmlFree (source);
}

static void
auth_dialog_init (AuthDialog *ad)
{
	ad->priv = AUTH_DIALOG_GET_PRIVATE (ad);
	
	ad->priv->dialog = liferea_dialog_new ("auth.ui", "authdialog");
	ad->priv->username = liferea_dialog_lookup (ad->priv->dialog, "usernameEntry");
	ad->priv->password = liferea_dialog_lookup (ad->priv->dialog, "passwordEntry");
	
	g_signal_connect (G_OBJECT (ad->priv->dialog), "response", G_CALLBACK (on_authdialog_response), ad);

	gtk_widget_show_all (ad->priv->dialog);
}


AuthDialog *
auth_dialog_new (subscriptionPtr subscription, gint flags) 
{
	AuthDialog *ad;
	
	if (subscription->activeAuth) {
		debug0 (DEBUG_UPDATE, "Missing/wrong authentication. Skipping, as a dialog is already active.");
		return NULL;
	}
	
	ad = AUTH_DIALOG (g_object_new (AUTH_DIALOG_TYPE, NULL));
	auth_dialog_load(ad, subscription, flags);
	return ad;
}
