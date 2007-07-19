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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

/* OS X needs this, otherwise socklen_t is not defined. */
#ifdef __APPLE__
#       define _BSD_SOCKLEN_T_
#endif

/* BeOS does not define socklen_t. Using uint as suggested by port creator. */
#ifdef __BEOS__
#       define socklen_t unsigned int
#endif

#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/stat.h>
#include <assert.h>

#include <libxml/uri.h>
#include <arpa/nameser.h>
#include <resolv.h>


#ifdef HAVE_GNUTLS
#include <gnutls/gnutls.h>
#include <gcrypt.h>
#include <pthread.h>

GCRY_THREAD_OPTION_PTHREAD_IMPL;

static gnutls_certificate_client_credentials xcred;

#endif

#include "../debug.h"
#include "../common.h"
#include "conversions.h"
#include "net-support.h"
#include "zlib_interface.h"
#include "../update.h"

static int const MAX_HTTP_REDIRECTS = 10;	/* Maximum number of redirects we will follow. */
extern int NET_TIMEOUT;				/* Global network timeout in sec */
static int const NET_READ = 1;
static int const NET_WRITE = 2;

extern char *proxyname;				/* Hostname of proxyserver. */
extern int proxyport;				/* Port on proxyserver to use. */
extern char *proxyusername;
extern char *proxypassword;
extern char *useragent;

enum netio_proto {
	NETIO_PROTO_INVALID,
	NETIO_PROTO_HTTP,
	NETIO_PROTO_HTTPS
};

int NetWrite(int fd, void *proto_data, enum netio_proto proto, const void *data, size_t len) {
	if (proto == NETIO_PROTO_HTTP) {
		debug2(DEBUG_NET, "writing %u bytes to socket %d", len, fd);
		return write(fd, data, len);
	}
#ifdef HAVE_GNUTLS
	else if (proto == NETIO_PROTO_HTTPS) {
		debug2(DEBUG_NET, "writing %u bytes to SSL session %p", len, proto_data);
		int s;
		do {
			s = gnutls_record_send((gnutls_session)proto_data, data, len);
		} while ((s == GNUTLS_E_AGAIN) || (s == GNUTLS_E_INTERRUPTED));
		return s;
	}
#endif
	else {
		return -1;
	}
}

static int NetRead(int fd, void *proto_data, enum netio_proto proto, char *data, size_t len) {
	int s;
	if (proto == NETIO_PROTO_HTTP) {
		s = read(fd, data, len-1);
		if (s >=0)
		    data[s] = '\0';
		debug2(DEBUG_NET, "read %u bytes from socket %d", len, s);
		return s;
	}
#ifdef HAVE_GNUTLS
	else if (proto == NETIO_PROTO_HTTPS) {
		do {
			s = gnutls_record_recv((gnutls_session)proto_data, data, len-1);
		} while ((s == GNUTLS_E_AGAIN) || (s == GNUTLS_E_INTERRUPTED));
	
		/* ignore some type of errors (happens with GMail for some reason) */
		if (GNUTLS_E_UNEXPECTED_PACKET_LENGTH == s)
			s = GNUTLS_E_SUCCESS;

		if (s > 0)
			data[s] = '\0';

		debug3(DEBUG_NET, "read %u bytes from SSL session %p (return code = %d)", len, proto_data, s);
		return s;
	}
#endif
	else {
		return -1;
	}
}
static void NetClose(int fd, void *proto_data, enum netio_proto proto) {
	close(fd);
	debug1(DEBUG_NET, "closed socket %d", fd);
#ifdef HAVE_GNUTLS
	if (proto == NETIO_PROTO_HTTPS) {
		/* do not wait for the peer to close the connection. */
		gnutls_bye ((gnutls_session)proto_data, GNUTLS_SHUT_WR);
		gnutls_deinit (proto_data);
		debug1(DEBUG_NET, "closed SSL session %p", proto_data);
	}
#endif
}
/* Waits NET_TIMEOUT seconds for the socket to return data.
 *
 * Returns
 *
 *	0	Socket is ready
 *	-1	Error occurred (netio_error is set)
 */
int NetPoll (struct feed_request * cur_ptr, int my_socket, int rw) {
	fd_set rfds;
	struct timeval tv;
	int retval;				/* FD_ISSET + assert == Heisenbug? */
	
	/* Set global network timeout */
	
	if ((rw != NET_READ) && (rw != NET_WRITE)) {
		cur_ptr->netio_error = NET_ERR_UNKNOWN;
		return -1;
	}
	
	do {
		tv.tv_sec = NET_TIMEOUT;
		tv.tv_usec = 0;
		FD_ZERO(&rfds);
		FD_SET(my_socket, &rfds);
		if (rw == NET_READ)
			retval = select (my_socket+1, &rfds, NULL, NULL, &tv);
		else
			retval = select (my_socket+1, NULL, &rfds, NULL, &tv);
	} while((retval == -1) && (errno == EINTR));

	if (retval == -1) {
		perror("Select returned error in netio: ");
		cur_ptr->netio_error = NET_ERR_SOCK_ERR;
		return -1;
	}
	
	if (!FD_ISSET(my_socket, &rfds)) {
		/* Timed out */
		debug1(DEBUG_NET, "timeout for socket %u", my_socket);
		cur_ptr->netio_error = NET_ERR_TIMEOUT;
		return -1;
	} else {
		return 0;
	}
}

/* This assumes that a line of the header will be no longer than half the receive buffer */
static int NetReadHeaderLine(int my_socket, void *proto_data, enum netio_proto proto,
                             struct feed_request * cur_ptr, char *recvbuf, size_t recvbuflen,
                             size_t *recvbufused, char **nextstr, char *headerline) {
	int i;
	char *ci;
	/* Look for line ending */
	if (*recvbufused > 0) {
		for(ci=*nextstr; ci <= recvbuf+*recvbufused-2; ci++) {
			if (ci[0] == '\r' && ci[1] == '\n' ) {
				strncpy(headerline, *nextstr, 2+(ci-*nextstr));
				headerline[2+(ci-*nextstr)] = '\0';
				*nextstr = ci+2;
				return 0;
			}
			if (*ci == '\r' || *ci == '\n' ) {
				strncpy(headerline, *nextstr, 1+(ci-*nextstr));
				headerline[2+(ci-*nextstr)] = '\0';
				*nextstr = ci+1;
				return 0;
			}
			
		}
	}
	
	/* Expand the buffer */
	if (*nextstr != NULL) {
		int dead;
		dead = *nextstr-recvbuf;
		memmove(recvbuf, nextstr, *recvbufused-(dead));
		*recvbufused -= dead;
	}
	*nextstr = recvbuf;
	if (NetPoll (cur_ptr, my_socket, NET_READ) == -1) {
		return -1;
	}
	i = NetRead(my_socket, proto_data, proto, recvbuf+*recvbufused, recvbuflen-*recvbufused);
	if (i <= 0)
		return -1;
	*recvbufused += i;
	*nextstr = recvbuf;
	/* And try to get a line (again) */

	for(ci=*nextstr; ci < recvbuf+*recvbufused; ci++) {
		if (ci[0] == '\r' && ci[1] == '\n' ) {
			strncpy(headerline, *nextstr, 2+(ci-*nextstr));
			headerline[2+(ci-*nextstr)] = '\0';
			*nextstr = ci+2;
			return 0;
		}
		if (*ci == '\r' || *ci == '\n' ) {
			strncpy(headerline, *nextstr, 1+(ci-*nextstr));
			headerline[2+(ci-*nextstr)] = '\0';
			*nextstr = ci+1;
			return 0;
		}
		
	}
	/* At this point, the line was too long, or not enough data or something */
	debug1(DEBUG_NET, "Fatal: too long header line read from socket %d!", my_socket);
	return -1;
}

#ifdef HAVE_GNUTLS
void *NetConnectGnutls(int *fd) {
	gnutls_session session = NULL;
	static const int cert_type_priority[2] = { GNUTLS_CRT_X509, 0 };
	int ret;

	gnutls_init(&session, GNUTLS_CLIENT);	
	gnutls_set_default_priority(session);
	gnutls_certificate_type_set_priority(session, cert_type_priority);
	gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, xcred);
	gnutls_transport_set_ptr(session, (gnutls_transport_ptr)*fd);
	do {
		ret = gnutls_handshake(session);
	} while ((ret == GNUTLS_E_AGAIN) || (ret == GNUTLS_E_INTERRUPTED));
	
	if (ret < 0) {
		close(*fd);
		*fd = -1;
		debug1(DEBUG_NET, "Fatal: failed to set up SSL session for socket %d", fd);
		return NULL;
	}
	debug2(DEBUG_NET, "SSL session %p set up for socket %d", session, fd);
	return session;
}
#endif

#define USE_PROXY ((proxyport != 0) && (cur_ptr->no_proxy != 1))

/* Connect network sockets.
 *
 * Returns
 *
 *	0	Connected
 *	-1	Error occured (netio_error is set)
 */
static int NetConnect (int * my_socket, char * host, struct feed_request * cur_ptr, enum netio_proto proto, int suppressoutput, void **proto_data) {
#ifdef HAVE_GETADDRINFO
	socklen_t len;
	int ret;
	struct addrinfo hints;
	struct addrinfo *res = NULL, *ai;
	char *realhost;
	char port_str[9];

	debug0(DEBUG_NET, "NetConnect() (with getaddrinfo)");
	
	if (proto  == NETIO_PROTO_INVALID
#ifndef HAVE_GNUTLS
		|| proto == NETIO_PROTO_HTTPS
#endif
		) {
		cur_ptr->netio_error = NET_ERR_PROTO_INVALID;
		debug0(DEBUG_NET, "invalid protocol!");
		return -1;
	}

	res = 0;
	memset(&hints, 0, sizeof(hints));
	memset(&port_str, 0, sizeof(port_str));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	if(!USE_PROXY) {
		realhost = g_strdup(host);
		if (sscanf (host, "%[^:]:%8s", realhost, port_str) != 2) {
			if (proto == NETIO_PROTO_HTTPS)
				strcpy(port_str, "443");
			else 
				strcpy(port_str, "80");				
		}
	} else {
		snprintf(port_str, sizeof(port_str), "%d", proxyport);
		realhost = g_strdup(proxyname);
	}
	debug2(DEBUG_NET, "host=%s port=%s", realhost, port_str);
	
	ret = getaddrinfo(realhost, port_str, &hints, &res);
	
	if (ret != 0 || res == NULL) {
		if (res != NULL)
			freeaddrinfo(res);
		res_init(); /* Reset the resolver */
		ret = getaddrinfo(realhost, port_str, &hints, &res);
		if (ret != 0 || res == NULL) {
			if (res != NULL)
				freeaddrinfo(res);
			cur_ptr->netio_error = NET_ERR_HOST_NOT_FOUND;
			debug1(DEBUG_NET, "error: could not resolve hostname \"%s\"", realhost);
			g_free(realhost);
			return -1;
		}
	}
	g_free(realhost);
	
	for (ai = res; ai != NULL; ai = ai->ai_next) {
		/* Create a socket. */
		*my_socket = socket (ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (*my_socket == -1) {
			cur_ptr->netio_error = NET_ERR_SOCK_ERR;
			debug0(DEBUG_NET, "error: failed to create socket");
			continue;
		}
	
		/* Connect socket. */
		cur_ptr->connectresult = connect (*my_socket, ai->ai_addr, ai->ai_addrlen);
		
		/* Check if we're already connected.
		   BSDs will return 0 on connect even in nonblock if connect was fast enough. */
		if (cur_ptr->connectresult != 0) {
			/* If errno is not EINPROGRESS, the connect went wrong. */
			if (errno != EINPROGRESS) {
				close (*my_socket);
				cur_ptr->netio_error = NET_ERR_CONN_REFUSED;
				debug0(DEBUG_NET, "error: connection refused");
				continue;
			}
			
			if ((NetPoll (cur_ptr, *my_socket, NET_WRITE)) == -1) {
				close (*my_socket);
				debug0(DEBUG_NET, "error: NetPoll() failed, closing socket");
				continue;
			}
			
			/* We get errno of connect back via getsockopt SO_ERROR (into connectresult). */
			len = sizeof(cur_ptr->connectresult);
			getsockopt(*my_socket, SOL_SOCKET, SO_ERROR, &cur_ptr->connectresult, &len);
			
			if (cur_ptr->connectresult != 0) {
				close (*my_socket);
				cur_ptr->netio_error = NET_ERR_CONN_FAILED;	/* ->strerror(cur_ptr->connectresult) */
				debug0(DEBUG_NET, "error: connection failed");
				continue;
			}
		}
		
		cur_ptr->netio_error = NET_ERR_OK;
		break;
	}
		
	freeaddrinfo(res);
	if (cur_ptr->netio_error != NET_ERR_OK)
		return -1;
	
	debug1(DEBUG_NET, "successfully connected socket %d", *my_socket);
	
#else
	struct sockaddr_in address;	
	struct hostent *remotehost;
	socklen_t len;
	char *realhost;
	unsigned short port;

	debug0(DEBUG_NET, "NetConnect() (without getaddrinfo)");
	
	if (proto  == NETIO_PROTO_INVALID
#ifndef HAVE_GNUTLS
		|| proto == NETIO_PROTO_HTTPS
#endif
		) {
		cur_ptr->netio_error = NET_ERR_PROTO_INVALID;
		debug0(DEBUG_NET, "invalid protocol!");
		return -1;
	}

	realhost = g_strdup(host);
	if (sscanf (host, "%[^:]:%hd", realhost, &port) != 2) {
		port = 80;
	}
	debug2(DEBUG_NET, "host=%s port=%u", realhost, port);
	
	/* Create a inet stream TCP socket. */
	*my_socket = socket (AF_INET, SOCK_STREAM, 0);
	if (*my_socket == -1) {
		cur_ptr->netio_error = NET_ERR_SOCK_ERR;
		debug0(DEBUG_NET, "error: failed to create socket");
		return -1;
	}
	
	/* If proxyport is 0 we didn't execute the if http_proxy statement in main
	   so there is no proxy. On any other value of proxyport do proxyrequests instead. */
	remotehost = gethostbyname (USE_PROXY?proxyname:realhost);
	
	/* Lookup remote IP. */
	if (remotehost == NULL) {
		res_init(); /* Reset resolver & force reread of resolv.conf */
		remotehost = gethostbyname (proxyname);
		if (remotehost == NULL) {
			close (*my_socket);
			debug1(DEBUG_NET, "error: could not resolve hostname \"%s\"", realhost);
			g_free (realhost);
			cur_ptr->netio_error = NET_ERR_HOST_NOT_FOUND;
			return -1;
		}
	}
	/* Set the remote address. */
	address.sin_family = AF_INET;
	address.sin_port = htons(USE_PROXY?proxyport:port);
	memcpy (&address.sin_addr.s_addr, remotehost->h_addr_list[0], remotehost->h_length);
	
	/* Connect socket. */
	cur_ptr->connectresult = connect (*my_socket, (struct sockaddr *) &address, sizeof(address));
	
	/* Check if we're already connected.
	   BSDs will return 0 on connect even in nonblock if connect was fast enough. */
	if (cur_ptr->connectresult != 0) {
		if (errno != EINPROGRESS) {
			close (*my_socket);
			g_free (realhost);
			cur_ptr->netio_error = NET_ERR_CONN_REFUSED;
			debug0(DEBUG_NET, "error: connection refused");
			return -1;
		}
		
		if ((NetPoll (cur_ptr, *my_socket, NET_WRITE)) == -1) {
			close (*my_socket);
			g_free (realhost);
			debug0(DEBUG_NET, "error: NetPoll() failed, closing socket");
			return -1;
		}
		
		len = sizeof(cur_ptr->connectresult);
		getsockopt(*my_socket, SOL_SOCKET, SO_ERROR, &cur_ptr->connectresult, &len);
		
		if (cur_ptr->connectresult != 0) {
			close (*my_socket);
			g_free (realhost);
			cur_ptr->netio_error = NET_ERR_CONN_FAILED;	/* ->strerror(cur_ptr->connectresult) */
			debug0(DEBUG_NET, "error: connection failed");
			return -1;
		}
	}
	
	debug1(DEBUG_NET, "successfully connected socket %d", *my_socket);
	
	g_free(realhost);
#endif /* HAVE_GETADDRINFO */
#ifdef HAVE_GNUTLS
	if (proto == NETIO_PROTO_HTTPS) {
		debug1(DEBUG_NET, "setting up SSL session for socket %d", *my_socket);
		*proto_data = NetConnectGnutls(my_socket);
		if (proto_data == NULL || *my_socket == -1)
			return -1;
	}
#endif
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
char * NetIO (char * host, char * url, struct feed_request * cur_ptr, char * authdata, enum netio_proto proto, int suppressoutput) {
	char netbuf[4096];          /* Network read buffer. */
	char *body;					/* XML body. */
	unsigned int length;
	int chunked = 0;			/* Content-Encoding: chunked received? */
	int redirectcount;			/* Number of HTTP redirects followed. */
	char httpstatus[4];			/* HTTP status sent by server. */
	char *tmpstatus;
	char *savestart;			/* Save start position of pointers. */
	char *tmphost;				/* Pointers needed to strsep operation. */
	char *newhost;				/* New hostname if we need to redirect. */
	char *newurl;				/* New document name ". */
	char *newlocation;
	char *tmpstring;			/* Temp pointers. */
	char *freeme, *freeme2, *nextstr;
	char *redirecttarget;
	char headerline[8192];
	int retval;
	int handled;
	int tmphttpstatus;
	int inflate = 0;			/* Whether feed data needs decompressed with zlib. */
	int len;
	size_t recvbufused;
	void * inflatedbody;
	int quirksmode = 0;			/* IIS operation mode. */
	int authfailed = 0;			/* Avoid repeating failed auth requests endlessly. */
	void *proto_data;
	int my_socket;
	
	debug2(DEBUG_NET, "downloading url=%s host=%s", url, host);
	
	if ((NetConnect (&my_socket, host, cur_ptr, proto, suppressoutput, &proto_data)) != 0) {
		cur_ptr->problem = 1;
		return NULL;
	}
	
	redirectcount = 0;
	
	/* Goto label to redirect reconnect. */
	tryagain:
	
	/* Reconstruct digest authinfo for every request so we don't reuse
	   the same nonce value for more than one request.
	   This happens one superflous time on 303 redirects. */
	if ((cur_ptr->authinfo != NULL) && (cur_ptr->servauth != NULL)) {
		if (strstr (cur_ptr->authinfo, " Digest ") != NULL) {
			NetSupportAuth(cur_ptr, authdata, url, cur_ptr->servauth);
		}
	}
	
	/* Again is proxyport == 0, non proxy mode, otherwise make proxy requests. */
	/* Request URL from HTTP server. */
	tmpstring = NULL;
	if (USE_PROXY && proxyusername != NULL && proxypassword != NULL && ((proxyusername[0] != '\0') || (proxypassword[0] != '\0')))
		/* construct auth function appends \r\n to string */
		tmpstring = ConstructBasicAuth(proxyusername,proxypassword);
		   snprintf(netbuf, sizeof(netbuf)-1,
		   "GET %s%s%s HTTP/1.0\r\n"
		   "Accept-Encoding: gzip\r\n"
		   "User-Agent: %s\r\n"
		   "Connection: close\r\n"
		   "Host: %s\r\n"
		   "%s%s%s" /* Last modified */
		   "%s" /* authinfo*/
		   "%s" /* cookies */
		   "%s%s" /* Proxy*/
		   "%s%s%s" /* etag */
		   "\r\n",
		   USE_PROXY ? "http://" : "",
		   USE_PROXY ? host : "",
		   url,
		   useragent,
		   host,
		   cur_ptr->lastmodified != NULL ? "If-Modified-Since: " : "",
		   cur_ptr->lastmodified != NULL ?  cur_ptr->lastmodified : "",
		   cur_ptr->lastmodified != NULL ? "\r\n" : "",
		   (cur_ptr->authinfo ? cur_ptr->authinfo : ""),
		   (cur_ptr->cookies ? cur_ptr->cookies : ""),
		   tmpstring != NULL ? "Proxy-": "",
		   tmpstring != NULL ? tmpstring : "",
		   cur_ptr->etag != NULL ? "If-None-Match: " : "",
		   cur_ptr->etag != NULL ? cur_ptr->etag : "",
		   cur_ptr->etag != NULL ? "\r\n" : ""
		   );
	
	if (tmpstring != NULL)
		g_free(tmpstring);
		
	if(debug_level & DEBUG_VERBOSE)
		debug2(DEBUG_NET, "sending HTTP request on socket %d: \n%s", my_socket, netbuf);
		
	NetWrite(my_socket, proto_data, proto, netbuf, strlen(netbuf));
	
	if ((NetPoll (cur_ptr, my_socket, NET_READ)) == -1) {
		debug1(DEBUG_NET, "no response on socket %d", my_socket);
		NetClose (my_socket, proto_data, proto);
		return NULL;
	}
	
	debug1(DEBUG_NET, "received HTTP response for socket %d", my_socket);
	
	nextstr = NULL;
	recvbufused = 0;
	if (NetReadHeaderLine(my_socket, proto_data, proto, cur_ptr,
							   netbuf, sizeof(netbuf), &recvbufused,
							   &nextstr, headerline) < 0) {
		debug1(DEBUG_NET, "failed to read HTTP header from socket %d", my_socket);
		NetClose (my_socket, proto_data, proto);
		return NULL;
	}
	
	if(debug_level & DEBUG_VERBOSE)
		debug1(DEBUG_NET, "read header line >>> %s", headerline);
		
	if (checkValidHTTPHeader(headerline, sizeof(headerline)) != 0) {
		cur_ptr->netio_error = NET_ERR_HTTP_PROTO_ERR;
		debug0(DEBUG_NET, "error: header line is invalid");
		NetClose (my_socket, proto_data, proto);		
		return NULL;
	}
	
	tmpstatus = g_strdup(headerline);
	savestart = tmpstatus;

	memset (httpstatus, 0, 4);	/* Nullify string so valgrind shuts up. */
	/* Set pointer to char after first space.
	   HTTP/1.0 200 OK
	            ^
	   Copy three bytes into httpstatus. */
	strsep (&tmpstatus, " ");
	if (tmpstatus == NULL) {
		cur_ptr->netio_error = NET_ERR_HTTP_PROTO_ERR;
		debug0(DEBUG_NET, "error: HTTP protocol failure");
		NetClose (my_socket, proto_data, proto);
		g_free (savestart);	/* Probably more leaks when doing auth and abort here. */
		return NULL;
	}
	strncpy (httpstatus, tmpstatus, 3);
	g_free (savestart);
	
	debug2(DEBUG_NET, "read HTTP status \"%s\" for socket %d", httpstatus, my_socket);
	cur_ptr->lasthttpstatus = atoi (httpstatus);
	
	/* If the redirectloop was run newhost and newurl were allocated.
	   We need to free them here. */
	if ((redirectcount > 0) && (authdata == NULL)) {
		g_free (host);
		g_free (url);
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
				handled = 1;
				break;
			case 300:	/* Multiple choice and everything 300 not handled is fatal. */			
				cur_ptr->netio_error = NET_ERR_HTTP_NON_200;
				debug1(DEBUG_NET, "unhandled 3xx status for socket %d", my_socket);
				NetClose (my_socket, proto_data, proto);
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
					debug1(DEBUG_NET, "too many redirects on socket %d", my_socket);
					NetClose (my_socket, proto_data, proto);
					return NULL;
				}
				
				while (1) {
					if (NetReadHeaderLine(my_socket, proto_data, proto, cur_ptr,
										  netbuf, sizeof(netbuf), &recvbufused,
										  &nextstr, headerline) < 0) {
						cur_ptr->netio_error = NET_ERR_HTTP_PROTO_ERR;
						debug0(DEBUG_NET, "error: HTTP protocol failure");
						NetClose (my_socket, proto_data, proto);
						return NULL;
					}
					
					if (checkValidHTTPHeader(headerline, sizeof(headerline)) != 0) {
						cur_ptr->netio_error = NET_ERR_HTTP_PROTO_ERR;
						debug1(DEBUG_NET, "error: invalid header line >>> %s", headerline);
						NetClose (my_socket, proto_data, proto);
						return NULL;
					}
					
					/* Split headerline into hostname and trailing url.
					   Place hostname in *newhost and tail into *newurl.
					   Close old connection and reconnect to server.
					   
					   Do not touch any of the following code! :P */
					if (strncasecmp (headerline, "Location", 8) == 0) {
						redirecttarget = g_strdup (headerline);
						freeme = redirecttarget;
						
						/* Remove trailing \r\n from line. */
						redirecttarget[strlen(redirecttarget)-2] = 0;
						
						/* In theory pointer should now be after the space char
						   after the word "Location:" */
						strsep (&redirecttarget, " ");
						
						if (redirecttarget == NULL) {
							cur_ptr->problem = 1;
							cur_ptr->netio_error = NET_ERR_REDIRECT_ERR;
							debug1(DEBUG_NET, "redirect error for socket %d", my_socket);
							g_free (freeme);
							NetClose (my_socket, proto_data, proto);
							return NULL;
						}
						
						/* Location must start with "http", otherwise switch on quirksmode. */
						if (strncmp(redirecttarget, "http", 4) != 0)
							quirksmode = 1;

						debug2(DEBUG_NET, "found Location header %s>>> %s", (quirksmode == 1)?"(quirks mode on)":"", redirecttarget);
						
						/* If the Location header is invalid we need to construct
						   a correct one here before proceeding with the program.
						   This makes headers like
						   "Location: fuck-the-protocol.rdf" work.
						   In violalation of RFC1945, RFC2616. */
						if (quirksmode) {
							len = 7 + strlen(host) + strlen(redirecttarget) + 3;
							newlocation = g_new0(gchar, len);
							strcat (newlocation, "http://");
							strcat (newlocation, host);
							if (redirecttarget[0] != '/')
								strcat (newlocation, "/");
							strcat (newlocation, redirecttarget);
						} else
							newlocation = g_strdup (redirecttarget);
						
						/* This also frees redirecttarget. */
						g_free (freeme);
						
						/* Change cur_ptr->feedurl on 301. */
						if (cur_ptr->lasthttpstatus == 301) {
							/* Check for valid redirection URL */
							if (checkValidHTTPURL(newlocation) != 0) {
								cur_ptr->problem = 1;
								cur_ptr->netio_error = NET_ERR_REDIRECT_ERR;
								debug0(DEBUG_NET, "error: invalid URL specified for redirect");
								NetClose (my_socket, proto_data, proto);
								return NULL;
							}
							/*if (!suppressoutput)
							  UIStatus (_("URL points to permanent redirect, updating with new location..."), 2);*/
							/*printlog (cur_ptr, _("URL points to permanent redirect, updating with new location..."));*/
							g_free (cur_ptr->feedurl);
							if (authdata == NULL)
								cur_ptr->feedurl = g_strdup (newlocation);
							else {
								/* Include authdata in newly constructed URL. */
								len = strlen(authdata) + strlen(newlocation) + 2;
								cur_ptr->feedurl = g_new0(gchar, len);
								newurl = g_strdup(newlocation);
								freeme2 = newurl;
								strsep (&newurl, "/");
								strsep (&newurl, "/");
								snprintf (cur_ptr->feedurl, len, "http://%s@%s", authdata, newurl);
								g_free (freeme2);
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
							NetClose (my_socket, proto_data, proto);
							return NULL;
						}
						
						newhost = g_strdup (tmphost);
						newlocation--;
						newlocation[0] = '/';
						newurl = g_strdup (newlocation);
					
						g_free (freeme);
						
						debug2(DEBUG_NET, "redirect to host=%s location=%s", newhost, newlocation);
						
						/* Close connection. */	
						NetClose (my_socket, proto_data, proto);
						
						/* Reconnect to server. */
						if ((NetConnect (&my_socket, newhost, cur_ptr, proto, suppressoutput, &proto_data)) != 0) {
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
				NetClose (my_socket, proto_data, proto);
				/* Received good status from server, clear problem field. */
				cur_ptr->netio_error = NET_ERR_OK;
				cur_ptr->problem = 0;
				
				/* This should be freed everywhere where we return
				   and current feed uses auth. */
				if ((redirectcount > 0) && (authdata != NULL)) {
					g_free (host);
					g_free (url);
				}
				return NULL;
			case 401:
				/* Authorization.
				   Parse rest of header and rerequest URL from server using auth mechanism
				   requested in WWW-Authenticate header field. (Basic or Digest) */
				break;
			case 404:
				cur_ptr->netio_error = NET_ERR_HTTP_404;
				NetClose (my_socket, proto_data, proto);
				return NULL;
			case 410: /* The feed is gone. Politely remind the user to unsubscribe. */
				cur_ptr->netio_error = NET_ERR_HTTP_410;
				NetClose (my_socket, proto_data, proto);
				return NULL;
			case 400:
				cur_ptr->netio_error = NET_ERR_HTTP_NON_200;
				NetClose (my_socket, proto_data, proto);
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
					debug1(DEBUG_NET, "unknown HTTP status read from socket %d", my_socket);
					NetClose (my_socket, proto_data, proto);
					return NULL;
				}
		}
	} while(!handled);
	
	/* Read rest of HTTP header and parse what we need. */
	while (1) {
		if (NetReadHeaderLine(my_socket, proto_data, proto, cur_ptr,
							  netbuf, sizeof(netbuf), &recvbufused,
							  &nextstr, headerline) < 0) {
			debug0(DEBUG_NET, "error: HTTP protocol failure");
			NetClose (my_socket, proto_data, proto);
			return NULL;
		}
		
		if (checkValidHTTPHeader(headerline, sizeof(headerline)) != 0) {
			cur_ptr->netio_error = NET_ERR_HTTP_PROTO_ERR;
			debug1(DEBUG_NET, "error: invalid header line >>> %s", headerline);
			NetClose (my_socket, proto_data, proto);
			return NULL;
		}
		
		debug2(DEBUG_NET, "processing header line from socket %d >>> %s", my_socket, headerline);
		
		if (strncasecmp (headerline, "Transfer-Encoding", 17) == 0) {
			/* Chunked transfer encoding. HTTP/1.1 extension.
			   http://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html#sec3.6.1 */
			if (strstr (headerline, "chunked") != NULL) {
				debug0(DEBUG_NET, "-> chunked encoding detected!");
				chunked = 1;
			}
		}
		
		/* Get last modified date. This is only relevant on HTTP 200. */
		if ((strncasecmp (headerline, "Last-Modified", 13) == 0) &&
			(cur_ptr->lasthttpstatus == 200)) {
			tmpstring = g_strdup(headerline);
			freeme = tmpstring;
			strsep (&tmpstring, " ");
			if (tmpstring == NULL)
				g_free (freeme);
			else {
				g_free(cur_ptr->lastmodified);
				cur_ptr->lastmodified = g_strdup(tmpstring);
				if (cur_ptr->lastmodified[strlen(cur_ptr->lastmodified)-1] == '\n')
					cur_ptr->lastmodified[strlen(cur_ptr->lastmodified)-1] = '\0';
				if (cur_ptr->lastmodified[strlen(cur_ptr->lastmodified)-1] == '\r')
					cur_ptr->lastmodified[strlen(cur_ptr->lastmodified)-1] = '\0';
				g_free(freeme);
				debug1(DEBUG_NET, "-> last modified \"%s\"", cur_ptr->lastmodified);
			}
		}
		
		/* Get the E-Tag */
		if ((strncasecmp (headerline, "ETag:", 5) == 0) &&
		    (cur_ptr->lasthttpstatus == 200)) {
			tmpstring = g_strdup(headerline);
			freeme = tmpstring;
			tmpstring += 5;
			while(*tmpstring != '\0' && (*tmpstring == ' ' || *tmpstring == '\t'))
				tmpstring++;
			if (tmpstring[strlen(tmpstring)-1] == '\n')
				tmpstring[strlen(tmpstring)-1] = '\0';
			if (tmpstring[strlen(tmpstring)-1] == '\r')
				tmpstring[strlen(tmpstring)-1] = '\0';
			if (cur_ptr->etag != NULL)
				g_free(cur_ptr->etag);
			cur_ptr->etag = g_strdup(tmpstring);
			g_free(freeme);
			debug1(DEBUG_NET, "-> etag \"%s\"", cur_ptr->etag);
		}
		/* Check and parse Content-Encoding header. */
		if (strncasecmp (headerline, "Content-Encoding", 16) == 0) {
			/* Will also catch x-gzip. */
			if (strstr (headerline, "gzip") != NULL) {
				inflate = 1;
				debug0(DEBUG_NET, "-> encoding is gzip");
			}
		}
		
		if (strncasecmp (headerline, "Content-Type", 12) == 0) {
			tmpstring = g_strdup(headerline);
			freeme = tmpstring;
			strsep(&tmpstring, " ");
			if (tmpstring == NULL)
				g_free (freeme);
			else {
				size_t slen;
				freeme2 = NULL;
				freeme2 = strstr(tmpstring, ";");
				if (freeme2 != NULL)
					freeme2[0] = '\0';
				g_free(cur_ptr->content_type);
				cur_ptr->content_type = g_strdup(tmpstring);
				slen = strlen(cur_ptr->content_type);
				if (slen && (cur_ptr->content_type[slen-1] == '\n')) {
					slen--;
					cur_ptr->content_type[slen] = '\0';
				}
				if (slen && (cur_ptr->content_type[slen-1] == '\r')) {
					cur_ptr->content_type[slen-1] = '\0';
				}
				g_free(freeme);
				debug1(DEBUG_NET, "-> content type \"%s\"", cur_ptr->content_type); 
			}
		}
		
		/* HTTP authentication
		 *
		 * RFC 2617 */
		if ((strncasecmp (headerline, "WWW-Authenticate", 16) == 0) &&
			(cur_ptr->lasthttpstatus == 401)) {
			size_t slen;
			if (authfailed) {
				/* Don't repeat authrequest if it already failed before! */
				cur_ptr->netio_error = NET_ERR_AUTH_FAILED;
				debug1(DEBUG_NET, "error: previous authentication for request %p failed", cur_ptr);
				NetClose (my_socket, proto_data, proto);
				return NULL;
			}

			/* Remove trailing \r\n from line. */
			slen = strlen(headerline);
			if (slen && (headerline[slen-1] == '\n')) {
				slen--;
				headerline[slen] = '\0';
			}
			if (slen && (headerline[slen-1] == '\r'))
				headerline[slen-1] = '\0';
				
			/* Make a copy of the WWW-Authenticate header. We use it to
			   reconstruct a new auth reply on every loop. */
			g_free (cur_ptr->servauth);
			
			cur_ptr->servauth = g_strdup (headerline);
			
			/* Load authinfo into cur_ptr->authinfo. */
			debug0(DEBUG_NET, "parsing authinfo");
			retval = NetSupportAuth(cur_ptr, authdata, url, headerline);
			
			switch (retval) {
				case 1:
					cur_ptr->netio_error = NET_ERR_AUTH_NO_AUTHINFO;
					debug0(DEBUG_NET, "error: invalid authentication header");
					NetClose (my_socket, proto_data, proto);
					return NULL;
					break;
				case 2:
					cur_ptr->netio_error = NET_ERR_AUTH_GEN_AUTH_ERR;
					debug0(DEBUG_NET, "error: generic authentication problem");
					NetClose (my_socket, proto_data, proto);
					return NULL;
					break;
				default:
					break;
			}
			
			if(-1 != retval) {			
				authfailed++;
				
				/* Close current connection and reconnect to server. */
				NetClose (my_socket, proto_data, proto);
				if ((NetConnect (&my_socket, host, cur_ptr, proto, suppressoutput, &proto_data)) != 0) {
					return NULL;
				}

				/* Now that we have an authinfo, repeat the current request. */
				debug2(DEBUG_NET, "created authinfo \"%s\", retrying request %p", cur_ptr->authinfo, cur_ptr);
				goto tryagain;
			}
		}
		/* This seems to be optional and probably not worth the effort since we
		   don't issue a lot of consecutive requests. */
		/*if ((strncasecmp (headerline, "Authentication-Info", 19) == 0) ||
			(cur_ptr->lasthttpstatus == 200)) {
		
		}*/
		
		/* HTTP RFC 2616, Section 19.3 Tolerant Applications.
		   Accept CRLF and LF line ends in the header field. */
		if ((strcmp(headerline, "\r\n") == 0) || (strcmp(headerline, "\n") == 0))
			break;
	}

	if(cur_ptr->lasthttpstatus == 401) {
		cur_ptr->netio_error = NET_ERR_AUTH_UNSUPPORTED;
		debug0(DEBUG_NET, "error: authentication unsupported");
		NetClose (my_socket, proto_data, proto);
		return NULL;
	}
	
	/* If the redirectloop was run newhost and newurl were allocated.
	   We need to free them here.
	   But _after_ the authentication code since it needs these values! */
	if ((redirectcount > 0) && (authdata != NULL)) {
		g_free (host);
		g_free (url);
	}
	
	/**********************
	 * End of HTTP header *
	 **********************/
	
	/* Init pointer so strncat works.
	   Workaround class hack. */
	length = recvbufused - (nextstr-netbuf);
	body = g_malloc(length+1);
	memcpy(body, nextstr, length);
	body[length] = '\0';

	/* Read stream until EOF and return it to parent. */
	while (1) {
		if ((NetPoll (cur_ptr, my_socket, NET_READ)) == -1) {
			NetClose (my_socket, proto_data, proto);
			return NULL;
		}
		retval = NetRead(my_socket, proto_data, proto, netbuf, sizeof(netbuf));
		if (retval == 0)
			break;
		else if (retval < 0) {
			g_free(body);
			cur_ptr->netio_error = NET_ERR_SOCK_ERR;
			debug1(DEBUG_NET, "error %d: while reading from socket", retval);
			return NULL;
		}
		body = g_realloc (body, length+retval);
		memcpy (body+length, netbuf, retval);
		length += retval;

	}
	body = g_realloc(body, length+1);
	body[length] = '\0';
	
	debug2(DEBUG_NET, "read %d bytes message body from socket %d", length, my_socket);
		
	/* Close connection. */
	NetClose (my_socket, proto_data, proto);
	
	if (chunked) {
		if (decodechunked(body, &length) == NULL) {
			g_free (body);
			cur_ptr->netio_error = NET_ERR_HTTP_PROTO_ERR;
			debug0(DEBUG_NET, "error: decoding chunked response failed");
			return NULL;
		}
	}
		
	len = length;
	
	/* If inflate==1 we need to decompress the content.. */
	if (inflate == 1) {
		/* gzipinflate */
		if (jg_gzip_uncompress (body, length, &inflatedbody, &len) != 0) {
			g_free (body);
			cur_ptr->netio_error = NET_ERR_GZIP_ERR;
			debug0(DEBUG_NET, "error: gzip uncompression failed");
			return NULL;
		}
		
		/* Copy uncompressed data back to body. */
		g_free (body);
		body = inflatedbody;
	}
	
	cur_ptr->contentlength = len;
	
	return body;
}

/* Returns allocated string with body of webserver reply.
   Various status info put into struct feed_request * cur_ptr.
   Set suppressoutput=1 to disable ncurses calls. */
char * DownloadFeed (char * url, struct feed_request * cur_ptr, int suppressoutput) {
	char *host;					/* Needs to freed. */
	char *tmphost;
	char *freeme;
	char *returndata;
	char *tmpstr;
	enum netio_proto proto = NETIO_PROTO_INVALID;
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
		if (uri != NULL)
			xmlFreeURI(uri);
		return NULL;
	}
	
	if (strcasecmp (uri->scheme, "http") == 0)
		proto = NETIO_PROTO_HTTP;
	else if (strcasecmp (uri->scheme, "https") == 0)
		proto = NETIO_PROTO_HTTPS;
	else {
		cur_ptr->problem = 1;
		cur_ptr->netio_error = NET_ERR_PROTO_INVALID;
		if (uri != NULL)
			xmlFreeURI(uri);
		return NULL;
	}
	
	/* If tmphost contains an '@', extract username and pwd. */
	if (strchr (tmphost, '@') != NULL) {
		tmpstr = tmphost;
		strsep (&tmphost, "@");
	}
	
	host = g_strdup (tmphost);
	
	/* netio() might change pointer of host to something else if redirect
	   loop is executed. Make a copy so we can correctly free everything. */
	freeme = host;
	url--;
	url[0] = '/';
	
	if (url[strlen(url)-1] == '\n') {
		url[strlen(url)-1] = '\0';
	}
	
	returndata = NetIO (host, url, cur_ptr, uri->user, proto, suppressoutput);
	
	if ((returndata == NULL) || (cur_ptr->netio_error != NET_ERR_OK)) {
		cur_ptr->problem = 1;
	}
	
	/* url will be freed in the calling function. */
	g_free (freeme);		/* This is *host. */
	xmlFreeURI(uri);
	return returndata;
}

void
netio_init (void)
{
#ifdef HAVE_GNUTLS
	gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
	
	gnutls_global_init ();
	gnutls_certificate_allocate_credentials (&xcred);
	gnutls_certificate_set_x509_trust_file (xcred, "ca.pem", GNUTLS_X509_FMT_PEM);
#endif
}

void
netio_deinit (void)
{
#ifdef HAVE_GNUTLS
	gnutls_certificate_free_credentials (xcred);
	
	gnutls_global_deinit ();
#endif
}
