/**
 * @file subscription_dialog.c  property dialog for feed subscriptions
 *
 * Copyright (C) 2004-2025 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include "ui/subscription_dialog.h"

#include <libxml/uri.h>
#include <string.h>

#include "common.h"
#include "conf.h"
#include "db.h"
#include "debug.h"
#include "node_providers/feed.h"
#include "feedlist.h"
#include "node.h"
#include "update.h"
#include "ui/feed_list_view.h"
#include "ui/liferea_dialog.h"
#include "ui/ui_common.h"

/* Note: these update interval literals should be kept in sync with the
   ones in ui_prefs.c! */

static const gchar * default_update_interval_unit_options[] = {
	N_("minutes"),
	N_("hours"),
	N_("days"),
	NULL
};

/* Common dialog data fields */
typedef struct ui_data {
	gint selector; /* Desiginates which fileselection dialog box is open.
				   Set to 'u' for source
				   Set to 'f' for filter */
} dialogData;

/* properties dialog */

static gchar *
ui_subscription_create_url (gchar *url,
                            gboolean auth,
			    const gchar *username,
			    const gchar *password)
{
	gchar	*source = NULL;
	gchar *str, *tmp2;

	/* First, strip leading and trailing whitespace */
	str = g_strstrip(url);

	/* Add https:// if needed */
	if (strstr(str, "://") == NULL) {
		tmp2 = g_strdup_printf("https://%s",str);
		g_free(str);
		str = tmp2;
	}

	/* Add trailing / if needed */
	if (strstr(strstr(str, "://") + 3, "/") == NULL) {
		tmp2 = g_strdup_printf("%s/", str);
		g_free(str);
		str = tmp2;
	}

	/* Use the values in the textboxes if also specified in the URL! */
	if (auth) {
		xmlURIPtr uri = xmlParseURI(str);
		if (uri != NULL) {
			xmlChar *sourceUrl;
			xmlFree(uri->user);
			uri->user = g_strdup_printf("%s:%s", username, password);
			sourceUrl = xmlSaveUri(uri);
			source = g_strdup((gchar *) sourceUrl);
			g_free(uri->user);
			uri->user = NULL;
			xmlFree(sourceUrl);
			xmlFreeURI(uri);
		} else
			source = g_strdup(str);
	} else {
		source = g_strdup(str);
	}
	g_free(str);

	return source;
}

static gchar *
ui_subscription_dialog_decode_source (GtkWidget *dialog)
{
	const	gchar	*entry = liferea_dialog_entry_get (dialog, "sourceEntry");
	gchar		*source = NULL;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "feed_loc_file"))))
		source = g_strdup (entry);

	else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "feed_loc_url"))))
		source = ui_subscription_create_url (g_strdup (entry),
		                                     gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "HTTPauthCheck"))),
		                                     liferea_dialog_entry_get (dialog, "usernameEntry"),
		                                     liferea_dialog_entry_get (dialog, "passwordEntry"));

	else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "feed_loc_cmd"))))
		source = g_strdup_printf ("|%s", entry);

	return source;
}

static void
on_dialog_response (GtkDialog *d, gint response_id, gpointer user_data)
{
	GtkWidget		*dialog = GTK_WIDGET (d);
	subscriptionPtr		subscription = (subscriptionPtr)user_data;
	g_autofree gchar	*newSource;
	const gchar		*newFilter;
	gboolean		needsUpdate = FALSE;
	Node			*node = subscription->node;

	if (response_id != GTK_RESPONSE_OK)
		return;

	/* "General" */
	node_set_title (node, liferea_dialog_entry_get (dialog, "feedNameEntry"));

	/* "Source" (if URL has changed...) */
	newSource = ui_subscription_dialog_decode_source (dialog);
	if (newSource && !g_str_equal (newSource, subscription_get_source (subscription))) {
		subscription_set_source (subscription, newSource);
		subscription_set_discontinued (subscription, FALSE);
		needsUpdate = TRUE;
	}

	/* Update interval handling */
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "updateIntervalNever"))))
		subscription_set_update_interval (subscription, -2);
	else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "updateIntervalDefault"))))
		subscription_set_update_interval (subscription, -1);
	else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "updateIntervalSpecific")))) {
		gint intervalUnit = gtk_combo_box_get_active (GTK_COMBO_BOX (liferea_dialog_lookup (dialog, "refreshIntervalUnitComboBox")));
		gint updateInterval = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (liferea_dialog_lookup (dialog, "refreshIntervalSpinButton")));
		if (intervalUnit == 1)
			updateInterval *= 60;	/* hours */
		if (intervalUnit == 2)
			updateInterval *= 1440;	/* days */

		subscription_set_update_interval (subscription, updateInterval);
	}

	if (SUBSCRIPTION_TYPE (subscription) == feed_get_subscription_type ()) {
		feedPtr	feed = (feedPtr)node->data;

		/* Filter handling */
		newFilter = liferea_dialog_entry_get (dialog, "filterEntry");
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "filterCheckbox"))) &&
			strcmp(newFilter,"")) { /* Maybe this should be a test to see if the file exists? */
			if (subscription_get_filter (subscription) == NULL ||
			    strcmp (newFilter, subscription_get_filter (subscription))) {
				subscription_set_filter (subscription, newFilter);
				needsUpdate = TRUE;
			}
		} else {
			if (subscription_get_filter (subscription)) {
				subscription_set_filter (subscription, NULL);
				needsUpdate = TRUE;
			}
		}

		/* "Archive" handling */
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "feedCacheDefault"))))
			feed->cacheLimit = CACHE_DEFAULT;
		else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "feedCacheDisable"))))
			feed->cacheLimit = CACHE_DISABLE;
		else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "feedCacheUnlimited"))))
			feed->cacheLimit = CACHE_UNLIMITED;
		else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "feedCacheLimited"))))
			feed->cacheLimit = gtk_spin_button_get_value (GTK_SPIN_BUTTON (liferea_dialog_lookup (dialog, "cacheItemLimit")));

		if (SUBSCRIPTION_TYPE (subscription) == feed_get_subscription_type ()) {
			/* "Download" Options */
			subscription->updateOptions->dontUseProxy = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET(dialog), "dontUseProxyCheck")));
		}

		/* "Advanced" options */
		feed->encAutoDownload = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "enclosureDownloadCheck")));
		feed->loadItemLink    = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "loadItemLinkCheck")));
		feed->ignoreComments  = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "ignoreCommentFeeds")));
		feed->markAsRead      = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "markAsReadCheck")));
		feed->html5Extract    = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "html5ExtractCheck")));
	}

	feedlist_node_was_updated (node);
	db_subscription_update (subscription);
	if (needsUpdate)
		subscription_update (subscription, UPDATE_REQUEST_PRIORITY_HIGH);
}

static void
on_feed_prop_filtercheck (GtkToggleButton *button, gpointer user_data)
{
	gtk_widget_set_sensitive (liferea_dialog_lookup (GTK_WIDGET (user_data), "filterbox"), gtk_toggle_button_get_active (button));
}

static void
ui_subscription_prop_enable_httpauth (GtkWidget *dialog, gboolean enable)
{
	// FIXME: very weird logic
	gboolean on = enable && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "HTTPauthCheck")));
	gtk_widget_set_sensitive (liferea_dialog_lookup (dialog, "HTTPauthCheck"), enable);
	gtk_widget_set_sensitive (liferea_dialog_lookup (dialog, "httpAuthBox"), on);
}

static void
on_feed_prop_authcheck (GtkToggleButton *button, gpointer user_data)
{
	GtkWidget *dialog = GTK_WIDGET (user_data);

	gboolean isUrl = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "feed_loc_url")));

	ui_subscription_prop_enable_httpauth (dialog, isUrl);
}

static void
on_feed_prop_url_radio (GtkToggleButton *button, gpointer user_data)
{
	GtkWidget *dialog = GTK_WIDGET (user_data);
	gboolean url  = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "feed_loc_url")));
	gboolean file = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "feed_loc_file")));
	gboolean cmd  = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "feed_loc_cmd")));

	ui_subscription_prop_enable_httpauth (dialog, url);
	gtk_widget_set_sensitive (liferea_dialog_lookup (dialog, "selectSourceFileButton"), file);
	gtk_widget_set_sensitive (liferea_dialog_lookup (dialog, "filterSelectFile"), cmd);
}

static void
on_select_local_file_cb (const gchar *filename, gpointer user_data)
{
	g_autofree gchar *utfname;

	if (!filename)
		return;

	utfname = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
	if (utfname)
		liferea_dialog_entry_set (GTK_WIDGET (user_data), "sourceEntry", utfname);
}

static void
on_select_filter_cb (const gchar *filename, gpointer user_data)
{
	g_autofree gchar *utfname;

	if (!filename)
		return;

	utfname = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
	if (utfname)
		liferea_dialog_entry_set (GTK_WIDGET (user_data), "filterEntry", utfname);
}

static void
on_select_local_file_pressed (GtkButton *button, gpointer user_data)
{
	g_autofree gchar *name = g_filename_from_utf8 (liferea_dialog_entry_get (GTK_WIDGET (user_data), "sourceEntry"), -1, NULL, NULL, NULL);
	ui_choose_file (_("Choose File"), _("_Open"), FALSE, on_select_local_file_cb, name, NULL, NULL, NULL, user_data);
}

static void
on_select_filter_pressed (GtkButton *button, gpointer user_data)
{
	g_autofree gchar *name = g_filename_from_utf8 (liferea_dialog_entry_get (GTK_WIDGET (user_data), "filterEntry"), -1, NULL, NULL, NULL);
	ui_choose_file (_("Choose File"), _("_Open"), FALSE, on_select_filter_cb, name, NULL, NULL, NULL, user_data);
}

static void
on_feed_prop_cache_radio (GtkToggleButton *button, gpointer user_data)
{
	gboolean limited = gtk_toggle_button_get_active (button);

	gtk_widget_set_sensitive (liferea_dialog_lookup (GTK_WIDGET (user_data), "cacheItemLimit"), limited);
}

static void
on_feed_prop_update_radio (GtkToggleButton *button, gpointer user_data)
{
	gboolean limited = gtk_toggle_button_get_active (button);

	gtk_widget_set_sensitive (liferea_dialog_lookup (GTK_WIDGET (user_data), "refreshIntervalSpinButton"), limited);
	gtk_widget_set_sensitive (liferea_dialog_lookup (GTK_WIDGET (user_data), "refreshIntervalUnitComboBox"), limited);
}

static void
subscription_prop_dialog_load (GtkWidget *dialog,
                               subscriptionPtr subscription)
{
	gint 		interval;
	gint		default_update_interval;
	gint		defaultInterval, spinSetInterval;
	gchar 		*defaultIntervalStr;
	Node		*node = subscription->node;
	feedPtr		feed = (feedPtr)node->data;

	/* General */
	liferea_dialog_entry_set (dialog, "feedNameEntry", node_get_title (node));

	interval = subscription_get_update_interval (subscription);
	defaultInterval = subscription_get_default_update_interval (subscription);
	conf_get_int_value (DEFAULT_UPDATE_INTERVAL, &default_update_interval);
	spinSetInterval = defaultInterval > 0 ? defaultInterval : default_update_interval;

	if (-2 >= interval) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "updateIntervalNever")), TRUE);
	} else if (-1 == interval) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "updateIntervalDefault")), TRUE);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "updateIntervalSpecific")), TRUE);
		spinSetInterval = interval;
	}

	/* Set refresh interval spin button and combo box */
	if (spinSetInterval % 1440 == 0) {	/* days */
		gtk_combo_box_set_active (GTK_COMBO_BOX (liferea_dialog_lookup (dialog, "refreshIntervalUnitComboBox")), 2);
		spinSetInterval /= 1440;
	} else if (spinSetInterval % 60 == 0) {	/* hours */
		gtk_combo_box_set_active (GTK_COMBO_BOX (liferea_dialog_lookup (dialog, "refreshIntervalUnitComboBox")), 1);
		spinSetInterval /= 60;
	} else {
		gtk_combo_box_set_active (GTK_COMBO_BOX (liferea_dialog_lookup (dialog, "refreshIntervalUnitComboBox")), 0);
	}
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (liferea_dialog_lookup (dialog, "refreshIntervalSpinButton")), spinSetInterval);

	gtk_widget_set_sensitive (liferea_dialog_lookup (dialog, "refreshIntervalSpinButton"), interval > 0);
	gtk_widget_set_sensitive (liferea_dialog_lookup (dialog, "refreshIntervalUnitComboBox"), interval > 0);

	/* setup info label about default update interval */
	if (-1 != defaultInterval)
		defaultIntervalStr = g_strdup_printf (ngettext ("The provider of this feed suggests an update interval of %d minute.",
		                                               "The provider of this feed suggests an update interval of %d minutes.",
		                                               defaultInterval), defaultInterval);
	else
		defaultIntervalStr = g_strdup (_("This feed specifies no default update interval."));

	gtk_label_set_text (GTK_LABEL (liferea_dialog_lookup (dialog, "feedUpdateInfo")), defaultIntervalStr);
	g_free (defaultIntervalStr);

	/* Source (only for feeds) */
	if (SUBSCRIPTION_TYPE (subscription) == feed_get_subscription_type ()) {
		if (subscription_get_source (subscription)[0] == '|') {
			liferea_dialog_entry_set (dialog, "sourceEntry", &(subscription_get_source (subscription)[1]));
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "feed_loc_command")), TRUE);
			ui_subscription_prop_enable_httpauth (dialog, FALSE);
			gtk_widget_set_sensitive (liferea_dialog_lookup (dialog, "selectSourceFileButton"), TRUE);
		} else if (strstr (subscription_get_source (subscription), "://") != NULL) {
			xmlURIPtr uri = xmlParseURI (subscription_get_source (subscription));
			xmlChar *parsedUrl;
			if (uri) {
				if (uri->user) {
					gchar *user = uri->user;
					gchar *pass = strstr(user, ":");
					if (pass) {
						pass[0] = '\0';
						pass++;
						liferea_dialog_entry_set (dialog, "passwordEntry", pass);
					}
					liferea_dialog_entry_set (dialog, "usernameEntry", user);
					xmlFree (uri->user);
					uri->user = NULL;
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "HTTPauthCheck")), TRUE);
				}
				parsedUrl = xmlSaveUri (uri);
				liferea_dialog_entry_set (dialog, "sourceEntry", (gchar *) parsedUrl);
				xmlFree (parsedUrl);
				xmlFreeURI (uri);
			} else {
				liferea_dialog_entry_set (dialog, "sourceEntry", subscription_get_source (subscription));
			}
			ui_subscription_prop_enable_httpauth (dialog, TRUE);
			gtk_widget_set_sensitive (liferea_dialog_lookup (dialog, "selectSourceFileButton"), FALSE);
		} else {
			/* File */
			liferea_dialog_entry_set (dialog, "sourceEntry", subscription_get_source (subscription));
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "feed_loc_file")), TRUE);
			
			ui_subscription_prop_enable_httpauth (dialog, FALSE);
			gtk_widget_set_sensitive (liferea_dialog_lookup (dialog, "selectSourceFileButton"), TRUE);
		}

		if (subscription_get_filter (subscription)) {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "filterCheckbox")), TRUE);
			liferea_dialog_entry_set (dialog, "filterEntry", subscription_get_filter (subscription));
		}
	}

	/* Archive */
	if (feed->cacheLimit == CACHE_DISABLE) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "feedCacheDisable")), TRUE);
	} else if (feed->cacheLimit == CACHE_DEFAULT) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "feedCacheDefault")), TRUE);
	} else if (feed->cacheLimit == CACHE_UNLIMITED) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "feedCacheUnlimited")), TRUE);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "feedCacheLimited")), TRUE);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (liferea_dialog_lookup (dialog, "cacheItemLimit")), feed->cacheLimit);
	}

	gtk_widget_set_sensitive (liferea_dialog_lookup (dialog, "cacheItemLimit"), feed->cacheLimit > 0);

	on_feed_prop_filtercheck (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "filterCheckbox")), dialog);

	/* Download */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "dontUseProxyCheck")), subscription->updateOptions->dontUseProxy);

	/* Advanced */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "enclosureDownloadCheck")), feed->encAutoDownload);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "loadItemLinkCheck")), feed->loadItemLink);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "ignoreCommentFeeds")), feed->ignoreComments);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "markAsReadCheck")), feed->markAsRead);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "html5ExtractCheck")), feed->html5Extract);

	/* Remove tabs we do not need... */
	if (SUBSCRIPTION_TYPE (subscription) != feed_get_subscription_type ()) {
		/* Remove "General", "Source" and "Download" tab */
		gtk_notebook_remove_page (GTK_NOTEBOOK (liferea_dialog_lookup (dialog, "subscriptionPropNotebook")), 0);
		gtk_notebook_remove_page (GTK_NOTEBOOK (liferea_dialog_lookup (dialog, "subscriptionPropNotebook")), 0);
		gtk_notebook_remove_page (GTK_NOTEBOOK (liferea_dialog_lookup (dialog, "subscriptionPropNotebook")), 1);
	}
}

void
subscription_prop_dialog_new (subscriptionPtr subscription)
{
	GtkWidget *dialog = liferea_dialog_new ("properties");

	/* set default update interval spin button and unit combo box */
	ui_common_setup_combo_menu (liferea_dialog_lookup (dialog, "refreshIntervalUnitComboBox"),
	                            default_update_interval_unit_options,
	                            NULL /* no callback */,
	                            -1 /* default value */ );

	g_signal_connect (liferea_dialog_lookup (dialog, "selectSourceFileButton"), "clicked", G_CALLBACK (on_select_local_file_pressed), dialog);
	g_signal_connect (liferea_dialog_lookup (dialog, "feed_loc_url"), "toggled", G_CALLBACK (on_feed_prop_url_radio), dialog);
	g_signal_connect (liferea_dialog_lookup (dialog, "feed_loc_file"), "toggled", G_CALLBACK (on_feed_prop_url_radio), dialog);
	g_signal_connect (liferea_dialog_lookup (dialog, "feed_loc_command"), "toggled", G_CALLBACK (on_feed_prop_url_radio), dialog);
	g_signal_connect (liferea_dialog_lookup (dialog, "HTTPauthCheck"), "toggled", G_CALLBACK (on_feed_prop_authcheck), dialog);

	g_signal_connect (liferea_dialog_lookup (dialog, "filterCheckbox"), "toggled", G_CALLBACK (on_feed_prop_filtercheck), dialog);
	g_signal_connect (liferea_dialog_lookup (dialog, "filterSelectFile"), "clicked", G_CALLBACK (on_select_filter_pressed), dialog);
	g_signal_connect (liferea_dialog_lookup (dialog, "feedCacheLimited"), "toggled", G_CALLBACK (on_feed_prop_cache_radio), dialog);
	g_signal_connect (liferea_dialog_lookup (dialog, "updateIntervalSpecific"), "toggled", G_CALLBACK(on_feed_prop_update_radio), dialog);

	subscription_prop_dialog_load (dialog, subscription);

	g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (on_dialog_response), (gpointer)subscription);
}

/* complex "New" dialog */

static void
on_complex_dialog_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
	if (response_id == GTK_RESPONSE_OK) {
		g_autofree gchar *source = NULL;
		const gchar *filter = NULL;
		updateOptionsPtr options;

		/* Source */
		source = ui_subscription_dialog_decode_source (GTK_WIDGET (dialog));

		/* Filter handling */
		filter = liferea_dialog_entry_get (GTK_WIDGET (dialog), "filterEntry");
		if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET (dialog), "filterCheckbox"))) ||
		    !strcmp(filter,"")) { /* Maybe this should be a test to see if the file exists? */
			filter = NULL;
		}

		options = g_new0 (struct updateOptions, 1);
		options->dontUseProxy = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET (dialog), "dontUseProxyCheck")));

		feedlist_add_subscription_check_duplicate (source, filter, options, UPDATE_REQUEST_PRIORITY_HIGH);
	}
}

static gboolean
subscription_dialog_complex_new (gpointer unused)
{
	GtkWidget *widget, *dialog = liferea_dialog_new ("new_subscription");

	widget = liferea_dialog_lookup (dialog, "sourceEntry");
	gtk_widget_grab_focus (GTK_WIDGET (widget));
	gtk_entry_set_activates_default (GTK_ENTRY (widget), TRUE);

	g_signal_connect (liferea_dialog_lookup (dialog, "selectSourceFileButton"), "clicked", G_CALLBACK (on_select_local_file_pressed), dialog);

	/* Feed location radio buttons */
	g_signal_connect (liferea_dialog_lookup (dialog, "feed_loc_file"), "toggled", G_CALLBACK (on_feed_prop_url_radio), dialog);
	g_signal_connect (liferea_dialog_lookup (dialog, "feed_loc_url"), "toggled", G_CALLBACK (on_feed_prop_url_radio), dialog);
	g_signal_connect (liferea_dialog_lookup (dialog, "feed_loc_command"), "toggled", G_CALLBACK (on_feed_prop_url_radio), dialog);

	g_signal_connect (liferea_dialog_lookup (dialog, "filterCheckbox"), "toggled", G_CALLBACK (on_feed_prop_filtercheck), dialog);
	g_signal_connect (liferea_dialog_lookup (dialog, "filterSelectFile"), "clicked", G_CALLBACK (on_select_filter_pressed), dialog);

	gtk_window_set_default_widget (GTK_WINDOW (dialog), liferea_dialog_lookup (dialog, "newfeedbtn"));
	g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (on_complex_dialog_response), dialog);

	on_feed_prop_filtercheck (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "filterCheckbox")), dialog);
	on_feed_prop_url_radio (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "feed_loc_url")), dialog);

	return FALSE;
}

/* simple "New" dialog */

static void
on_simple_dialog_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
	g_autofree gchar *source = NULL;

	if (response_id == GTK_RESPONSE_OK) {
		source = ui_subscription_create_url (g_strdup (liferea_dialog_entry_get (GTK_WIDGET (dialog), "sourceEntry")),
		                                      FALSE /* auth */, NULL /* user */, NULL /* passwd */);

		feedlist_add_subscription_check_duplicate (source, NULL, NULL, UPDATE_REQUEST_PRIORITY_HIGH);
	}

	/* APPLY code misused for "Advanced" */
	if (response_id == GTK_RESPONSE_APPLY)
		g_idle_add (subscription_dialog_complex_new, NULL);
}

void
subscription_dialog_new (void)
{
	GtkWidget *dialog = liferea_dialog_new ("simple_subscription");
	GtkWidget *entry = liferea_dialog_lookup (dialog, "sourceEntry");

	gtk_widget_grab_focus (entry);
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);

	g_signal_connect (G_OBJECT (dialog), "response",
	                  G_CALLBACK (on_simple_dialog_response), NULL);
}
