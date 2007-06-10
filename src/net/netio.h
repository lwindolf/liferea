/* HTTP feed download, Liferea reuses the only slightly
 * adapted Snownews code:
 *
 * Snownews - A lightweight console RSS newsreader
 * 
 * Copyright 2003-2004 Oliver Feiler <kiza@kcore.de>
 * http://kiza.kcore.de/software/snownews/
 *
 * netio.h
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

#ifndef _NETIO_H
#define _NETIO_H

#include "compat.h"

struct feed_request {
	/* Supplied and overwritten/freed */
	char *feedurl;                          /* Non hashified URL */
	char *lastmodified;                     /* Content of header as sent by the server. */
	char *etag;

	/* Set by netio code */
	char *content_type;
	size_t contentlength;
	int lasthttpstatus;
	char *cookies;                          /* Login cookies for this feed. */
	char *authinfo;                         /* HTTP authinfo string. */
	char *servauth;                         /* Server supplied authorization header. */
	int problem;                            /* Set if there was a problem downloading the feed. */
	int netio_error;
	int connectresult;
	int no_proxy;				/* 1 = don't use a proxy, 0 = use proxy */
};

void netio_init (void);

void netio_deinit (void);

char * DownloadFeed (char *url, struct feed_request *request, int suppressoutput);

#endif
