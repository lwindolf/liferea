/**
 * @file social.c social networking integration
 * 
 * Copyright (C) 2006 Lars Lindner <lars.lindner@gmx.net>
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

#include "common.h"
#include "conf.h"
#include "favicon.h"
#include "render.h"
#include "social.h"

/* The following link collection was spied from http://ekstreme.com/socializer/ */
struct socialBookmarkSite socialBookmarkSites[] = {
 	{ "Backflip",		"http://www.backflip.com/add_page_pop.ihtml?url=%s&title=%s", TRUE, FALSE },
	{ "BlinkBits",		"http://blinkbits.com/bookmarklets/save.php?v=1&source_url=%s&title=%s", TRUE, FALSE },
	{ "Blinklist",		"http://www.blinklist.com/index.php?Action=Blink/addblink.php&Title=%s&Url=%s", TRUE, TRUE },
	{ "blogmarks",		"http://blogmarks.net/my/new.php?mini=1&title=%s&url=%s", TRUE, TRUE },
	{ "Buddymarks",		"http://buddymarks.com/add_bookmark.php?bookmark_title=%s&bookmark_url=%s", TRUE, TRUE },
	{ "CiteUlike",		"http://www.citeulike.org/posturl?url=%s&title=%s", TRUE, FALSE },
	{ "del.icio.us",	"http://del.icio.us/post?url=%s&title=%s", TRUE, FALSE },
	{ "de.lirio.us",	"http://de.lirio.us/rubric/post?uri=%s&title=%s", TRUE, FALSE },
	{ "digg",		"http://digg.com/submit?phase=2&url=%s", FALSE, FALSE },
	{ "ekstreme",		"http://ekstreme.com/socializer/?url=%s&title=%s", TRUE, FALSE },
	{ "FeedMarker",		"http://www.feedmarker.com/admin.php?do=bookmarklet_mark&url=%s&title=%s", TRUE, FALSE },
	{ "Feed Me Links!",	"http://feedmelinks.com/categorize?from=toolbar&op=submit&name=%s&url=%s&version=0.7", TRUE, TRUE },
	{ "Furl",		"http://www.furl.net/storeIt.jsp?t=%s&u=%s", TRUE, TRUE },
	{ "Give a Link",	"http://www.givealink.org/cgi-pub/bookmarklet/bookmarkletLogin.cgi?&uri=%s&title=%s", TRUE, FALSE },
	{ "Gravee",		"http://www.gravee.com/account/bookmarkpop?u=%s&t=%s", TRUE, FALSE },
	{ "Hyperlinkomatic",	"http://www.hyperlinkomatic.com/lm2/add.html?LinkTitle=%s&LinkUrl=%s", TRUE, TRUE },
	{ "igooi",		"http://www.igooi.com/addnewitem.aspx?self=1&noui=yes&jump=close&url=%s&title=%s", TRUE, FALSE },
	{ "kinja",		"http://kinja.com/id.knj?url=%s", FALSE, FALSE },
	{ "Lilisto",		"http://lister.lilisto.com/?t=%s&l=%s", TRUE, TRUE },
	{ "Linkagogo",		"http://www.linkagogo.com/go/AddNoPopup?title=%s&url=%s", TRUE, TRUE },
	{ "Linkroll",		"http://www.linkroll.com/index.php?action=insertLink&url=%s&title=%s", TRUE, FALSE },
	{ "looklater",		"http://api.looklater.com/bookmarks/save?url=%s&title=%s", TRUE, FALSE },
	{ "Magnolia",		"http://ma.gnolia.com/bookmarklet/add?url=%s&title=%s", TRUE, FALSE },
	{ "maple",		"http://www.maple.nu/bookmarks/bookmarklet?bookmark[url]=%s&bookmark[name]=%s", TRUE, FALSE },
	{ "MesFavs",		"http://mesfavs.com/bookmarks.php/?action=add&address=%s&title=%s", TRUE, FALSE },
	{ "netvouz",		"http://netvouz.com/action/submitBookmark?url=%s&title=%s", TRUE, FALSE },
	{ "Newsvine",		"http://www.newsvine.com/_wine/save?u=%s&h=%s", TRUE, FALSE },
	{ "Raw Sugar", 		"http://www.rawsugar.com/tagger/?turl=%s&tttl=%s&editorInitialized=1", TRUE, FALSE },
	{ "reddit",		"http://reddit.com/submit?url=%s&title=%s", TRUE, FALSE },
	{ "Rojo",		"http://www.rojo.com/add-subscription/?resource=%s", FALSE, FALSE },
	{ "Scuttle",		"http://scuttle.org/bookmarks.php/?action=add&address=%s&title=%s", TRUE, FALSE },
	{ "Segnalo",		"http://segnalo.com/post.html.php?url=%s&title=%s", TRUE, FALSE },
	{ "Shadows",		"http://www.shadows.com/shadows.aspx?url=%s", FALSE, FALSE },
	{ "Simpy",		"http://simpy.com/simpy/LinkAdd.do?title=%s&href=%s&v=6&src=bookmarklet", TRUE, TRUE },
	{ "Spurl",		"http://www.spurl.net/spurl.php?v=3&title=%s&url=%s", TRUE, TRUE },
	{ "Squidoo",		"http://www.squidoo.com/lensmaster/bookmark?%s", FALSE, FALSE },
	{ "tagtooga",		"http://www.tagtooga.com/tapp/db.exe?c=jsEntryForm&b=fx&title=%s&url=%s", TRUE, TRUE },
	{ "Tailrank",		"http://tailrank.com/share/?title=%s&link_href=%s", TRUE, TRUE },
	{ "Technorati",		"http://technorati.com/faves/?add=%s", FALSE, FALSE },
	{ "unalog",		"http://unalog.com/my/stack/link?url=%s&title=%s", TRUE, FALSE },
	{ "Wink",		"http://www.wink.com/_/tag?url=%s&doctitle=%s", TRUE, FALSE },
	{ "wists",		"http://www.wists.com/r.php?r=%s&title=%s", TRUE, FALSE },
	{ "Yahoo My Web",	"http://myweb2.search.yahoo.com/myresults/bookmarklet?u=%s&t=%s", TRUE, FALSE },
	{ "zurpy",		"http://tag.zurpy.com/?box=1&url=%s&title=%s", TRUE, FALSE },
	{ NULL, NULL, FALSE, FALSE }
};

static socialBookmarkSitePtr site = NULL;
/*static gchar *siteIcon = NULL;*/

void social_set_site(const gchar *name) {
	socialBookmarkSitePtr	iter = socialBookmarkSites;
	
	/*if(siteIcon)
		g_free(siteIcon);
	siteIcon = NULL;*/
	site = NULL;

	while(iter->name) {
		if(g_str_equal(iter->name, name)) {
			setStringConfValue(SOCIAL_BM_SITE, name);
			/* check if we have favicons for each bookmarking site if not download it... */
			/*gchar *id = common_strreplace(g_strdup_printf("socialbm_%s", iter->name), " ", "_");
			siteIcon = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "favicons", id, "png");
			if(!g_file_test(siteIcon, G_FILE_TEST_EXISTS)) {
				g_print("Doing initial favicon download for %s\n", iter->name);
				favicon_download(g_strdup(id), NULL, iter->url, NULL, NULL);
				// FIXME: leaking id...
			}*/

			site = iter;
			break;
		}
		iter++;
	}
}

/*const gchar * social_get_icon(void) { 

	return siteIcon;
}*/

const gchar * social_get_site(void) { return site->name; }

gchar * social_get_url(const gchar *link, const gchar *title) { 
	gchar	*url;

	g_assert(site);
	g_assert(link);
	g_assert(title);
	if(title) {
		if(site->titleFirst) 
			url = g_strdup_printf(site->url, title, link);
		else
			url = g_strdup_printf(site->url, link, title);
	} else {
		url = g_strdup_printf(site->url, link);
	}
	
	return url;
}

void social_init(void) {

	social_set_site(getStringConfValue(SOCIAL_BM_SITE));
	
	if(!site)
		social_set_site("del.icio.us");		/* set default if necessary */
		
	g_assert(site);
}
