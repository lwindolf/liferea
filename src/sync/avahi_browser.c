/**
 * @file avahi_browser.c  cache synchronization using AVAHI
 * 
 * Copyright (C) 2007 Lars Lindner <lars.lindner@gmail.com>
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

#include "sync/avahi_browser.h"
#include "conf.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>

#include <avahi-client/lookup.h>
#include <avahi-client/publish.h>
#include <avahi-client/client.h>
#include <avahi-common/error.h>
#include <avahi-glib/glib-malloc.h>
#include <avahi-glib/glib-watch.h>

static void liferea_avahi_browser_class_init	(LifereaAvahiBrowserClass *klass);
static void liferea_avahi_browser_init	(LifereaAvahiBrowser	*browser);
static void liferea_avahi_browser_finalize	(GObject *object);

#define LIFEREA_AVAHI_BROWSER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), LIFEREA_AVAHI_BROWSER_TYPE, LifereaAvahiBrowserPrivate))

struct LifereaAvahiBrowserPrivate {
	AvahiClient		*client;
	AvahiGLibPoll		*poll;
	AvahiServiceBrowser	*serviceBrowser;
	
	GSList			*resolvers;
};

enum {
	SERVICE_ADDED,
	SERVICE_REMOVED,
	LAST_SIGNAL
};

enum {
	PROP_0
};

static guint	     signals [LAST_SIGNAL] = { 0, };
static GObjectClass *parent_class = NULL;

static gpointer liferea_avahi_browser = NULL;

GQuark
liferea_avahi_browser_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("liferea_avahi_browser_error");

	return quark;
}

static void
client_cb (AvahiClient         *client,
	   AvahiClientState     state,
	   LifereaAvahiBrowser *browser)
{
	/* Called whenever the client or server state changes */

	switch (state) {
	case AVAHI_CLIENT_FAILURE:
		g_warning ("Client failure: %s\n", avahi_strerror (avahi_client_errno (client)));
		break;
	default:
		break;
	}
}

static void
avahi_client_init (LifereaAvahiBrowser *browser)
{
	gint error = 0;

	avahi_set_allocator (avahi_glib_allocator ());

	browser->priv->poll = avahi_glib_poll_new (NULL, G_PRIORITY_DEFAULT);

	if (!browser->priv->poll)
		g_warning ("Unable to create AvahiGlibPoll object for mDNS");

	AvahiClientFlags flags;
	flags = 0;

	browser->priv->client = avahi_client_new (avahi_glib_poll_get (browser->priv->poll),
						    flags,
						    (AvahiClientCallback)client_cb,
						    browser,
						    &error);
}

static void
resolve_cb (AvahiServiceResolver  *service_resolver,
	    AvahiIfIndex           interface,
	    AvahiProtocol          protocol,
	    AvahiResolverEvent     event,
	    const char            *service_name,
	    const char            *type,
	    const char            *domain,
	    const char            *host_name,
	    const AvahiAddress    *address,
	    uint16_t               port,
	    AvahiStringList       *text,
	    AvahiLookupResultFlags flags,
	    LifereaAvahiBrowser    *browser)
{
	if (event == AVAHI_RESOLVER_FOUND) {
		char    *name = NULL;
		char     host [AVAHI_ADDRESS_STR_MAX];
		gboolean pp = FALSE;

		if (text) {
			AvahiStringList *l;

			for (l = text; l != NULL; l = l->next) {
				size_t size;
				char  *key;
				char  *value;
				int    ret;

				ret = avahi_string_list_get_pair (l, &key, &value, &size);
				if (ret != 0 || key == NULL) {
					continue;
				}

				if (strcmp (key, "Password") == 0) {
					if (size >= 4 && strncmp (value, "true", 4) == 0) {
						pp = TRUE;
					}
				} else if (strcmp (key, "Machine Name") == 0) {
					name = g_strdup (value);
				}

				g_free (key);
				g_free (value);
			}
		}

		if (name == NULL) {
			name = g_strdup (service_name);
		}

		avahi_address_snprint (host, AVAHI_ADDRESS_STR_MAX, address);

		g_signal_emit (browser,
			       signals [SERVICE_ADDED],
			       0,
			       service_name,
			       name,
			       host,
			       port,
			       pp);

		g_free (name);
	}

	browser->priv->resolvers = g_slist_remove (browser->priv->resolvers, service_resolver);
	avahi_service_resolver_free (service_resolver);
}

static gboolean
liferea_avahi_browser_resolve (LifereaAvahiBrowser *browser,
			       const char         *name)
{
	AvahiServiceResolver *service_resolver;

	service_resolver = avahi_service_resolver_new (browser->priv->client,
						       AVAHI_IF_UNSPEC,
						       AVAHI_PROTO_INET,
						       name,
						       "_liferea._tcp",
						       NULL,
						       AVAHI_PROTO_UNSPEC,
						       0,
						       (AvahiServiceResolverCallback)resolve_cb,
						       browser);
	if (service_resolver == NULL) {
		rb_debug ("Error starting mDNS resolving using AvahiServiceResolver");
		return FALSE;
	}

	browser->priv->resolvers = g_slist_prepend (browser->priv->resolvers, service_resolver);

	return TRUE;
}

void
liferea_avahi_browser_start (LifereaAvahiBrowser *browser)
{
}

void
liferea_avahi_browser_stop (LifereaAvahiBrowser *browser)
{
}

GType
liferea_avahi_browser_get_type (void) 
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) 
	{
		static const GTypeInfo our_info = 
		{
			sizeof (LifereaAvahiBrowserClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) liferea_avahi_browser_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (LifereaAvahiBrowser),
			0, /* n_preallocs */
			(GInstanceInitFunc) liferea_avahi_browser_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "LifereaAvahiBrowser",
					       &our_info, 0);
	}

	return type;
}

static void
liferea_avahi_browser_class_init (LifereaAvahiBrowserClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = liferea_avahi_browser_finalize;

	g_type_class_add_private (object_class, sizeof(LifereaAvahiBrowserPrivate));
}

static void
liferea_avahi_browser_init (LifereaAvahiBrowser *browser)
{
	browser->priv = LIFEREA_AVAHI_BROWSER_GET_PRIVATE (browser);
	
	avahi_client_init (browser);
}

static void
liferea_avahi_browser_finalize (GObject *object)
{
	LifereaAvahiBrowser *browser;
	
	browser = LIFEREA_AVAHI_BROWSER (object);
	
	g_return_if_fail (browser->priv != NULL);
	
/*	if (browser->priv->entry_group)
		avahi_entry_group_free (browser->priv->entry_group);
	*/	
	if (browser->priv->client)
		avahi_client_free (browser->priv->client);
		
	if (browser->priv->poll)
		avahi_glib_poll_free (browser->priv->poll);
		
//	g_free (browser->priv->name);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

LifereaAvahiBrowser *
liferea_avahi_browser_new (void)
{
	gchar	*serviceName;
	
	/* check service name preference and set default if necessary */
	serviceName = conf_get_str_value (SYNC_AVAHI_SERVICE_NAME);
	if (g_str_equal (serviceName, "")) {
		g_free (serviceName);
		serviceName = g_strdup_printf (_("Liferea Sync %s@%s"), g_get_user_name(), g_get_host_name ());
		conf_set_str_value (SYNC_AVAHI_SERVICE_NAME, serviceName);
	}
	g_free (serviceName);

	if (liferea_avahi_browser) {
		g_object_ref (liferea_avahi_browser);
	} else {
		liferea_avahi_browser = g_object_new (LIFEREA_AVAHI_BROWSER_TYPE, NULL);
		g_object_add_weak_pointer (liferea_avahi_browser, (gpointer *) &liferea_avahi_browser);
	}
	
	return liferea_avahi_browser;
}
