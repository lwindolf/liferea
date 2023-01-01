/**
 * @file subscription_dialog.c  property dialog for feed subscriptions
 *
 * Copyright (C) 2004-2018 Lars Windolf <lars.windolf@gmx.de>
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
#include "feed.h"
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

	GtkWidget *dialog;
	GtkWidget *feedNameEntry;
	GtkWidget *refreshInterval;
	GtkWidget *refreshIntervalUnit;
	GtkWidget *sourceEntry;
	GtkWidget *selectFile;
	GtkWidget *fileRadio;
	GtkWidget *urlRadio;
	GtkWidget *cmdRadio;
	GtkWidget *authcheckbox;
	GtkWidget *credTable;
	GtkWidget *username;
	GtkWidget *password;
} dialogData;

struct _SubscriptionPropDialog {
	GObject	parentInstance;

	subscriptionPtr subscription;	/** used only for "properties" dialog */
	dialogData		ui_data;
};

struct _NewSubscriptionDialog {
	GObject	parentInstance;

	subscriptionPtr subscription;	/** used only for "properties" dialog */
	dialogData		ui_data;
};

struct _SimpleSubscriptionDialog {
	GObject	parentInstance;

	subscriptionPtr subscription;	/** used only for "properties" dialog */
	dialogData		ui_data;
};

/* properties dialog */

G_DEFINE_TYPE (SubscriptionPropDialog, subscription_prop_dialog, G_TYPE_OBJECT);

static void
subscription_prop_dialog_finalize (GObject *object)
{
	SubscriptionPropDialog *spd = SUBSCRIPTION_PROP_DIALOG (object);

	gtk_widget_destroy (spd->ui_data.dialog);
}

static void
subscription_prop_dialog_class_init (SubscriptionPropDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = subscription_prop_dialog_finalize;
}

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
ui_subscription_dialog_decode_source (dialogData *ui_data)
{
	gchar	*source = NULL;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ui_data->fileRadio)))
		source = g_strdup (gtk_entry_get_text (GTK_ENTRY (ui_data->sourceEntry)));

	else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ui_data->urlRadio)))
		source = ui_subscription_create_url (g_strdup (gtk_entry_get_text (GTK_ENTRY (ui_data->sourceEntry))),
		                                     ui_data->authcheckbox &&
		                                     gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ui_data->authcheckbox)),
		                                     ui_data->username?gtk_entry_get_text (GTK_ENTRY (ui_data->username)):NULL,
		                                     ui_data->password?gtk_entry_get_text (GTK_ENTRY (ui_data->password)):NULL);
	else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ui_data->cmdRadio)))
		source = g_strdup_printf ("|%s", gtk_entry_get_text (GTK_ENTRY (ui_data->sourceEntry)));

	return source;
}

static void
on_propdialog_response (GtkDialog *dialog,
                        gint response_id,
			gpointer user_data)
{
	SubscriptionPropDialog *spd = (SubscriptionPropDialog *)user_data;

	if (response_id == GTK_RESPONSE_OK) {
		gchar		*newSource;
		const gchar	*newFilter;
		gboolean	needsUpdate = FALSE;
		subscriptionPtr	subscription = spd->subscription;
		nodePtr		node = spd->subscription->node;
		feedPtr		feed = (feedPtr)node->data;

		if (SUBSCRIPTION_TYPE(subscription) == feed_get_subscription_type ()) {
			/* "General" */
			node_set_title(node, gtk_entry_get_text(GTK_ENTRY(spd->ui_data.feedNameEntry)));

			/* Source */
			newSource = ui_subscription_dialog_decode_source(&(spd->ui_data));

			/* Filter handling */
			newFilter = gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (spd->ui_data.dialog, "filterEntry")));
			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (liferea_dialog_lookup (spd->ui_data.dialog, "filterCheckbox"))) &&
			   strcmp(newFilter,"")) { /* Maybe this should be a test to see if the file exists? */
				if (subscription_get_filter(subscription) == NULL ||
				   strcmp(newFilter, subscription_get_filter(subscription))) {
					subscription_set_filter(subscription, newFilter);
					needsUpdate = TRUE;
				}
			} else {
				if (subscription_get_filter(subscription)) {
					subscription_set_filter(subscription, NULL);
					needsUpdate = TRUE;
				}
			}

			/* if URL has changed... */
			if (strcmp(newSource, subscription_get_source(subscription))) {
				subscription_set_source(subscription, newSource);
				subscription_set_discontinued (subscription, FALSE);
				needsUpdate = TRUE;
			}
			g_free(newSource);

			/* Update interval handling */
			if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET (dialog), "updateIntervalNever"))))
				subscription_set_update_interval (subscription, -2);
			else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET (dialog), "updateIntervalDefault"))))
				subscription_set_update_interval (subscription, -1);
			else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET (dialog), "updateIntervalSpecific")))) {
				gint intervalUnit = gtk_combo_box_get_active (GTK_COMBO_BOX (spd->ui_data.refreshIntervalUnit));
				gint updateInterval = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spd->ui_data.refreshInterval));
				if (intervalUnit == 1)
					updateInterval *= 60;	/* hours */
				if (intervalUnit == 2)
					updateInterval *= 1440;	/* days */

				subscription_set_update_interval (subscription, updateInterval);
				db_subscription_update (subscription);
			}
		}

		/* "Archive" handling */
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET(dialog), "feedCacheDefault"))))
			feed->cacheLimit = CACHE_DEFAULT;
		else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET(dialog), "feedCacheDisable"))))
			feed->cacheLimit = CACHE_DISABLE;
		else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET(dialog), "feedCacheUnlimited"))))
			feed->cacheLimit = CACHE_UNLIMITED;
		else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET(dialog), "feedCacheLimited"))))
			feed->cacheLimit = gtk_spin_button_get_value(GTK_SPIN_BUTTON (liferea_dialog_lookup (GTK_WIDGET(dialog), "cacheItemLimit")));

		if (SUBSCRIPTION_TYPE(subscription) == feed_get_subscription_type ()) {
			/* "Download" Options */
			subscription->updateOptions->dontUseProxy = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET(dialog), "dontUseProxyCheck")));
		}

		/* "Advanced" options */
		feed->encAutoDownload = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET (dialog), "enclosureDownloadCheck")));
		node->loadItemLink    = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET (dialog), "loadItemLinkCheck")));
		feed->ignoreComments  = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET (dialog), "ignoreCommentFeeds")));
		feed->markAsRead      = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET (dialog), "markAsReadCheck")));
		feed->html5Extract    = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET (dialog), "html5ExtractCheck")));

		feed_list_view_update_node (node->id);
		feedlist_schedule_save ();
		db_subscription_update (subscription);
		if (needsUpdate)
			subscription_update (subscription, FEED_REQ_PRIORITY_HIGH);
	}

	g_object_unref(spd);
}

static void
on_feed_prop_filtercheck (GtkToggleButton *button,
                          gpointer user_data)
{
	dialogData *ui_data = (dialogData *)user_data;

	gboolean filter = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (ui_data->dialog, "filterCheckbox")));
	gtk_widget_set_sensitive (liferea_dialog_lookup (ui_data->dialog, "filterbox"), filter);
}

static void
ui_subscription_prop_enable_httpauth (dialogData *ui_data, gboolean enable)
{
	gboolean on;

	if (ui_data->authcheckbox) {
		on = enable && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ui_data->authcheckbox));
		gtk_widget_set_sensitive (ui_data->authcheckbox, enable);
		gtk_widget_set_sensitive (ui_data->credTable, on);
	}
}

static void
on_feed_prop_authcheck (GtkToggleButton *button, gpointer user_data)
{
	dialogData *ui_data = (dialogData *)user_data;
	gboolean url = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ui_data->urlRadio));

	ui_subscription_prop_enable_httpauth (ui_data, url);
}

static void
on_feed_prop_url_radio (GtkToggleButton *button, gpointer user_data)
{
	dialogData *ui_data = (dialogData *)user_data;
	gboolean url  = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ui_data->urlRadio));
	gboolean file = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ui_data->fileRadio));
	gboolean cmd  = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ui_data->cmdRadio));

	ui_subscription_prop_enable_httpauth (ui_data, url);
	gtk_widget_set_sensitive (ui_data->selectFile, file || cmd);
}

static void
on_selectfileok_clicked (const gchar *filename, gpointer user_data)
{
	dialogData *ui_data = (dialogData *)user_data;
	gchar *utfname;

	if (!filename)
		return;

	utfname = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);

	if (utfname) {
		if (ui_data->selector == 'u')
			gtk_entry_set_text (GTK_ENTRY (ui_data->sourceEntry), utfname);
		else
			gtk_entry_set_text (GTK_ENTRY (liferea_dialog_lookup (ui_data->dialog, "filterEntry")), utfname);
	}

	g_free (utfname);
}

static void
on_selectfile_pressed (GtkButton *button, gpointer user_data)
{
	dialogData *ui_data = (dialogData *)user_data;
	const gchar *utfname;
	gchar *name;

	if (GTK_WIDGET (button) == ui_data->selectFile) {
		ui_data->selector = 'u';
		utfname = gtk_entry_get_text (GTK_ENTRY (ui_data->sourceEntry));
	} else {
		ui_data->selector = 'f';
		utfname = gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (ui_data->dialog, "filterEntry")));
	}

	name = g_filename_from_utf8 (utfname, -1, NULL, NULL, NULL);
	ui_choose_file (_("Choose File"), _("_Open"), FALSE, on_selectfileok_clicked, name, NULL, NULL, NULL, ui_data);
	g_free (name);
}

static void
on_feed_prop_cache_radio (GtkToggleButton *button, gpointer user_data)
{
	dialogData *ui_data = (dialogData *)user_data;
	gboolean limited = gtk_toggle_button_get_active (button);

	gtk_widget_set_sensitive (liferea_dialog_lookup (GTK_WIDGET (ui_data->dialog), "cacheItemLimit"), limited);
}

static void
on_feed_prop_update_radio (GtkToggleButton *button, gpointer user_data)
{
	dialogData *ui_data = (dialogData *)user_data;
	gboolean limited = gtk_toggle_button_get_active (button);

	gtk_widget_set_sensitive (ui_data->refreshInterval, limited);
	gtk_widget_set_sensitive (ui_data->refreshIntervalUnit, limited);
}

static void
subscription_prop_dialog_load (SubscriptionPropDialog *spd,
                               subscriptionPtr subscription)
{
	gint 		interval;
	gint		default_update_interval;
	gint		defaultInterval, spinSetInterval;
	gchar 		*defaultIntervalStr;
	nodePtr		node = subscription->node;
	feedPtr		feed = (feedPtr)node->data;

	spd->subscription = subscription;

	/* General */
	gtk_entry_set_text (GTK_ENTRY (spd->ui_data.feedNameEntry), node_get_title (node));

	spd->ui_data.refreshInterval = liferea_dialog_lookup (spd->ui_data.dialog, "refreshIntervalSpinButton");

	interval = subscription_get_update_interval (subscription);
	defaultInterval = subscription_get_default_update_interval (subscription);
	conf_get_int_value (DEFAULT_UPDATE_INTERVAL, &default_update_interval);
	spinSetInterval = defaultInterval > 0 ? defaultInterval : default_update_interval;

	if (-2 >= interval) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (spd->ui_data.dialog, "updateIntervalNever")), TRUE);
	} else if (-1 == interval) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (spd->ui_data.dialog, "updateIntervalDefault")), TRUE);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (spd->ui_data.dialog, "updateIntervalSpecific")), TRUE);
		spinSetInterval = interval;
	}

	/* Set refresh interval spin button and combo box */
	if (spinSetInterval % 1440 == 0) {	/* days */
		gtk_combo_box_set_active (GTK_COMBO_BOX (spd->ui_data.refreshIntervalUnit), 2);
		spinSetInterval /= 1440;
	} else if (spinSetInterval % 60 == 0) {	/* hours */
		gtk_combo_box_set_active (GTK_COMBO_BOX (spd->ui_data.refreshIntervalUnit), 1);
		spinSetInterval /= 60;
	} else {
		gtk_combo_box_set_active (GTK_COMBO_BOX (spd->ui_data.refreshIntervalUnit), 0);
	}
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (spd->ui_data.refreshInterval), spinSetInterval);

	gtk_widget_set_sensitive (spd->ui_data.refreshInterval, interval > 0);
	gtk_widget_set_sensitive (spd->ui_data.refreshIntervalUnit, interval > 0);

	/* setup info label about default update interval */
	if (-1 != defaultInterval)
		defaultIntervalStr = g_strdup_printf (ngettext ("The provider of this feed suggests an update interval of %d minute.",
		                                               "The provider of this feed suggests an update interval of %d minutes.",
		                                               defaultInterval), defaultInterval);
	else
		defaultIntervalStr = g_strdup (_("This feed specifies no default update interval."));

	gtk_label_set_text (GTK_LABEL (liferea_dialog_lookup (spd->ui_data.dialog, "feedUpdateInfo")), defaultIntervalStr);
	g_free (defaultIntervalStr);

	/* Source (only for feeds) */
	if (SUBSCRIPTION_TYPE (subscription) == feed_get_subscription_type ()) {
		if (subscription_get_source (subscription)[0] == '|') {
			gtk_entry_set_text (GTK_ENTRY (spd->ui_data.sourceEntry), &(subscription_get_source (subscription)[1]));
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (spd->ui_data.cmdRadio), TRUE);
			ui_subscription_prop_enable_httpauth (&(spd->ui_data), FALSE);
			gtk_widget_set_sensitive (spd->ui_data.selectFile, TRUE);
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
						gtk_entry_set_text (GTK_ENTRY (spd->ui_data.password), pass);
					}
					gtk_entry_set_text (GTK_ENTRY (spd->ui_data.username), user);
					xmlFree (uri->user);
					uri->user = NULL;
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (spd->ui_data.authcheckbox), TRUE);
				}
				parsedUrl = xmlSaveUri (uri);
				gtk_entry_set_text (GTK_ENTRY(spd->ui_data.sourceEntry), (gchar *) parsedUrl);
				xmlFree (parsedUrl);
				xmlFreeURI (uri);
			} else {
				gtk_entry_set_text (GTK_ENTRY (spd->ui_data.sourceEntry), subscription_get_source (subscription));
			}
			ui_subscription_prop_enable_httpauth (&(spd->ui_data), TRUE);
			gtk_widget_set_sensitive (spd->ui_data.selectFile, FALSE);
		} else {
			/* File */
			gtk_entry_set_text (GTK_ENTRY (spd->ui_data.sourceEntry), subscription_get_source (subscription));
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (spd->ui_data.fileRadio), TRUE);
			ui_subscription_prop_enable_httpauth (&(spd->ui_data), FALSE);
			gtk_widget_set_sensitive (spd->ui_data.selectFile, TRUE);
		}

		if (subscription_get_filter (subscription)) {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (spd->ui_data.dialog, "filterCheckbox")), TRUE);
			gtk_entry_set_text (GTK_ENTRY (liferea_dialog_lookup (spd->ui_data.dialog, "filterEntry")), subscription_get_filter (subscription));
		}
	}

	/* Archive */
	if (feed->cacheLimit == CACHE_DISABLE) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (spd->ui_data.dialog, "feedCacheDisable")), TRUE);
	} else if (feed->cacheLimit == CACHE_DEFAULT) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (spd->ui_data.dialog, "feedCacheDefault")), TRUE);
	} else if (feed->cacheLimit == CACHE_UNLIMITED) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (spd->ui_data.dialog, "feedCacheUnlimited")), TRUE);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (spd->ui_data.dialog, "feedCacheLimited")), TRUE);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (liferea_dialog_lookup (spd->ui_data.dialog, "cacheItemLimit")), feed->cacheLimit);
	}

	gtk_widget_set_sensitive (liferea_dialog_lookup (spd->ui_data.dialog, "cacheItemLimit"), feed->cacheLimit > 0);

	on_feed_prop_filtercheck (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (spd->ui_data.dialog, "filterCheckbox")), &(spd->ui_data));

	/* Download */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (liferea_dialog_lookup (spd->ui_data.dialog, "dontUseProxyCheck")), subscription->updateOptions->dontUseProxy);

	/* Advanced */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (spd->ui_data.dialog, "enclosureDownloadCheck")), feed->encAutoDownload);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (spd->ui_data.dialog, "loadItemLinkCheck")), node->loadItemLink);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (spd->ui_data.dialog, "ignoreCommentFeeds")), feed->ignoreComments);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (spd->ui_data.dialog, "markAsReadCheck")), feed->markAsRead);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (spd->ui_data.dialog, "html5ExtractCheck")), feed->html5Extract);

	/* Remove tabs we do not need... */
	if (SUBSCRIPTION_TYPE (subscription) != feed_get_subscription_type ()) {
		/* Remove "General", "Source" and "Download" tab */
		gtk_notebook_remove_page (GTK_NOTEBOOK (liferea_dialog_lookup (spd->ui_data.dialog, "subscriptionPropNotebook")), 0);
		gtk_notebook_remove_page (GTK_NOTEBOOK (liferea_dialog_lookup (spd->ui_data.dialog, "subscriptionPropNotebook")), 0);
		gtk_notebook_remove_page (GTK_NOTEBOOK (liferea_dialog_lookup (spd->ui_data.dialog, "subscriptionPropNotebook")), 1);
	}
}

static void
subscription_prop_dialog_init (SubscriptionPropDialog *spd)
{
	GtkWidget	*propdialog;

	spd->ui_data.dialog = propdialog = liferea_dialog_new ("properties");

	/* set default update interval spin button and unit combo box */
	ui_common_setup_combo_menu (liferea_dialog_lookup (propdialog, "refreshIntervalUnitComboBox"),
	                            default_update_interval_unit_options,
	                            NULL /* no callback */,
	                            -1 /* default value */ );

	spd->ui_data.feedNameEntry = liferea_dialog_lookup (propdialog, "feedNameEntry");
	spd->ui_data.refreshInterval = liferea_dialog_lookup (propdialog, "refreshIntervalSpinButton");
	spd->ui_data.refreshIntervalUnit = liferea_dialog_lookup (propdialog, "refreshIntervalUnitComboBox");
	spd->ui_data.sourceEntry = liferea_dialog_lookup (propdialog, "sourceEntry");
	spd->ui_data.selectFile = liferea_dialog_lookup (propdialog, "selectSourceFileButton");
	spd->ui_data.fileRadio = liferea_dialog_lookup (propdialog, "feed_loc_file");
	spd->ui_data.urlRadio = liferea_dialog_lookup (propdialog, "feed_loc_url");
	spd->ui_data.cmdRadio = liferea_dialog_lookup (propdialog, "feed_loc_command");

	spd->ui_data.authcheckbox = liferea_dialog_lookup (propdialog, "HTTPauthCheck");
	spd->ui_data.username = liferea_dialog_lookup (propdialog, "usernameEntry");
	spd->ui_data.password = liferea_dialog_lookup (propdialog, "passwordEntry");
	spd->ui_data.credTable = liferea_dialog_lookup (propdialog, "httpAuthBox");

	g_signal_connect (spd->ui_data.selectFile, "clicked", G_CALLBACK (on_selectfile_pressed), &(spd->ui_data));
	g_signal_connect (spd->ui_data.urlRadio, "toggled", G_CALLBACK (on_feed_prop_url_radio), &(spd->ui_data));
	g_signal_connect (spd->ui_data.fileRadio, "toggled", G_CALLBACK (on_feed_prop_url_radio), &(spd->ui_data));
	g_signal_connect (spd->ui_data.cmdRadio, "toggled", G_CALLBACK (on_feed_prop_url_radio), &(spd->ui_data));
	g_signal_connect (spd->ui_data.authcheckbox, "toggled", G_CALLBACK (on_feed_prop_authcheck), &(spd->ui_data));

	g_signal_connect (liferea_dialog_lookup (propdialog, "filterCheckbox"), "toggled", G_CALLBACK (on_feed_prop_filtercheck), &(spd->ui_data));
	g_signal_connect (liferea_dialog_lookup (propdialog, "filterSelectFile"), "clicked", G_CALLBACK (on_selectfile_pressed), &(spd->ui_data));
	g_signal_connect (liferea_dialog_lookup (propdialog, "feedCacheLimited"), "toggled", G_CALLBACK (on_feed_prop_cache_radio), &(spd->ui_data));
	g_signal_connect (liferea_dialog_lookup (propdialog, "updateIntervalSpecific"), "toggled", G_CALLBACK(on_feed_prop_update_radio), &(spd->ui_data));

	g_signal_connect (G_OBJECT (propdialog), "response", G_CALLBACK (on_propdialog_response), spd);

	gtk_widget_show_all (propdialog);
}

SubscriptionPropDialog *
subscription_prop_dialog_new (subscriptionPtr subscription)
{
	SubscriptionPropDialog *spd;

	spd = SUBSCRIPTION_PROP_DIALOG (g_object_new (SUBSCRIPTION_PROP_DIALOG_TYPE, NULL));
	subscription_prop_dialog_load(spd, subscription);
	return spd;
}

/* complex "New" dialog */

G_DEFINE_TYPE (NewSubscriptionDialog, new_subscription_dialog, G_TYPE_OBJECT);

static void
new_subscription_dialog_finalize (GObject *object)
{
	NewSubscriptionDialog *nsd = NEW_SUBSCRIPTION_DIALOG (object);

	gtk_widget_destroy (nsd->ui_data.dialog);
}

static void
new_subscription_dialog_class_init (NewSubscriptionDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = new_subscription_dialog_finalize;
}

static void
on_newdialog_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
	NewSubscriptionDialog *nsd = (NewSubscriptionDialog *)user_data;

	if (response_id == GTK_RESPONSE_OK) {
		gchar *source = NULL;
		const gchar *filter = NULL;
		updateOptionsPtr options;

		/* Source */
		source = ui_subscription_dialog_decode_source (&(nsd->ui_data));

		/* Filter handling */
		filter = gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (nsd->ui_data.dialog, "filterEntry")));
		if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (nsd->ui_data.dialog, "filterCheckbox"))) ||
		    !strcmp(filter,"")) { /* Maybe this should be a test to see if the file exists? */
			filter = NULL;
		}

		options = g_new0 (struct updateOptions, 1);
		options->dontUseProxy = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (nsd->ui_data.dialog, "dontUseProxyCheck")));

		feedlist_add_subscription_check_duplicate (source, filter, options, FEED_REQ_PRIORITY_HIGH);
		g_free (source);
	}

	g_object_unref (nsd);
}

static void
new_subscription_dialog_init (NewSubscriptionDialog *nsd)
{
	GtkWidget	*newdialog;

	nsd->ui_data.dialog = newdialog = liferea_dialog_new ("new_subscription");

	/* Setup source entry */
	nsd->ui_data.sourceEntry = liferea_dialog_lookup (newdialog, "sourceEntry");
	gtk_widget_grab_focus (GTK_WIDGET (nsd->ui_data.sourceEntry));
	gtk_entry_set_activates_default (GTK_ENTRY (nsd->ui_data.sourceEntry), TRUE);

	nsd->ui_data.selectFile = liferea_dialog_lookup (newdialog, "selectSourceFileButton");
	g_signal_connect (nsd->ui_data.selectFile, "clicked", G_CALLBACK (on_selectfile_pressed), &(nsd->ui_data));

	/* Feed location radio buttons */
	nsd->ui_data.fileRadio = liferea_dialog_lookup (newdialog, "feed_loc_file");
	nsd->ui_data.urlRadio = liferea_dialog_lookup (newdialog, "feed_loc_url");
	nsd->ui_data.cmdRadio = liferea_dialog_lookup (newdialog, "feed_loc_command");

	g_signal_connect (nsd->ui_data.urlRadio, "toggled", G_CALLBACK (on_feed_prop_url_radio), &(nsd->ui_data));
	g_signal_connect (nsd->ui_data.fileRadio, "toggled", G_CALLBACK (on_feed_prop_url_radio), &(nsd->ui_data));
	g_signal_connect (nsd->ui_data.cmdRadio, "toggled", G_CALLBACK (on_feed_prop_url_radio), &(nsd->ui_data));

	g_signal_connect (liferea_dialog_lookup (newdialog, "filterCheckbox"), "toggled", G_CALLBACK (on_feed_prop_filtercheck), &(nsd->ui_data));
	g_signal_connect (liferea_dialog_lookup (newdialog, "filterSelectFile"), "clicked", G_CALLBACK (on_selectfile_pressed), &(nsd->ui_data));

	gtk_widget_grab_default (liferea_dialog_lookup (newdialog, "newfeedbtn"));
	g_signal_connect (G_OBJECT (newdialog), "response", G_CALLBACK (on_newdialog_response), nsd);

	on_feed_prop_filtercheck (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (newdialog, "filterCheckbox")), &(nsd->ui_data));
	on_feed_prop_url_radio (GTK_TOGGLE_BUTTON (nsd->ui_data.urlRadio), &(nsd->ui_data));

	gtk_widget_show_all (newdialog);
}

static NewSubscriptionDialog *
ui_complex_subscription_dialog_new (void)
{
	NewSubscriptionDialog *nsd;

	nsd = NEW_SUBSCRIPTION_DIALOG (g_object_new (NEW_SUBSCRIPTION_DIALOG_TYPE, NULL));
	return nsd;
}

/* simple "New" dialog */

G_DEFINE_TYPE (SimpleSubscriptionDialog, simple_subscription_dialog, G_TYPE_OBJECT);

static void
simple_subscription_dialog_finalize (GObject *object)
{
	SimpleSubscriptionDialog *ssd = SIMPLE_SUBSCRIPTION_DIALOG (object);

	gtk_widget_destroy (ssd->ui_data.dialog);
}

static void
simple_subscription_dialog_class_init (SimpleSubscriptionDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = simple_subscription_dialog_finalize;
}

static void
on_simple_newdialog_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
	SimpleSubscriptionDialog *ssd = (SimpleSubscriptionDialog *) user_data;
	gchar *source = NULL;

	if (response_id == GTK_RESPONSE_OK) {
		source = ui_subscription_create_url (g_strdup (gtk_entry_get_text (GTK_ENTRY(ssd->ui_data.sourceEntry))),
		                                      FALSE /* auth */, NULL /* user */, NULL /* passwd */);

		feedlist_add_subscription_check_duplicate (source, NULL, NULL, FEED_REQ_PRIORITY_HIGH);
		g_free (source);
	}

	if (response_id == GTK_RESPONSE_APPLY) /* misused for "Advanced" */
		ui_complex_subscription_dialog_new ();

	g_object_unref (ssd);
}

static void
simple_subscription_dialog_init (SimpleSubscriptionDialog *ssd)
{
	GtkWidget	*newdialog;

	ssd->ui_data.dialog = newdialog = liferea_dialog_new ("simple_subscription");

	/* Setup source entry */
	ssd->ui_data.sourceEntry = liferea_dialog_lookup (newdialog, "sourceEntry");
	gtk_widget_grab_focus (GTK_WIDGET (ssd->ui_data.sourceEntry));
	gtk_entry_set_activates_default (GTK_ENTRY (ssd->ui_data.sourceEntry), TRUE);

	g_signal_connect (G_OBJECT (newdialog), "response",
	                  G_CALLBACK (on_simple_newdialog_response), ssd);

	gtk_widget_show_all (newdialog);
}

SimpleSubscriptionDialog *
subscription_dialog_new (void)
{
	SimpleSubscriptionDialog *ssd;

	ssd = SIMPLE_SUBSCRIPTION_DIALOG (g_object_new (SIMPLE_SUBSCRIPTION_DIALOG_TYPE, NULL));
	return ssd;
}
