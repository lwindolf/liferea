/** 
 * @file ui_notification.c mini popup windows
 *
 * Copyright (c) 2004, Karl Soderstrom <ks@xanadunet.net>
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
#include "feed.h"
#include "item.h"
#include "ui_feedlist.h"
#include "callbacks.h"
#include "support.h"

#define NOTIF_WIN_WIDTH 350
#define NOTIF_WIN_HEIGHT -1
#define NOTIF_WIN_POS_X 0
#define NOTIF_WIN_POS_Y 0

#define NOTIF_BULLET "\342\227\217" /* U+25CF BLACK CIRCLE */

/* Time a notification should be dislayed (in ms) */
#define DISPLAY_TIME 10000

typedef struct {
	feedPtr		feed_p;
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
static int notifCompare (gconstpointer a, gconstpointer b);
static feedNotif_t *notifCreateFeedNotif (feedPtr feed_p);
static void notifCheckFeedNotif (feedNotif_t *feedNotif_p);
static void notifAddFeedNotif (feedNotif_t *feedNotif_p);
static void notifRemoveFeedNotif (feedNotif_t *feedNotif_p);
static GtkWidget *notifCreateWin (void);
static gint feedNotifTimeoutCallback (gpointer data);
static void notifRemoveWin();
static gboolean onNotificationButtonPressed (GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean notifDeleteWinCb (GtkWidget *widget, GdkEvent *event, gpointer user_data);

static gboolean onNotificationButtonPressed (GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
	feedNotif_t *feedNotif_p = (feedNotif_t *) user_data;

	g_assert (feedNotif_p != NULL);
	if (event->type != GDK_BUTTON_PRESS) {
		return FALSE;
	} else if (event->button == 1) {
		ui_feedlist_select((nodePtr)feedNotif_p->feed_p);
		gdk_window_raise (mainwindow->window);
	} else {
		/* Any button except LMB will remove the notification */
		notifRemoveFeedNotif (feedNotif_p);
	}
	return TRUE;
}

static gint feedNotifTimeoutCallback (gpointer data) {
	feedNotif_t *feedNotif_p = (feedNotif_t *) data;
	notifRemoveFeedNotif (feedNotif_p);
	notifRemoveWin();
	return FALSE;
}

/* This function can only be used to find a feed in the notifications_p*/
static int notifCompare (gconstpointer a, gconstpointer b) {
	if (((feedNotif_t *) a)->feed_p == (feedPtr) b) {
		return 0;
	} else {
		return 1;
	}
}

void ui_notification_update(const feedPtr feed_p) {
	feedNotif_t *curNotif_p = NULL;
	GSList *list_p = NULL;
	
	if(!getBooleanConfValue(SHOW_POPUP_WINDOWS))
		return;

	list_p = g_slist_find_custom (notifications_p, feed_p, notifCompare);

	if (list_p != NULL) {
		curNotif_p = (feedNotif_t *) list_p->data;
	} else {
		if(0 == feed_get_new_counter(feed_p))
			return;

		curNotif_p = notifCreateFeedNotif (feed_p);
		notifications_p = g_slist_append (notifications_p, (gpointer) curNotif_p);
		g_assert (curNotif_p != NULL);
	}
	
	notifCheckFeedNotif (curNotif_p);
}	

static feedNotif_t *notifCreateFeedNotif (feedPtr feed_p) {
	feedNotif_t *feedNotif_p = NULL;

	g_assert (feed_p != NULL);
	
	feedNotif_p = g_new0 (feedNotif_t, 1);
	if (feedNotif_p != NULL) {
		feedNotif_p->feed_p = feed_p;
	}
	return feedNotif_p;
}

static void notifCheckFeedNotif (feedNotif_t *feedNotif_p) {
	if (feedNotif_p->newCount < feed_get_new_counter(feedNotif_p->feed_p)) {
		if (notifWin_p == NULL) {
			notifWin_p = notifCreateWin();
		}
		notifAddFeedNotif (feedNotif_p);
	} else if (feedNotif_p->newCount > feed_get_new_counter(feedNotif_p->feed_p)) {
		notifRemoveFeedNotif (feedNotif_p);
		notifRemoveWin();
	}
}

static void notifAddFeedNotif (feedNotif_t *feedNotif_p) {
	GtkWidget *label_p = NULL;
	gchar *labelText_p = NULL;
	itemPtr item_p = NULL;
	GSList *list_p = NULL;

	if (feedNotif_p->eventBox_p != NULL) {
		notifRemoveFeedNotif (feedNotif_p);
	}

	feedNotif_p->eventBox_p = gtk_event_box_new ();
	
	/* Listen on button clicks */
	g_signal_connect (G_OBJECT(feedNotif_p->eventBox_p), "button-press-event", (GCallback) onNotificationButtonPressed, (gpointer) feedNotif_p);

	feedNotif_p->box_p = gtk_vbox_new (FALSE, 4);
	gtk_container_add (GTK_CONTAINER(feedNotif_p->eventBox_p), feedNotif_p->box_p);
	
	/* Add the header label */
	label_p = gtk_label_new (NULL);
	gtk_label_set_use_markup (GTK_LABEL(label_p), TRUE);
	labelText_p = g_strdup_printf ("<b><u>%s</u></b>", feed_get_title(feedNotif_p->feed_p));
	gtk_label_set_markup (GTK_LABEL(label_p), labelText_p);
	g_free (labelText_p);
	gtk_misc_set_alignment (GTK_MISC(label_p), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC(label_p), 15, 10);
	gtk_box_pack_start (GTK_BOX(feedNotif_p->box_p), label_p, TRUE, TRUE, 0);

	/* Add the new items */
	list_p = feedNotif_p->feed_p->items;
	while (list_p != NULL) {
		item_p = list_p->data;
		if(item_get_new_status(item_p)) {
			item_set_new_status(item_p, FALSE);
			labelText_p = g_strdup_printf ("%s %s", NOTIF_BULLET, item_get_title(item_p) != NULL ? item_get_title(item_p) : _("Untitled"));
			label_p = gtk_label_new (labelText_p);
			g_free(labelText_p);
			gtk_label_set_line_wrap (GTK_LABEL(label_p), TRUE);
			gtk_label_set_justify (GTK_LABEL(label_p), GTK_JUSTIFY_LEFT);
			gtk_misc_set_alignment (GTK_MISC(label_p), 0.0, 0.5);
			gtk_misc_set_padding (GTK_MISC(label_p), 25, 0);
			gtk_box_pack_start (GTK_BOX(feedNotif_p->box_p), label_p, TRUE, TRUE, 0);
		}
		list_p = g_slist_next (list_p);
	}
	
	gtk_widget_show_all (feedNotif_p->eventBox_p);

	{
		GList *list_p = NULL;
		list_p = gtk_container_get_children (GTK_CONTAINER(notifWin_p));
		g_assert(list_p != NULL);
		gtk_box_pack_start(GTK_BOX(list_p->data), feedNotif_p->eventBox_p, FALSE, FALSE, 0);
	}
	
	feedNotif_p->newCount = feed_get_new_counter(feedNotif_p->feed_p);

	/* Add timer */
	feedNotif_p->timerTag = g_timeout_add (DISPLAY_TIME, feedNotifTimeoutCallback, (gpointer) feedNotif_p);
}

static void notifRemoveFeedNotif (feedNotif_t *feedNotif_p) {
	if (feedNotif_p->eventBox_p != NULL) {
		gtk_widget_destroy (feedNotif_p->eventBox_p);
		feedNotif_p->eventBox_p = NULL;
	}
	if (feedNotif_p->timerTag) {
		g_source_remove (feedNotif_p->timerTag);
		feedNotif_p->timerTag = 0;
	}
	feedNotif_p->newCount = feed_get_new_counter(feedNotif_p->feed_p);
}

/* to be called when a feed is removed and needs to be removed
   from the notification window too */
void ui_notification_remove_feed(feedPtr fp) {
	feedNotif_t	*feedNotif_p;
	GSList		*iter; 
	
	iter = notifications_p;
	while(NULL != iter) {
		feedNotif_p = iter->data;
		if(fp == feedNotif_p->feed_p) {
			notifRemoveFeedNotif(feedNotif_p);
			return;
		}
		iter = g_slist_next(iter);
	}
}

static GtkWidget *notifCreateWin (void) {
	GtkWidget *window_p = NULL;
	GtkWidget *vbox_p = NULL;

	window_p = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW(window_p), _("Liferea notification"));
	
	/* Don't make any special placement */
	gtk_window_set_position(GTK_WINDOW(window_p), GTK_WIN_POS_NONE);

	/* The user shouldn't be able to resize it */
	gtk_window_set_resizable (GTK_WINDOW(window_p), FALSE);

	/* Stick on all desktops */
	gtk_window_stick (GTK_WINDOW(window_p));

	/* Not in taskbar */
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW(window_p), TRUE);
	
	gtk_widget_set_size_request (window_p, NOTIF_WIN_WIDTH, NOTIF_WIN_HEIGHT);
	
	vbox_p = gtk_vbox_new (FALSE, 4);
	gtk_container_add (GTK_CONTAINER(window_p), vbox_p);

	gtk_widget_realize(window_p);
	gdk_window_set_decorations(window_p->window, GDK_DECOR_BORDER);
	
	g_signal_connect(window_p, "destroy", G_CALLBACK(notifDeleteWinCb), NULL);
	
	gtk_widget_show_all (window_p);
	
	gtk_window_move (GTK_WINDOW(window_p), NOTIF_WIN_POS_X, NOTIF_WIN_POS_Y);
	
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

void notifRemoveWin () {
	if (notifWin_p != NULL)		
	{
		GtkWidget *container_p;
		GList *list_p = gtk_container_get_children(GTK_CONTAINER(notifWin_p));
		if (list_p != NULL) {
			container_p = (GtkWidget *) list_p->data;
			g_list_free (list_p);
			list_p = gtk_container_get_children(GTK_CONTAINER(container_p));
			if (list_p == NULL) {
				/* Window is empty, destroy it */
				gtk_widget_destroy (notifWin_p);
				notifWin_p = NULL;
			} else {
				g_list_free (list_p);
			}
		}
	}
}
