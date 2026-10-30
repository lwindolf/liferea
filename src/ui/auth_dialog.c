/**
 * @file auth_dialog.c  authentication dialog
 *
 * Copyright (C) 2007-2026  Lars Windolf <lars.windolf@gmx.de>
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
#include <libadwaita-1/adwaita.h>

#include "common.h"
#include "debug.h"
#include "update.h"
#include "ui/liferea_dialog.h"
#include "ui/liferea_shell.h"

/* The GtkBuilder inside LifereaDialog keeps its own reference on the dialog, so
   a closed dialog is not necessarily finalized. Weak pointers would therefore
   not be cleared, so singleton pointers are reset on the "closed" signal. */
static void
on_singleton_dialog_closed (AdwDialog *closed, gpointer user_data)
{
	GtkWidget **singleton = (GtkWidget **)user_data;

	if (*singleton == GTK_WIDGET (closed))
		*singleton = NULL;
}

/* token dialog */

typedef struct {
	AuthTokenCallback	cb;
	gpointer		user_data;
} AuthTokenContext;

static void
auth_token_context_free (gpointer user_data, GClosure* closure) 
{
	g_free (user_data);
}

/* There is only one token dialog at any time */
static GtkWidget *tokenDialog = NULL;

static void
on_auth_token_changed (GtkEditable *editable, gpointer user_data)
{
	g_autofree gchar *token = g_strstrip (g_strdup (gtk_editable_get_text (editable)));

	adw_alert_dialog_set_response_enabled (ADW_ALERT_DIALOG (user_data), "ok", token[0] != '\0');
}

static void
on_auth_token_dialog_response (AdwDialog *dialog, gchar *response_id, gpointer user_data)
{
	AuthTokenContext *ctx = (AuthTokenContext *)user_data;
	g_autofree gchar *token = NULL;

	if (g_strcmp0 (response_id, "ok") == 0)
		token = g_strstrip (g_strdup (liferea_dialog_entryrow_get (GTK_WIDGET (dialog), "tokenEntry")));

	ctx->cb (token, ctx->user_data);
}

void
auth_token_dialog_new (const gchar *url, const gchar *tokenId, AuthTokenCallback cb, gpointer user_data)
{
	GtkWidget		*dialog;
	AuthTokenContext	*ctx;
	g_autofree gchar	*promptStr = NULL;

	g_return_if_fail (tokenId != NULL);
	g_return_if_fail (cb != NULL);

	if (tokenDialog)
		adw_dialog_force_close (ADW_DIALOG (tokenDialog));

	dialog = liferea_dialog_new ("auth_token");
	if (!dialog)
		return;

	tokenDialog = dialog;
	adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "cancel", _("_Cancel"));
	adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "ok", _("_OK"));
	adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog), "ok", ADW_RESPONSE_SUGGESTED);
	adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "ok");
	adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (dialog), "cancel");
	adw_alert_dialog_set_response_enabled (ADW_ALERT_DIALOG (dialog), "ok", FALSE);

	promptStr = g_strdup_printf (_("Please enter the app token for \"%s\":"), url);
	gtk_label_set_text (GTK_LABEL (liferea_dialog_lookup (dialog, "prompt")), promptStr);

	g_signal_connect (liferea_dialog_lookup (dialog, "tokenEntry"), "changed", G_CALLBACK (on_auth_token_changed), dialog);

	ctx = g_new0 (AuthTokenContext, 1);
	ctx->cb = cb;
	ctx->user_data = user_data;
	g_signal_connect_data (dialog, "response", G_CALLBACK (on_auth_token_dialog_response), ctx, (GClosureNotify)auth_token_context_free, 0);
	g_signal_connect (dialog, "closed", G_CALLBACK (on_singleton_dialog_closed), &tokenDialog);

	liferea_dialog_present (dialog);
}

/* user + password dialog */

static void
on_authdialog_response (AdwDialog *dialog,
                        gchar *response_id,
			gpointer userdata)
{

	subscriptionPtr subscription = (subscriptionPtr)userdata;
g_print("auth response %s\n", response_id);
	if (g_strcmp0 (response_id, "ok") == 0) {
		subscription_set_auth_info (subscription,
		                            liferea_dialog_entryrow_get (GTK_WIDGET (dialog), "usernameEntry"),
		                            liferea_dialog_entryrow_get (GTK_WIDGET (dialog), "passwordEntry"));
		subscription_update (subscription, UPDATE_REQUEST_PRIORITY_HIGH);
	} else {
g_print ("disabled reauth\n");
		/* ensure we do not automatically ask again */
		subscription->authCancelled = TRUE;
	}
}

static GtkWidget *dialog = NULL;

void
auth_dialog_new (subscriptionPtr subscription, gint unused)
{
	gchar			*promptStr;
	gchar			*source = NULL;
	xmlURIPtr		uri;

	subscription->authPending = TRUE;

	/* never show 2 auth dialogs at once, we rely on auto update to repeat a 2nd dialog later */
	if (dialog)
		return;

	dialog = liferea_dialog_new ("auth");
	adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "cancel",  _("_Cancel"));
	adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "ok",  _("_OK"));
	adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog), "ok", ADW_RESPONSE_SUGGESTED);
	adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "ok");

	g_signal_connect (dialog, "closed", G_CALLBACK (on_singleton_dialog_closed), &dialog);
	g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (on_authdialog_response), subscription);

	uri = xmlParseURI (subscription_get_source (subscription));
	if (uri) {
		if (uri->user) {
			gchar *user = uri->user;
			gchar *pass = strstr (user, ":");
			if(pass)
				pass[0] = '\0';
				
			liferea_dialog_entry_set (dialog, "usernameEntry", user);
			xmlFree (uri->user);
			uri->user = NULL;
		}
		xmlFree (uri->user);
		uri->user = NULL;
		source = (gchar *) xmlSaveUri (uri);
		xmlFreeURI (uri);
	}

	promptStr = g_strdup_printf ( _("Credentials requested by \"%s\" (%s):"),
	                             node_get_title (subscription->node),
	                             source?source:_("Unknown source"));
	gtk_label_set_text (GTK_LABEL (liferea_dialog_lookup (dialog, "prompt")), promptStr);
	g_free (promptStr);
	if (source)
		xmlFree (source);

	liferea_dialog_present (dialog);
}