/*
 * HTTP feed download, Liferea reuses the only slightly
 * adapted Snownews code...
 *
 * Snownews - A lightweight console RSS newsreader
 * 
 * Copyright 2003-2004 Oliver Feiler <kiza@kcore.de>
 * http://kiza.kcore.de/software/snownews/
 *
 * netio.c
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

/* OS X needs this, otherwise socklen_t is not defined. */
#ifdef __APPLE__
#       define _BSD_SOCKLEN_T_
#endif

/* BeOS does not define socklen_t. Using uint as suggested by port creator. */
#ifdef __BEOS__
#       define socklen_t unsigned int
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/stat.h>
#include <assert.h>

#include <libxml/uri.h>

#include "conversions.h"
#include "net-support.h"
#include "../update.h"

static int const MAX_HTTP_REDIRECTS = 10;	/* Maximum number of redirects we will follow. */
static int const NET_TIMEOUT = 30;			/* Global network timeout in sec */
static int const NET_READ = 1;
static int const NET_WRITE = 2;

extern char *proxyname;						/* Hostname of proxyserver. */
extern unsigned short proxyport;			/* Port on proxyserver to use. */
extern char *useragent;

/* Waits NET_TIMEOUT seconds for the socket to return data.
 *
 * Returns
 *
 *	0	Socket is ready
 *	-1	Error occured (netio_error is set)
 */
int NetPoll (struct feed_request * cur_ptr, int * my_socket, int rw) {
	fd_set rfdsr;
	fd_set rfdsw;
	struct timeval tv;
	int retval;				/* FD_ISSET + assert == Heisenbug? */
	
	/* Set global network timeout */
	tv.tv_sec = NET_TIMEOUT;
	tv.tv_usec = 0;
	
	FD_ZERO(&rfdsr);
	FD_ZERO(&rfdsw);
	
	if (rw == NET_READ) {
		FD_SET(*my_socket, &rfdsr);
		if (select (*my_socket+1, &rfdsr, NULL, NULL, &tv) == 0) {
			/* Timed out */
			cur_ptr->netio_error = NET_ERR_TIMEOUT;
			return -1;
		}
		retval = FD_ISSET (*my_socket, &rfdsr);
		assert (retval);
		if (!retval) {
			/* Wtf? */
			cur_ptr->netio_error = NET_ERR_UNKNOWN;
			return -1;
		}
	} else if (rw == NET_WRITE) {
		FD_SET(*my_socket, &rfdsw);
		if (select (*my_socket+1, NULL, &rfdsw, NULL, &tv) == 0) {
			/* Timed out */
			cur_ptr->netio_error = NET_ERR_TIMEOUT;
			return -1;
		}
		retval = FD_ISSET (*my_socket, &rfdsw);
		assert (retval);
		if (!retval) {
			/* Wtf? */
			cur_ptr->netio_error = NET_ERR_UNKNOWN;
			return -1;
		}
	} else {
		cur_ptr->netio_error = NET_ERR_UNKNOWN;
		return -1;
	}
	
	return 0;
}


/* Connect network sockets.
 *
 * Returns
 *
 *	0	Connected
 *	-1	Error occured (netio_error is set)
 */
int NetConnect (int * my_socket, char * host, struct feed_request * cur_ptr, int httpproto, int suppressoutput) {
	struct sockaddr_in address;	
	struct hostent *remotehost;
	socklen_t len;
	char *realhost;
	unsigned short port;
	
	realhost = strdup(host);
	if (sscanf (host, "%[^:]:%hd", realhost, &port) != 2) {
		port = 80;
	}
	/*
	if (!suppressoutput) {
		if (cur_ptr->title == NULL)
			snprintf (tmp, sizeof(tmp), _("Downloading \"%s\""), cur_ptr->feedurl);
		else
			snprintf (tmp, sizeof(tmp), _("Downloading \"%s\""), cur_ptr->title);

			UIStatus (tmp, 0);
			}*/
	
	/* Create a inet stream TCP socket. */
	*my_socket = socket (AF_INET, SOCK_STREAM, 0);
	if (*my_socket == -1) {
		cur_ptr->netio_error = NET_ERR_SOCK_ERR;
		return -1;
	}
	
	/* Set socket to nonblock mode to make it possible to interrupt the connect. */
	fcntl(*my_socket, F_SETFL, fcntl(*my_socket, F_GETFL, 0) | O_NONBLOCK);
	
	/* If proxyport is 0 we didn't execute the if http_proxy statement in main
	   so there is no proxy. On any other value of proxyport do proxyrequests instead. */
	if (proxyport == 0) {
		/* Lookup remote IP. */
		remotehost = gethostbyname (realhost);
		if (remotehost == NULL) {
			close (*my_socket);
			free (realhost);
			cur_ptr->netio_error = NET_ERR_HOST_NOT_FOUND;
			return -1;
		}
		
		/* Set the remote address. */
		address.sin_family = AF_INET;
		address.sin_port = htons(port);
		memcpy (&address.sin_addr.s_addr, remotehost->h_addr_list[0], remotehost->h_length);
			
		/* Connect socket. */
		cur_ptr->connectresult = connect (*my_socket, (struct sockaddr *) &address, sizeof(address));
		
		/* Check if we're already connected.
		   BSDs will return 0 on connect even in nonblock if connect was fast enough. */
		if (cur_ptr->connectresult != 0) {
			/* If errno is not EINPROGRESS, the connect went wrong. */
			if (errno != EINPROGRESS) {
				close (*my_socket);
				free (realhost);
				cur_ptr->netio_error = NET_ERR_CONN_REFUSED;
				return -1;
			}
			
			if ((NetPoll (cur_ptr, my_socket, NET_WRITE)) == -1) {
				close (*my_socket);
				free (realhost);
				return -1;
			}
			
			/* We get errno of connect back via getsockopt SO_ERROR (into connectresult). */
			len = sizeof(cur_ptr->connectresult);
			getsockopt(*my_socket, SOL_SOCKET, SO_ERROR, &cur_ptr->connectresult, &len);
			
			if (cur_ptr->connectresult != 0) {
				close (*my_socket);
				free (realhost);
				cur_ptr->netio_error = NET_ERR_CONN_FAILED;	/* ->strerror(cur_ptr->connectresult) */
				return -1;
			}
		}
	} else {
		/* Lookup proxyserver IP. */
		remotehost = gethostbyname (proxyname);
		if (remotehost == NULL) {
			close (*my_socket);
			free (realhost);
			cur_ptr->netio_error = NET_ERR_HOST_NOT_FOUND;
			return -1;
		}
		
		/* Set the remote address. */
		address.sin_family = AF_INET;
		address.sin_port = htons(proxyport);
		memcpy (&address.sin_addr.s_addr, remotehost->h_addr_list[0], remotehost->h_length);
		
		/* Connect socket. */
		cur_ptr->connectresult = connect (*my_socket, (struct sockaddr *) &address, sizeof(address));
		
		/* Check if we're already connected.
		   BSDs will return 0 on connect even in nonblock if connect was fast enough. */
		if (cur_ptr->connectresult != 0) {
			if (errno != EINPROGRESS) {
				close (*my_socket);
				free (realhost);
				cur_ptr->netio_error = NET_ERR_CONN_REFUSED;
				return -1;
			}
		
			if ((NetPoll (cur_ptr, my_socket, NET_WRITE)) == -1) {
				close (*my_socket);
				free (realhost);
				return -1;
			}
			
			len = sizeof(cur_ptr->connectresult);
			getsockopt(*my_socket, SOL_SOCKET, SO_ERROR, &cur_ptr->connectresult, &len);
			
			if (cur_ptr->connectresult != 0) {
				close (*my_socket);
				free (realhost);
				cur_ptr->netio_error = NET_ERR_CONN_FAILED;	/* ->strerror(cur_ptr->connectresult) */
				return -1;
			}
		}
	}
	
	/* Set socket back to blocking mode. */
	fcntl(*my_socket, F_SETFL, fcntl(*my_socket, F_GETFL, 0) & ~O_NONBLOCK);
	
	free (realhost);
	return 0;
}


/*
 * Main network function.
 * (Now with a useful function description *g*)
 *
 * This function returns the HTTP request's body (deflating gzip encoded data
 * if needed).
 * Updates passed feed struct with values gathered from webserver.
 * Handles all redirection and HTTP status decoding.
 * Returns NULL pointer if no data was received and sets netio_error.
 */
char * NetIO (int * my_socket, char * host, char * url, struct feed_request * cur_ptr, char * authdata, int httpproto, int suppressoutput) {
	char netbuf[4096];			/* Network read buffer. */
	char *body;					/* XML body. */
	int length;
	FILE *stream;				/* Stream socket. */
	int chunked = 0;			/* Content-Encoding: chunked received? */
	int redirectcount;			/* Number of HTTP redirects followed. */
	char httpstatus[4];			/* HTTP status sent by server. */
	char servreply[128];		/* First line of server reply */
	char *tmpstatus;
	char *savestart;			/* Save start position of pointers. */
	char *tmphost;				/* Pointers needed to strsep operation. */
	char *newhost;				/* New hostname if we need to redirect. */
	char *newurl;				/* New document name ". */
	char *newlocation;
	char *tmpstring;			/* Temp pointers. */
	char *freeme, *freeme2;
	char *redirecttarget;
	int retval;
	int handled;
	int tmphttpstatus;
	int inflate = 0;			/* Whether feed data needs decompressed with zlib. */
	int len;
	char * inflatedbody;
	int quirksmode = 0;			/* IIS operation mode. */
	int authfailed = 0;			/* Avoid repeating failed auth requests endlessly. */

	
	if (!suppressoutput) {
		/*if (cur_ptr->title == NULL)
			snprintf (tmp, sizeof(tmp), _("Downloading \"http://%s%s\""), host, url);
		else
			snprintf (tmp, sizeof(tmp), _("Downloading \"%s\""), cur_ptr->title);

			UIStatus (tmp, 0);*/
	}
	
	redirectcount = 0;
	
	/* Goto label to redirect reconnect. */
	tryagain:
	
	/* Reconstruct digest authinfo for every request so we don't reuse
	   the same nonce value for more than one request.
	   This happens on superflous time on 303 redirects. */
	if ((cur_ptr->authinfo != NULL) && (cur_ptr->servauth != NULL)) {
		if (strstr (cur_ptr->authinfo, " Digest ") != NULL) {
			NetSupportAuth(cur_ptr, authdata, url, cur_ptr->servauth);
		}
	}
	
	/* Open socket. */	
	stream = fdopen (*my_socket, "r+");
	if (stream == NULL) {
		/* This is a serious non-continueable OS error as it will probably not
		   go away if we retry.
		   
		   BeOS will stupidly return SUCCESS here making this code silently fail on BeOS. */
		cur_ptr->netio_error = NET_ERR_SOCK_ERR;
		return NULL;
	}
	
	/* Again is proxyport == 0, non proxy mode, otherwise make proxy requests. */
	if (proxyport == 0) {
		/* Request URL from HTTP server. */
		if (cur_ptr->lastmodified != NULL) {
			fprintf(stream,
					"GET %s HTTP/1.0\r\nAccept-Encoding: gzip\r\nUser-Agent: %s\r\nConnection: close\r\nHost: %s\r\nIf-Modified-Since: %s\r\n%s%s\r\n",
					url,
					useragent,
					host,
					cur_ptr->lastmodified,
					(cur_ptr->authinfo ? cur_ptr->authinfo : ""),
					(cur_ptr->cookies ? cur_ptr->cookies : ""));
		} else {
			fprintf(stream,
					"GET %s HTTP/1.0\r\nAccept-Encoding: gzip\r\nUser-Agent: %s\r\nConnection: close\r\nHost: %s\r\n%s%s\r\n",
					url,
					useragent,
					host,
					(cur_ptr->authinfo ? cur_ptr->authinfo : ""),
					(cur_ptr->cookies ? cur_ptr->cookies : ""));
		}
		fflush(stream);		/* We love Solaris, don't we? */
	} else {
		/* Request URL from HTTP server. */
		if (cur_ptr->lastmodified != NULL) {
			fprintf(stream,
					"GET http://%s%s HTTP/1.0\r\nAccept-Encoding: gzip\r\nUser-Agent: %s\r\nConnection: close\r\nHost: %s\r\nIf-Modified-Since: %s\r\n%s%s\r\n",
					host,
					url,
					useragent,
					host,
					cur_ptr->lastmodified,
					(cur_ptr->authinfo ? cur_ptr->authinfo : ""),
					(cur_ptr->cookies ? cur_ptr->cookies : ""));
		} else {
			fprintf(stream,
					"GET http://%s%s HTTP/1.0\r\nAccept-Encoding: gzip\r\nUser-Agent: %s\r\nConnection: close\r\nHost: %s\r\n%s%s\r\n",
					host,
					url,
					useragent,
					host,
					(cur_ptr->authinfo ? cur_ptr->authinfo : ""),
					(cur_ptr->cookies ? cur_ptr->cookies : ""));
		}
		fflush(stream);		/* We love Solaris, don't we? */
	}
	
	if ((NetPoll (cur_ptr, my_socket, NET_READ)) == -1) {
		fclose (stream);
		return NULL;
	}
	
	if ((fgets (servreply, sizeof(servreply), stream)) == NULL) {
		fclose (stream);
		return NULL;
	}
	tmpstatus = strdup(servreply);
	savestart = tmpstatus;

	memset (httpstatus, 0, 4);	/* Nullify string so valgrind shuts up. */
	/* Set pointer to char after first space.
	   HTTP/1.0 200 OK
	            ^
	   Copy three bytes into httpstatus. */
	strsep (&tmpstatus, " ");
	if (tmpstatus == NULL) {
		cur_ptr->netio_error = NET_ERR_HTTP_PROTO_ERR;
		fclose (stream);
		free (savestart);	/* Probably more leaks when doing auth and abort here. */
		return NULL;
	}
	strncpy (httpstatus, tmpstatus, 3);
	free (savestart);
	
	cur_ptr->lasthttpstatus = atoi (httpstatus);
	
	/* If the redirectloop was run newhost and newurl were allocated.
	   We need to free them here. */
	if ((redirectcount > 0) && (authdata == NULL)) {
		free (host);
		free (url);
	}
	
	tmphttpstatus = cur_ptr->lasthttpstatus;
	handled = 1;
	/* Check HTTP server response and handle redirects. */
	do {
		switch (tmphttpstatus) {
			case 200:	/* OK */
				/* Received good status from server, clear problem field. */
				cur_ptr->netio_error = NET_ERR_OK;
				cur_ptr->problem = 0;
				break;
			case 300:	/* Multiple choice and everything 300 not handled is fatal. */
				cur_ptr->netio_error = NET_ERR_HTTP_NON_200;
				fclose (stream);
				return NULL;
			case 301:
				/* Permanent redirect. Change feed->feedurl to new location.
				   Done some way down when we have extracted the new url. */
			case 302:	/* Found */
			case 303:	/* See Other */
			case 307:	/* Temp redirect. This is HTTP/1.1 */
				redirectcount++;
			
				/* Give up if we reach MAX_HTTP_REDIRECTS to avoid loops. */
				if (redirectcount > MAX_HTTP_REDIRECTS) {
					cur_ptr->netio_error = NET_ERR_REDIRECT_COUNT_ERR;
					fclose (stream);
					return NULL;
				}
				
				while (!feof(stream)) {
					if ((fgets (netbuf, sizeof(netbuf), stream)) == NULL) {
						/* Something bad happened. Server sent stupid stuff. */
						cur_ptr->netio_error = NET_ERR_HTTP_PROTO_ERR;
						fclose (stream);
						return NULL;
					}
					/* Split netbuf into hostname and trailing url.
					   Place hostname in *newhost and tail into *newurl.
					   Close old connection and reconnect to server.
					   
					   Do not touch any of the following code! :P */
					if (strncasecmp (netbuf, "Location", 8) == 0) {
						redirecttarget = strdup (netbuf);
						freeme = redirecttarget;
						
						/* Remove trailing \r\n from line. */
						redirecttarget[strlen(redirecttarget)-2] = 0;
						
						/* In theory pointer should now be after the space char
						   after the word "Location:" */
						strsep (&redirecttarget, " ");
						
						/* Location must start with "http", otherwise switch on quirksmode. */
						if (strncmp(redirecttarget, "http", 4) != 0)
							quirksmode = 1;
						
						/* If the Location header is invalid we need to construct
						   a correct one here before proceeding with the program.
						   This makes headers like
						   "Location: fuck-the-protocol.rdf" work.
						   In violalation of RFC1945, RFC2616. */
						if (quirksmode) {
							len = 7 + strlen(host) + strlen(redirecttarget) + 3;
							newlocation = malloc(len);
							memset (newlocation, 0, len);
							strcat (newlocation, "http://");
							strcat (newlocation, host);
							if (redirecttarget[0] != '/')
								strcat (newlocation, "/");
							strcat (newlocation, redirecttarget);
						} else
							newlocation = strdup (redirecttarget);
						
						/* This also frees redirecttarget. */
						free (freeme);
						
						/* Change cur_ptr->feedurl on 301. */
						if (cur_ptr->lasthttpstatus == 301) {
							if (!suppressoutput)
								UIStatus (_("URL points to permanent redirect, updating with new location..."), 2);
							//printlog (cur_ptr, _("URL points to permanent redirect, updating with new location..."));
							free (cur_ptr->feedurl);
							if (authdata == NULL)
								cur_ptr->feedurl = strdup (newlocation);
							else {
								/* Include authdata in newly constructed URL. */
								len = strlen(authdata) + strlen(newlocation) + 2;
								cur_ptr->feedurl = malloc (len);
								newurl = strdup(newlocation);
								freeme2 = newurl;
								strsep (&newurl, "/");
								strsep (&newurl, "/");
								snprintf (cur_ptr->feedurl, len, "http://%s@%s", authdata, newurl);
								free (freeme2);
							}
						}
						
						freeme = newlocation;
						strsep (&newlocation, "/");
						strsep (&newlocation, "/");
						tmphost = newlocation;
						/* The following line \0-terminates tmphost in overwriting the first
						   / after the hostname. */
						strsep (&newlocation, "/");
						
						/* newlocation must now be the absolute path on newhost.
						   If not we've been redirected to somewhere totally stupid
						   (oh yeah, no offsite linking, go to our fucking front page).
						   Say goodbye to the webserver in this case. In fact, we don't
						   even say goodbye, but just drop the connection. */
						if (newlocation == NULL) {
							cur_ptr->netio_error = NET_ERR_REDIRECT_ERR;
							fclose (stream);
							return NULL;
						}
						
						newhost = strdup (tmphost);
						newlocation--;
						newlocation[0] = '/';
						newurl = strdup (newlocation);
					
						free (freeme);
						
						/* Close connection. */	
						fclose (stream);
						
						/* Reconnect to server. */
						if ((NetConnect (my_socket, newhost, cur_ptr, httpproto, suppressoutput)) != 0) {
							return NULL;
						}
					
						host = newhost;
						url = newurl;
						
						goto tryagain;
					}
				}
				break;
			case 304:
				/* Not modified received. We can close stream and return from here.
				   Not very friendly though. :) */
				fclose (stream);
				/* Received good status from server, clear problem field. */
				cur_ptr->netio_error = NET_ERR_OK;
				cur_ptr->problem = 0;
				
				/* This should be freed everywhere where we return
				   and current feed uses auth. */
				if ((redirectcount > 0) && (authdata != NULL)) {
					free (host);
					free (url);
				}
				return NULL;
			case 401:
				/* Authorization.
				   Parse rest of header and rerequest URL from server using auth mechanism
				   requested in WWW-Authenticate header field. (Basic or Digest) */
				break;
			case 404:
				cur_ptr->netio_error = NET_ERR_HTTP_404;
				fclose (stream);
				return NULL;
			case 410: /* The feed is gone. Politely remind the user to unsubscribe. */
				cur_ptr->netio_error = NET_ERR_HTTP_410;
				fclose (stream);
				return NULL;
			case 400:
				cur_ptr->netio_error = NET_ERR_HTTP_NON_200;
				fclose (stream);
				return NULL;
			default:
				/* unknown error codes have to be treated like the base class */
				if (handled) {
					/* first pass, modify error code to base class */
					handled = 0;
					tmphttpstatus -= tmphttpstatus % 100;
				} else {
					/* second pass, give up on unknown error base class */
					cur_ptr->netio_error = NET_ERR_HTTP_NON_200;
					//printlog (cur_ptr, servreply);
					fclose (stream);
					return NULL;
				}
		}
	} while(!handled);
	
	/* Read rest of HTTP header and parse what we need. */
	while (!feof(stream)) {	
		if ((NetPoll (cur_ptr, my_socket, NET_READ)) == -1) {
			fclose (stream);
			return NULL;
		}

		/* Max line length of sizeof(netbuf) is assumed here.
		   If header has longer lines than 4096 bytes something may go wrong. :) */
		if ((fgets (netbuf, sizeof(netbuf), stream)) == NULL)
			break;
		
		if (strstr (netbuf, "chunked") != NULL) {
			/* Server sent junked encoding. Until I understand how it works
			   and snownews uses HTTP/1.1 we must reject this answer.
			   Chunked encoding is not defined in HTTP/1.0. */
			chunked = 1;
			cur_ptr->netio_error = NET_ERR_HTTP_PROTO_ERR;
			fclose (stream);
			return NULL;
		}
		/* Get last modified date. This is only relevant on HTTP 200. */
		if ((strncasecmp (netbuf, "Last-Modified", 13) == 0) &&
			(cur_ptr->lasthttpstatus == 200)) {
			tmpstring = strdup(netbuf);
			freeme = tmpstring;
			strsep (&tmpstring, ":");
			tmpstring++;
			free(cur_ptr->lastmodified);
			cur_ptr->lastmodified = malloc (strlen(tmpstring)+1);
			strncpy (cur_ptr->lastmodified, tmpstring, strlen(tmpstring)+1);
			if (cur_ptr->lastmodified[strlen(cur_ptr->lastmodified)-1] == '\n')
				cur_ptr->lastmodified[strlen(cur_ptr->lastmodified)-1] = '\0';
			if (cur_ptr->lastmodified[strlen(cur_ptr->lastmodified)-1] == '\r')
				cur_ptr->lastmodified[strlen(cur_ptr->lastmodified)-1] = '\0';
			free(freeme);
		}
		/* Check and parse Content-Encoding header. */
		if (strncasecmp (netbuf, "Content-Encoding", 16) == 0) {
			/* Will also catch x-gzip. */
			if (strstr (netbuf, "gzip") != NULL)
				inflate = 1;
		}		
		/* HTTP authentication
		 *
		 * RFC 2617 */
		if ((strncasecmp (netbuf, "WWW-Authenticate", 16) == 0) &&
			(cur_ptr->lasthttpstatus == 401)) {
			if (authfailed) {
				/* Don't repeat authrequest if it already failed before! */
				cur_ptr->netio_error = NET_ERR_AUTH_FAILED;
				fclose (stream);
				return NULL;
			}

			/* Remove trailing \r\n from line. */
			if (netbuf[strlen(netbuf)-1] == '\n')
				netbuf[strlen(netbuf)-1] = '\0';
			if (netbuf[strlen(netbuf)-1] == '\r')
				netbuf[strlen(netbuf)-1] = '\0';
			
			authfailed++;
			
			/* Make a copy of the WWW-Authenticate header. We use it to
			   reconstruct a new auth reply on every loop. */
			free (cur_ptr->servauth);
			
			cur_ptr->servauth = strdup (netbuf);
			
			/* Load authinfo into cur_ptr->authinfo. */
			retval = NetSupportAuth(cur_ptr, authdata, url, netbuf);
			
			switch (retval) {
				case 1:
					cur_ptr->netio_error = NET_ERR_AUTH_NO_AUTHINFO;
					fclose (stream);
					return NULL;
					break;
				case 2:
					cur_ptr->netio_error = NET_ERR_AUTH_GEN_AUTH_ERR;
					fclose (stream);
					return NULL;
					break;
				case -1:
					cur_ptr->netio_error = NET_ERR_AUTH_UNSUPPORTED;
					fclose (stream);
					return NULL;
					break;
				default:
					break;
			}
			
			/* Close current connection and reconnect to server. */
			fclose (stream);
			if ((NetConnect (my_socket, host, cur_ptr, httpproto, suppressoutput)) != 0) {
				return NULL;
			}

			/* Now that we have an authinfo, repeat the current request. */
			goto tryagain;
		}
		/* This seems to be optional and probably not worth the effort since we
		   don't issue a lot of consecutive requests. */
		/*if ((strncasecmp (netbuf, "Authentication-Info", 19) == 0) ||
			(cur_ptr->lasthttpstatus == 200)) {
		
		}*/
		
		/* HTTP RFC 2616, Section 19.3 Tolerant Applications.
		   Accept CRLF and LF line ends in the header field. */
		if ((strcmp(netbuf, "\r\n") == 0) || (strcmp(netbuf, "\n") == 0))
			break;
	}
	
	/* If the redirectloop was run newhost and newurl were allocated.
	   We need to free them here.
	   But _after_ the authentication code since it needs these values! */
	if ((redirectcount > 0) && (authdata != NULL)) {
		free (host);
		free (url);
	}
	
	/**********************
	 * End of HTTP header *
	 **********************/
	
	/* Init pointer so strncat works.
	   Workaround class hack. */
	body = malloc(1);
	body[0] = '\0';
	
	length = 0;

	/* Read stream until EOF and return it to parent. */
	while (!feof(stream)) {
		if ((NetPoll (cur_ptr, my_socket, NET_READ)) == -1) {
			fclose (stream);
			return NULL;
		}
		
		/* Since we handle binary data if we read compressed input we
		   need to use fread instead of fgets after reading the header. */ 
		retval = fread (netbuf, 1, sizeof(netbuf), stream);
		if (retval == 0)
			break;
		body = realloc (body, length+retval);
		memcpy (body+length, netbuf, retval);
		length += retval;
		if (retval != 4096)
			break;
	}
	body = realloc(body, length+1);
	body[length] = '\0';
		
	/* Close connection. */
	fclose (stream);
	
	/* If inflate==1 we need to decompress the content.. */
	if (inflate == 1) {
		/* gzipinflate */
		inflatedbody = gzip_uncompress (body, length, NULL);
		
		if (inflatedbody == NULL) {
			free (body);
			cur_ptr->netio_error = NET_ERR_GZIP_ERR;
			return NULL;
		}
		
		/* Copy uncompressed data back to body. */
		free (body);
		body = strdup (inflatedbody);
		free (inflatedbody);
	}
	
	cur_ptr->contentlength = strlen (body);
	
	return body;
}

/* Returns allocated string with body of webserver reply.
   Various status info put into struct feed_request * cur_ptr.
   Set suppressoutput=1 to disable ncurses calls. */
char * DownloadFeed (char * url, struct feed_request * cur_ptr, int suppressoutput) {
	int my_socket = 0;
	char *host;					/* Needs to freed. */
	char *tmphost;
	char *freeme;
	char *returndata;
	char *tmpstr;
	int httpproto = 0;			/* 0: http; 1: https */
	xmlURIPtr uri;

	uri = xmlParseURI(url);
	
	/* The following code will not handle the following URLs:
	   http://foo.bar
	   
	   Must add at least a / ! */
	strsep (&url, "/");
	strsep (&url, "/");
	tmphost = url;
	strsep (&url, "/");
	if (url == NULL || uri == NULL) {
		/* End of string url. Probably entered invalid stuff like
		   http://asdf or something. Since we don't want to crash and burn
		   return the dreaded NULL pointer. */
		cur_ptr->problem = 1;
		cur_ptr->netio_error = NET_ERR_URL_INVALID;
		return NULL;
	}
	
	if (strcasecmp (uri->scheme, "https") == 0)
		httpproto = 1;
	else
		httpproto = 0;
	
	
	/* If tmphost contains an '@', extract username and pwd. */
	if (strchr (tmphost, '@') != NULL) {
		tmpstr = tmphost;
		strsep (&tmphost, "@");
	}
	
	host = strdup (tmphost);
	
	/* netio() might change pointer of host to something else if redirect
	   loop is executed. Make a copy so we can correctly free everything. */
	freeme = host;
	url--;
	url[0] = '/';
	
	if (url[strlen(url)-1] == '\n') {
		url[strlen(url)-1] = '\0';
	}
	
	if ((NetConnect (&my_socket, host, cur_ptr, httpproto, suppressoutput)) != 0) {
		free (freeme);
		cur_ptr->problem = 1;
		return NULL;
	}
		
	returndata = NetIO (&my_socket, host, url, cur_ptr, uri->user, httpproto, suppressoutput);
	
	if ((returndata == NULL) && (cur_ptr->netio_error != NET_ERR_OK)) {
		cur_ptr->problem = 1;
	}
	
	/* url will be freed in the calling function. */
	free (freeme);		/* This is *host. */
	xmlFreeURI(uri);
	return returndata;
}

/*-----------------------------------------------------------------------*/
/* some Liferea specific code...					 */

#include "../debug.h"
#include "../update.h"
#include "downloadlib.h"

/* Downloads a feed and returns the data or NULL as return value.
   The url of the has to be passed in the feed structure.
   If the the webserver reports a permanent redirection, the
   feed url will be modified and the old URL 'll be freed. The
   request structure 'll also contain the HTTP status and the
   last modified string.
 */

void downloadlib_init() {
}

void downloadlib_process_url(struct request *request) {
	struct feed_request cur_ptr;
	gchar *oldurl = g_strdup(request->source);
	debug1(DEBUG_UPDATE, "downloading %s", request->source);

	cur_ptr.feedurl = request->source;
	cur_ptr.problem = 0;
	if (request->lastmodified.tv_sec > 0)
		cur_ptr.lastmodified = createRFC822Date(&(request->lastmodified.tv_sec));
	else
		cur_ptr.lastmodified = NULL;
	
	cur_ptr.contentlength = 0;
	cur_ptr.cookies = NULL;
	cur_ptr.authinfo = NULL;
	cur_ptr.servauth = NULL;
	cur_ptr.lasthttpstatus = 0; /* This might, or might not mean something to someone */
	
	/* Fixme: assert that it is a http:// URL */

	request->data = DownloadFeed (oldurl, &cur_ptr, 0);

	g_free(oldurl);
	if (request->data == NULL)
		cur_ptr.problem = 1;
	request->size = cur_ptr.contentlength;
	request->httpstatus = cur_ptr.lasthttpstatus == 0 ? 404 : cur_ptr.lasthttpstatus;
	request->returncode = cur_ptr.netio_error;
	request->source = cur_ptr.feedurl;
	if (cur_ptr.lastmodified != NULL)
		request->lastmodified.tv_sec = parseRFC822Date(cur_ptr.lastmodified);
	else
		request->lastmodified.tv_sec = 0L;
	
	request->lastmodified.tv_usec = 0L;
	g_free(cur_ptr.lastmodified);
	g_free(cur_ptr.cookies);
	g_free(cur_ptr.servauth);
	debug3(DEBUG_UPDATE, "download result - HTTP status: %d, error: %d, data: %d", request->httpstatus, cur_ptr.problem, request->data);
	return;
}
