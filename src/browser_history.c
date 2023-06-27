/**
 * @file browser_history.c  managing the internal browser history
 *
 * Copyright (C) 2012-2020 Lars Windolf <lars.windolf@gmx.de>
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

#include "browser_history.h"

browserHistory *
browser_history_new (void)
{
	return g_new0 (browserHistory, 1);
}

static void
browser_history_clear (browserHistory *history)
{
	GList	*iter;

	iter = history->locations;
	while (iter) {
		g_free (iter->data);
		iter = g_list_next (iter);
	}
	g_list_free (history->locations);

	history->locations = NULL;
	history->current = NULL;
}

void
browser_history_free (browserHistory *history)
{
	g_return_if_fail (NULL != history);
	browser_history_clear (history);
	g_free (history);
}

gchar *
browser_history_forward (browserHistory *history)
{
	GList	*url = history->current;

	url = g_list_next (url);
	history->current = url;

	return url?url->data:NULL;
}

gchar *
browser_history_back (browserHistory *history)
{
	GList	*url = history->current;

	url = g_list_previous (url);
	history->current = url;

	return url?url->data:NULL;
}

gboolean
browser_history_can_go_forward (browserHistory *history)
{
	return (NULL != g_list_next (history->current));
}

gboolean
browser_history_can_go_back (browserHistory *history)
{
	return (NULL != g_list_previous (history->current));
}

void
browser_history_add_location (browserHistory *history, const gchar *url)
{
	GList 	*iter;

	/* Do not add the same URL twice in a row... */
	if (history->current &&
	   g_str_equal (history->current->data, url))
		return;

	/* If current URL is not at the end of the list,
	   truncate the rest of the list */
	if (history->locations) {
		while (1) {
			iter = g_list_last (history->locations);
			if (!iter)
				break;
			if (iter == history->current)
				break;
			g_free (iter->data);
			history->locations = g_list_remove (history->locations, iter->data);
		}
	}

	history->locations = g_list_append (history->locations, g_strdup (url));
	history->current = g_list_last (history->locations);
}
