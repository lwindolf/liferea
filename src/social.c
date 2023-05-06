/*
 * @file social.c  social networking integration
 * 
 * Copyright (C) 2006-2023 Lars Windolf <lars.windolf@gmx.de>
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

#include "social.h"

#include "conf.h"
#include "debug.h"
#include "ui/browser_tabs.h"

/* list of registered bookmarking sites */
GSList *bookmarkSites = NULL;

/* the currently configured bookmarking site */
socialSitePtr bookmarkSite = NULL;

static socialSitePtr
social_find_bookmark_site (const gchar *name)
{
	GSList	*iter = bookmarkSites;

	while (iter) {
		bookmarkSite = iter->data;
		if (g_str_equal (bookmarkSite->name, name))
			return bookmarkSite;

		iter = g_slist_next (iter);
	}

	return NULL;
}

void
social_unregister_bookmark_site (const gchar *name)
{
	socialSitePtr site;

	site = social_find_bookmark_site (name);
	if (!site)
		return;

	bookmarkSites = g_slist_remove (bookmarkSites, site);
	g_free (site->name);
	g_free (site->url);
	g_free (site);
}

void
social_register_bookmark_site (const gchar *name, const gchar *url)
{
	socialSitePtr newSite;

	g_assert (name);
	g_assert (url);

	if (strstr (url, "{url}")) {
		newSite = g_new0 (struct socialSite, 1);
		newSite->name = g_strdup (name);
		newSite->url = g_strdup (url);

		bookmarkSites = g_slist_append (bookmarkSites, newSite);
	} else {
		debug1 (DEBUG_GUI, "Missing {url} placeholder in social bookmarking URL for '%s'!", name);
	}
}

void
social_set_bookmark_site (const gchar *name)
{
	socialSitePtr site;

	site = social_find_bookmark_site (name);
	if (site)
		conf_set_str_value (SOCIAL_BM_SITE, name);
	else
		debug1 (DEBUG_GUI, "Unknown social bookmarking site \"%s\"!", name);
}

const gchar *
social_get_bookmark_site (void) { return bookmarkSite->name; }

gchar *
social_get_bookmark_url (const gchar *link, const gchar *title)
{ 
	gchar	*result;
	gchar	**tmp;

	g_assert (bookmarkSite);
	g_assert (link);
	g_assert (title);

	// '{url}' placeholder is guaranteed to be there
	tmp = g_strsplit (bookmarkSite->url, "{url}", 2);
	result = g_strjoin ("", tmp[0], link, tmp[1], NULL);
	g_strfreev (tmp);

	// '{title}' placeholder is optionally replaced
	if (strstr (result, "{title}")) {
		tmp = g_strsplit (result, "{title}", 2);
		g_free (result);
		result = g_strjoin ("", tmp[0], title, tmp[1], NULL);
		g_strfreev (tmp);
	} 
	
	return result;
}

void
social_add_bookmark (const itemPtr item)
{
	gchar *link = item_make_link (item);
	gchar *url  = social_get_bookmark_url (link, item_get_title (item));
	(void)browser_tabs_add_new (url, social_get_bookmark_site(), TRUE);
	g_free (link);
	g_free (url);
}

void
social_init (void)
{
	gchar *tmp;
	
	social_register_bookmark_site ("blogmarks",	"https://blogmarks.net/my/new.php?mini=1&title={title}&url={url}");
	social_register_bookmark_site ("digg",		"https://digg.com/submit?phase=2&url={url}");
	social_register_bookmark_site ("diigo",		"https://www.diigo.com/post?url={url}&title={title}&desc=");
	social_register_bookmark_site ("Facebook",	"https://www.facebook.com/share.php?u={url}");
	social_register_bookmark_site ("Google Bookmarks",	"https://www.google.com/bookmarks/mark?op=edit&output=&bkmk={url}&title={title}");
	social_register_bookmark_site ("Instapaper",	"https://www.instapaper.com/hello2?url={url}&title={title}");
	social_register_bookmark_site ("Linkagogo",	"http://www.linkagogo.com/go/AddNoPopup?title={title}&url={url}");
	social_register_bookmark_site ("Linkroll",	"https://www.linkroll.com/index.php?action=insertLink&url={url}&title={title}");
	social_register_bookmark_site ("netvouz",	"https://netvouz.com/action/submitBookmark?url={url}&title={title}");
	social_register_bookmark_site ("Reddit",	"https://www.reddit.com/submit?url={url}&title={title}");
	social_register_bookmark_site ("Twitter",	"https://twitter.com/intent/tweet?text={title}&url={url}");

	conf_get_str_value (SOCIAL_BM_SITE, &tmp);
	social_set_bookmark_site (tmp);
	g_free (tmp);
	
	if (!bookmarkSite)
		bookmarkSite = bookmarkSites->data;	// use first in list
}

void
social_free (void)
{
	socialSitePtr	site;
	GSList		*iter;
	
	iter = bookmarkSites;	
	while (iter) {
		site = (socialSitePtr) iter->data;
		g_free (site->name);
		g_free (site->url);
		g_free (site);
		iter = g_slist_next(iter);
	}
	g_slist_free (bookmarkSites);
	bookmarkSites = NULL;
}
