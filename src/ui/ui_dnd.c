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
ui_dnd_feed_draggable (Node *node)
{
	if (!node || !node->parent)
		return FALSE;

	return (NODE_SOURCE_TYPE (node->parent)->capabilities & NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST) != 0;
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
        if (NODE_SOURCE_TYPE (newParent) != NODE_SOURCE_TYPE (sourceNode))
		return FALSE;

	/* Never drop folders into flat feedlists. */
	if (IS_FOLDER (sourceNode) && !(NODE_SOURCE_TYPE (newParent)->capabilities & NODE_SOURCE_CAPABILITY_HIERARCHIC_FEEDLIST))
		return FALSE;

	return TRUE;
}

static const gchar *
ui_dnd_feed_drop_feedback_class (Node *sourceNode, Node *targetNode, gdouble y, GtkWidget *rowWidget)
{
	Node *newParent;
	gboolean dropInto = FALSE;
	gdouble h;

	if (!sourceNode)
		return "dnd-drop-into";

	if (!ui_dnd_feed_draggable (sourceNode))
		return "dnd-drop-invalid";

	if (!targetNode)
		return ui_dnd_feed_drop_possible (sourceNode, feedlist_get_root ()) ? "dnd-drop-after" : "dnd-drop-invalid";

	h = gtk_widget_get_height (rowWidget);
	if (IS_FEED (sourceNode) && IS_FOLDER (targetNode)) {
		newParent = targetNode;
		dropInto = TRUE;
	} else if ((IS_FOLDER (targetNode) || IS_NODE_SOURCE (targetNode)) && (h > 0.0) && (y > h * 0.25) && (y < h * 0.75)) {
		newParent = targetNode;
		dropInto = TRUE;
	} else {
		newParent = targetNode->parent ? targetNode->parent : feedlist_get_root ();
	}

	if (!ui_dnd_feed_drop_possible (sourceNode, newParent))
		return "dnd-drop-invalid";

	if (dropInto)
		return "dnd-drop-into";

	return (y > h / 2.0) ? "dnd-drop-after" : "dnd-drop-before";
}

static void
ui_dnd_move_feed_node (Node *node, Node *newParent, gint insertPos)
{
	Node *oldParent;
	gint oldPos;

	if (!node || !newParent)
		return;

	oldParent = node->parent;
	if (!oldParent)
		return;

	oldPos = g_slist_index (oldParent->children, node);
	oldParent->children = g_slist_remove (oldParent->children, node);

	if ((oldParent == newParent) && insertPos > oldPos)
		insertPos--;

	newParent->children = g_slist_insert (newParent->children, node, insertPos);
	node->parent = newParent;

	db_node_update (node);

	if (NODE_SOURCE_TYPE (node)->capabilities & NODE_SOURCE_CAPABILITY_REPARENT_NODE)
		NODE_SOURCE_TYPE (node)->reparent_node (node, oldParent, newParent);

	/* Emit all so remote sources can track feed migrations. */
	feedlist_node_was_updated (node);
	feedlist_node_was_updated (oldParent);
	if (newParent != oldParent)
		feedlist_node_was_updated (newParent);
}

static gboolean
ui_dnd_feed_drop_common (const gchar *text, Node *targetNode, gdouble y, GtkWidget *rowWidget)
{
	Node *sourceNode;
	Node *newParent;
	gint insertPos = -1;
	gboolean dropInto = FALSE;

	if (!text)
		return FALSE;

	sourceNode = node_from_id (text);

	/* URL drops are still handled as feed subscription additions. */
	if (!sourceNode) {
		feedlist_add_subscription_check_duplicate (subscription_new (text, NULL, NULL));
		return TRUE;
	}

	if (!ui_dnd_feed_draggable (sourceNode))
		return FALSE;

	if (!targetNode) {
		newParent = feedlist_get_root ();
		if (!ui_dnd_feed_drop_possible (sourceNode, newParent))
			return FALSE;
		ui_dnd_move_feed_node (sourceNode, newParent, -1);
		return TRUE;
	}

	if (IS_FEED (sourceNode) && IS_FOLDER (targetNode)) {
		dropInto = TRUE;
	} else if (IS_FOLDER (targetNode) || IS_NODE_SOURCE (targetNode)) {
		gint h = gtk_widget_get_height (rowWidget);
		dropInto = (h > 0) && (y > h * 0.25) && (y < h * 0.75);
	}

	if (dropInto) {
		newParent = targetNode;
		insertPos = -1;
	} else {
		newParent = targetNode->parent ? targetNode->parent : feedlist_get_root ();
		insertPos = g_slist_index (newParent->children, targetNode);
		if (insertPos < 0)
			insertPos = -1;
		else if (y > gtk_widget_get_height (rowWidget) / 2.0)
			insertPos++;
	}

	if (!ui_dnd_feed_drop_possible (sourceNode, newParent))
		return FALSE;

	ui_dnd_move_feed_node (sourceNode, newParent, insertPos);
	return TRUE;
}

static GdkContentProvider *
on_feed_drag_prepare (GtkDragSource *source, double x, double y, gpointer user_data)
{
	GtkWidget *row = GTK_WIDGET (user_data);
	Node *node = g_object_get_data (G_OBJECT (row), "node");

	if (!ui_dnd_feed_draggable (node))
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
	Node *sourceNode = feedlist_dragged_node_id ? node_from_id (feedlist_dragged_node_id) : NULL;
	const gchar *css_class = ui_dnd_feed_drop_feedback_class (sourceNode, targetNode, y, row);

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
	GtkWidget *row = GTK_WIDGET (data);
	Node *targetNode = g_object_get_data (G_OBJECT (row), "node");
	const gchar *text = g_value_get_string (value);
	Node *sourceNode = node_from_id (text);
	const gchar *css_class = ui_dnd_feed_drop_feedback_class (sourceNode, targetNode, y, row);
	gboolean result;

	ui_dnd_set_row_feedback_class (row, css_class);
	result = ui_dnd_feed_drop_common (text, targetNode, y, row);
	ui_dnd_clear_drop_feedback ();
	return result;
}

static gboolean
on_feed_drop_on_listbox (GtkDropTarget *target,
	             const GValue *value,
	             double x,
	             double y,
	             gpointer data)
{
	GtkWidget *listview = GTK_WIDGET (data);
	GtkWidget *row = gtk_widget_pick (listview, x, y, GTK_PICK_DEFAULT);
	Node *targetNode = NULL;
	const gchar *text = g_value_get_string (value);

	while (row) {
		targetNode = g_object_get_data (G_OBJECT (row), "node");
		if (targetNode)
			break;
		row = gtk_widget_get_parent (row);
	}

	if (row) {
		double row_y = y;
		graphene_point_t src = GRAPHENE_POINT_INIT (0.0f, (float)y);
		graphene_point_t dst;

		if (gtk_widget_compute_point (listview, GTK_WIDGET (row), &src, &dst))
			row_y = dst.y;
		gboolean result = ui_dnd_feed_drop_common (text, targetNode, row_y, GTK_WIDGET (row));
		ui_dnd_clear_drop_feedback ();
		return result;
	}

	gboolean result = ui_dnd_feed_drop_common (text, NULL, y, listview);
	ui_dnd_clear_drop_feedback ();
	return result;
}

void
ui_dnd_setup_feedlist (GtkWidget *feedlist)
{
	GtkDropTarget *target;

	feedlist_widget = feedlist;

	target = gtk_drop_target_new (G_TYPE_INVALID, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_drop_target_set_gtypes (target, (GType[1]) { G_TYPE_STRING }, 1);
	g_signal_connect (target, "drop", G_CALLBACK (on_feed_drop_on_listbox), feedlist);
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
	g_signal_connect (drag, "drag-end", G_CALLBACK (on_feed_drag_end), row);
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
