/*
 * @file itemview.c  viewing feed content in different presentation modes
 * 
 * Copyright (C) 2006-2015 Lars Windolf <lars.windolf@gmx.de>
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
#include "item_history.h"
#include "itemlist.h"
#include "itemview.h"
#include "node.h"
#include "ui/ui_common.h"
#include "ui/enclosure_list_view.h"
#include "ui/liferea_shell.h"
#include "ui/item_list_view.h"
#include "ui/liferea_htmlview.h"

/* The item view is the layer that switches item list presentations:
   a HTML single item or list and GtkTreeView list presentation. 
   It hides the item loading behaviour of GtkTreeView and HTML view.

   The item view does not handle item filtering, which is done by
   the item list implementation.
 */

#define ITEMVIEW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), ITEMVIEW_TYPE, ItemViewPrivate))

struct ItemViewPrivate {
	gboolean	htmlOnly;		/**< TRUE if HTML only mode */
	guint		mode;			/**< current item view mode */
	nodePtr		node;			/**< the node whose items are displayed */
	gboolean	browsing;		/**< TRUE if itemview is used as internal browser right now */
	gboolean	needsHTMLViewUpdate;	/**< flag to be set when HTML rendering is to be 
						     updated, used to delay HTML updates */
	gboolean	hasEnclosures;		/**< TRUE if at least one item of the current itemset has an enclosure */
						     
	nodeViewType	viewMode;		/**< current viewing mode */
	guint		currentLayoutMode;	/**< layout mode (3 pane, 2 pane, wide view) */
								     
	GtkWidget	*itemListViewContainer;	/**< scrolled window holding item list tree view */
	ItemListView	*itemListView;		/**< widget instance used to present items in list mode */

	EnclosureListView	*enclosureView;	/**< Enclosure list widget */
	LifereaHtmlView		*htmlview;	/**< HTML rendering widget instance used to render single items and summaries mode */

	gfloat			zoom;		/**< HTML rendering widget zoom level */
};

enum {
	PROP_NONE,
	PROP_ITEM_LIST_VIEW,
	PROP_HTML_VIEW
};

static GObjectClass *parent_class = NULL;
static ItemView *itemview = NULL;

G_DEFINE_TYPE (ItemView, itemview, G_TYPE_OBJECT);

static void
itemview_finalize (GObject *object)
{
	ItemViewPrivate *priv = ITEMVIEW_GET_PRIVATE (object);

	if (priv->htmlview) {
		/* save zoom preferences */
		conf_set_int_value (LAST_ZOOMLEVEL, (gint)(100.* liferea_htmlview_get_zoom (priv->htmlview)));

		g_object_unref (priv->htmlview);
	}
	
	if (priv->enclosureView)
		g_object_unref (priv->enclosureView);
	if (priv->itemListView)
		g_object_unref (priv->itemListView);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
itemview_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	ItemView *itemview = ITEMVIEW (object);

	switch (prop_id) {
		case PROP_ITEM_LIST_VIEW:
			g_value_set_object (value, itemview->priv->itemListView);
			break;
		case PROP_HTML_VIEW:
			g_value_set_object (value, itemview->priv->htmlview);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
itemview_class_init (ItemViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = itemview_get_property;

	object_class->finalize = itemview_finalize;

	/* ItemView:item-list-view: */
	g_object_class_install_property (object_class,
					PROP_ITEM_LIST_VIEW,
					g_param_spec_object (
						"item-list-view",
						"ItemListView",
						"ItemListView object",
						ITEM_LIST_VIEW_TYPE,
						G_PARAM_READABLE));

	/* ItemView:html-view: */
	g_object_class_install_property (object_class,
					PROP_HTML_VIEW,
					g_param_spec_object (
						"html-view",
						"LifereaHtmlView",
						"LifereaHtmlView object",
						LIFEREA_HTMLVIEW_TYPE,
						G_PARAM_READABLE));

	g_type_class_add_private (object_class, sizeof(ItemViewPrivate));
}

void
itemview_clear (void) 
{
	item_list_view_clear (itemview->priv->itemListView);
	htmlview_clear ();
	enclosure_list_view_hide (itemview->priv->enclosureView);
	itemview->priv->hasEnclosures = FALSE;
	itemview->priv->needsHTMLViewUpdate = TRUE;
	itemview->priv->browsing = FALSE;
}

void
itemview_set_mode (itemViewMode mode)
{
	if (itemview->priv->mode != mode) {
		/* FIXME: Not being able to call itemview_clear() here is awful! */
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

	/* 1. Reset view state */
	itemview_clear ();

	/* 2. And prepare HTML view */
	htmlview_set_displayed_node (node);
}

void
itemview_add_item (itemPtr item)
{
	itemview->priv->hasEnclosures |= item->hasEnclosure;

	if (ITEMVIEW_ALL_ITEMS != itemview->priv->mode)
		/* add item in 3 pane mode */
		item_list_view_add_item (itemview->priv->itemListView, item);
	else
		/* force HTML update in 2 pane mode */
		itemview->priv->needsHTMLViewUpdate = TRUE;
		
	htmlview_add_item (item);
}

void
itemview_remove_item (itemPtr item)
{
	if (!item_list_view_contains_id (itemview->priv->itemListView, item->id))
		return;

	if (ITEMVIEW_ALL_ITEMS != itemview->priv->mode)
		/* remove item in 3 pane mode */
		item_list_view_remove_item (itemview->priv->itemListView, item);
	else
		/* force HTML update in 2 pane mode */
		itemview->priv->needsHTMLViewUpdate = TRUE;

	htmlview_remove_item (item);
}

void
itemview_select_item (itemPtr item)
{
	ItemViewPrivate *ivp = itemview->priv;

	/* Enforce single item mode as we currently know no other way
	   to select a single item... */
	itemview_set_mode (ITEMVIEW_SINGLE_ITEM);

	ivp->needsHTMLViewUpdate = TRUE;
	ivp->browsing = FALSE;
	
	item_list_view_select (ivp->itemListView, item);
	htmlview_select_item (item);

	if (item)
		enclosure_list_view_load (ivp->enclosureView, item);
	else
		enclosure_list_view_hide (ivp->enclosureView);

	if (item)
		item_history_add (item->id);
}

void
itemview_select_enclosure (guint position)
{
	enclosure_list_view_select (itemview->priv->enclosureView, position);
}

void
itemview_update_item (itemPtr item)
{
	/* Always update the GtkTreeView (bail-out done in ui_itemlist_update_item() */
	if (ITEMVIEW_ALL_ITEMS != itemview->priv->mode)
		item_list_view_update_item (itemview->priv->itemListView, item);

	/* Bail if we do internal browsing, and no item is shown */
	if (itemview->priv->browsing)
		return;

	/* Bail out if no HTML update necessary */
	switch (itemview->priv->mode) {
		case ITEMVIEW_ALL_ITEMS:
			/* No HTML update needed if 2 pane mode and item not in item set */
			if (!item_list_view_contains_id (itemview->priv->itemListView, item->id))
				return;
			break;
		case ITEMVIEW_SINGLE_ITEM:		
			/* No HTML update needed if 3 pane mode and item not displayed */
			if (item->id != itemlist_get_selected_id ())
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
	/* Always update the GtkTreeView (bail-out done in ui_itemlist_update_item() */
	if (ITEMVIEW_ALL_ITEMS != itemview->priv->mode)
		item_list_view_update_all_items (itemview->priv->itemListView);

	itemview->priv->needsHTMLViewUpdate = TRUE;
	htmlview_update_all_items ();
}

void
itemview_update_node_info (nodePtr node)
{
	/* Bail if we do internal browsing, and no item is shown */
	if (itemview->priv->browsing)
		return;

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
	item_list_view_update (itemview->priv->itemListView, itemview->priv->hasEnclosures);
	
	if (itemview->priv->node) {
		item_list_view_enable_favicon_column (itemview->priv->itemListView, NODE_TYPE (itemview->priv->node)->capabilities & NODE_CAPABILITY_SHOW_ITEM_FAVICONS);
		item_list_view_set_sort_column (itemview->priv->itemListView, itemview->priv->node->sortColumn, itemview->priv->node->sortReversed);
	}
	
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

/* next unread selection logic */

itemPtr
itemview_find_unread_item (gulong startId) 
{
	itemPtr	result = NULL;
	
	/* Note: to select in sorting order we need to do it in the ItemListView
	   otherwise we would have to sort the item list here... */

	/* First do a scan from the start position (usually the selected 
	   item to the end of the sorted item list) if one is given. */	
	if (startId)
		result = item_list_view_find_unread_item (itemview->priv->itemListView, startId);
	
	/* Now perform a wrap around by searching again from the top */
	if (!result)
		result = item_list_view_find_unread_item (itemview->priv->itemListView, 0);

	return result;
}

void
itemview_scroll (void)
{
	liferea_htmlview_scroll (itemview->priv->htmlview);
}

void
itemview_move_cursor (int step)
{
	ui_common_treeview_move_cursor (item_list_view_get_widget (itemview->priv->itemListView), step);
}

void
itemview_move_cursor_to_first (void)
{
	ui_common_treeview_move_cursor_to_first (item_list_view_get_widget (itemview->priv->itemListView));
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
	
	if (newMode == ivp->currentLayoutMode)
		return;
	ivp->currentLayoutMode = newMode;

	liferea_shell_set_view_mode (newMode);
	
	if (!ivp->htmlview) {
		GtkWidget *renderWidget;
		
		debug0 (DEBUG_GUI, "Creating HTML widget");
		htmlview_init ();
		ivp->htmlview = liferea_htmlview_new (FALSE);
		liferea_htmlview_set_headline_view (ivp->htmlview);
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
	g_assert (htmlWidgetName);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (liferea_shell_lookup ("itemtabs")), newMode);
	gtk_widget_reparent (liferea_htmlview_get_widget (ivp->htmlview), liferea_shell_lookup (htmlWidgetName));

	/* Recreate the item list view */
	if (ivp->itemListViewContainer)
		gtk_widget_destroy (ivp->itemListViewContainer);

	if (ilWidgetName) {
		ivp->itemListView = item_list_view_create (newMode == NODE_VIEW_MODE_WIDE);
		ivp->itemListViewContainer = gtk_widget_get_parent (GTK_WIDGET (item_list_view_get_widget (ivp->itemListView)));
		g_assert (ivp->itemListViewContainer);
		gtk_container_add (GTK_CONTAINER (liferea_shell_lookup (ilWidgetName)), GTK_WIDGET (ivp->itemListViewContainer));
	}
	
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
itemview_create (GtkWidget *window)
{
	gint zoom;

	g_object_new (ITEMVIEW_TYPE, NULL);

	/* 1. Load preferences */
	conf_get_int_value (LAST_ZOOMLEVEL, &zoom);
	if (zoom == 0) {
		zoom = 100;
		conf_set_int_value (LAST_ZOOMLEVEL, zoom);
	}
	itemview->priv->zoom = zoom;

	/* 2. Set initial layout (because no node selected yet) */
	itemview_set_layout (NODE_VIEW_MODE_WIDE);
	
	return itemview;
}

void
itemview_launch_URL (const gchar *url, gboolean forceInternal)
{
	gboolean internal;
	
	if (forceInternal) {
		itemview->priv->browsing = TRUE;
		liferea_htmlview_launch_URL_internal (itemview->priv->htmlview, url);
		return;
	}
		
	/* Otherwise let the HTML view figure out if we want to browse internally. */
	internal = liferea_htmlview_handle_URL (itemview->priv->htmlview, url);
	
	if (!internal)
		liferea_htmlview_launch_URL_internal (itemview->priv->htmlview, url);
}

void
itemview_do_zoom (gboolean in)
{
	if (itemview->priv->htmlview == NULL)
		return;

	liferea_htmlview_do_zoom (itemview->priv->htmlview, in);
}

