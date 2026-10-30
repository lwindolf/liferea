/*
 * @file micropub.c  micropub integration
 * 
 * Copyright (C) 2026 Lars Windolf <lars.windolf@gmx.de>
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

#include "micropub.h"

#include "auth.h"
#include "browser.h"
#include "common.h"
#include "conf.h"
#include "debug.h"
#include "item.h"
#include "update.h"
#include "update_job.h"
#include "xml.h"
#include "ui/auth_dialog.h"
#include "ui/liferea_dialog.h"
#include "ui/liferea_shell.h"

/* This microsub implementation supports the following authentication types:

        1. user-provided auth token (asked on demand if no authorization endpoint found)
        2. IndieAuth
        3. OAuth2
  */

/* micropub dialog singleton */

/* The dialog singleton binds asynchronous OAuth2 callbacks and token input by user
   together. There must be only one dialog and one auth flow at any time. The dialog
   "Post" buttons stays disabled until we have a token. The dialog stays open while
   posting and is not auto-closed on auth failure. Instead a new auth flow is started,
   the "Post" button is disabled and stays this way until a valid auth token is available.
   The user can close the dialog and cancel everything at any time. */
static GtkWidget *micropubDialog = NULL;

/* Callback of the currently active auth flow. As there is only one auth flow
   at any time this is handed over to the token dialog via static state, as
   the flow job itself ends when the dialog is opened. */
//static micropub_auth_cb pendingAuthCb = NULL;
//static gpointer pendingAuthData = NULL;

static gchar *
micropub_get_auth_token_id (void)
{
        g_autofree gchar *url = NULL;
        conf_get_str_value (MICROPUB_ENDPOINT, &url);
        return g_strdup_printf ("micropub:%s", url);
}

static gchar *
micropub_get_auth_token (void)
{
        g_autofree gchar *tokenId = micropub_get_auth_token_id ();
        return liferea_auth_info_get (tokenId);
}

/* micropub auth flow */

typedef enum {
        MICRO_PUB_AUTH_START            = 0,
        MICRO_PUB_AUTH_DISCOVER         = 1
} StepMicroPubAuth;

typedef struct {
        StepMicroPubAuth step;
        GCallback callback;             // callback to be run once a token is available
        gchar *url;                     // microblog URL
        gchar *microPubEndpoint;
        gchar *tokenEndpoint;
} FlowMicroPubAuth;

static void
micropub_auth_flow_free (gpointer user_data)
{
        FlowMicroPubAuth *flow = (FlowMicroPubAuth *)user_data;

        g_free (flow->microPubEndpoint);
        g_free (flow->tokenEndpoint);
        g_free (flow->url);
        g_free (flow);
}

static void
micropub_auth_token_dialog_cb (const gchar *token, gpointer user_data)
{
        GCallback cb = (GCallback)user_data;

        if (!token || !*token) {
                liferea_shell_toast ("%s", _("App token is mandatory, cannot post without it!"));
                adw_dialog_close (ADW_DIALOG (micropubDialog));
                return;
        }

        debug (DEBUG_UPDATE, "micropub_auth_token_dialog_cb: token entered by user");
        g_autofree gchar *tokenId = micropub_get_auth_token_id ();
        liferea_auth_info_set (tokenId, tokenId /* as label */, token);
        (*cb) ();
}

// PKCE flow
static gboolean
micropub_auth_flow (UpdateJob *job) {
        FlowMicroPubAuth *flow = (FlowMicroPubAuth *)job->user_data;
        UpdateResult *result = job->result;

        debug (DEBUG_UPDATE, "micropub_auth_flow: step=%d", flow->step);

        switch (flow->step) {
                case MICRO_PUB_AUTH_START: {
                        /* first we need to discover the microsub endpoints from HTML, so GET fetch the endpoint first */
                        g_autofree gchar *url = NULL;
                        conf_get_str_value (MICROPUB_ENDPOINT, &url);
                        g_assert (url);

                        g_assert (!job->request);
                        job->request = update_request_new ("GET", url, NULL, NULL);
                        flow->step = MICRO_PUB_AUTH_DISCOVER;
                        break;
                }
                case MICRO_PUB_AUTH_DISCOVER:
                        if (result->httpstatus >= 200 && result->httpstatus < 300) {
                                /* extract microsub endpoints from HTML <link rel="micropub" href="..."> */
                                xmlDocPtr doc = xhtml_parse (result->data, result->size);
                                if (doc) {
                                        xmlNodePtr root = xmlDocGetRootElement (doc);
                                        xmlNodePtr node = xpath_find (root, "//link[@rel='micropub']");
                                        if (node)
                                                flow->microPubEndpoint = xml_get_attribute (node, "href");
                                        node = xpath_find (root, "//link[@rel='token_endpoint']");
                                        if (node)
                                                flow->tokenEndpoint = xml_get_attribute (node, "href");

                                        xmlFreeDoc (doc);
                                } else {
                                        debug (DEBUG_UPDATE, "micropub_auth_flow: HTML parsing failed!");
                                }

                                debug (DEBUG_UPDATE, "micropub_auth_flow: micropub endpoint discovered: >>>%s<<<", flow->microPubEndpoint);
                                debug (DEBUG_UPDATE, "micropub_auth_flow: token endpoint discovered: >>>%s<<<", flow->tokenEndpoint);

                                if (!flow->microPubEndpoint) {
                                        liferea_shell_toast (_("ERROR: The configured blog URL does need to specify a micropub endpoint!"));
                                        return TRUE;
                                }

                                if (!flow->tokenEndpoint) {
                                        /* No authorization endpoint: variant 1, ask the user for a token */
                                        g_autofree gchar *tokenId = micropub_get_auth_token_id ();
                                        auth_token_dialog_new (flow->url, tokenId, micropub_auth_token_dialog_cb, flow->callback);
                                } else {
                                        /* variant 2/3: launch external browser for OAuth2 */
                                        // redirect URI should be "net.sf.liferea://<micropub uri>"
                                }
                        } else {
                                liferea_shell_toast ("ERROR: HTTP error when accessing micro blog!");
                                return TRUE;   // cancel flow
                        }
                        return TRUE;    // end flow
                        break;
                default:
                        g_assert_not_reached ();
        }

        return FALSE;  /* continue the flow */
}

static void
micropub_auth (GCallback cb) {
        FlowMicroPubAuth *flow;

        flow = g_new0 (FlowMicroPubAuth, 1);
        flow->step = MICRO_PUB_AUTH_START;
        flow->callback = cb;
        conf_get_str_value (MICROPUB_ENDPOINT, &flow->url);

        update_job_new_flow (
                NULL /* owner */,
                micropub_auth_flow,
                flow,
                micropub_auth_flow_free,
                UPDATE_REQUEST_PRIORITY_HIGH
        );
}

// OAuth2 callback
void
micropub_auth_callback (const gchar *url) {
        // FIXME: extract token
        // Run flow callback
}

/* notes:

A practical posting flow:

1. Get the user’s site URL. Let them enter the domain or profile URL they use to sign in.
2. Discover the Micropub endpoint. Fetch that page and look for <link rel="micropub" 
   href="…"> in its HTML, or a Link response header with rel="micropub". W3C
3. Authorize the client. Use the site’s OAuth/IndieAuth flow to obtain an access 
   token with the permission needed to create posts. Keep the token securely and 
   send it as Authorization: Bearer …. W3C
4. Optionally query configuration. Send an authenticated GET to the endpoint with
   q=config to discover supported options, such as a media endpoint. Don’t assume 
   the endpoint’s host or capabilities. W3C
5. Compose and preview the bookmark. Show the headline, destination URL, and any 
   excerpt or note; let the user edit or cancel before publishing.
6. POST the entry. Start with the required form-encoded syntax, which Micropub 
   clients and servers must support. For example:

POST /micropub HTTP/1.1
Authorization: Bearer TOKEN
Content-Type: application/x-www-form-urlencoded

h=entry&name=Interesting+headline&bookmark-of=https%3A%2F%2Fnews.example%2Farticle&content=Why+I+saved+this

7. Handle the response. Treat a successful response and any returned Location header as the created post’s URL; show the result and mark the feed item as shared so it’s not accidentally posted twice.

For a feed reader, make sharing explicitly user-triggered rather than automatic, and retain the original article URL as bookmark-of. If you later support other post types, JSON, editing, or media uploads, add those as separate capabilities; JSON and media support are optional in the spec. 
*/

// Open microblogging dialog, auth check/reauth/initial auth will be done on submit

static void
micropub_post_request (void)
{
        g_assert (micropubDialog);

       	const gchar *text = liferea_dialog_entryrow_get (micropubDialog, "postEntry");
        g_warning("POST implement me token=%s text=%s!", micropub_get_auth_token(), text);
}

static void
on_micropub_post_response (GtkWidget *dialog, gpointer user_data)
{
        g_autofree gchar *token = micropub_get_auth_token ();

        if (!token) {
                debug (DEBUG_UPDATE, "micropub_post: we have no auth token yet -> trigger auth");
                micropub_auth (micropub_post_request);
        } else {
                debug (DEBUG_UPDATE, "micropub_post: using existing token");
                micropub_post_request ();
        }
}

void
micropub_post (LifereaItem *item)
{
        g_autofree gchar *url = NULL;
        conf_get_str_value (MICROPUB_ENDPOINT, &url);
        if (!url) {
                liferea_shell_toast (_("Please configure a blog URL in the preferences first!"));
                return;
        }

        micropubDialog = liferea_dialog_run ("micropub", on_micropub_post_response, NULL, NULL);
        g_object_add_weak_pointer (G_OBJECT (micropubDialog), (gpointer *)&micropubDialog);
}
