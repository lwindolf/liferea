/*
 * @file link_actions.c  link actions
 *
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2007-2025 Lars Windolf <lars.windolf@gmx.de>
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

#include "link_actions.h"

#include "browser.h"
#include "common.h"
#include "social.h"
#include "ui/browser_tabs.h"
#include "ui/itemview.h"
#include "ui/liferea_shell.h"
#include "ui/ui_common.h"

static void
on_open_link_in_browser (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemview_launch_URL (g_variant_get_string (parameter, NULL), TRUE /* use internal browser */);
}

static void
on_open_link_in_external_browser (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	browser_launch_URL_external (g_variant_get_string (parameter, NULL));
}

static void
on_open_link_in_tab (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	browser_tabs_add_new (g_variant_get_string (parameter, NULL), g_variant_get_string (parameter, NULL), FALSE);
}

static void
on_social_bookmark_link (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	gchar *social_url, *link, *title;

	g_variant_get (parameter, "(ss)", &link, &title);
	social_url = social_get_bookmark_url (link, title);
	(void)browser_tabs_add_new (social_url, social_url, TRUE);
	g_free (social_url);
}

static void
on_copy_link_to_clipboard (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	g_autofree gchar *link = (gchar *) common_uri_sanitize (BAD_CAST g_variant_get_string (parameter, NULL));

	liferea_shell_copy_to_clipboard (link);
}

static const GActionEntry gaction_entries[] = {
	{"open-link-in-tab", on_open_link_in_tab, "s", NULL, NULL},
	{"open-link-in-browser", on_open_link_in_browser, "s", NULL, NULL},
	{"open-link-in-external-browser", on_open_link_in_external_browser, "s", NULL, NULL},
	/* The parameters are link, then title. */
	{"social-bookmark-link", on_social_bookmark_link, "(ss)", NULL, NULL},
	{"copy-link-to-clipboard", on_copy_link_to_clipboard, "s", NULL, NULL}
};

GActionGroup *
link_actions_create (void)
{
        GActionGroup *ag = liferea_shell_add_actions (gaction_entries, G_N_ELEMENTS (gaction_entries));

        // FIXME: do we need any type of update function?

	return ag;
}