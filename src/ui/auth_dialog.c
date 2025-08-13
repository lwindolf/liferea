/**
 * @file auth_dialog.c  authentication dialog
 *
 * Copyright (C) 2007-2025  Lars Windolf <lars.windolf@gmx.de>
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
#include "update.h"
#include "ui/liferea_dialog.h"

static void
on_authdialog_response (GtkDialog *dialog,
                        gint response_id,
			gpointer userdata)
{

	subscriptionPtr subscription = (subscriptionPtr)userdata;
	subscription->activeAuth = FALSE;

	if (response_id == GTK_RESPONSE_OK) {
		subscription_set_auth_info (subscription,
		                            liferea_dialog_entry_get (GTK_WIDGET (dialog), "usernameEntry"),
		                            liferea_dialog_entry_get (GTK_WIDGET (dialog), "passwordEntry"));
		subscription_update (subscription, UPDATE_REQUEST_PRIORITY_HIGH);
	}

	gtk_window_close (GTK_WINDOW (dialog));
}

void
auth_dialog_new (subscriptionPtr subscription, gint unused)
{
	GtkWidget		*dialog;
	gchar			*promptStr;
	gchar			*source = NULL;
	xmlURIPtr		uri;

	if (subscription->activeAuth)
		debug (DEBUG_UPDATE, "Missing/wrong authentication. Skipping, as a dialog is already active.");
	subscription->activeAuth = TRUE;

	dialog = liferea_dialog_new ("auth");
	gtk_window_present (GTK_WINDOW (dialog));
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

	promptStr = g_strdup_printf ( _("Enter the username and password for \"%s\" (%s):"),
	                             node_get_title (subscription->node),
	                             source?source:_("Unknown source"));
	gtk_label_set_text (GTK_LABEL (liferea_dialog_lookup (dialog, "prompt")), promptStr);
	g_free (promptStr);
	if (source)
		xmlFree (source);
}