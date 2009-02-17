/**
 * @file itemview.c  viewing feed content in different presentation modes
 * 
 * Copyright (C) 2006-2008 Lars Lindner <lars.lindner@gmail.com>
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

#include <string.h>

#include "common.h"
#include "conf.h"
#include "debug.h"
#include "folder.h"
#include "htmlview.h"
#include "itemlist.h"
#include "itemview.h"
#include "node.h"
#include "vfolder.h"
#include "ui/ui_common.h"
#include "ui/liferea_shell.h"
#include "ui/ui_itemlist.h"
#include "ui/liferea_htmlview.h"

static void itemview_class_init	(ItemViewClass *klass);
static void itemview_init	(ItemView *fl);

#define ITEMVIEW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), ITEMVIEW_TYPE, ItemViewPrivate))

struct ItemViewPrivate {
	gboolean	htmlOnly;		/**< TRUE if HTML only mode */
	guint		mode;			/**< current item view mode */
	nodePtr		node;			/**< the node whose item are displayed */
	gchar 		*userDefinedDateFmt;	/**< user defined date formatting string */
	gboolean	needsHTMLViewUpdate;	/**< flag to be set when HTML rendering is to be 
						     updated, used to delay HTML updates */
						     
	nodeViewType	viewMode;		/**< current viewing mode */
	guint		currentLayoutMode;	/**< layout mode (3 pane, 2 pane, wide view) */
								     
	itemPtr		resetSelection;		/**< if set indicates an incorrect item selection */

	GtkWidget	*itemlistContainer;	/**< scrolled window holding item list tree view */
	GtkTreeView	*itemlist;		

	EnclosureListView	*enclosureView;	/**< Enclosure list widget */
	LifereaHtmlView		*htmlview;	/**< HTML rendering widget used for item display */

	gfloat			zoom;		/**< HTML rendering widget zoom level */
};

static GObjectClass *parent_class = NULL;
static ItemView *itemview = NULL;

GType
itemview_get_type (void) 
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) 
	{
		static const GTypeInfo our_info = 
		{
			sizeof (ItemViewClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) itemview_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (ItemView),
			0, /* n_preallocs */
			(GInstanceInitFunc) itemview_init,
			NULL /* value_table */
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "ItemView",
					       &our_info, 0);
	}

	return type;
}

static void
itemview_finalize (GObject *object)
{
	// ItemView *iv = ITEMVIEW (object);

	/* save preferences */
	conf_set_int_value (LAST_ZOOMLEVEL, (gint)(100.* liferea_htmlview_get_zoom (itemview->priv->htmlview)));

	// FIXME: free everything	

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
itemview_class_init (ItemViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = itemview_finalize;

	g_type_class_add_private (object_class, sizeof(ItemViewPrivate));
}

void
itemview_clear (void) 
{

	ui_itemlist_clear ();
	htmlview_clear ();
	enclosure_list_view_hide (itemview->priv->enclosureView);
	
	itemview->priv->needsHTMLViewUpdate = TRUE;
}

void
itemview_set_mode (guint mode)
{
	if (itemview->priv->mode != mode) {
		itemview->priv->mode = mode;
		htmlview_clear ();	/* drop HTML rendering cache */
	}
}

void
itemview_set_displayed_node (nodePtr node)
{
	if (node == itemview->priv->node)
		return;
		
	itemview->priv->node = node;

	/* 1. Perform UI item list preparations ... */

	/* Free the old itemstore and create a new one; this is the only way to disable sorting */
	ui_itemlist_reset_tree_store ();	 /* this also clears the itemlist. */

	/* Disable attachment icon column (will be enabled when loading first item with an enclosure) */
	ui_itemlist_enable_encicon_column (FALSE);

	if (node) {
		ui_itemlist_enable_favicon_column (NODE_TYPE (node)->capabilities & NODE_CAPABILITY_SHOW_ITEM_FAVICONS);
		ui_itemlist_set_sort_column (node->sortColumn, node->sortReversed);
	}

	/* 2. Reset view state */
	itemview_clear ();

	/* 3. And prepare HTML view */
	htmlview_set_displayed_node (node);
}

void
itemview_add_item (itemPtr item)
{
	if (ITEMVIEW_ALL_ITEMS != itemview->priv->mode)
		/* add item in 3 pane mode */
		ui_itemlist_add_item (item);
	else
		/* force HTML update in 2 pane mode */
		itemview->priv->needsHTMLViewUpdate = TRUE;
		
	htmlview_add_item (item);
}

void
itemview_remove_item (itemPtr item)
{
	if (!ui_itemlist_contains_item (item->id))
		return;

	if (ITEMVIEW_ALL_ITEMS != itemview->priv->mode)
		/* remove item in 3 pane mode */
		ui_itemlist_remove_item (item);
	else
		/* force HTML update in 2 pane mode */
		itemview->priv->needsHTMLViewUpdate = TRUE;

	htmlview_remove_item (item);
}

void
itemview_set_invalid_selection (itemPtr item)
{
	debug0 (DEBUG_GUI, "Setting marker for unwanted selection!");
	itemview->priv->resetSelection = item;
}

void
itemview_select_item (itemPtr item)
{
	ItemViewPrivate *ivp = itemview->priv;
	
	if (!ivp->node)
		return;
		
	ivp->needsHTMLViewUpdate = TRUE;
	
	if (ivp->resetSelection == item) {
		debug0 (DEBUG_GUI, "Fixing unwanted selection!");
		ui_itemlist_select (NULL);
		ivp->resetSelection = NULL;
	}
		
	ui_itemlist_select (item);
	htmlview_select_item (item);

	if (item)
		enclosure_list_view_load (ivp->enclosureView, item);
	else
		enclosure_list_view_hide (ivp->enclosureView);
}

void
itemview_update_item (itemPtr item)
{
	if (!itemview->priv->node)
		return;
		
	/* Always update the GtkTreeView (bail-out done in ui_itemlist_update_item() */
	if (ITEMVIEW_ALL_ITEMS != itemview->priv->mode)
		ui_itemlist_update_item (item);

	/* Bail out if no HTML update necessary */
	switch (itemview->priv->mode) {
		case ITEMVIEW_ALL_ITEMS:
			/* No HTML update needed if 2 pane mode and item not in item set */
			if (!ui_itemlist_contains_item (item->id))
				return;
			break;
		case ITEMVIEW_SINGLE_ITEM:		
			/* No HTML update needed if 3 pane mode and item not displayed */
			if ((item != itemlist_get_selected ()) && 
			    !ui_itemlist_contains_item (item->id))
				return;
			break;
		default:
			/* Return in all other display modes */
			return;
			break;
	}
	
	itemview->priv->needsHTMLViewUpdate = TRUE;
	htmlview_update_item (item);
}

void
itemview_update_all_items (void)
{
	if (!itemview->priv->node)
		return;
		
	/* Always update the GtkTreeView (bail-out done in ui_itemlist_update_item() */
	if (ITEMVIEW_ALL_ITEMS != itemview->priv->mode)
		ui_itemlist_update_all_items ();
		
	itemview->priv->needsHTMLViewUpdate = TRUE;
	htmlview_update_all_items ();
}

void
itemview_update_node_info (nodePtr node)
{
	if (!itemview->priv->node)
		return;
	
	if (itemview->priv->node != node)
		return;

	if (ITEMVIEW_NODE_INFO != itemview->priv->mode)
		return;

	itemview->priv->needsHTMLViewUpdate = TRUE;
	/* Just setting the update flag, because node info is not cached */
}

void
itemview_update (void)
{
	if (itemview->priv->needsHTMLViewUpdate) {
		itemview->priv->needsHTMLViewUpdate = FALSE;
		htmlview_update (itemview->priv->htmlview, itemview->priv->mode);
	}
	if (itemview->priv->node)
		liferea_shell_update_allitems_actions (0 != itemview->priv->node->itemCount, 0 != itemview->priv->node->unreadCount);
}

void
itemview_display_info (const gchar *html)
{
	liferea_htmlview_write (itemview->priv->htmlview, html, NULL);
}

/* date format handling (not sure if this is the right place) */

gchar *
itemview_format_date (time_t date)
{
	gchar		*timestr;

	if (itemview->priv->userDefinedDateFmt)
		timestr = common_format_date (date, itemview->priv->userDefinedDateFmt);
	else
		timestr = common_format_nice_date (date);
	
	return timestr;
}

void
itemview_scroll (void)
{
	/* We try to scroll the HTML view, but if we are already at the
	   bottom of the item view the scrolling will return FALSE and 
	   we trigger Next-Unread to realize easy headline skimming. */
	if (liferea_htmlview_scroll (itemview->priv->htmlview) == FALSE)
		on_next_unread_item_activate (NULL, NULL);
		
	/* Note the above condition is duplicated in mozilla/mozsupport.cpp! */
}

void
itemview_move_cursor (int step)
{
	ui_common_treeview_move_cursor (itemview->priv->itemlist, step);
}

void
itemview_move_cursor_to_first (void)
{
	ui_common_treeview_move_cursor_to_first (itemview->priv->itemlist);
}

static void
itemview_init (ItemView *iv)
{
	debug_enter("itemview_init");
	
	/* 0. Prepare globally accessible singleton */
	g_assert (NULL == itemview);
	itemview = iv;
	
	itemview->priv = ITEMVIEW_GET_PRIVATE (iv);
	
	
	debug_exit("itemview_init");
}

static void
on_important_status_message (gpointer obj, gchar *url)
{
	liferea_shell_set_important_status_bar ("%s", url);
}

void
itemview_set_layout (nodeViewType newMode)
{
	ItemViewPrivate *ivp = itemview->priv;
	const gchar	*htmlWidgetName, *ilWidgetName, *encViewVBoxName;
	// GtkRadioAction	*action;
	
	if (newMode == ivp->currentLayoutMode)
		return;
	ivp->currentLayoutMode = newMode;

// FIXME:	action = GTK_RADIO_ACTION (gtk_action_group_get_action (ivp->generalActions, "NormalView"));
//		gtk_radio_action_set_current_value (action, newMode);
	
	if (!ivp->htmlview) {
		GtkWidget *renderWidget;
		
		debug0 (DEBUG_GUI, "Creating HTML widget");
		ivp->htmlview = liferea_htmlview_new (FALSE);
		g_signal_connect (ivp->htmlview, "statusbar-changed", 
		                  G_CALLBACK (on_important_status_message), NULL);
		renderWidget = liferea_htmlview_get_widget (ivp->htmlview);
		gtk_container_add (GTK_CONTAINER (liferea_shell_lookup ("normalViewHtml")), renderWidget);
		gtk_widget_show (renderWidget);
		
		/* Set initial zoom */
		liferea_htmlview_set_zoom (itemview->priv->htmlview, itemview->priv->zoom/100.);
	} else {	
		liferea_htmlview_clear (ivp->htmlview);
	}

	debug1 (DEBUG_GUI, "Setting item list layout mode: %d", newMode);
	
	switch (newMode) {
		case NODE_VIEW_MODE_NORMAL:
			htmlWidgetName = "normalViewHtml";
			ilWidgetName = "normalViewItems";
			encViewVBoxName = "normalViewVBox";
			break;
		case NODE_VIEW_MODE_WIDE:
			htmlWidgetName = "wideViewHtml";
			ilWidgetName = "wideViewItems";
			encViewVBoxName = "wideViewVBox";
			break;
		case NODE_VIEW_MODE_COMBINED:
			htmlWidgetName = "combinedViewHtml";
			ilWidgetName = NULL;
			encViewVBoxName = NULL;
			break;
		default:
			g_warning("fatal: illegal viewing mode!");
			return;
			break;
	}

	/* Reparenting HTML view. This avoids the overhead of new browser instances. */
	gtk_notebook_set_current_page (GTK_NOTEBOOK (liferea_shell_lookup ("itemtabs")), newMode);
	gtk_widget_reparent (liferea_htmlview_get_widget (ivp->htmlview), liferea_shell_lookup (htmlWidgetName));
	if (ilWidgetName)
		gtk_widget_reparent (GTK_WIDGET (ivp->itemlistContainer), liferea_shell_lookup (ilWidgetName));
	
	/* Destroy previous enclosure list. */
	if (ivp->enclosureView) {
		gtk_widget_destroy (enclosure_list_view_get_widget (ivp->enclosureView));
		ivp->enclosureView = NULL;
	}

	/* Create a new enclosure list GtkTreeView. */		
	if (encViewVBoxName) {
		ivp->enclosureView = enclosure_list_view_new ();
		gtk_box_pack_end (GTK_BOX (liferea_shell_lookup (encViewVBoxName)),
		                  enclosure_list_view_get_widget (ivp->enclosureView),
				  FALSE, FALSE, 0);
		gtk_widget_show_all (liferea_shell_lookup (encViewVBoxName));
	}
}

ItemView *
itemview_create (GtkWidget *ilc)
{
	/* 1. Create widgets, load preferences */
	
	g_object_new (ITEMVIEW_TYPE, NULL);
	
	itemview->priv->currentLayoutMode = NODE_VIEW_MODE_INVALID;
	itemview->priv->itemlistContainer = ilc;
	itemview->priv->itemlist = GTK_TREE_VIEW (gtk_bin_get_child (GTK_BIN (itemview->priv->itemlistContainer)));
	itemview->priv->zoom = conf_get_int_value (LAST_ZOOMLEVEL);
	itemview->priv->userDefinedDateFmt = conf_get_str_value (DATE_FORMAT);

	/* initially we pack the item list in the normal view pane,
	   which is later changed in itemview_set_layout() */
	gtk_container_add (GTK_CONTAINER (liferea_shell_lookup ("normalViewItems")), itemview->priv->itemlistContainer);

	/* 2. Sanity checks on the settings... */
	
	/* We now have an empty string or a format string... */
	if (itemview->priv->userDefinedDateFmt && !strlen (itemview->priv->userDefinedDateFmt)) {
		/* It's empty and useless... */
		g_free (itemview->priv->userDefinedDateFmt);
		itemview->priv->userDefinedDateFmt = NULL;
	}
	
	if (itemview->priv->userDefinedDateFmt) {
		debug1 (DEBUG_GUI, "user defined date format is: >>>%s<<<", itemview->priv->userDefinedDateFmt);
	} else {
		debug0 (DEBUG_GUI, "using default date format");
	}

	if (0 == itemview->priv->zoom) {	/* workaround for scheme problem with the last releases */
		itemview->priv->zoom = 100;
		conf_set_int_value (LAST_ZOOMLEVEL, 100);
	}
	
	/* 3. Prepare HTML rendering */
	htmlview_init ();
	
	return itemview;
}

GtkStyle *
itemview_get_style (void)
{
	return gtk_widget_get_style (liferea_htmlview_get_widget (itemview->priv->htmlview));
}

void
itemview_launch_URL (const gchar *url, gboolean forceInternal)
{
	liferea_htmlview_launch_URL (itemview->priv->htmlview, url, forceInternal);
}

void
itemview_do_zoom (gboolean in)
{
	liferea_htmlview_do_zoom (itemview->priv->htmlview, in);
}
