/**
 * @file social.c social networking integration
 * 
 * Copyright (C) 2006-2007 Lars Lindner <lars.lindner@gmail.com>
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

#include "conf.h"
#include "debug.h"
#include "social.h"

/** list of registered bookmarking sites */
GSList *socialBookmarkSites = NULL;

/** the currently configured site */
static socialBookmarkSitePtr site = NULL;

void
social_register_site (gchar *name, gchar *url, gboolean title, gboolean titleFirst)
{
	socialBookmarkSitePtr newSite;

	g_assert (name);
	g_assert (url);	
	newSite = g_new0 (struct socialBookmarkSite, 1);
	newSite->name = g_strdup (name);
	newSite->url = g_strdup (url);
	newSite->title = title;
	newSite->titleFirst = titleFirst;

	socialBookmarkSites = g_slist_append (socialBookmarkSites, newSite);
}

void
social_set_site (const gchar *name)
{
	GSList	*iter = socialBookmarkSites;
	
	while (iter) {
		site = iter->data;
		if (g_str_equal (site->name, name)) {
			setStringConfValue (SOCIAL_BM_SITE, name);
			return;
		}
		iter = g_slist_next (iter);
	}
	
	debug1 (DEBUG_GUI, "Unknown social bookmarking site \"%s\"!", name);
}

const gchar *
social_get_site (void) { return site->name; }

gchar *
social_get_url (const gchar *link, const gchar *title)
{ 
	gchar	*url;

	g_assert (site);
	g_assert (link);
	g_assert (title);
	if (title) {
		if (site->titleFirst) 
			url = g_strdup_printf (site->url, title, link);
		else
			url = g_strdup_printf (site->url, link, title);
	} else {
		url = g_strdup_printf (site->url, link);
	}
	
	return url;
}

void
social_init (void)
{
	/* The following link collection was derived from http://ekstreme.com/socializer/ */
 	social_register_site ("Backflip",	"http://www.backflip.com/add_page_pop.ihtml?url=%s&title=%s", TRUE, FALSE);
	social_register_site ("BlinkBits",	"http://blinkbits.com/bookmarklets/save.php?v=1&source_url=%s&title=%s", TRUE, FALSE);
	social_register_site ("Blinklist",	"http://www.blinklist.com/index.php?Action=Blink/addblink.php&Title=%s&Url=%s", TRUE, TRUE);
	social_register_site ("blogmarks",	"http://blogmarks.net/my/new.php?mini=1&title=%s&url=%s", TRUE, TRUE);
	social_register_site ("Buddymarks",	"http://buddymarks.com/add_bookmark.php?bookmark_title=%s&bookmark_url=%s", TRUE, TRUE);
	social_register_site ("CiteUlike",	"http://www.citeulike.org/posturl?url=%s&title=%s", TRUE, FALSE);
	social_register_site ("del.icio.us",	"http://del.icio.us/post?url=%s&title=%s", TRUE, FALSE);
	social_register_site ("de.lirio.us",	"http://de.lirio.us/rubric/post?uri=%s&title=%s", TRUE, FALSE);
	social_register_site ("digg",		"http://digg.com/submit?phase=2&url=%s", FALSE, FALSE);
	social_register_site ("ekstreme",	"http://ekstreme.com/socializer/?url=%s&title=%s", TRUE, FALSE);
	social_register_site ("FeedMarker",	"http://www.feedmarker.com/admin.php?do=bookmarklet_mark&url=%s&title=%s", TRUE, FALSE);
	social_register_site ("Feed Me Links!",	"http://feedmelinks.com/categorize?from=toolbar&op=submit&name=%s&url=%s&version=0.7", TRUE, TRUE);
	social_register_site ("Furl",		"http://www.furl.net/storeIt.jsp?t=%s&u=%s", TRUE, TRUE);
	social_register_site ("Give a Link",	"http://www.givealink.org/cgi-pub/bookmarklet/bookmarkletLogin.cgi?&uri=%s&title=%s", TRUE, FALSE);
	social_register_site ("Gravee",		"http://www.gravee.com/account/bookmarkpop?u=%s&t=%s", TRUE, FALSE);
	social_register_site ("Hyperlinkomatic","http://www.hyperlinkomatic.com/lm2/add.html?LinkTitle=%s&LinkUrl=%s", TRUE, TRUE);
	social_register_site ("igooi",		"http://www.igooi.com/addnewitem.aspx?self=1&noui=yes&jump=close&url=%s&title=%s", TRUE, FALSE);
	social_register_site ("kinja",		"http://kinja.com/id.knj?url=%s", FALSE, FALSE);
	social_register_site ("Lilisto",	"http://lister.lilisto.com/?t=%s&l=%s", TRUE, TRUE);
	social_register_site ("Linkagogo",	"http://www.linkagogo.com/go/AddNoPopup?title=%s&url=%s", TRUE, TRUE);
	social_register_site ("Linkroll",	"http://www.linkroll.com/index.php?action=insertLink&url=%s&title=%s", TRUE, FALSE);
	social_register_site ("looklater",	"http://api.looklater.com/bookmarks/save?url=%s&title=%s", TRUE, FALSE);
	social_register_site ("Magnolia",	"http://ma.gnolia.com/bookmarklet/add?url=%s&title=%s", TRUE, FALSE);
	social_register_site ("maple",		"http://www.maple.nu/bookmarks/bookmarklet?bookmark[url]=%s&bookmark[name]=%s", TRUE, FALSE);
	social_register_site ("MesFavs",	"http://mesfavs.com/bookmarks.php/?action=add&address=%s&title=%s", TRUE, FALSE);
	social_register_site ("netvouz",	"http://netvouz.com/action/submitBookmark?url=%s&title=%s", TRUE, FALSE);
	social_register_site ("Newsvine",	"http://www.newsvine.com/_wine/save?u=%s&h=%s", TRUE, FALSE);
	social_register_site ("Raw Sugar", 	"http://www.rawsugar.com/tagger/?turl=%s&tttl=%s&editorInitialized=1", TRUE, FALSE);
	social_register_site ("reddit",		"http://reddit.com/submit?url=%s&title=%s", TRUE, FALSE);
	social_register_site ("Rojo",		"http://www.rojo.com/add-subscription/?resource=%s", FALSE, FALSE);
	social_register_site ("Scuttle",	"http://scuttle.org/bookmarks.php/?action=add&address=%s&title=%s", TRUE, FALSE);
	social_register_site ("Segnalo",	"http://segnalo.com/post.html.php?url=%s&title=%s", TRUE, FALSE);
	social_register_site ("Shadows",	"http://www.shadows.com/shadows.aspx?url=%s", FALSE, FALSE);
	social_register_site ("Simpy",		"http://simpy.com/simpy/LinkAdd.do?title=%s&href=%s&v=6&src=bookmarklet", TRUE, TRUE);
	social_register_site ("Spurl",		"http://www.spurl.net/spurl.php?v=3&title=%s&url=%s", TRUE, TRUE);
	social_register_site ("Squidoo",	"http://www.squidoo.com/lensmaster/bookmark?%s", FALSE, FALSE);
	social_register_site ("tagtooga",	"http://www.tagtooga.com/tapp/db.exe?c=jsEntryForm&b=fx&title=%s&url=%s", TRUE, TRUE);
	social_register_site ("Tailrank",	"http://tailrank.com/share/?title=%s&link_href=%s", TRUE, TRUE);
	social_register_site ("Technorati",	"http://technorati.com/faves/?add=%s", FALSE, FALSE);
	social_register_site ("unalog",		"http://unalog.com/my/stack/link?url=%s&title=%s", TRUE, FALSE);
	social_register_site ("Wink",		"http://www.wink.com/_/tag?url=%s&doctitle=%s", TRUE, FALSE);
	social_register_site ("wists",		"http://www.wists.com/r.php?r=%s&title=%s", TRUE, FALSE);
	social_register_site ("Yahoo My Web",	"http://myweb2.search.yahoo.com/myresults/bookmarklet?u=%s&t=%s", TRUE, FALSE);
	social_register_site ("zurpy",		"http://tag.zurpy.com/?box=1&url=%s&title=%s", TRUE, FALSE);

	gchar *social_site=getStringConfValue (SOCIAL_BM_SITE);
	social_set_site (social_site);
	g_free (social_site);
	
	if (!site)
		social_set_site ("del.icio.us");		/* set default if necessary */
		
	g_assert (site);
}

void
social_free (void)
{
	socialBookmarkSitePtr	site;
	GSList			*iter = socialBookmarkSites;
	
	while (iter) {
		site = (socialBookmarkSitePtr) iter->data;
		g_free (site->name);
		g_free (site->url);
		g_free (site);
		iter = g_slist_next(iter);
	}
	g_slist_free (socialBookmarkSites);
}
