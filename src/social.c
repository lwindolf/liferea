/**
 * @file social.c  social networking integration
 * 
 * Copyright (C) 2006-2013 Lars Windolf <lars.windolf@gmx.de>
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

/** list of registered bookmarking sites */
GSList *bookmarkSites = NULL;

/** the currently configured bookmarking site */
static socialSitePtr bookmarkSite = NULL;

void
social_register_bookmark_site (const gchar *name, const gchar *url, gboolean title, gboolean titleFirst)
{
	socialSitePtr newSite;

	g_assert (name);
	g_assert (url);	
	newSite = g_new0 (struct socialSite, 1);
	newSite->name = g_strdup (name);
	newSite->url = g_strdup (url);
	newSite->title = title;
	newSite->titleFirst = titleFirst;

	bookmarkSites = g_slist_append (bookmarkSites, newSite);
}

void
social_set_bookmark_site (const gchar *name)
{
	GSList	*iter = bookmarkSites;
	
	while (iter) {
		bookmarkSite = iter->data;
		if (g_str_equal (bookmarkSite->name, name)) {
			conf_set_str_value (SOCIAL_BM_SITE, name);
			return;
		}
		iter = g_slist_next (iter);
	}
	
	debug1 (DEBUG_GUI, "Unknown social bookmarking site \"%s\"!", name);
}

const gchar *
social_get_bookmark_site (void) { return bookmarkSite->name; }

gchar *
social_get_bookmark_url (const gchar *link, const gchar *title)
{ 
	gchar	*url;

	g_assert (bookmarkSite);
	g_assert (link);
	g_assert (title);
	if (title) {
		if (bookmarkSite->titleFirst) 
			url = g_strdup_printf (bookmarkSite->url, title, link);
		else
			url = g_strdup_printf (bookmarkSite->url, link, title);
	} else {
		url = g_strdup_printf (bookmarkSite->url, link);
	}
	
	return url;
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
	
	social_register_bookmark_site ("Blinklist",	"http://www.blinklist.com/index.php?Action=Blink/addblink.php&Title=%s&Url=%s", TRUE, TRUE);
	social_register_bookmark_site ("blogmarks",	"http://blogmarks.net/my/new.php?mini=1&title=%s&url=%s", TRUE, TRUE);
	social_register_bookmark_site ("Buddymarks",	"http://buddymarks.com/add_bookmark.php?bookmark_title=%s&bookmark_url=%s", TRUE, TRUE);
	social_register_bookmark_site ("CiteUlike",	"http://www.citeulike.org/posturl?url=%s&title=%s", TRUE, FALSE);
	social_register_bookmark_site ("del.icio.us",	"http://del.icio.us/post?url=%s&title=%s", TRUE, FALSE);
	social_register_bookmark_site ("digg",		"http://digg.com/submit?phase=2&url=%s", FALSE, FALSE);
	social_register_bookmark_site ("diigo",		"http://www.diigo.com/post?url=%s&title=%s&desc=", TRUE, FALSE);
	social_register_bookmark_site ("Facebook",	"http://www.facebook.com/share.php?u=%s", FALSE, FALSE);
	social_register_bookmark_site ("Google Bookmarks",	"https://www.google.com/bookmarks/mark?op=edit&output=&bkmk=%s&title=%s", TRUE, FALSE);
	social_register_bookmark_site ("Google Plus",	"https://plus.google.com/share?url=%s", FALSE, FALSE);
	social_register_bookmark_site ("identi.ca"	,"http://identi.ca//index.php?action=bookmarklet&status_textarea=%%E2%%80%%9C%s%%E2%%80%%9D%%20%%E2%%80%%94%%20%s", TRUE, TRUE);
	social_register_bookmark_site ("Instapaper",	"https://www.instapaper.com/edit?url=%s&title=%s", TRUE, FALSE);
	social_register_bookmark_site ("Lilisto",	"http://lister.lilisto.com/?t=%s&l=%s", TRUE, TRUE);
	social_register_bookmark_site ("Linkagogo",	"http://www.linkagogo.com/go/AddNoPopup?title=%s&url=%s", TRUE, TRUE);
	social_register_bookmark_site ("Linkroll",	"http://www.linkroll.com/index.php?action=insertLink&url=%s&title=%s", TRUE, FALSE);
	social_register_bookmark_site ("netvouz",	"http://netvouz.com/action/submitBookmark?url=%s&title=%s", TRUE, FALSE);
	social_register_bookmark_site ("Newsvine",	"http://www.newsvine.com/_wine/save?u=%s&h=%s", TRUE, FALSE);
	social_register_bookmark_site ("reddit",	"http://reddit.com/submit?url=%s&title=%s", TRUE, FALSE);
	social_register_bookmark_site ("Slashdot",	"http://slashdot.org/slashdot-it.pl?op=basic&amp;url=%s&title=%s", TRUE, FALSE);
	social_register_bookmark_site ("Squidoo",	"http://www.squidoo.com/lensmaster/bookmark?%s", FALSE, FALSE);
	social_register_bookmark_site ("StumbleUpon",	"http://www.stumbleupon.com/submit/?url=%s&title=%s", TRUE, FALSE);
	social_register_bookmark_site ("Twitter",	"http://twitter.com/home?status=%s", FALSE, FALSE);
	social_register_bookmark_site ("Yahoo My Web",	"http://myweb2.search.yahoo.com/myresults/bookmarklet?u=%s&t=%s", TRUE, FALSE);

	conf_get_str_value (SOCIAL_BM_SITE, &tmp);
	social_set_bookmark_site (tmp);
	g_free (tmp);
	
	if (!bookmarkSite)
		social_set_bookmark_site ("del.icio.us");		/* set default if necessary */
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
