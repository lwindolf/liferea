/**
 * @file ui_dnd.c everything concerning Drag&Drop
 *
 * Copyright (C) 2003-2026 Lars Windolf <lars.windolf@gmx.de>
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

#include "ui/ui_dnd.h"

#include <string.h>

#include "db.h"
#include "debug.h"
#include "feedlist.h"
#include "node_providers/feed.h"
#include "node_source.h"
#include "node_providers/folder.h"
#include "subscription.h"
#include "ui/feed_list_view.h"

/*
    Why does Liferea need such a complex DnD handling (for the feed list)?

     -> Because parts of the feed list might be un-draggable.
     -> Because drag source and target might be different node sources
        with even incompatible subscription types.
     -> Because removal at drag source and insertion at drop target
        must be atomic to avoid subscription losses.

    For simplicity the DnD code reuses the UI node removal and insertion
    methods that asynchronously apply the actions at the node source.

    (FIXME: implement the last part)
 */

static GtkWidget *feedlist_widget;
static GtkWidget *feedlist_drop_feedback_row;
static const gchar *feedlist_dragged_node_id;

static void
ui_dnd_clear_row_feedback_classes (GtkWidget *row)
{
	if (!row)
		return;

	gtk_widget_remove_css_class (row, "dnd-drop-before");
	gtk_widget_remove_css_class (row, "dnd-drop-into");
	gtk_widget_remove_css_class (row, "dnd-drop-after");
	gtk_widget_remove_css_class (row, "dnd-drop-invalid");
}

static void
ui_dnd_set_row_feedback_class (GtkWidget *row, const gchar *css_class)
{
	if (!row)
		return;

	if (feedlist_drop_feedback_row && feedlist_drop_feedback_row != row)
		ui_dnd_clear_row_feedback_classes (feedlist_drop_feedback_row);

	ui_dnd_clear_row_feedback_classes (row);
	if (css_class)
		gtk_widget_add_css_class (row, css_class);

	feedlist_drop_feedback_row = css_class ? row : NULL;
}

static void
ui_dnd_clear_drop_feedback (void)
{
	if (!feedlist_drop_feedback_row)
		return;

	ui_dnd_clear_row_feedback_classes (feedlist_drop_feedback_row);
	feedlist_drop_feedback_row = NULL;
}

static gboolean
ui_dnd_node_is_descendant (Node *candidate, Node *ancestor)
{
	Node *iter = candidate;

	while (iter) {
		if (iter == ancestor)
			return TRUE;
		iter = iter->parent;
	}

	return FALSE;
}

static gboolean
ui_dnd_feed_drop_possible (Node *sourceNode, Node *newParent)
{
	if (!sourceNode || !newParent)
		return FALSE;

	if (sourceNode == newParent)
		return FALSE;

	/* Never drop into our own subtree. */
	if (ui_dnd_node_is_descendant (newParent, sourceNode))
		return FALSE;

	/* Never drop into read-only node sources. */
	if (!(NODE_SOURCE_TYPE (newParent)->capabilities & NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST))
		return FALSE;

        /* Never drop into another node source as this arises to many problems
           (e.g. remote sync, different subscription type, e.g. SF #2855990) */
	// FIXME: this should be made possible for migration paths out of Liferea
	// into a future web app (using e.g. the WebDAV backend)
        if (NODE_SOURCE_TYPE (newParent) != NODE_SOURCE_TYPE (sourceNode))
		return FALSE;

	/* Never drop folders into flat feedlists. */
	if (IS_FOLDER (sourceNode) && !(NODE_SOURCE_TYPE (newParent)->capabilities & NODE_SOURCE_CAPABILITY_HIERARCHIC_FEEDLIST))
		return FALSE;

	return TRUE;
}

static const gchar *
ui_dnd_feed_drop_feedback_class (Node *targetNode, gdouble y, GtkWidget *row)
{
	gdouble	h = gtk_widget_get_height (row);

	if (!targetNode)
		return "dnd-drop-after";	// drop into feed list root always works
		
	if ((IS_FOLDER (targetNode) || IS_NODE_SOURCE (targetNode)) && (h > 0.0) && (y > h * 0.25) && (y < h * 0.75))
		return "dnd-drop-into";

	return (y > h / 2.0) ? "dnd-drop-after" : "dnd-drop-before";
}

static GdkContentProvider *
on_feed_drag_prepare (GtkDragSource *source, double x, double y, gpointer user_data)
{
	GtkWidget *row = GTK_WIDGET (user_data);
	Node *node = g_object_get_data (G_OBJECT (row), "node");

	if ((NODE_SOURCE_TYPE (node)->capabilities & NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST) == 0)
		return NULL;

	feedlist_dragged_node_id = node->id;

	return gdk_content_provider_new_typed (G_TYPE_STRING, node->id);
}

static void
on_feed_drag_end (GtkDragSource *source, GdkDrag *drag, gboolean delete_data, gpointer user_data)
{
	feedlist_dragged_node_id = NULL;
	ui_dnd_clear_drop_feedback ();
}

static void
on_feed_drop_motion (GtkDropControllerMotion *motion, gdouble x, gdouble y, gpointer data)
{
	GtkWidget *row = GTK_WIDGET (data);
	Node *targetNode = g_object_get_data (G_OBJECT (row), "node");
	const gchar *css_class = ui_dnd_feed_drop_feedback_class (targetNode, y, row);

	ui_dnd_set_row_feedback_class (row, css_class);
}

static void
on_feed_drop_leave (GtkDropControllerMotion *motion, gpointer data)
{
	GtkWidget *row = GTK_WIDGET (data);

	if (feedlist_drop_feedback_row == row)
		ui_dnd_clear_drop_feedback ();
}

static gboolean
on_feed_drop_on_row (GtkDropTarget *target,
	             const GValue *value,
	             double x,
	             double y,
	             gpointer data)
{
	GtkWidget	*row = GTK_WIDGET (data);
	Node		*targetNode = g_object_get_data (G_OBJECT (row), "node");
	Node		*node, *newParent;
	gint		insertPos = -1;

	if (IS_FOLDER (targetNode) || IS_NODE_SOURCE (targetNode)) {
		newParent = targetNode;
		insertPos = -1;
	} else {
		newParent = targetNode->parent;
		insertPos = g_slist_index (newParent->children, targetNode);
		if (insertPos < 0)
			insertPos = -1;
	}

	node = node_from_id (g_value_get_string (value));
	if (!ui_dnd_feed_drop_possible (node, newParent)) {
		ui_dnd_clear_drop_feedback ();
		return FALSE;
	}

	/* URL drops are still handled as feed subscription additions. */
	if (!node) {
		feedlist_add_subscription_check_duplicate (subscription_new (g_value_get_string (value), NULL, NULL));
		ui_dnd_clear_drop_feedback ();
		return TRUE;
	}

	feedlist_node_was_moved (node, newParent, insertPos, TRUE);
	return TRUE;
}

void
ui_dnd_setup_feedlist (GtkWidget *feedlist)
{
	GtkDropTarget *target;

	feedlist_widget = feedlist;

	target = gtk_drop_target_new (G_TYPE_INVALID, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_drop_target_set_gtypes (target, (GType[1]) { G_TYPE_STRING }, 1);
	// FIXME
	//g_signal_connect (target, "drop", G_CALLBACK (on_feed_drop), feedlist);
	gtk_widget_add_controller (feedlist, GTK_EVENT_CONTROLLER (target));
}

void
ui_dnd_setup_feedlist_row (GtkWidget *row)
{
	GtkDragSource *drag;
	GtkDropTarget *drop;
	GtkEventController *motion;

	if (!feedlist_widget)
		return;

	drag = gtk_drag_source_new ();
	gtk_drag_source_set_actions (drag, GDK_ACTION_MOVE);
	g_signal_connect (drag, "prepare", G_CALLBACK (on_feed_drag_prepare), row);
	//g_signal_connect (drag, "drag-end", G_CALLBACK (on_feed_drag_end), row);
	gtk_widget_add_controller (row, GTK_EVENT_CONTROLLER (drag));

	drop = gtk_drop_target_new (G_TYPE_INVALID, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_drop_target_set_gtypes (drop, (GType[1]) { G_TYPE_STRING }, 1);
	g_signal_connect (drop, "drop", G_CALLBACK (on_feed_drop_on_row), row);
	gtk_widget_add_controller (row, GTK_EVENT_CONTROLLER (drop));

	motion = gtk_drop_controller_motion_new ();
	g_signal_connect (motion, "motion", G_CALLBACK (on_feed_drop_motion), row);
	g_signal_connect (motion, "leave", G_CALLBACK (on_feed_drop_leave), row);
	gtk_widget_add_controller (row, motion);
}
