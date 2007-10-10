/**
 * @file ui_subscription.c subscription dialogs
 *
 * Copyright (C) 2004-2007 Lars Lindner <lars.lindner@gmail.com>
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
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
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
#include "ui/ui_dialog.h"
#include "ui/ui_mainwindow.h"	/* for ui_choose_file() */
#include "ui/ui_node.h"
#include "ui/ui_subscription.h"

/** common private structure for all subscription dialogs */
struct SubscriptionDialogPrivate {

	subscriptionPtr subscription;	/** used only for "properties" dialog */
	nodePtr		parentNode;	/** used only for "new" dialogs */

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
};

/* properties dialog */

static void subscription_prop_dialog_class_init	(SubscriptionPropDialogClass *klass);
static void subscription_prop_dialog_init	(SubscriptionPropDialog *spd);

#define SUBSCRIPTION_PROP_DIALOG_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), SUBSCRIPTION_PROP_DIALOG_TYPE, SubscriptionDialogPrivate))

static GObjectClass *parent_class = NULL;

GType
subscription_prop_dialog_get_type (void) 
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) 
	{
		static const GTypeInfo our_info = 
		{
			sizeof (SubscriptionPropDialogClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) subscription_prop_dialog_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (SubscriptionPropDialog),
			0, /* n_preallocs */
			(GInstanceInitFunc) subscription_prop_dialog_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "SubscriptionPropDialog",
					       &our_info, 0);
	}

	return type;
}

static void
subscription_prop_dialog_finalize (GObject *object)
{
	SubscriptionPropDialog *spd = SUBSCRIPTION_PROP_DIALOG (object);

	gtk_widget_destroy (spd->priv->dialog);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
subscription_prop_dialog_class_init (SubscriptionPropDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = subscription_prop_dialog_finalize;

	g_type_class_add_private (object_class, sizeof(SubscriptionDialogPrivate));
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

	/* Add http:// if needed */
	if (strstr(str, "://") == NULL) {
		tmp2 = g_strdup_printf("http://%s",str);
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
	if(auth) {
		xmlURIPtr uri = xmlParseURI(BAD_CAST str);
		if (uri != NULL) {
			xmlChar *sourceUrl;
			xmlFree(uri->user);
			uri->user = g_strdup_printf("%s:%s", username, password);
			sourceUrl = xmlSaveUri(uri);
			source = g_strdup(sourceUrl);
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
ui_subscription_dialog_decode_source (SubscriptionDialogPrivate *ui_data) 
{
	gchar	*source = NULL;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ui_data->fileRadio)))
		source = g_strdup (gtk_entry_get_text (GTK_ENTRY (ui_data->sourceEntry)));
		
	else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ui_data->urlRadio)))
		source = ui_subscription_create_url (g_strdup (gtk_entry_get_text (GTK_ENTRY (ui_data->sourceEntry))),
		                                    ui_data->authcheckbox &&
		                                    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ui_data->authcheckbox)),
		                                    gtk_entry_get_text (GTK_ENTRY (ui_data->username)),
		                                    gtk_entry_get_text (GTK_ENTRY (ui_data->password)));
					    
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
	
	if(response_id == GTK_RESPONSE_OK) {
		gchar		*newSource;
		const gchar	*newFilter;
		gboolean	needsUpdate = FALSE;
		subscriptionPtr	subscription = spd->priv->subscription;
		nodePtr		node = spd->priv->subscription->node;
		feedPtr		feed = (feedPtr)node->data;
		
		/* "General" */
		node_set_title(node, gtk_entry_get_text(GTK_ENTRY(spd->priv->feedNameEntry)));
		
		/* Source */
		newSource = ui_subscription_dialog_decode_source(spd->priv);
		
		/* Filter handling */
		newFilter = gtk_entry_get_text(GTK_ENTRY(liferea_dialog_lookup(spd->priv->dialog, "filterEntry")));
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(spd->priv->dialog, "filterCheckbox"))) &&
		   strcmp(newFilter,"")) { /* Maybe this should be a test to see if the file exists? */
			if(subscription_get_filter(subscription) == NULL ||
			   strcmp(newFilter, subscription_get_filter(subscription))) {
				subscription_set_filter(subscription, newFilter);
				needsUpdate = TRUE;
			}
		} else {
			if(subscription_get_filter(subscription)) {
				subscription_set_filter(subscription, NULL);
				needsUpdate = TRUE;
			}
		}
		
		/* if URL has changed... */
		if(strcmp(newSource, subscription_get_source(subscription))) {
			subscription_set_source(subscription, newSource);
			needsUpdate = TRUE;
		}
		g_free(newSource);

		/* Update interval handling */
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET (dialog), "updateIntervalNever"))))
			subscription_set_update_interval (subscription, -2);
		else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET (dialog), "updateIntervalDefault"))))
			subscription_set_update_interval (subscription, -1);
		else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET (dialog), "updateIntervalSpecific")))) {
			gint intervalUnit = gtk_combo_box_get_active (GTK_COMBO_BOX (spd->priv->refreshIntervalUnit));
			gint updateInterval = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spd->priv->refreshInterval));
			if (intervalUnit == 1)
				updateInterval *= 60;	/* hours */
			if (intervalUnit == 2)
				updateInterval *= 1440;	/* days */
			
			subscription_set_update_interval (subscription, updateInterval);
			db_subscription_update (subscription);
		}
			
		/* "Archive" handling */
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(GTK_WIDGET(dialog), "feedCacheDefault"))))
			feed->cacheLimit = CACHE_DEFAULT;
		else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(GTK_WIDGET(dialog), "feedCacheDisable"))))
			feed->cacheLimit = CACHE_DISABLE;
		else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(GTK_WIDGET(dialog), "feedCacheUnlimited"))))
			feed->cacheLimit = CACHE_UNLIMITED;
		else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(GTK_WIDGET(dialog), "feedCacheLimited"))))
			feed->cacheLimit = gtk_spin_button_get_value(GTK_SPIN_BUTTON(liferea_dialog_lookup(GTK_WIDGET(dialog), "cacheItemLimit")));

		/* "Download" Options */
		subscription->updateOptions->dontUseProxy = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(GTK_WIDGET(dialog), "dontUseProxyCheck")));

		/* "Advanced" options */
		feed->encAutoDownload = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(GTK_WIDGET(dialog), "enclosureDownloadCheck")));
		feed->loadItemLink = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(GTK_WIDGET(dialog), "loadItemLinkCheck")));

		ui_node_update (node->id);
		feedlist_schedule_save ();
		db_subscription_update (subscription);
		if (needsUpdate)
			subscription_update (subscription, FEED_REQ_AUTH_DIALOG | FEED_REQ_PRIORITY_HIGH);
	}

	g_object_unref(spd);
}

static void
on_feed_prop_filtercheck (GtkToggleButton *button,
                          gpointer user_data) 
{
	SubscriptionDialogPrivate *ui_data = (SubscriptionDialogPrivate *)user_data;
	
	gboolean filter = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(ui_data->dialog, "filterCheckbox")));
	if(filter)
		gtk_widget_show(liferea_dialog_lookup(ui_data->dialog, "innerfiltervbox"));
	else
		gtk_widget_hide(liferea_dialog_lookup(ui_data->dialog, "innerfiltervbox"));
}

static void
ui_subscription_prop_enable_httpauth (SubscriptionDialogPrivate *ui_data,
                                      gboolean enable) 
{
	gboolean on;

	if(ui_data->authcheckbox) {
		on = enable && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->authcheckbox));
		gtk_widget_set_sensitive(ui_data->authcheckbox,enable);
		gtk_widget_set_sensitive(ui_data->credTable,on);
	}
}

static void
on_feed_prop_authcheck (GtkToggleButton *button,
                        gpointer user_data) 
{
	SubscriptionDialogPrivate *ui_data = (SubscriptionDialogPrivate *)user_data;
	gboolean url = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->urlRadio));

	ui_subscription_prop_enable_httpauth(ui_data, url);
}

static void
on_feed_prop_url_radio (GtkToggleButton *button,
                        gpointer user_data) 
{
	SubscriptionDialogPrivate *ui_data = (SubscriptionDialogPrivate *)user_data;
	gboolean url = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->urlRadio));
	gboolean file = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->fileRadio));
	gboolean cmd = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui_data->cmdRadio));
	
	ui_subscription_prop_enable_httpauth(ui_data, url);
	gtk_widget_set_sensitive(ui_data->selectFile, file || cmd);
}

static void
on_selectfileok_clicked (const gchar *filename,
                         gpointer user_data) 
{
	SubscriptionDialogPrivate *ui_data = (SubscriptionDialogPrivate *)user_data;
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
on_selectfile_pressed (GtkButton *button,
                       gpointer user_data) 
{
	SubscriptionDialogPrivate *ui_data = (SubscriptionDialogPrivate *)user_data;
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
	ui_choose_file (_("Choose File"), GTK_STOCK_OPEN, FALSE, on_selectfileok_clicked, name, NULL, ui_data);
	g_free (name);
}
 
static void
on_feed_prop_cache_radio (GtkToggleButton *button,
                          gpointer user_data) 
{
	SubscriptionDialogPrivate *ui_data = (SubscriptionDialogPrivate *)user_data;
	gboolean limited = gtk_toggle_button_get_active(button);
	
	gtk_widget_set_sensitive(liferea_dialog_lookup(GTK_WIDGET(ui_data->dialog), "cacheItemLimit"), limited);
}

static void
on_feed_prop_update_radio (GtkToggleButton *button,
                           gpointer user_data) 
{
	SubscriptionDialogPrivate *priv = (SubscriptionDialogPrivate *) user_data;
	gboolean limited = gtk_toggle_button_get_active (button);
	
	gtk_widget_set_sensitive (priv->refreshInterval, limited);
	gtk_widget_set_sensitive (priv->refreshIntervalUnit, limited);
}

static void
ui_subscription_prop_dialog_load (SubscriptionPropDialog *spd, 
                                  subscriptionPtr subscription) 
{
	gint 		interval;
	guint		defaultInterval, spinSetInterval;
	gchar 		*defaultIntervalStr;
	nodePtr		node = subscription->node;
	feedPtr		feed = (feedPtr)node->data;

	spd->priv->subscription = subscription;

	/* General */
	gtk_entry_set_text(GTK_ENTRY(spd->priv->feedNameEntry), node_get_title(node));

	spd->priv->refreshInterval = liferea_dialog_lookup(spd->priv->dialog,"refreshIntervalSpinButton");
	
	interval = subscription_get_update_interval(subscription);
	defaultInterval = subscription_get_default_update_interval(subscription);
	spinSetInterval = defaultInterval;
	
	if (-2 >= interval) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup(spd->priv->dialog, "updateIntervalNever")), TRUE);
	} else if (-1 == interval) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup(spd->priv->dialog, "updateIntervalDefault")), TRUE);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup(spd->priv->dialog, "updateIntervalSpecific")), TRUE);
		spinSetInterval = interval;
	}
	
	/* Set refresh interval spin button and combo box */
	if (spinSetInterval % 1440 == 0) {	/* days */
		gtk_combo_box_set_active (GTK_COMBO_BOX (spd->priv->refreshIntervalUnit), 2);
		spinSetInterval /= 1440;
	} else if (spinSetInterval % 60 == 0) {	/* hours */
		gtk_combo_box_set_active (GTK_COMBO_BOX (spd->priv->refreshIntervalUnit), 1);
		spinSetInterval /= 60;
	} else {
		gtk_combo_box_set_active (GTK_COMBO_BOX (spd->priv->refreshIntervalUnit), 0);
	}
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (spd->priv->refreshInterval), spinSetInterval);

	gtk_widget_set_sensitive (spd->priv->refreshInterval, interval > 0);
	gtk_widget_set_sensitive (spd->priv->refreshIntervalUnit, interval > 0);
	
	/* setup info label about default update interval */
	if(-1 != defaultInterval)
		defaultIntervalStr = g_strdup_printf(ngettext("The provider of this feed suggests an update interval of %d minute.", 
		                                              "The provider of this feed suggests an update interval of %d minutes.",
		                                              defaultInterval), defaultInterval);
	else
		defaultIntervalStr = g_strdup(_("This feed specifies no default update interval."));

	gtk_label_set_text(GTK_LABEL(liferea_dialog_lookup(spd->priv->dialog, "feedUpdateInfo")), defaultIntervalStr);
	g_free(defaultIntervalStr);

	/* Source */
	if(subscription_get_source(subscription)[0] == '|') {
		gtk_entry_set_text(GTK_ENTRY(spd->priv->sourceEntry), &(subscription_get_source(subscription)[1]));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(spd->priv->cmdRadio), TRUE);
		ui_subscription_prop_enable_httpauth(spd->priv, FALSE);
		gtk_widget_set_sensitive(spd->priv->selectFile, TRUE);
	} else if(strstr(subscription_get_source(subscription), "://") != NULL) {
		xmlURIPtr uri = xmlParseURI(BAD_CAST subscription_get_source(subscription));
		xmlChar *parsedUrl;
		if(uri) {
			if(uri->user) {
				gchar *user = uri->user;
				gchar *pass = strstr(user, ":");
				if(pass) {
					pass[0] = '\0';
					pass++;
					gtk_entry_set_text(GTK_ENTRY(spd->priv->password), pass);
				}
				gtk_entry_set_text(GTK_ENTRY(spd->priv->username), user);
				xmlFree(uri->user);
				uri->user = NULL;
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(spd->priv->authcheckbox), TRUE);
			}
			parsedUrl = xmlSaveUri(uri);
			gtk_entry_set_text(GTK_ENTRY(spd->priv->sourceEntry), parsedUrl);
			xmlFree(parsedUrl);
			xmlFreeURI(uri);
		} else {
			gtk_entry_set_text(GTK_ENTRY(spd->priv->sourceEntry), subscription_get_source(subscription));
		}
		ui_subscription_prop_enable_httpauth(spd->priv, TRUE);
		gtk_widget_set_sensitive(spd->priv->selectFile, FALSE);
	} else {
		/* File */
		gtk_entry_set_text(GTK_ENTRY(spd->priv->sourceEntry), subscription_get_source(subscription));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(spd->priv->fileRadio), TRUE);
		ui_subscription_prop_enable_httpauth(spd->priv, FALSE);
		gtk_widget_set_sensitive(spd->priv->selectFile, TRUE);
	}

	if(subscription_get_filter(subscription)) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(spd->priv->dialog, "filterCheckbox")), TRUE);
		gtk_entry_set_text(GTK_ENTRY(liferea_dialog_lookup(spd->priv->dialog, "filterEntry")), subscription_get_filter(subscription));
	}

	/* Archive */
	if(feed->cacheLimit == CACHE_DISABLE) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(spd->priv->dialog, "feedCacheDisable")), TRUE);
	} else if(feed->cacheLimit == CACHE_DEFAULT) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(spd->priv->dialog, "feedCacheDefault")), TRUE);
	} else if(feed->cacheLimit == CACHE_UNLIMITED) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(spd->priv->dialog, "feedCacheUnlimited")), TRUE);
	} else {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(spd->priv->dialog, "feedCacheLimited")), TRUE);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(liferea_dialog_lookup(spd->priv->dialog, "cacheItemLimit")), feed->cacheLimit);
	}

	gtk_widget_set_sensitive(liferea_dialog_lookup(spd->priv->dialog, "cacheItemLimit"), feed->cacheLimit > 0);

	on_feed_prop_filtercheck(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(spd->priv->dialog, "filterCheckbox")), spd->priv);
	
	/* Download */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(spd->priv->dialog, "dontUseProxyCheck")), subscription->updateOptions->dontUseProxy);

	/* Advanced */	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(spd->priv->dialog, "enclosureDownloadCheck")), feed->encAutoDownload);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(spd->priv->dialog, "loadItemLinkCheck")), feed->loadItemLink);
}

static void
subscription_prop_dialog_init (SubscriptionPropDialog *spd)
{
	GtkWidget	*propdialog;
	
	spd->priv = SUBSCRIPTION_PROP_DIALOG_GET_PRIVATE (spd);
	spd->priv->dialog = propdialog = liferea_dialog_new (NULL, "propdialog");
	
	spd->priv->feedNameEntry = liferea_dialog_lookup (propdialog, "feedNameEntry");
	spd->priv->refreshInterval = liferea_dialog_lookup (propdialog, "refreshIntervalSpinButton");
	spd->priv->refreshIntervalUnit = liferea_dialog_lookup (propdialog, "refreshIntervalUnitComboBox");
	spd->priv->sourceEntry = liferea_dialog_lookup (propdialog, "sourceEntry");
	spd->priv->selectFile = liferea_dialog_lookup (propdialog, "selectSourceFileButton");
	spd->priv->fileRadio = liferea_dialog_lookup (propdialog, "feed_loc_file");
	spd->priv->urlRadio = liferea_dialog_lookup (propdialog, "feed_loc_url");
	spd->priv->cmdRadio = liferea_dialog_lookup (propdialog, "feed_loc_command");

	spd->priv->authcheckbox = liferea_dialog_lookup (propdialog, "HTTPauthCheck");
	spd->priv->username = liferea_dialog_lookup (propdialog, "usernameEntry");
	spd->priv->password = liferea_dialog_lookup (propdialog, "passwordEntry");
	spd->priv->credTable = liferea_dialog_lookup (propdialog, "table4");
	
	g_signal_connect (spd->priv->selectFile, "clicked", G_CALLBACK (on_selectfile_pressed), spd->priv);
	g_signal_connect (spd->priv->urlRadio, "toggled", G_CALLBACK (on_feed_prop_url_radio), spd->priv);
	g_signal_connect (spd->priv->fileRadio, "toggled", G_CALLBACK (on_feed_prop_url_radio), spd->priv);
	g_signal_connect (spd->priv->cmdRadio, "toggled", G_CALLBACK (on_feed_prop_url_radio), spd->priv);
	g_signal_connect (spd->priv->authcheckbox, "toggled", G_CALLBACK (on_feed_prop_authcheck), spd->priv);

	g_signal_connect (liferea_dialog_lookup (propdialog, "filterCheckbox"), "toggled", G_CALLBACK (on_feed_prop_filtercheck), spd->priv);
	g_signal_connect (liferea_dialog_lookup (propdialog, "filterSelectFile"), "clicked", G_CALLBACK (on_selectfile_pressed), spd->priv);
	g_signal_connect (liferea_dialog_lookup (propdialog, "feedCacheLimited"), "toggled", G_CALLBACK (on_feed_prop_cache_radio), spd->priv);
	g_signal_connect (liferea_dialog_lookup (propdialog, "updateIntervalSpecific"), "toggled", G_CALLBACK(on_feed_prop_update_radio), spd->priv);
	
	g_signal_connect (G_OBJECT (propdialog), "response", G_CALLBACK (on_propdialog_response), spd);

	gtk_widget_show_all (propdialog);
}

SubscriptionPropDialog *
ui_subscription_prop_dialog_new (subscriptionPtr subscription) 
{
	SubscriptionPropDialog *spd;
	
	spd = SUBSCRIPTION_PROP_DIALOG (g_object_new (SUBSCRIPTION_PROP_DIALOG_TYPE, NULL));
	ui_subscription_prop_dialog_load(spd, subscription);
	return spd;
}

/* complex "New" dialog */
 
static void new_subscription_dialog_class_init	(NewSubscriptionDialogClass *klass);
static void new_subscription_dialog_init	(NewSubscriptionDialog *ns);

#define NEW_SUBSCRIPTION_DIALOG_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), NEW_SUBSCRIPTION_DIALOG_TYPE, SubscriptionDialogPrivate))

GType
new_subscription_dialog_get_type (void) 
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) 
	{
		static const GTypeInfo our_info = 
		{
			sizeof (NewSubscriptionDialogClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) new_subscription_dialog_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (NewSubscriptionDialog),
			0, /* n_preallocs */
			(GInstanceInitFunc) new_subscription_dialog_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "NewSubscriptionDialog",
					       &our_info, 0);
	}

	return type;
}

static void
new_subscription_dialog_finalize (GObject *object)
{
	NewSubscriptionDialog *nsd = NEW_SUBSCRIPTION_DIALOG (object);
	
	gtk_widget_destroy (nsd->priv->dialog);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
new_subscription_dialog_class_init (NewSubscriptionDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = new_subscription_dialog_finalize;

	g_type_class_add_private (object_class, sizeof(SubscriptionDialogPrivate));
}

static void
on_newdialog_response (GtkDialog *dialog,
                       gint response_id, 
		       gpointer user_data) 
{
	NewSubscriptionDialog *nsd = (NewSubscriptionDialog *)user_data;
	
	if(response_id == GTK_RESPONSE_OK) {
		gchar *source = NULL;
		const gchar *filter = NULL;
		updateOptionsPtr options;

		/* Source */
		source = ui_subscription_dialog_decode_source(nsd->priv);

		/* Filter handling */
		filter = gtk_entry_get_text(GTK_ENTRY(liferea_dialog_lookup(nsd->priv->dialog, "filterEntry")));
		if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(nsd->priv->dialog, "filterCheckbox"))) ||
		   !strcmp(filter,"")) { /* Maybe this should be a test to see if the file exists? */
			filter = NULL;
		} 
		
		options = g_new0(struct updateOptions, 1);
		options->dontUseProxy = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(GTK_WIDGET(dialog), "dontUseProxyCheck")));
		
		node_request_automatic_add(source, NULL, filter, options,
					   FEED_REQ_RESET_TITLE | 
					   FEED_REQ_RESET_UPDATE_INT | 
					   FEED_REQ_AUTO_DISCOVER | 
					   FEED_REQ_PRIORITY_HIGH | 
					   FEED_REQ_DOWNLOAD_FAVICON | 
					   FEED_REQ_AUTH_DIALOG);
		g_free(source);
	}

	g_object_unref(nsd);
}

static void
new_subscription_dialog_init (NewSubscriptionDialog *nsd)
{
	GtkWidget	*newdialog;
	
	nsd->priv = NEW_SUBSCRIPTION_DIALOG_GET_PRIVATE (nsd);
	nsd->priv->dialog = newdialog = liferea_dialog_new (NULL, "newdialog");
	
	/* Setup source entry */
	nsd->priv->sourceEntry = liferea_dialog_lookup (newdialog,"sourceEntry");
	gtk_widget_grab_focus (GTK_WIDGET (nsd->priv->sourceEntry));
	gtk_entry_set_activates_default (GTK_ENTRY (nsd->priv->sourceEntry), TRUE);
		
	nsd->priv->selectFile = liferea_dialog_lookup (newdialog,"selectSourceFileButton");
	g_signal_connect (nsd->priv->selectFile, "clicked", G_CALLBACK (on_selectfile_pressed), nsd->priv);
	
	/* Feed location radio buttons */
	nsd->priv->fileRadio = liferea_dialog_lookup (newdialog, "feed_loc_file");
	nsd->priv->urlRadio = liferea_dialog_lookup (newdialog, "feed_loc_url");
	nsd->priv->cmdRadio = liferea_dialog_lookup (newdialog, "feed_loc_command");

	g_signal_connect (nsd->priv->urlRadio, "toggled", G_CALLBACK (on_feed_prop_url_radio), nsd->priv);
	g_signal_connect (nsd->priv->fileRadio, "toggled", G_CALLBACK (on_feed_prop_url_radio), nsd->priv);
	g_signal_connect (nsd->priv->cmdRadio, "toggled", G_CALLBACK (on_feed_prop_url_radio), nsd->priv);

	g_signal_connect (liferea_dialog_lookup (newdialog, "filterCheckbox"), "toggled", G_CALLBACK (on_feed_prop_filtercheck), nsd->priv);
	g_signal_connect (liferea_dialog_lookup (newdialog, "filterSelectFile"), "clicked", G_CALLBACK (on_selectfile_pressed), nsd->priv);

	gtk_widget_grab_default (liferea_dialog_lookup (newdialog, "newfeedbtn"));
	g_signal_connect (G_OBJECT (newdialog), "response", G_CALLBACK (on_newdialog_response), nsd);
	
	on_feed_prop_filtercheck (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (newdialog, "filterCheckbox")), nsd->priv);
	on_feed_prop_url_radio (GTK_TOGGLE_BUTTON (nsd->priv->urlRadio), nsd->priv);
	
	gtk_widget_show_all (newdialog);
}

NewSubscriptionDialog *
ui_complex_subscription_dialog_new (nodePtr parent) 
{
	NewSubscriptionDialog *nsd;
	
	nsd = NEW_SUBSCRIPTION_DIALOG (g_object_new (NEW_SUBSCRIPTION_DIALOG_TYPE, NULL));
	nsd->priv->parentNode = parent;
	return nsd;
}

/* simple "New" dialog */

static void simple_subscription_dialog_class_init	(SimpleSubscriptionDialogClass *klass);
static void simple_subscription_dialog_init		(SimpleSubscriptionDialog *ssd);

#define SIMPLE_SUBSCRIPTION_DIALOG_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), SIMPLE_SUBSCRIPTION_DIALOG_TYPE, SubscriptionDialogPrivate))

GType
simple_subscription_dialog_get_type (void) 
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) 
	{
		static const GTypeInfo our_info = 
		{
			sizeof (SimpleSubscriptionDialogClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) simple_subscription_dialog_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (SimpleSubscriptionDialog),
			0, /* n_preallocs */
			(GInstanceInitFunc) simple_subscription_dialog_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "SimpleSubscriptionDialog",
					       &our_info, 0);
	}

	return type;
}

static void
simple_subscription_dialog_finalize (GObject *object)
{
	SimpleSubscriptionDialog *ssd = SIMPLE_SUBSCRIPTION_DIALOG (object);
	
	gtk_widget_destroy (ssd->priv->dialog);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
simple_subscription_dialog_class_init (SimpleSubscriptionDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = simple_subscription_dialog_finalize;

	g_type_class_add_private (object_class, sizeof (SubscriptionDialogPrivate));
}

static void
on_simple_newdialog_response (GtkDialog *dialog,
                              gint response_id,
			      gpointer user_data) 
{
	SimpleSubscriptionDialog *ssd = (SimpleSubscriptionDialog *)user_data;
	gchar *source = NULL;
	
	if (response_id == GTK_RESPONSE_OK) {
		source = ui_subscription_create_url( g_strdup (gtk_entry_get_text (GTK_ENTRY(ssd->priv->sourceEntry))),
		                                     FALSE /* auth */, NULL /* user */, NULL /* passwd */);

		node_request_automatic_add(source, NULL, NULL, NULL,
					   FEED_REQ_RESET_TITLE | 
					   FEED_REQ_RESET_UPDATE_INT | 
					   FEED_REQ_AUTO_DISCOVER | 
					   FEED_REQ_PRIORITY_HIGH | 
					   FEED_REQ_DOWNLOAD_FAVICON | 
					   FEED_REQ_AUTH_DIALOG);
		g_free(source);
	}
	
	if (response_id == GTK_RESPONSE_APPLY) /* misused for "Advanced" */
		ui_complex_subscription_dialog_new (ssd->priv->parentNode);
		
	g_object_unref (ssd);
}

static void
simple_subscription_dialog_init (SimpleSubscriptionDialog *ssd)
{
	GtkWidget	*newdialog;
	
	ssd->priv = SIMPLE_SUBSCRIPTION_DIALOG_GET_PRIVATE (ssd);
	ssd->priv->dialog = newdialog = liferea_dialog_new (NULL, "simplenewdialog");
	
	/* Setup source entry */
	ssd->priv->sourceEntry = liferea_dialog_lookup (newdialog, "sourceEntry");
	gtk_widget_grab_focus (GTK_WIDGET (ssd->priv->sourceEntry));
	gtk_entry_set_activates_default (GTK_ENTRY (ssd->priv->sourceEntry), TRUE);

	g_signal_connect (G_OBJECT (newdialog), "response",
	                  G_CALLBACK (on_simple_newdialog_response), ssd);
	
	gtk_widget_show_all (newdialog);
}

SimpleSubscriptionDialog *
ui_subscription_dialog_new (nodePtr parent) 
{
	SimpleSubscriptionDialog *ssd;
	
	ssd = SIMPLE_SUBSCRIPTION_DIALOG (g_object_new (SIMPLE_SUBSCRIPTION_DIALOG_TYPE, NULL));
	ssd->priv->parentNode = parent;
	return ssd;
}
