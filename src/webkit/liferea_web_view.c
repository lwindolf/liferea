/**
 * @file liferea_web_view.c  Webkit2 widget for Liferea
 *
 * Copyright (C) 2016 Leiaz <leiaz@free.fr>
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

#include "liferea_web_view.h"

#include "../debug.h"
#include "browser.h"
#include "common.h"
#include "enclosure.h"
#include "feedlist.h"
#include "social.h"
#include "ui/browser_tabs.h"
#include "ui/liferea_htmlview.h"
#include "ui/item_list_view.h"
#include "ui/itemview.h"
#include "web_extension/liferea_web_extension_names.h"

struct _LifereaWebView {
	WebKitWebView		parent;

	GActionGroup            *menu_action_group;
	GDBusConnection 	*dbus_connection;
};

struct _LifereaWebViewClass {
	WebKitWebViewClass parent_class;
};

G_DEFINE_TYPE (LifereaWebView, liferea_web_view, WEBKIT_TYPE_WEB_VIEW)

static void
liferea_web_view_finalize(GObject *gobject)
{
	LifereaWebView *self = LIFEREA_WEB_VIEW(gobject);

	if (self->dbus_connection) {
		g_object_remove_weak_pointer (G_OBJECT (self->dbus_connection), (gpointer *) &self->dbus_connection);
	}

	/* Chaining finalize from parent class. */
	G_OBJECT_CLASS(liferea_web_view_parent_class)->finalize(gobject);
}

static void
liferea_web_view_class_init(LifereaWebViewClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->finalize = liferea_web_view_finalize;
}

static void
can_copy_callback (GObject *web_view, GAsyncResult *result, gpointer user_data)
{
	gboolean 	enabled;
	GError 		*error = NULL;
	GActionGroup 	*action_group;
	GSimpleAction 	*copy_action;

	enabled = webkit_web_view_can_execute_editing_command_finish (WEBKIT_WEB_VIEW (web_view), result, &error);

	if (error) {
		g_warning ("Error can_execute_editing_command callback : %s\n", error->message);
		g_error_free (error);
		return;
	}

	action_group = LIFEREA_WEB_VIEW (web_view)->menu_action_group;
	copy_action = G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (action_group), "copy-selection"));
	g_simple_action_set_enabled (copy_action, enabled);
}

static void
liferea_web_view_update_actions_sensitivity (LifereaWebView *self)
{
	webkit_web_view_can_execute_editing_command (
		WEBKIT_WEB_VIEW (self),
		WEBKIT_EDITING_COMMAND_COPY,
		NULL,
		can_copy_callback,
                NULL);
}

/*
 * Copied from liferea_htmlview.c Perhaps could go in common.h ?
 */
static gboolean
liferea_web_view_is_special_url (const gchar *url)
{
	/* match against all special protocols, simple
	   convention: all have to start with "liferea-" */
	if (url == strstr (url, "liferea-"))
		return TRUE;

	return FALSE;
}

static void
menu_add_item (GMenu *menu, const gchar *label, const gchar *action, const gchar *parameter)
{
	GMenuItem *menu_item;

	menu_item = g_menu_item_new (label, NULL);
	g_menu_item_set_action_and_target (menu_item, action, "s", parameter);
	g_menu_append_item (menu, menu_item);
	g_object_unref (menu_item);
}

/**
 * Callback for WebKitWebView::context-menu signal.
 *
 * @view: the object on which the signal is emitted
 * @context_menu: the context menu proposed by WebKit
 * @event: the event that triggered the menu
 * @hit_result: result of hit test at that location.
 *
 * When a context menu is about to be displayed this signal is emitted.
 *
 */
static gboolean
liferea_web_view_on_menu (WebKitWebView 	*view,
			WebKitContextMenu   	*context_menu,
			GdkEvent            	*event,
			WebKitHitTestResult 	*hit_result,
			gpointer             	user_data)
{
	GtkWidget 		*menu;
	GMenu 			*menu_model,*section;
	gchar			*image_uri = NULL;
	gchar			*link_uri = NULL;
	gchar			*link_title = NULL;
	gboolean 		link, image;

	if (webkit_hit_test_result_context_is_link (hit_result))
		g_object_get (hit_result, "link-uri", &link_uri, "link-title", &link_title, NULL);
	if (webkit_hit_test_result_context_is_image (hit_result))
		g_object_get (hit_result, "image-uri", &image_uri, NULL);
	if (webkit_hit_test_result_context_is_media (hit_result))
		g_object_get (hit_result, "media-uri", &link_uri, NULL);		/* treat media as normal link */

	/* Making the menu */
	link = (link_uri != NULL);
	image = (image_uri != NULL);

	/* do not expose internal links */
	if (link_uri && liferea_web_view_is_special_url (link_uri) && !g_str_has_prefix (link_uri, "javascript:") && !g_str_has_prefix (link_uri, "data:"))
		link = FALSE;

	liferea_web_view_update_actions_sensitivity (LIFEREA_WEB_VIEW (view));

	menu_model = g_menu_new ();
	section = g_menu_new (); /* Sections are used to get separators.*/

	/* and now add all we want to see */
	if (link) {
		gchar *path;
		GMenuItem *menu_item;

		menu_add_item (section, _("Open Link In _Tab"), "app.open-link-in-tab", link_uri);
		menu_add_item (section, _("Open Link In Browser"), "app.open-link-in-browser", link_uri);
		menu_add_item (section, _("Open Link In External Browser"), "app.open-link-in-external-browser", link_uri);

		g_menu_append_section (menu_model, NULL, G_MENU_MODEL (section));
		g_object_unref (section);
		section = g_menu_new ();

		path = g_strdup_printf (_("_Bookmark Link at %s"), social_get_bookmark_site ());
		menu_item = g_menu_item_new (path, NULL);
		g_menu_item_set_action_and_target (menu_item, "app.social-bookmark-link", "(ss)", link_uri, link_title?link_title:"");
		g_menu_append_item (section, menu_item);
		g_object_unref (menu_item);
		g_free (path);

		menu_add_item (section, _("_Copy Link Location"), "app.copy-link-to-clipboard", link_uri);
	}
	if (image) {
		menu_add_item (section, _("_View Image"),           "app.open-link-in-tab", image_uri);
		menu_add_item (section, _("_Copy Image Location"),  "app.copy-link-to-clipboard", image_uri);
	}
	if (link) {
		menu_add_item (section, _("S_ave Link As"), "liferea_web_view.save-link", link_uri);
	}
	if (image) {
		menu_add_item (section, _("S_ave Image As"), "liferea_web_view.save-link", image_uri);
	}
	if (link) {
		g_menu_append_section (menu_model, NULL, G_MENU_MODEL (section));
		g_object_unref (section);
		section = g_menu_new ();

		menu_add_item (section, _("_Subscribe..."), "liferea_web_view.subscribe-link", link_uri);
	}

	if(!link && !image) {
		g_menu_append (section, _("_Copy"), "liferea_web_view.copy-selection");

		g_menu_append_section (menu_model, NULL, G_MENU_MODEL (section));
		g_object_unref (section);
		section = g_menu_new ();

		g_menu_append (section, _("_Increase Text Size"), "liferea_web_view.zoom-in");
		g_menu_append (section, _("_Decrease Text Size"), "liferea_web_view.zoom-out");
	}

	g_menu_append_section (menu_model, NULL, G_MENU_MODEL (section));
	g_object_unref (section);
	g_free (link_uri);
	g_free (image_uri);
	g_free (link_title);

	if(debug_level & DEBUG_HTML) {
		section = g_menu_new ();
		g_menu_append (section, "Inspect", "liferea_web_view.web-inspector");
		g_menu_append_section (menu_model, NULL, G_MENU_MODEL (section));
		g_object_unref (section);
	}

	menu = gtk_menu_new_from_model (G_MENU_MODEL (menu_model));
	gtk_menu_attach_to_widget (GTK_MENU (menu), GTK_WIDGET (view), NULL);

	gtk_menu_popup_at_pointer (GTK_MENU (menu), event);

	return TRUE; // TRUE to ignore WebKit's menu as we make our own menu.
}

static void
on_popup_copy_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	webkit_web_view_execute_editing_command (WEBKIT_WEB_VIEW (user_data), WEBKIT_EDITING_COMMAND_COPY);
}

static void
on_popup_save_link_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	enclosure_download (NULL, g_variant_get_string (parameter, NULL), TRUE);
}

static void
on_popup_subscribe_link_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	feedlist_add_subscription (g_variant_get_string (parameter, NULL), NULL, NULL, 0);
}

static void
on_popup_zoomin_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	LifereaHtmlView *htmlview = g_object_get_data (G_OBJECT (user_data), "htmlview");
	liferea_htmlview_do_zoom (htmlview, TRUE);
}

static void
on_popup_zoomout_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	LifereaHtmlView *htmlview = g_object_get_data (G_OBJECT (user_data), "htmlview");
	liferea_htmlview_do_zoom (htmlview, FALSE);
}

static void
on_popup_webinspector_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	WebKitSettings *settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (user_data));
	g_object_set (G_OBJECT(settings), "enable-developer-extras", TRUE, NULL);

	WebKitWebInspector *inspector = webkit_web_view_get_inspector (WEBKIT_WEB_VIEW (user_data));
	webkit_web_inspector_show (WEBKIT_WEB_INSPECTOR(inspector));
}

static const GActionEntry liferea_web_view_gaction_entries[] = {
	{"save-link", on_popup_save_link_activate, "s", NULL, NULL},
	{"subscribe-link", on_popup_subscribe_link_activate, "s", NULL, NULL},
	{"copy-selection", on_popup_copy_activate, NULL, NULL, NULL},
	{"zoom-in", on_popup_zoomin_activate, NULL, NULL, NULL},
	{"zoom-out", on_popup_zoomout_activate, NULL, NULL, NULL},
	{"web-inspector", on_popup_webinspector_activate, NULL, NULL, NULL}
};

static void
liferea_web_view_title_changed (WebKitWebView *view, GParamSpec *pspec, gpointer user_data)
{
	LifereaHtmlView	*htmlview;
	gchar *title;

	htmlview = g_object_get_data (G_OBJECT (view), "htmlview");
	g_object_get (view, "title", &title, NULL);

	liferea_htmlview_title_changed (htmlview, title);
	g_free (title);
}

/*
 *  Callback for the mouse-target-changed signal.
 *
 *  Updates selected_url with hovered link.
 */
static void
liferea_web_view_on_mouse_target_changed (WebKitWebView 	*view,
					  WebKitHitTestResult 	*hit_result,
					  guint                	modifiers,
					  gpointer             	user_data)
{
	LifereaHtmlView	*htmlview;
	gchar *selected_url;

	htmlview = g_object_get_data (G_OBJECT (view), "htmlview");
	selected_url = g_object_get_data (G_OBJECT (view), "selected_url");
	if (selected_url)
		g_free (selected_url);

	if (webkit_hit_test_result_context_is_link (hit_result))
	{
		g_object_get (hit_result, "link-uri", &selected_url, NULL);
	} else {
		selected_url = g_strdup ("");
	}

	/* overwrite or clear last status line text */
	liferea_htmlview_on_url (htmlview, selected_url);

	g_object_set_data (G_OBJECT (view), "selected_url", selected_url);
}

struct FullscreenData {
	GtkWidget *me;
	gboolean visible;
};
/**
 * callback for fullscreen mode gtk_container_foreach()
 */
static void
fullscreen_toggle_widget_visible(GtkWidget *wid, gpointer user_data) {
	gchar* data_label;
	struct FullscreenData *fdata;
	gboolean old_v;
	gchar *propName;

	fdata = user_data;

	// remove shadow of scrolled window
	if (GTK_IS_SCROLLED_WINDOW(wid)) {
		GtkShadowType shadow_type;

		data_label = "fullscreen_shadow_type";
		propName = "shadow-type";

		if (fdata->visible == FALSE) {
			g_object_get(G_OBJECT(wid),
					propName, &shadow_type, NULL);
			g_object_set(G_OBJECT(wid),
					propName, GTK_SHADOW_NONE, NULL);
			g_object_set_data(G_OBJECT(wid), data_label,
					GINT_TO_POINTER(shadow_type));
		} else {
			shadow_type = GPOINTER_TO_INT(g_object_steal_data(
						G_OBJECT(wid), data_label));
			if (shadow_type && shadow_type != GTK_SHADOW_NONE) {
				g_object_set(G_OBJECT(wid),
						propName, shadow_type, NULL);
			}
		}
	}

	if (wid == fdata->me && !GTK_IS_NOTEBOOK(wid)) {
		return;
	}

	data_label = "fullscreen_visible";
	if (GTK_IS_NOTEBOOK(wid)) {
		propName = "show-tabs";
	} else {
		propName = "visible";
	}

	if (fdata->visible == FALSE) {
		g_object_get(G_OBJECT(wid), propName, &old_v, NULL);
		g_object_set(G_OBJECT(wid), propName, FALSE, NULL);
		g_object_set_data(G_OBJECT(wid), data_label,
				GINT_TO_POINTER(old_v));
	} else {
		old_v = GPOINTER_TO_INT(g_object_steal_data(
					G_OBJECT(wid), data_label));
		if (old_v == TRUE) {
			g_object_set(G_OBJECT(wid), propName, TRUE, NULL);
		}
	}
}

/**
 * For fullscreen mode, hide everything except the current webview
 */
static void
fullscreen_toggle_parent_visible(GtkWidget *me, gboolean visible) {
	GtkWidget *parent;
	struct FullscreenData *fdata;
	fdata = (struct FullscreenData *)g_new0(struct FullscreenData, 1);

	// Flag fullscreen status
	g_object_set_data(G_OBJECT(me), "fullscreen_on",
			GINT_TO_POINTER(!visible));

	parent = gtk_widget_get_parent(me);
	fdata->visible = visible;
	while (parent != NULL) {
		fdata->me = me;
		gtk_container_foreach(GTK_CONTAINER(parent),
				(GtkCallback)fullscreen_toggle_widget_visible,
				(gpointer)fdata);
		me = parent;
		parent = gtk_widget_get_parent(me);
	}
	g_free(fdata);
}

/**
 * WebKitWebView "enter-fullscreen" signal
 * Hide all the widget except current WebView
 */
static gboolean
liferea_web_view_entering_fullscreen (WebKitWebView *view, gpointer user_data)
{
	fullscreen_toggle_parent_visible(GTK_WIDGET(view), FALSE);
	return FALSE;
}

/**
 * WebKitWebView "leave-fullscreen" signal
 * Restore visibility of hidden widgets
 */
static gboolean
liferea_web_view_leaving_fullscreen (WebKitWebView *view, gpointer user_data)
{
	fullscreen_toggle_parent_visible(GTK_WIDGET(view), TRUE);
	return FALSE;
}

/**
 * A link has been clicked
 *
 * When a link has been clicked the link management is dispatched to Liferea
 * core in order to manage the different filetypes, remote URLs.
 */
static gboolean
liferea_web_view_link_clicked ( WebKitWebView 		*view,
				WebKitPolicyDecision 	*policy_decision)
{
	const gchar			*uri;
	WebKitNavigationAction 		*navigation_action;
	WebKitURIRequest		*request;
	WebKitNavigationType		reason;
	gboolean			url_handled;

	g_return_val_if_fail (WEBKIT_IS_WEB_VIEW (view), FALSE);
	g_return_val_if_fail (WEBKIT_IS_POLICY_DECISION (policy_decision), FALSE);

	navigation_action = webkit_navigation_policy_decision_get_navigation_action (WEBKIT_NAVIGATION_POLICY_DECISION (policy_decision));
	reason = webkit_navigation_action_get_navigation_type (navigation_action);

	/* iframes in items return WEBKIT_WEB_NAVIGATION_REASON_OTHER
	   and shouldn't be handled as clicks                          */
	if (reason != WEBKIT_NAVIGATION_TYPE_LINK_CLICKED)
		return FALSE;

	request = webkit_navigation_action_get_request (navigation_action);
	uri = webkit_uri_request_get_uri (request);

	if (webkit_navigation_action_get_mouse_button (navigation_action) == 2) { /* middle click */
		browser_tabs_add_new (uri, uri, FALSE);
		webkit_policy_decision_ignore (policy_decision);
		return TRUE;
	}

	url_handled = liferea_htmlview_handle_URL (g_object_get_data (G_OBJECT (view), "htmlview"), uri);

	if (url_handled)
		webkit_policy_decision_ignore (policy_decision);

	return url_handled;
}

/**
 * A new window was requested. This is the case e.g. if the link
 * has target="_blank". In that case, we don't open the link in a new
 * tab, but do what the user requested as if it didn't have a target.
 */
static gboolean
liferea_web_view_new_window_requested (	WebKitWebView *view,
					WebKitPolicyDecision *policy_decision)
{
	WebKitNavigationAction 		*navigation_action;
	WebKitURIRequest		*request;
	const gchar 			*uri;

	navigation_action = webkit_navigation_policy_decision_get_navigation_action (WEBKIT_NAVIGATION_POLICY_DECISION (policy_decision));
	request = webkit_navigation_action_get_request (navigation_action);
	uri = webkit_uri_request_get_uri (request);

	if (webkit_navigation_action_get_mouse_button (navigation_action) == 2) {
		/* middle-click, let's open the link in a new tab */
		browser_tabs_add_new (uri, uri, FALSE);
	} else if (liferea_htmlview_handle_URL (g_object_get_data (G_OBJECT (view), "htmlview"), uri)) {
		/* The link is to be opened externally, let's do nothing here */
	} else {
		/* If the link is not to be opened in a new tab, nor externally,
		 * it was likely a normal click on a target="_blank" link.
		 * Let's open it in the current view to not disturb users */
		webkit_web_view_load_uri (view, uri);
	}

	/* We handled the request ourselves */
	webkit_policy_decision_ignore (policy_decision);
	return TRUE;
}

static gboolean
liferea_web_view_response_decision_requested (WebKitWebView *view,
                                              WebKitPolicyDecision *decision)
{
	g_return_val_if_fail (WEBKIT_IS_RESPONSE_POLICY_DECISION (decision), FALSE);

	if (!webkit_response_policy_decision_is_mime_type_supported (WEBKIT_RESPONSE_POLICY_DECISION (decision))) {
		webkit_policy_decision_download (decision);
		return TRUE;
	}
	return FALSE;
}

static gboolean
liferea_web_view_decide_policy (WebKitWebView *view,
			      WebKitPolicyDecision *decision,
			      WebKitPolicyDecisionType type)
{
	switch (type)
	{
		case WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION:
			return liferea_web_view_link_clicked (view, decision);
		case WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION:
			return liferea_web_view_new_window_requested(view, decision);
		case WEBKIT_POLICY_DECISION_TYPE_RESPONSE:
                        return liferea_web_view_response_decision_requested (view, decision);
		default:
			return FALSE;
	}
	return FALSE;
}

/**
 *  e.g. after a click on javascript:openZoom()
 */
static WebKitWebView*
liferea_web_view_create_web_view (WebKitWebView *view, WebKitNavigationAction *action, gpointer user_data)
{
	LifereaHtmlView *htmlview;
	GtkWidget	*container;
	GtkWidget	*htmlwidget;
	GList 		*children;

	htmlview = browser_tabs_add_new (NULL, NULL, TRUE);
	container = liferea_htmlview_get_widget (htmlview);

	/* Ugly lookup of the webview. LifereaHtmlView uses a GtkBox
	   with first a URL bar (sometimes invisble) and the HTML renderer
	   as 2nd child */
	children = gtk_container_get_children (GTK_CONTAINER (container));
	htmlwidget = children->next->data;

	return WEBKIT_WEB_VIEW (htmlwidget);
}

static void
liferea_webkit_load_status_changed (WebKitWebView *view, WebKitLoadEvent event, gpointer user_data)
{
	LifereaHtmlView	*htmlview;
	gboolean isFullscreen;

	switch (event) {
		case WEBKIT_LOAD_STARTED:
			// Hack to force webview exit from fullscreen mode on new page
			isFullscreen = GPOINTER_TO_INT(g_object_steal_data(
						G_OBJECT(view), "fullscreen_on"));
			if (isFullscreen == TRUE) {
				webkit_web_view_run_javascript (view, "document.webkitExitFullscreen();", NULL, NULL, NULL);
			}
			break;
		case WEBKIT_LOAD_COMMITTED:
			htmlview = g_object_get_data (G_OBJECT (view), "htmlview");
			liferea_htmlview_location_changed (htmlview, webkit_web_view_get_uri (view));
			break;
		default:
			break;
	}
}

static void
liferea_web_view_init(LifereaWebView *self)
{
	self->dbus_connection = NULL;

	g_signal_connect (
		self,
		"context-menu",
		G_CALLBACK (liferea_web_view_on_menu),
		NULL
	);

	/* Context menu actions */
	self->menu_action_group = G_ACTION_GROUP (g_simple_action_group_new ());
	g_action_map_add_action_entries (G_ACTION_MAP(self->menu_action_group), liferea_web_view_gaction_entries, G_N_ELEMENTS (liferea_web_view_gaction_entries), self);
	gtk_widget_insert_action_group (GTK_WIDGET (self), "liferea_web_view", self->menu_action_group);


	g_signal_connect (
		self,
		"notify::title",
		G_CALLBACK (liferea_web_view_title_changed),
		NULL
	);
	g_signal_connect (
		self,
		"mouse-target-changed",
		G_CALLBACK (liferea_web_view_on_mouse_target_changed),
		NULL
	);
	g_signal_connect (
		self,
		"enter-fullscreen",
		G_CALLBACK (liferea_web_view_entering_fullscreen),
		NULL
	);
	g_signal_connect (
		self,
		"leave-fullscreen",
		G_CALLBACK (liferea_web_view_leaving_fullscreen),
		NULL
	);
	g_signal_connect (
		self,
		"decide-policy",
		G_CALLBACK (liferea_web_view_decide_policy),
		NULL
	);
	g_signal_connect (
		self,
		"create",
		G_CALLBACK (liferea_web_view_create_web_view),
		NULL
	);
	g_signal_connect (
		self,
		"load-changed",
		G_CALLBACK (liferea_webkit_load_status_changed),
		NULL
	);
}

static void
scroll_pagedown_callback (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	GVariant 	*result = NULL;
	GError 		*error = NULL;
	gboolean 	scrolled;

	result = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object), res, &error);

	if (result == NULL) {
		g_warning ("Error invoking scrollPageDown: %s\n", error->message);
		g_error_free (error);
		return;
        }

	g_variant_get (result, "(b)", &scrolled);

	if (!scrolled) {
		on_next_unread_item_activate (NULL, NULL, NULL);
	}
}

void
liferea_web_view_scroll_pagedown (LifereaWebView *self)
{
	if (!self->dbus_connection) return;


	g_dbus_connection_call (self->dbus_connection,
		 LIFEREA_WEB_EXTENSION_BUS_NAME,
		 LIFEREA_WEB_EXTENSION_OBJECT_PATH,
		 LIFEREA_WEB_EXTENSION_INTERFACE_NAME,
		"ScrollPageDown",
		g_variant_new ("(t)", webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (self))),
		((const GVariantType *) "(b)"),
		G_DBUS_CALL_FLAGS_NONE,
		-1, /* Default timeout */
		NULL,
		scroll_pagedown_callback,
		NULL);
}

void
liferea_web_view_set_dbus_connection (LifereaWebView *self, GDBusConnection *connection)
{
	if (self->dbus_connection) {
		g_object_remove_weak_pointer (G_OBJECT (self->dbus_connection), (gpointer *) &self->dbus_connection);
	}
	self->dbus_connection = connection;
	g_object_add_weak_pointer (G_OBJECT (self->dbus_connection), (gpointer *) &self->dbus_connection);
}

LifereaWebView *
liferea_web_view_new ()
{
	return g_object_new(LIFEREA_TYPE_WEB_VIEW, NULL);
}
