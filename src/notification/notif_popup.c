/** 
 * @file ui_notification.c mini popup windows
 *
 * Copyright (c) 2004, Karl Soderstrom <ks@xanadunet.net>
 * Copyright (c) 2005, Nathan Conrad <t98502@users.sourceforge.net>
 *	      
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include "conf.h"
#include "debug.h"
#include "node.h"
#include "item.h"
#include "callbacks.h"
#include "support.h"
#include "plugin.h"
#include "ui/ui_feedlist.h"
#include "notification/notif_plugin.h"

#define NOTIF_WIN_WIDTH 350
#define NOTIF_WIN_HEIGHT -1

#define NOTIF_BULLET "\342\227\217" /* U+25CF BLACK CIRCLE */

/* Time a notification should be dislayed (in ms) */
#define DISPLAY_TIME 10000

typedef struct {
	nodePtr		node_p;
	GtkWidget	*box_p;
	GtkWidget	*eventBox_p;
	gint		newCount;
	gint		timerTag;
} feedNotif_t;

extern GtkWidget *mainwindow;
extern GThread *updateThread;

/* List of all the current notifications */
static GSList *notifications_p = NULL;

/* The notification window */
static GtkWidget *notifWin_p = NULL;

/* Function prototypes */
static int notifCompare(gconstpointer a, gconstpointer b);
static feedNotif_t *notifCreateFeedNotif(nodePtr node_p);
static void notifCheckFeedNotif(feedNotif_t *feedNotif_p);
static void notifAddFeedNotif(feedNotif_t *feedNotif_p);
static void notifRemoveFeedNotif(feedNotif_t *feedNotif_p);
static GtkWidget *notifCreateWin(void);
static gint feedNotifTimeoutCallback(gpointer data);
static void notifRemoveWin();
static gboolean onNotificationButtonPressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean notifDeleteWinCb (GtkWidget *widget, GdkEvent *event, gpointer user_data);

static gboolean onNotificationButtonPressed (GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
	feedNotif_t *feedNotif_p = (feedNotif_t *)user_data;

	g_assert(feedNotif_p != NULL);
	if(event->type != GDK_BUTTON_PRESS) {
		return FALSE;
	} else if(event->button == 1) {
		ui_feedlist_select(feedNotif_p->node_p);
		gtk_window_present(GTK_WINDOW(mainwindow));
		gdk_window_raise(mainwindow->window);
	} else {
		/* Any button except LMB will remove the notification */
		notifRemoveFeedNotif(feedNotif_p);
	}
	return TRUE;
}

static void notifUpdatePosition(GtkWindow *window_p) {
	gint	max_x, notif_win_pos_x, w;
	gint	max_y, notif_win_pos_y, h;
	
	if(window_p == NULL)
		return;
	
	/* These two lines are necessary to get gtk_window_get_size() to
	   return the real height (it returns only the visible window 
	   height...) */
	gtk_widget_hide(GTK_WIDGET(window_p));	
	gtk_window_move(window_p, 0, 0);
	
	max_x = gdk_screen_width();
	max_y = gdk_screen_height();
	gtk_window_get_size(window_p, &w, &h);
	switch(getNumericConfValue(POPUP_PLACEMENT)) {
		default:
		case 1:
			gtk_window_set_gravity(GTK_WINDOW(window_p), GDK_GRAVITY_NORTH_WEST);
			notif_win_pos_x = 0;
			notif_win_pos_y = 0;
			break;
		case 2:
			gtk_window_set_gravity(GTK_WINDOW(window_p), GDK_GRAVITY_NORTH_WEST);
			notif_win_pos_x = max_x - NOTIF_WIN_WIDTH;
			notif_win_pos_y = 0;
			break;
		case 3:
			gtk_window_set_gravity(GTK_WINDOW(window_p), GDK_GRAVITY_SOUTH_EAST);
			notif_win_pos_x = max_x -  NOTIF_WIN_WIDTH;
			notif_win_pos_y = max_y - h;
			break;
		case 4:
			gtk_window_set_gravity(GTK_WINDOW(window_p), GDK_GRAVITY_SOUTH_EAST);
			notif_win_pos_x = 0;
			notif_win_pos_y = max_y - h;
			break;
	}
	gtk_window_move(window_p, notif_win_pos_x, notif_win_pos_y);
	gtk_widget_show(GTK_WIDGET(window_p));
}

static gint feedNotifTimeoutCallback(gpointer data) {

	feedNotif_t *feedNotif_p = (feedNotif_t *) data;
	notifRemoveFeedNotif(feedNotif_p);
	notifRemoveWin();	
	return FALSE;
}

/* This function can only be used to find a feed in the notifications_p*/
static int notifCompare(gconstpointer a, gconstpointer b) {

	if(((feedNotif_t *)a)->node_p == (nodePtr)b) {
		return 0;
	} else {
		return 1;
	}
}

static void notif_popup_new_items(const nodePtr node_p) {
	feedNotif_t *curNotif_p = NULL;
	GSList *list_p = NULL;
	
	if(!getBooleanConfValue(SHOW_POPUP_WINDOWS))
		return;
	
	list_p = g_slist_find_custom(notifications_p, node_p, notifCompare);
	
	if(list_p != NULL) {
		curNotif_p = (feedNotif_t *)list_p->data;
	} else {
		if(0 == node_p->popupCount)
			return;
		
		curNotif_p = notifCreateFeedNotif(node_p);
		notifications_p = g_slist_append(notifications_p, (gpointer) curNotif_p);
		g_assert(curNotif_p != NULL);
	}
	
	notifCheckFeedNotif(curNotif_p);
}	

static feedNotif_t *notifCreateFeedNotif(nodePtr node_p) {
	feedNotif_t *feedNotif_p = NULL;

	g_assert(node_p != NULL);
	
	feedNotif_p = g_new0(feedNotif_t, 1);
	if(feedNotif_p != NULL) {
		feedNotif_p->node_p = node_p;
	}
	return feedNotif_p;
}

static void notifCheckFeedNotif(feedNotif_t *feedNotif_p) {

	if(feedNotif_p->newCount < feedNotif_p->node_p->popupCount) {
		if(notifWin_p == NULL) {
			notifWin_p = notifCreateWin();
		}
		notifAddFeedNotif(feedNotif_p);
	} else if(feedNotif_p->newCount > feedNotif_p->node_p->popupCount) {
		notifRemoveFeedNotif(feedNotif_p);
		notifRemoveWin();
	}
}

static void notifAddFeedNotif(feedNotif_t *feedNotif_p) {
	GtkWidget *hbox_p, *icon_p, *label_p = NULL;
	gchar *labelText_p = NULL;
	itemPtr item_p = NULL;
	GList *list_p = NULL;

	if(feedNotif_p->eventBox_p != NULL) {
		notifRemoveFeedNotif(feedNotif_p);
	}

	feedNotif_p->eventBox_p = gtk_event_box_new();
	
	/* Listen on button clicks */
	g_signal_connect(G_OBJECT(feedNotif_p->eventBox_p), "button-press-event", (GCallback) onNotificationButtonPressed, (gpointer) feedNotif_p);

	feedNotif_p->box_p = gtk_vbox_new(FALSE, 4);
	gtk_container_add(GTK_CONTAINER(feedNotif_p->eventBox_p), feedNotif_p->box_p);
	
	/* Add the header label */
	hbox_p = gtk_hbox_new(FALSE, 0);
	label_p = gtk_label_new(NULL);
	gtk_label_set_use_markup(GTK_LABEL(label_p), TRUE);
	labelText_p = g_strdup_printf("<b><u>%s</u></b>", node_get_title(feedNotif_p->node_p));
	gtk_label_set_markup(GTK_LABEL(label_p), labelText_p);
	g_free(labelText_p);
	gtk_misc_set_alignment(GTK_MISC(label_p), 0.0, 0.5);

	if(NULL != feedNotif_p->node_p->icon) {
		icon_p = gtk_image_new_from_pixbuf(feedNotif_p->node_p->icon);
		gtk_box_pack_start(GTK_BOX(hbox_p), icon_p, FALSE, FALSE, 5);
		gtk_misc_set_padding(GTK_MISC(label_p), 5, 10);
	} else {
		gtk_misc_set_padding(GTK_MISC(label_p), 15, 10);
	}
	gtk_box_pack_start(GTK_BOX(hbox_p), label_p, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(feedNotif_p->box_p), hbox_p, TRUE, TRUE, 0);
	
	/* Add the new items */
	list_p = feedNotif_p->node_p->itemSet->items;
	while(list_p != NULL) {
		item_p = list_p->data;
		if(TRUE == item_p->popupStatus) {
			item_p->popupStatus = FALSE;
			labelText_p = g_strdup_printf ("%s %s", NOTIF_BULLET, item_get_title(item_p) != NULL ? item_get_title(item_p) : _("Untitled"));
			label_p = gtk_label_new (labelText_p);
			g_free(labelText_p);
			gtk_label_set_line_wrap(GTK_LABEL(label_p), TRUE);
			gtk_label_set_justify(GTK_LABEL(label_p), GTK_JUSTIFY_LEFT);
			gtk_misc_set_alignment(GTK_MISC(label_p), 0.0, 0.5);
			gtk_misc_set_padding(GTK_MISC(label_p), 25, 0);
			gtk_box_pack_start(GTK_BOX(feedNotif_p->box_p), label_p, TRUE, TRUE, 0);
		}
		list_p = g_list_next(list_p);
	}
	
	gtk_widget_show_all(feedNotif_p->eventBox_p);

	{
		GList *list_p = NULL;
		list_p = gtk_container_get_children(GTK_CONTAINER(notifWin_p));
		g_assert(list_p != NULL);
		gtk_box_pack_start(GTK_BOX(list_p->data), feedNotif_p->eventBox_p, FALSE, FALSE, 0);
	}

	feedNotif_p->newCount = feedNotif_p->node_p->popupCount;
	
	notifUpdatePosition(GTK_WINDOW(notifWin_p));

	/* Add timer */
	feedNotif_p->timerTag = g_timeout_add(DISPLAY_TIME, feedNotifTimeoutCallback, (gpointer) feedNotif_p);
}


/** This removes all traces of a feed from the notification window,
    but does not remove the feed from the list of notifications.... */
static void notifRemoveFeedNotif (feedNotif_t *feedNotif_p) {

	if(feedNotif_p->eventBox_p != NULL) {
		gtk_widget_destroy(feedNotif_p->eventBox_p);
		feedNotif_p->eventBox_p = NULL;
	}
	if(feedNotif_p->timerTag) {
		g_source_remove(feedNotif_p->timerTag);
		feedNotif_p->timerTag = 0;
	}
	feedNotif_p->newCount = feedNotif_p->node_p->popupCount;
	
	notifUpdatePosition(GTK_WINDOW(notifWin_p));
}

/* to be called when a feed is deleted.. when all traces of the feed
   must be removed under penalty of segfault. */
static void notif_popup_node_removed(const nodePtr np) {
	feedNotif_t	*feedNotif_p;
	GSList		*iter; 
	
	iter = notifications_p;
	while(NULL != iter) {
		feedNotif_p = iter->data;
		if(np == feedNotif_p->node_p) {
			notifRemoveFeedNotif(feedNotif_p);
			g_free(feedNotif_p);
			notifications_p = g_slist_delete_link(notifications_p, iter);
			notifRemoveWin();
			return;
		}
		iter = g_slist_next(iter);
	}
}

static GtkWidget *notifCreateWin(void) {
	GtkWidget	*window_p = NULL;
	GtkWidget	*vbox_p = NULL;

	window_p = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window_p), _("Liferea notification"));
	
	/* Don't make any special placement */
	gtk_window_set_position(GTK_WINDOW(window_p), GTK_WIN_POS_NONE);

	/* The user shouldn't be able to resize it */
	gtk_window_set_resizable(GTK_WINDOW(window_p), FALSE);

	/* Stick on all desktops */
	gtk_window_stick(GTK_WINDOW(window_p));

	/* Not in taskbar */
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window_p), TRUE);

	/* On top */
	gtk_window_set_keep_above(GTK_WINDOW(window_p), TRUE);
	
	gtk_widget_set_size_request(window_p, NOTIF_WIN_WIDTH, NOTIF_WIN_HEIGHT);
	
	vbox_p = gtk_vbox_new(FALSE, 4);
	gtk_container_add(GTK_CONTAINER(window_p), vbox_p);

	gtk_widget_realize(window_p);
	gdk_window_set_decorations(window_p->window, GDK_DECOR_BORDER);
	
	g_signal_connect(window_p, "destroy", G_CALLBACK(notifDeleteWinCb), NULL);
	
	notifUpdatePosition(GTK_WINDOW(window_p));

	gtk_widget_show_all(window_p);
	
	return window_p;
}


static gboolean notifDeleteWinCb (GtkWidget *widget, GdkEvent *event, gpointer user_data) {
	GSList *iter;
	notifWin_p = NULL;

	for(iter = notifications_p; iter != NULL; iter = iter->next) {
		((feedNotif_t*)(iter->data))->box_p = NULL;
		((feedNotif_t*)(iter->data))->eventBox_p = NULL;
	}
	return FALSE;
}

void notifRemoveWin(void) {

	if(notifWin_p != NULL) {
		GtkWidget *container_p;
		GList *list_p = gtk_container_get_children(GTK_CONTAINER(notifWin_p));
		if(list_p != NULL) {
			container_p = (GtkWidget *)list_p->data;
			g_list_free(list_p);
			list_p = gtk_container_get_children(GTK_CONTAINER(container_p));
			if(list_p == NULL) {
				/* Window is empty, destroy it */
				gtk_widget_destroy(notifWin_p);
				notifWin_p = NULL;
			} else {
				g_list_free(list_p);
			}
		}
	}
}

static void notif_popup_enable(void) { 

	debug0(DEBUG_GUI, "simple popups enabled");
}

static void notif_popup_disable(void) {

	debug0(DEBUG_GUI, "simple popups disabled");
	notifRemoveWin();
}

static gboolean notif_popup_init(void) { }

static void notif_popup_deinit(void) { }

/* notification plugin definition */

static struct notificationPlugin npi = {
	NOTIFICATION_PLUGIN_API_VERSION,
	NOTIFICATION_TYPE_POPUP,
	1,
	notif_popup_init,
	notif_popup_deinit,
	notif_popup_enable,
	notif_popup_disable,
	notif_popup_new_items,
	notif_popup_node_removed
};

static struct plugin pi = {
	PLUGIN_API_VERSION,
	"Mini popup window notification",
	PLUGIN_TYPE_NOTIFICATION,
	//"Simple implementation of a notification using mini popup windows.",
	&npi
};

DECLARE_PLUGIN(pi);
DECLARE_NOTIFICATION_PLUGIN(npi);
