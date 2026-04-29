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

#include <string.h>		/* For strcmp */
#include "common.h"
#include "db.h"
#include "node_providers/feed.h"
#include "feedlist.h"
#include "node_providers/folder.h"
#include "debug.h"
#include "ui/item_list_view.h"
#include "ui/feed_list_view.h"
#include "ui/liferea_shell.h"
#include "ui/ui_dnd.h"
#include "node_source.h"

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

static gboolean
on_drop (GtkDropTarget *target,
         const GValue *value,
         double x,
         double y,
         gpointer data)
{
	feedlist_add_subscription_check_duplicate (subscription_new (g_value_get_string (value), NULL, NULL));
	return TRUE;
}

void
ui_dnd_setup_feedlist (GtkTreeStore *feedstore)
{
	GtkDropTarget *target = gtk_drop_target_new (G_TYPE_INVALID, GDK_ACTION_COPY);

	gtk_drop_target_set_gtypes (target, (GType[1]) { G_TYPE_STRING }, 1);

	g_signal_connect (target, "drop", G_CALLBACK (on_drop), NULL);

	gtk_widget_add_controller (GTK_WIDGET (liferea_shell_lookup ("feedlist")), GTK_EVENT_CONTROLLER (target));
}
