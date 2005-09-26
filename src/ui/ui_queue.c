/** 
 * @file ui_queue.c GUI callback managment
 *
 * Most of this code was derived from 
 *
 * Pan - A Newsreader for Gtk+
 * Copyright (C) 2002  Charles Kerr <charles@rebelbase.com>
 *
 * and modified to be suitable for Liferea
 *
 * Copyright (C) 2004  Lars Lindner <lars.lindner@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
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
#include "ui_queue.h"

extern GThread	*mainThread;	/* main.c */

/* FIXME: remove the ArgSet code... */
typedef struct
{
	GPtrArray * args;
}
ArgSet;

ArgSet*
argset_new (void) {
	ArgSet * argset = g_new0 (ArgSet, 1);
	argset->args = g_ptr_array_new ();
	return argset;
}
ArgSet* argset_new1 (gpointer p1)
{
	ArgSet * argset = argset_new ();
	g_ptr_array_add (argset->args, p1);
	return argset;
}
ArgSet*
argset_new2 (gpointer p1, gpointer p2) {
	ArgSet * argset = argset_new1 (p1);
	g_ptr_array_add (argset->args, p2);
	return argset;
}
ArgSet*
argset_new3 (gpointer p1, gpointer p2, gpointer p3) {
	ArgSet * argset = argset_new2 (p1, p2);
	g_ptr_array_add (argset->args, p3);
	return argset;
}
ArgSet*
argset_new4 (gpointer p1, gpointer p2, gpointer p3, gpointer p4) {
	ArgSet * argset = argset_new3 (p1, p2, p3);
	g_ptr_array_add (argset->args, p4);
	return argset;
}
ArgSet*
argset_new5 (gpointer p1, gpointer p2, gpointer p3, gpointer p4, gpointer p5) {
	ArgSet * argset = argset_new4 (p1, p2, p3, p4);
	g_ptr_array_add (argset->args, p5);
	return argset;
}
ArgSet*
argset_new6 (gpointer p1, gpointer p2, gpointer p3, gpointer p4, gpointer p5, gpointer p6) {
	ArgSet * argset = argset_new5 (p1, p2, p3, p4, p5);
	g_ptr_array_add (argset->args, p6);
	return argset;
}


gpointer
argset_get (ArgSet* argset, gint index) {
	g_return_val_if_fail (argset!=NULL, NULL);
	g_return_val_if_fail (argset->args!=NULL, NULL);
	g_return_val_if_fail (index>=0, NULL);
	g_return_val_if_fail (index<argset->args->len, NULL);
	return g_ptr_array_index (argset->args, index);
}

void
argset_add (ArgSet* argset, gpointer add) {
	g_return_if_fail (argset!=NULL);
	g_return_if_fail (argset->args!=NULL);
	g_ptr_array_add (argset->args, add);
}

void
argset_free (ArgSet * argset) {
	g_return_if_fail (argset!=NULL);
	g_return_if_fail (argset->args!=NULL);
	g_ptr_array_free (argset->args, TRUE);
	g_free (argset);
}
/* -------------------------------------------------------------------- */
/* idle functions							*/
/* -------------------------------------------------------------------- */

/**
 * Thus spoke the GTK FAQ: "Callbacks require a bit of attention.
 * Callbacks from GTK+ (signals) are made within the GTK+ lock. However
 * callbacks from GLib (timeouts, IO callbacks, and idle functions) are
 * made outside of the GTK+ lock. So, within a signal handler you do not
 * need to call gdk_threads_enter(), but within the other types of
 * callbacks, you do."
 *
 * ui_timeout_add() is a wrapper around a glib-level callbacks that
 * ensures ui_lock() works properly to a gdk_threads_enter() lock.
 */

static gboolean main_thread_is_in_glib_callback = FALSE;

static gint
ui_timeout_wrapper (gpointer data)
{
	ArgSet * argset = (ArgSet*) data;
	GtkFunction func = (GtkFunction) argset_get (argset, 0);
	gpointer user_data = (gpointer) argset_get (argset, 1);
	gint retval;

	/* sanity clause */
	g_assert (g_thread_self() == mainThread);

	/* call the timer func */
	main_thread_is_in_glib_callback = TRUE;
	retval = (*func)(user_data);
	main_thread_is_in_glib_callback = FALSE;

	/* cleanup */
	if (!retval)
		argset_free (argset);
	return retval;
}

guint
ui_timeout_add (guint32 interval, GSourceFunc func, gpointer arg)
{
	return g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE,
	                           interval,
	                           ui_timeout_wrapper,
	                           argset_new2 (func, arg),
	                           (GDestroyNotify)argset_free);
}

/* -------------------------------------------------------------------- */
/* GUI callback queue methods						*/
/* -------------------------------------------------------------------- */

static unsigned int ui_queue_timer_id = 0;
static GStaticMutex ui_queue_mutex = G_STATIC_MUTEX_INIT;
static GSList * ui_queue_slist = NULL;

typedef struct
{
	GSourceFunc run_func;
	gpointer user_data;
	GDestroyNotify remove_func;
	guint item_id;
}
GuiQueueItem;

static int
ui_queue_timer_cb (gpointer user_data)
{
	GSList * list;

	/* get the work list */
	g_static_mutex_lock (&ui_queue_mutex);
	list = ui_queue_slist;
	ui_queue_slist = NULL;
	g_static_mutex_unlock (&ui_queue_mutex);

	if (list != NULL)
	{
		GSList * l;

		/* ui_queue_slist is LIFO for efficiency of adding nodes;
		 * reverse the nodes to make list FIFO */
		list = g_slist_reverse (list);

		/* run the work functions */
		for (l=list; l!=NULL; l=l->next)
		{
			GuiQueueItem * queue_item = (GuiQueueItem*) l->data;
			(*queue_item->run_func)(queue_item->user_data);
			g_free (queue_item);
		}
		g_slist_free (list);
	}

	return 1;
}

void
ui_queue_init (void)
{
	ui_queue_timer_id = ui_timeout_add (200, ui_queue_timer_cb, NULL);
}

void
ui_queue_shutdown (void)
{
	g_source_remove (ui_queue_timer_id);
	ui_queue_timer_id = 0;
}

static gboolean
find_node_from_item_id (gconstpointer item_gpointer, gconstpointer id_key_gpointer)
{
	register const GuiQueueItem * item = (const GuiQueueItem *) item_gpointer;
	register const guint id_key = GPOINTER_TO_UINT (id_key_gpointer);

	/* return 0 when the desired element is found */
	return id_key == item->item_id ? 0 : 1;
}

void
ui_queue_remove (guint item_id)
{
	GSList * l = NULL;

	/* remove the item from the list */
	g_static_mutex_lock (&ui_queue_mutex);
	{
		l = g_slist_find_custom (ui_queue_slist, GUINT_TO_POINTER(item_id), find_node_from_item_id);
		if (l != NULL)
			ui_queue_slist = g_slist_remove_link (ui_queue_slist, l);
	}
	g_static_mutex_unlock (&ui_queue_mutex);

	/* run the remove function */
	if (l != NULL)
	{
		GuiQueueItem * queue_item = (GuiQueueItem*) l->data;
		if (queue_item->remove_func != NULL)
			(*queue_item->remove_func)(queue_item->user_data);
		g_free (queue_item);
		g_slist_free_1 (l);
	}
}

static void
ui_queue_remove_gpointer (gpointer item_id_gpointer)
{
	ui_queue_remove (GPOINTER_TO_UINT (item_id_gpointer));
}

guint
ui_queue_add_full_to_g_object (GSourceFunc run_func, gpointer user_data, GDestroyNotify remove_func, GObject * object)
{
	guint retval;
	GuiQueueItem * item;

	/* create the new queue item */	
	item = g_new (GuiQueueItem, 1);
	item->run_func = run_func;
	item->user_data = user_data;
	item->remove_func = remove_func;

	/* add it to the queue */
	g_static_mutex_lock (&ui_queue_mutex);
	{
		static guint item_id = 0;
		retval = item->item_id = item_id++;
		ui_queue_slist = g_slist_prepend (ui_queue_slist, item);

		/* are we attaching it to a GObject? */
		if (object!=NULL && G_IS_OBJECT(object))
		{
			char str [64];
			g_snprintf (str, sizeof(str), "ui_queue_item_%u", retval); /* make sure the GObject's data key is unique */
			g_object_set_data_full (object, str, GUINT_TO_POINTER(retval), ui_queue_remove_gpointer);
		}
	}
	g_static_mutex_unlock (&ui_queue_mutex);

	return retval;
}

guint
ui_queue_add (GSourceFunc run_func, gpointer user_data)
{
	return ui_queue_add_full_to_g_object (run_func, user_data, NULL, NULL);
}


/* -------------------------------------------------------------------- */
/* locking functions 							*/
/* -------------------------------------------------------------------- */
 
static const GThread * has_lock_thr = NULL;

void
ui_lock_from (const gchar * file, const gchar * func, int line)
{
	static const gchar * last_file = NULL;
	static const gchar * last_func = NULL;
	static int last_line = -1;
	const GThread * thr = g_thread_self ();
	/**
	 * If pan_lock() is called from the main thread while it has a GUI lock
	 * (typically from a gtk signals, like a button press signal etc.)
	 * then we don't need to lock.
	 *
	 * However if pan_lock() is called from a worker thread, or the main
	 * thread inside a glib idle function (via pan_timeout_add())
	 * then we _do_ need to obtain a gtk lock.
	 */
	if (thr==mainThread && !main_thread_is_in_glib_callback)
	{
		g_print ("mainthread %p attempted unnecessary lock from %s:%d (%s)", thr, file, line, func);
	}
	else if (thr == has_lock_thr)
	{
		g_error ("thread %p attempted double lock!\nfirst lock was in %s:%d (%s),\nnow trying for another one from %s:%d (%s)",
			thr,
			last_file, last_line, last_func,
			file, line, func);
	}
	else
	{
	       	/* if (thr==Pan.main_t && main_thread_is_in_glib_callback)
			odebug3 ("idle func %s:%d (%s) getting a pan lock", file, line, func);*/

		gdk_threads_enter();
		last_file = file;
		last_func = func;
		last_line = line;
		has_lock_thr = thr;
		/*g_print ("thread %p entered gdk_threads from %s %d", thr, file, line); */
	}
}

void
ui_unlock_from (const gchar* file, const gchar * func, int line)
{
	const GThread* thr = g_thread_self ();

	if (thr==mainThread && !has_lock_thr)
	{
		g_print ("mainthread %p attempted unnecessary unlock from %s:%d (%s)", thr, file, line, func);
	}
	else if(has_lock_thr != thr)
	{
		g_error ("thread %p attempted to remove a lock it didn't have from %s:%d (%s)", thr, file, line, func);
	}
	else
	{
		has_lock_thr = NULL;
		gdk_threads_leave();
		/*g_print ("thread %p left gdk_threads from %s:%d (%s)", g_thread_self(), file, line, func);*/
	}
}
