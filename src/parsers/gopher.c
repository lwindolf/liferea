/**
 * @file gopher.c  Parsing phlog gopher listings
 *
 * Copyright (C) 2025 Lars Windolf <lars.windolf@gmx.de>
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

#include "gopher.h"

#include "common.h"
#include "debug.h"
#include "feed_parser.h"
#include "item.h"
#include "update_job.h"

#define MAX_GOPHER_ITEMS 25

void
gopher_process_request (const UpdateJob *job)
{
        g_autofree gchar *host = NULL;
        g_autofree gchar *port = NULL;
        g_autofree gchar *path = NULL;

        /* Parse the gopher URL (e.g., gopher://host:port/path) */
        gchar **parts = g_strsplit (job->request->source + 9, "/", 2);
        gchar **host_port = g_strsplit (parts[0], ":", 2);

        host = g_strdup (host_port[0]);
        port = g_strdup (host_port[1] ? host_port[1] : "70"); // Default gopher port is 70
        path = g_strdup ((parts[1] && parts[1][0] != 0) ? parts[1] + 1 : "");
        debug (DEBUG_UPDATE, "GOPHER: host=%s port=%s path=%s\n", host, port, path);
        g_strfreev (host_port);
        g_strfreev (parts);

        if (!host || !port || !path) {
                debug (DEBUG_UPDATE, "Invalid gopher URL: %s", job->request->source);
                return;
        }

        g_autoptr(GError) error = NULL;
        g_autoptr(GSocket) socket = g_socket_new (G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP, &error);
        if (!socket) {
                debug (DEBUG_UPDATE, "Failed to create socket: %s", error->message);
                return;
        }
        g_socket_set_timeout (socket, 15 * 1000); // 15 seconds timeout

        g_autoptr(GSocketAddress) address = NULL;
        g_autoptr(GResolver) resolver = g_resolver_get_default ();
        g_autoptr(GList) addresses = g_resolver_lookup_by_name (resolver, host, NULL, &error);
        if (!addresses) {
                debug (DEBUG_UPDATE, "Failed to resolve host: %s", error ? error->message : "Unknown error");
                return;
        }

        g_autoptr(GInetAddress) inet_address = G_INET_ADDRESS (addresses->data);
        if (inet_address)
                address = g_inet_socket_address_new (inet_address, atoi (port));

        if (!address || !g_socket_connect (socket, address, NULL, &error)) {
                debug (DEBUG_UPDATE, "Failed to connect: %s", error ? error->message : "Unknown error");
                return;
        }

        /* Send the gopher request */
        g_autofree gchar *request = g_strdup_printf ("/%s\r\n", path);
        gssize bytes_written = g_socket_send (socket, request, strlen (request), NULL, &error);
        if (bytes_written < 0) {
                debug (DEBUG_UPDATE, "Failed to send request: %s", error->message);
                return;
        }

        /* Read the response */
        gchar buffer[1024*1024*5];      // FIXME: limit to 5MB for now
        gssize bytes_read = g_socket_receive (socket, buffer, sizeof (buffer) - 1, NULL, &error);
        if (bytes_read < 0) {
                debug (DEBUG_UPDATE, "Failed to read response: %s", error->message);
                job->result->data = NULL;
		job->result->size = 0;
        } else {
                buffer[bytes_read] = '\0';
                job->result->data = g_strdup (buffer);
                job->result->size = bytes_read;
                debug (DEBUG_UPDATE, "Received response: %s", buffer);
        }

       	update_job_finished (job);
}

/**
 * Parses given data as a gopher document
 *
 * @param ctxt		the feed parser context
 * @param data		the raw data of the document
 */
static void
gopher_feed_parse (feedParserCtxtPtr ctxt, const gchar *data)
{
	ctxt->subscription->time = time (NULL);
        ctxt->subscription->html5Extract = TRUE;        // force text enrichment
        ctxt->title = g_strdup (ctxt->subscription->source + strlen ("gopher://"));

	/* For gopher the homepage is the source */
	subscription_set_homepage (ctxt->subscription, ctxt->subscription->source);

        /* Find all gopher items (type 0) */
        gchar **lines = g_strsplit (data, "\n", -1);
        for (gint i = 0; lines[i]; i++) {
                g_strchomp (lines[i]);
                gchar **fields = g_strsplit (lines[i], "\t", 5);
                /* Format is <"0"><title>\t<host>\t<port>\t<path> */
                if (fields[0] && fields[0][0] == '0' && fields[1] && fields[2] && fields[3]) {
                        itemPtr item = item_new ();
                        item_set_title (item, fields[0] + 1);
                        item_set_source (item, g_strdup_printf ("gopher://%s:%s/0%s", fields[2], fields[3], fields[1]));
                        item_set_description (item, "");        // content fetching happens async in subscription_enrich_item()
                        item->sourceId = g_strdup (item->source);
                        item->time = ctxt->subscription->time;
                        ctxt->items = g_list_append (ctxt->items, item);

                        // be friendly to very long phlog listings
                        if (g_list_length (ctxt->items) > MAX_GOPHER_ITEMS)
                                break;
                }
                g_strfreev (fields);
        }
        g_strfreev (lines);
}

static gboolean
gopher_feed_check (const gchar *data, const gchar *url)
{
        /* If the URL contains gopher://, not starts with to support "| curl gopher://" use cases */
        return NULL != strstr (url, "gopher://");
}

feedHandlerPtr
gopher_init_feed_handler (void)
{
	feedHandlerPtr	fhp;

	fhp = g_new0 (struct feedHandler, 1);

        fhp->typeStr = "gopher";
	fhp->textFeedParser = gopher_feed_parse;
	fhp->checkTextFormat = gopher_feed_check;

	return fhp;
}
