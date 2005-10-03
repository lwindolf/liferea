/**
 * @file feedlist.c feedlist handling
 *
 * Copyright (C) 2005 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2005 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include <libxml/uri.h>
#include "support.h"
#include "feed.h"
#include "feedlist.h"
#include "item.h"
#include "itemlist.h"
#include "update.h"
#include "conf.h"
#include "debug.h"
#include "callbacks.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_htmlview.h"
#include "ui/ui_notification.h"
#include "ui/ui_tray.h"
#include "ui/ui_feed.h"
#include "fl_providers/fl_plugin.h"

static guint unreadCount = 0;
static guint newCount = 0;

static nodePtr	selectedNode = NULL;
extern nodePtr	displayed_node;

static guint feedlist_save_timer = 0;
static gboolean feedlistLoading = TRUE;

/* helper functions */

typedef enum {
	NEW_ITEM_COUNT,
	UNREAD_ITEM_COUNT
} countType;

/* statistic handling methods */

int feedlist_get_unread_item_count(void) { return unreadCount; }
int feedlist_get_new_item_count(void) { return newCount; }

void feedlist_update_counters(gint unreadDiff, gint newDiff) {

	unreadCount += unreadDiff;
	newCount += newDiff;

	if((0 != newDiff) || (0 != unreadDiff))
		ui_tray_update();
}

static void feedlist_unset_new_items(nodePtr np) {
	GSList *iter;
	
	if(0 == ((feedPtr)np)->newCount)
		return;
		
	feedlist_load_feed((feedPtr)np);
	
	iter = feed_get_item_list((feedPtr)np);
	while(NULL != iter) {
		item_set_new_status((itemPtr)iter->data, FALSE);
		iter = g_slist_next(iter);

	}
		
	feedlist_unload_feed((feedPtr)np);
}

void feedlist_reset_new_item_count(void) {

	if(0 != newCount) {
		ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED | ACTION_FILTER_DIRECTORY, feedlist_unset_new_items);
		newCount = 0;
		ui_tray_update();
	}
}

void feedlist_add_node(nodePtr parent, nodePtr np, gint position) {

	ui_feedlist_add(parent, np, position);
	ui_feedlist_update();
}

void feedlist_update_node(nodePtr np) {

	node_request_update(np);
}

static void feedlist_remove_node_(nodePtr np) { 
	
	// FIXME: feedPtr!!!
	//ui_notification_remove_feed((feedPtr)np);	/* removes an existing notification for this feed */
	ui_folder_remove_node(np);
	ui_feedlist_update();
	
	node_load(np);
	node_remove(np);	
}

static void feedlist_remove_folder(nodePtr np) {

	ui_feedlist_do_for_all(np, ACTION_FILTER_CHILDREN | ACTION_FILTER_ANY, feedlist_remove_node_);
	ui_feedlist_update();
	node_remove(np);	
}

void feedlist_remove_node(nodePtr np) {

	if(np == displayed_node) {
		itemlist_load(NULL);
		ui_htmlview_clear(ui_mainwindow_get_active_htmlview());
	}

	if(FST_FOLDER != np->type)
		feedlist_remove_node_(np);
	else
		feedlist_remove_folder(np);
}

static void feedlist_merge_itemset_cb(nodePtr np, gpointer userdata) {
	itemSetPtr sp = (itemSetPtr)userdata;

	switch(np->type) {
		case FST_FOLDER:
			return; /* a sub folder has no own itemset to add */
			break;
		case FST_FEED:
		case FST_PLUGIN:
			if(NULL != FL_PLUGIN(np)->node_load)
				FL_PLUGIN(np)->node_load(np);
			break;
		case FST_VFOLDER:
			/* FIXME */		
			return;
			break;
		default:
			g_warning("internal error: unknown node type!");
			return;
			break;
	}

	sp->items = g_slist_concat(sp->items, np->itemSet->items);
	sp->newCount += np->itemSet->newCount;
	sp->unreadCount += np->itemSet->unreadCount;
}

void feedlist_load_node(nodePtr np) {

	node_load(np);

	if(FST_FOLDER == np->type)
		ui_feedlist_do_foreach_data(np, feedlist_merge_itemset_cb, (gpointer)&(np->itemSet));
}

static gboolean feedlist_auto_update(void *data) {

	debug_enter("feedlist_auto_update");
	if(download_is_online()) {
		ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED, (gpointer)node_auto_update);
	} else {
		debug0(DEBUG_UPDATE, "no update processing because we are offline!");
	}
	debug_exit("feedlist_auto_update");

	return TRUE;
}


// FIXME: move method to feed specific code
/**
 * Creates a new error description according to the passed
 * HTTP status and the feeds parser errors. If the HTTP
 * status is a success status and no parser errors occurred
 * no error messages is created. The created error message 
 * can be queried with feed_get_error_description().
 *
 * @param fp		feed
 * @param httpstatus	HTTP status
 * @param resultcode the update code's return code (see update.h)
 */
static void feed_set_error_description(feedPtr fp, gint httpstatus, gint resultcode, gchar *filterErrors) {
	gchar		*tmp1, *tmp2 = NULL, *buffer = NULL;
	gboolean	errorFound = FALSE;

	g_assert(NULL != fp);
	g_free(fp->errorDescription);
	fp->errorDescription = NULL;
	
	if(((httpstatus >= 200) && (httpstatus < 400)) && /* HTTP codes starting with 2 and 3 mean no error */
	   (NULL == filterErrors) && (NULL == fp->parseErrors))
		return;
	addToHTMLBuffer(&buffer, UPDATE_ERROR_START);
	
	if((200 != httpstatus) || (resultcode != NET_ERR_OK)) {
		/* first specific codes */
		switch(httpstatus) {
			case 401:tmp2 = g_strdup(_("You are unauthorized to download this feed. Please update your username and "
			                           "password in the feed properties dialog box."));break;
			case 402:tmp2 = g_strdup(_("Payment Required"));break;
			case 403:tmp2 = g_strdup(_("Access Forbidden"));break;
			case 404:tmp2 = g_strdup(_("Resource Not Found"));break;
			case 405:tmp2 = g_strdup(_("Method Not Allowed"));break;
			case 406:tmp2 = g_strdup(_("Not Acceptable"));break;
			case 407:tmp2 = g_strdup(_("Proxy Authentication Required"));break;
			case 408:tmp2 = g_strdup(_("Request Time-Out"));break;
			case 410:tmp2 = g_strdup(_("Gone. Resource doesn't exist. Please unsubscribe!"));break;
		}
		/* Then, netio errors */
		if(tmp2 == NULL) {
			switch(resultcode) {
			case NET_ERR_URL_INVALID:    tmp2 = g_strdup(_("URL is invalid")); break;
			case NET_ERR_PROTO_INVALID:    tmp2 = g_strdup(_("Unsupported network protocol")); break;
			case NET_ERR_UNKNOWN:
			case NET_ERR_CONN_FAILED:
			case NET_ERR_SOCK_ERR:       tmp2 = g_strdup(_("Error connecting to remote host")); break;
			case NET_ERR_HOST_NOT_FOUND: tmp2 = g_strdup(_("Hostname could not be found")); break;
			case NET_ERR_CONN_REFUSED:   tmp2 = g_strdup(_("Network connection was refused by the remote host")); break;
			case NET_ERR_TIMEOUT:        tmp2 = g_strdup(_("Remote host did not finish sending data")); break;
				/* Transfer errors */
			case NET_ERR_REDIRECT_COUNT_ERR: tmp2 = g_strdup(_("Too many HTTP redirects were encountered")); break;
			case NET_ERR_REDIRECT_ERR:
			case NET_ERR_HTTP_PROTO_ERR: 
			case NET_ERR_GZIP_ERR:           tmp2 = g_strdup(_("Remote host sent an invalid response")); break;
				/* These are handled above	
				   case NET_ERR_HTTP_410:
				   case NET_ERR_HTTP_404:
				   case NET_ERR_HTTP_NON_200:
				*/
			case NET_ERR_AUTH_FAILED:
			case NET_ERR_AUTH_NO_AUTHINFO: tmp2 = g_strdup(_("Authentication failed")); break;
			case NET_ERR_AUTH_GEN_AUTH_ERR:
			case NET_ERR_AUTH_UNSUPPORTED: tmp2 = g_strdup(_("Webserver's authentication method incompatible with Liferea")); break;
			}
		}
		/* And generic messages in the unlikely event that the above didn't work */
		if(NULL == tmp2) {
			switch(httpstatus / 100) {
			case 3:tmp2 = g_strdup(_("Feed not available: Server requested unsupported redirection!"));break;
			case 4:tmp2 = g_strdup(_("Client Error"));break;
			case 5:tmp2 = g_strdup(_("Server Error"));break;
			default:tmp2 = g_strdup(_("(unknown networking error happened)"));break;
			}
		}
		errorFound = TRUE;
		tmp1 = g_strdup_printf(HTTP_ERROR_TEXT, httpstatus, tmp2);
		addToHTMLBuffer(&buffer, tmp1);
		g_free(tmp1);
		g_free(tmp2);
	}
	
	/* add filtering error messages */
	if(NULL != filterErrors) {	
		errorFound = TRUE;
		tmp1 = g_markup_printf_escaped(FILTER_ERROR_TEXT2, _("Show Details"), filterErrors);
		addToHTMLBuffer(&buffer, FILTER_ERROR_TEXT);
		addToHTMLBuffer(&buffer, tmp1);
		g_free(tmp1);
	}
	
	/* add parsing error messages */
	if(NULL != fp->parseErrors) {
		errorFound = TRUE;
		tmp1 = g_strdup_printf(PARSE_ERROR_TEXT2, _("Show Details"), fp->parseErrors);
		addToHTMLBuffer(&buffer, PARSE_ERROR_TEXT);
		addToHTMLBuffer(&buffer, tmp1);
		if (feed_get_source(fp) != NULL && (NULL != strstr(feed_get_source(fp), "://"))) {
			xmlChar *escsource;
			addToHTMLBufferFast(&buffer,_("<br>You may want to validate the feed using "
			                              "<a href=\"http://feedvalidator.org/check.cgi?url="));
			escsource = xmlURIEscapeStr(feed_get_source(fp),NULL);
			addToHTMLBufferFast(&buffer,escsource);
			xmlFree(escsource);
			addToHTMLBuffer(&buffer,_("\">FeedValidator</a>."));
		}
		addToHTMLBuffer(&buffer, "</span>");
		g_free(tmp1);
	}
	
	/* if none of the above error descriptions matched... */
	if(!errorFound) {
		tmp1 = g_strdup_printf(_("There was a problem while reading this subscription. Please check the URL and console output."));
		addToHTMLBuffer(&buffer, tmp1);
		g_free(tmp1);
	}
	
	addToHTMLBuffer(&buffer, UPDATE_ERROR_END);
	fp->errorDescription = buffer;
}

/* direct user callbacks */

void feedlist_selection_changed(nodePtr np) {

	if(np != selectedNode) {
		selectedNode = np;
	
		/* when the user selects a feed in the feed list we
		   assume that he got notified of the new items or
		   isn't interested in the event anymore... */
		feedlist_reset_new_item_count();
	}
}

void on_menu_delete(GtkMenuItem *menuitem, gpointer user_data) {

	ui_feedlist_delete_prompt((nodePtr)ui_feedlist_get_selected());
}

void on_popup_refresh_selected(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	nodePtr ptr = (nodePtr)callback_data;

	if(ptr == NULL) {
		ui_show_error_box(_("You have to select a feed entry"));
		return;
	}

	if(download_is_online()) {
		if(FST_FEED == ptr->type)
			feed_schedule_update((feedPtr)ptr, FEED_REQ_PRIORITY_HIGH);
		else
			ui_feedlist_do_for_all(ptr, ACTION_FILTER_FEED, feedlist_update_node);
	} else
		ui_mainwindow_set_status_bar(_("Liferea is in offline mode. No update possible."));
}

void on_refreshbtn_clicked(GtkButton *button, gpointer user_data) { 

	ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED, feedlist_update_node);
}

void on_menu_feed_update(GtkMenuItem *menuitem, gpointer user_data) {

	on_popup_refresh_selected((gpointer)ui_feedlist_get_selected(), 0, NULL);
}

void on_menu_update(GtkMenuItem *menuitem, gpointer user_data) {
	gpointer ptr = (gpointer)ui_feedlist_get_selected();
	
	if(ptr != NULL) {
		on_popup_refresh_selected((gpointer)ptr, 0, NULL);
	} else {
		g_warning("You have found a bug in Liferea. You must select a node in the feedlist to do what you just did.");
	}
}

void on_popup_allunread_selected(void) {
	nodePtr	np;
	
	if(NULL != (np = ui_feedlist_get_selected())) {
		itemlist_mark_all_read(np);
		ui_feedlist_update();
	}
}

void on_popup_allfeedsunread_selected(void) {

	ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED, itemlist_mark_all_read);
}

void on_popup_mark_as_read(gpointer callback_data, guint callback_action, GtkWidget *widget) {

	on_popup_allunread_selected();
}

void on_popup_delete(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	
	ui_feedlist_delete_prompt((nodePtr)callback_data);
}

static gboolean feedlist_schedule_save_cb(gpointer user_data) {

	feedlist_save();	// FIXME: iterate over folders to find feed list plugins and trigger save for each one
	feedlist_save_timer = 0;
	return FALSE;
}

void feedlist_schedule_save(void) {

	if(!feedlistLoading && !feedlist_save_timer) {
		debug0(DEBUG_CONF, "Scheduling feedlist save");
		feedlist_save_timer = g_timeout_add(5000, feedlist_schedule_save_cb, NULL);
	}
}

void feedlist_init(void) {

	/* initial update of feed list */
	feedlist_auto_update(NULL);

	/* setup one minute timer for automatic updating */
 	(void)g_timeout_add(60*1000, feedlist_auto_update, NULL);
}

