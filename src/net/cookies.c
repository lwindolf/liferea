/* Snownews - A lightweight console RSS newsreader
 * $Id$
 * 
 * Copyright 2003-2004 Oliver Feiler <kiza@kcore.de>
 * http://kiza.kcore.de/software/snownews/
 *
 * cookies.c
 *
 * Please read the file README.patching before changing any code in this file!
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/* Netscape cookie file format:
   (http://www.cookiecentral.com/faq/#3.5)

   .host.com	host_match[BOOL]	/path	secure[BOOL]	expire[unix time]	NAME	VALUE
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../support.h"	/* for gettext() */
#include "compat.h"

char * CookieCutter (const char *feedurl, FILE * cookies) {
	char buf[4096];					/* File read buffer. */
	char tmp[512];
	char *result = NULL;
	char *host;						/* Current feed hostname. */
	char *path;						/* Current feed path. */
	char *url;
	char *freeme, *tmpstr;
	char *tmphost;
	char *cookie;
	char *cookiehost = NULL;
	char *cookiepath = NULL;
	char *cookiename = NULL;
	char *cookievalue = NULL;
	int cookieexpire = 0;
	int cookiesecure = 0;
	int i;
	int len = 0;
	int cookienr = 0;
	time_t tunix;
	
	/* Get current time. */
	tunix = time(0);
	
	url = strdup (feedurl);
	freeme = url;
	
	strsep (&url, "/");
	strsep (&url, "/");
	tmphost = url;
	strsep (&url, "/");
	if (url == NULL) {
		free (freeme);
		return NULL;
	}
	
	/* If tmphost contains an '@' strip authinfo off url. */
	if (strchr (tmphost, '@') != NULL) {
		strsep (&tmphost, "@");
	}
	
	host = strdup (tmphost);
	url--;
	url[0] = '/';
	if (url[strlen(url)-1] == '\n') {
		url[strlen(url)-1] = '\0';
	}
	
	path = strdup (url);
	free (freeme);
	freeme = NULL;
	
	while (!feof(cookies)) {
		if ((fgets (buf, sizeof(buf), cookies)) == NULL)
			break;
		
		/* Filter \n lines. But ignore them so we can read a NS cookie file. */
		if (buf[0] == '\n')
			continue;
		
		/* Allow adding of comments that start with '#'.
		   Makes it possible to symlink Mozilla's cookies.txt. */
		if (buf[0] == '#')
			continue;
				
		cookie = strdup (buf);
		freeme = cookie;
		
		/* Munch trailing newline. */
		if (cookie[strlen(cookie)-1] == '\n')
			cookie[strlen(cookie)-1] = '\0';
		
		/* Decode the cookie string. */
		for (i = 0; i <= 6; i++) {
			tmpstr = strsep (&cookie, "\t");
			
			if (tmpstr == NULL)
				break;
			
			switch (i) {
				case 0:
					/* Cookie hostname. */
					cookiehost = strdup (tmpstr);
					break;
				case 1:
					/* Discard host match value. */
					break;
				case 2:
					/* Cookie path. */
					cookiepath = strdup (tmpstr);
					break;
				case 3:
					/* Secure cookie? */
					if (strcasecmp (tmpstr, "TRUE") == 0)
						cookiesecure = 1;
					break;
				case 4:
					/* Cookie expiration date. */
					cookieexpire = atoi (tmpstr);
					break;
				case 5:
					/* NAME */
					cookiename = strdup (tmpstr);
					break;
				case 6:
					/* VALUE */
					cookievalue = strdup (tmpstr);
					break;
				default:
					break;
			}
		}
		
		/* See if current cookie matches cur_ptr.
		   Hostname and path must match.
		   Ignore secure cookies.
		   Discard cookie if it has expired. */
		if ((strstr (host, cookiehost) != NULL) &&
			(strstr (path, cookiepath) != NULL) &&
			(!cookiesecure) &&
			(cookieexpire > (int) tunix)) {					/* Cast time_t tunix to int. */
			cookienr++;
			
			/* Construct and append cookiestring.
			
			   Cookie: NAME=VALUE; NAME=VALUE */
			if (cookienr == 1) {
				len = 8 + strlen(cookiename) + 1 + strlen(cookievalue) + 1;
				result = malloc (len);
				strcpy (result, "Cookie: ");
				strcat (result, cookiename);
				strcat (result, "=");
				strcat (result, cookievalue);
			} else {
				len += strlen(cookiename) + 1 + strlen(cookievalue) + 3;
				result = realloc (result, len);
				strcat (result, "; ");
				strcat (result, cookiename);
				strcat (result, "=");
				strcat (result, cookievalue);
			}
		} else if ((strstr (host, cookiehost) != NULL) &&
					(strstr (path, cookiepath) != NULL) &&
					(cookieexpire < (int) tunix)) {			/* Cast time_t tunix to int. */
			/* Print cookie expire warning. */
			snprintf (tmp, sizeof(tmp), _("Cookie for %s has expired!"), cookiehost);
			UIStatus (tmp, 1);
		}

		free (freeme);
		freeme = NULL;
		free (cookiehost);
		free (cookiepath);
		free (cookiename);
		free (cookievalue);
	}
	
	free (host);
	free (path);
	free (freeme);
	
	/* Append \r\n to result */
	if (result != NULL) {
		result = realloc (result, len+2);
		strcat (result, "\r\n");
	}
	
	return result;
}


gchar * cookies_find_matching(const gchar *url) {
	gchar	*filename;
	gchar	*result;
	FILE	*cookies;

	filename = common_create_cache_filename("", "cookies", "txt");
	cookies = fopen (filename, "r");
	g_free(filename);
	
	if (cookies == NULL) {
		/* No cookies to load. */
		return NULL;
	} else {
		result = CookieCutter (url, cookies);
	}
	fclose (cookies);

	return result;
}
