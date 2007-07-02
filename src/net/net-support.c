/* Snownews - A lightweight console RSS newsreader
 * 
 * Copyright 2003-2004 Oliver Feiler <kiza@kcore.de>
 * http://kiza.kcore.de/software/snownews/
 *
 * net-support.c
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "conversions.h"
#include "digcalc.h"
#include "net-support.h"

char * ConstructBasicAuth (char * username, char * password) {
	int len;
	char * authinfo;
	char * authstring;
	char * tmpstr;

	/* Create base64 authinfo.

	RFC 2617. Basic HTTP authentication.
	Authorization: Basic username:password[base64 encoded] */

	/* Construct the cleartext authstring. */
	len = strlen(username) + 1 + strlen(password) + 1;
	authstring = g_malloc (len);
	snprintf (authstring, len, "%s:%s", username, password);
	tmpstr = base64encode (authstring, len-1);
	
	/* "Authorization: Basic " + base64str + \r\n\0 */
	len = 21 + strlen(tmpstr) + 3;
	authinfo = g_malloc (len);
	snprintf (authinfo, len, "Authorization: Basic %s\r\n", tmpstr);
	
	g_free (tmpstr);
	g_free (authstring);
	
	return authinfo;
}

char * GetRandomBytes (void) {
	char * randomness = NULL;
	char raw[8];
	int i;
	FILE * devrandom;
	
	devrandom = fopen ("/dev/random", "r");
	if (devrandom == NULL) {
		/* Use rand() if we don't have access to /dev/random. */
		for (i = 0; i <= 7; i++) {
			raw[i] = 1+(float)rand() / (float)RAND_MAX * 255;
		}
	} else {
		fread (raw, 8, 1, devrandom);
		fclose (devrandom);
	}
	
	randomness = g_malloc (17);
	snprintf (randomness, 17, "%hhx%hhx%hhx%hhx%hhx%hhx%hhx%hhx",
		raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6], raw[7]);
	
	return randomness;
}

char * ExtractValue (char **token) {
	char *value, *valueEnd;
	
	value = strchr (*token, '=');
	value++;
	
	if ('"' == *value) {
		value++;
		valueEnd = strchr (value + 1, '"');
	} else {
		valueEnd = strpbrk (value, " ,");
	}
	
	if (*valueEnd) {
		*valueEnd = '\0';
		do {
			valueEnd++;
		} while ((*valueEnd == ',') || (*valueEnd == ' '));
		*token = valueEnd;
	} else {
		return NULL;
	}
	
	return g_strdup (value);
}

char * ConstructDigestAuth (char * username, char * password, char * url, char * authdata) {
	char * authinfo = NULL;			/* Authorization header as sent to the server. */
	char * token;
	int len;
	char * realm = NULL;		/* Variables for the overcomplicated and annoying HTTP digest algo. */
	char * qop = NULL;
	char * nonce = NULL;
	char * opaque = NULL;
	char * cnonce;
	char szNonceCount[9] = "00000001";	/* Can be always 1 if we never use the same cnonce twice. */
	HASHHEX HA1;
	HASHHEX HA2 = "";
	HASHHEX Response;
	
	cnonce = GetRandomBytes();
	
	if (!authdata)
		return;
	
	token = authdata;
	while (1) {
		while (*token == ' ') token++;

		if (*token == '\0')
			break;
		
		if (strncasecmp (token, "realm=", 6) == 0) {
			realm = ExtractValue (&token);
		} else if (strncasecmp (token, "qop=", 4) == 0) {
			qop = ExtractValue (&token);
		} else if (strncasecmp (token, "nonce=", 6) == 0) {
			nonce = ExtractValue (&token);
		} else if (strncasecmp (token, "opaque=", 7) == 0) {
			opaque = ExtractValue (&token);
		} else {
			/* unknown key=value pair, skipping */
			gchar *useless = ExtractValue (&token);
			g_free (useless);
		}
	}
	
	if(username && realm && password && nonce && cnonce) {
		DigestCalcHA1 ("md5", username, realm, password, nonce, cnonce, HA1);
		DigestCalcResponse(HA1, nonce, szNonceCount, cnonce, "auth", "GET", url, HA2, Response);

		/* Determine length of Authorize header.
		 *
		 * Authorization: Digest username="(username)", realm="(realm)",
		 * nonce="(nonce)", uri="(url)", algorithm=MD5, response="(Response)",
		 * qop=(auth), nc=(szNonceCount), cnonce="deadbeef"
		 */
		if (opaque == NULL)
			len = 32 + strlen(username) + 10 + strlen(realm) + 10 + strlen(nonce) + 8 + strlen(url) + 28 + strlen(Response) + 16 + strlen(szNonceCount) + 10 + strlen(cnonce) + 4 ;
		else
			len = 32 + strlen(username) + 10 + strlen(realm) + 10 + strlen(nonce) + 8 + strlen(url) + 28 + strlen(Response) + 16 + strlen(szNonceCount) + 10 + strlen(cnonce) + 10 + strlen(opaque) + 4;

		authinfo = g_malloc (len);

		if (opaque == NULL) {
			snprintf (authinfo, len, "Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", algorithm=MD5, response=\"%s\", qop=auth, nc=%s, cnonce=\"%s\"\r\n",
				username, realm, nonce, url, Response, szNonceCount, cnonce);
		} else {
			snprintf (authinfo, len, "Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", algorithm=MD5, response=\"%s\", qop=auth, nc=%s, cnonce=\"%s\", opaque=\"%s\"\r\n",
				username, realm, nonce, url, Response, szNonceCount, cnonce, opaque);
		}

	}

	g_free (realm);
	g_free (qop);
	g_free (nonce);
	g_free (cnonce);
	g_free (opaque);
		
	return authinfo;
}


/*
Authorization: Digest username="(username)", realm="(realm)",
nonce="(nonce)", uri="(url)", algorithm=MD5, response="(Response)",
qop=(auth), nc=(szNonceCount), cnonce="deadbeef"
*/
int NetSupportAuth (struct feed_request * cur_ptr, char * authdata, char * url, char * netbuf) {
	char * header;
	char * tmpstr;
	char * freeme;
	char * username = NULL;
	char * password = NULL;
	char * authtype = NULL;
	
	/* Reset cur_ptr->authinfo. */
	g_free (cur_ptr->authinfo);
	cur_ptr->authinfo = NULL;
	
	/* Catch invalid authdata. */
	if (authdata == NULL) {
		return 1;
	} else if (strchr (authdata, ':') == NULL){
		/* No authinfo found in URL. This should not happen. */
		return 1;
	}
	
	tmpstr = g_strdup (authdata);
	freeme = tmpstr;
	
	strsep (&tmpstr, ":");
	username = g_strdup (freeme);
	password = g_strdup (tmpstr);
	
	/* Free allocated string in tmpstr. */
	g_free (freeme);
	
	/* Extract requested auth type from webserver reply. */
	header = g_strdup (netbuf);
	freeme = header;
	strsep (&header, " ");
	authtype = header;
	
	/* Catch invalid server replies. authtype should contain at least _something_. */
	if (authtype == NULL) {
		g_free (freeme);
		g_free (username);
		g_free (password);
		return -1;
	}
	
	strsep (&header, " ");
	/* header now contains:
	   Basic auth:  realm
	   Digest auth: realm + a lot of other stuff somehow needed by digest auth. */
	
	/* Determine auth type the server requests. */
	if (strncasecmp (authtype, "Basic", 5) == 0) {
		/* Basic auth. */
		cur_ptr->authinfo = ConstructBasicAuth (username, password);
	} else if (strncasecmp (authtype, "Digest", 6) == 0) {
		/* Digest auth. */
		cur_ptr->authinfo = ConstructDigestAuth (username, password, url, header);
	} else {
		/* Unkown auth type. */
		g_free (freeme);
		g_free (username);
		g_free (password);
		return -1;
	}
	
	g_free (username);
	g_free (password);
	g_free (freeme);

	if (cur_ptr->authinfo == NULL) {
		return 2;
	}
	
	return 0;
}

/* HTTP header may only contain ASCII characters.
 *
 * Ensure that we don't hit the terminating \0 in a string
 * with the for loop.
 * The function also ensures that there is no NULL byte in the string.
 * If given binary data return at once if we read beyond
 * the boundary of sizeof(header).
 */
int checkValidHTTPHeader (const unsigned char * header, int size) {
	int i, len;
	
	len = strlen(header);
	if (len > size)
		return -1;
	
	for (i = 0; i < len; i++) {
		if (((header[i] < 32) || (header[i] > 127)) &&
			(header[i] != '\t') && (header[i] != '\r') && (header[i] != '\n'))
			return -1;
	}
	return 0;
}

int checkValidHTTPURL (const unsigned char * url) {
	int i, len;
	
	if (strncasecmp(url, "http://", 7) != 0)
		return -1;
	
	len = strlen(url);
		
	for (i = 0; i < len; i++) {
		if ((url[i] < 32) || (url[i] > 126))
			return -1;
	}
	
	return 0;
}
