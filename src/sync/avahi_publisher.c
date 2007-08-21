/**
 * @file avahi_publisher.c  cache synchronization using AVAHI
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

#include "sync/avahi_publisher.h"
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

static void liferea_avahi_publisher_class_init	(LifereaAvahiPublisherClass *klass);
static void liferea_avahi_publisher_init	(LifereaAvahiPublisher	*publisher);
static void liferea_avahi_publisher_finalize	(GObject *object);

#define LIFEREA_AVAHI_PUBLISHER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), LIFEREA_AVAHI_PUBLISHER_TYPE, LifereaAvahiPublisherPrivate))

struct LifereaAvahiPublisherPrivate {
	AvahiClient	*client;
	AvahiGLibPoll	*poll;
	AvahiEntryGroup	*entry_group;
	
	gchar		*name;
	guint		port;
};

enum {
	PUBLISHED,
	NAME_COLLISION,
	LAST_SIGNAL
};

enum {
	PROP_0
};

static guint	     signals [LAST_SIGNAL] = { 0, };
static GObjectClass *parent_class = NULL;

static gpointer liferea_avahi_publisher = NULL;

GQuark
liferea_avahi_publisher_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("liferea_avahi_publisher_error");

	return quark;
}

static void
client_cb (AvahiClient         *client,
	   AvahiClientState     state,
	   LifereaAvahiPublisher *publisher)
{
	/* FIXME
	 * check to make sure we're in the _RUNNING state before we publish
	 * check for COLLISION state and remove published information
	 */

	/* Called whenever the client or server state changes */

	switch (state) {
	case AVAHI_CLIENT_S_RUNNING:

		/* The server has startup successfully and registered its host
		 * name on the network, so it's time to create our services */

		break;

	case AVAHI_CLIENT_S_COLLISION:

		 /* Let's drop our registered services. When the server is back
		  * in AVAHI_SERVER_RUNNING state we will register them
		  * again with the new host name. */
		 if (publisher->priv->entry_group) {
			 avahi_entry_group_reset (publisher->priv->entry_group);
		 }
		 break;

	case AVAHI_CLIENT_FAILURE:

		 g_warning ("Client failure: %s\n", avahi_strerror (avahi_client_errno (client)));
		 break;
	case AVAHI_CLIENT_CONNECTING:
	case AVAHI_CLIENT_S_REGISTERING:
	default:
		break;
	}
}

static void
avahi_client_init (LifereaAvahiPublisher *publisher)
{
	gint error = 0;

	avahi_set_allocator (avahi_glib_allocator ());

	publisher->priv->poll = avahi_glib_poll_new (NULL, G_PRIORITY_DEFAULT);

	if (! publisher->priv->poll) {
		g_warning ("Unable to create AvahiGlibPoll object for mDNS");
	}

	AvahiClientFlags flags;
	flags = 0;

	publisher->priv->client = avahi_client_new (avahi_glib_poll_get (publisher->priv->poll),
						    flags,
						    (AvahiClientCallback)client_cb,
						    publisher,
						    &error);
}

static void
entry_group_cb (AvahiEntryGroup     *group,
		AvahiEntryGroupState state,
		LifereaAvahiPublisher *publisher)
{
	if (state == AVAHI_ENTRY_GROUP_ESTABLISHED) {

		g_signal_emit (publisher, signals [PUBLISHED], 0, publisher->priv->name);

	} else if (state == AVAHI_ENTRY_GROUP_COLLISION) {
		g_warning ("MDNS name collision");

		g_signal_emit (publisher, signals [NAME_COLLISION], 0, publisher->priv->name);
	}
}

static gboolean
create_service (LifereaAvahiPublisher *publisher)
{
	int         ret;

	if (publisher->priv->entry_group == NULL) {
		publisher->priv->entry_group = avahi_entry_group_new (publisher->priv->client,
								      (AvahiEntryGroupCallback)entry_group_cb,
								      publisher);
	} else {
		avahi_entry_group_reset (publisher->priv->entry_group);
	}

	if (publisher->priv->entry_group == NULL) {
		g_warning ("Could not create AvahiEntryGroup for publishing");
		return FALSE;
	}

	ret = avahi_entry_group_add_service (publisher->priv->entry_group,
					     AVAHI_IF_UNSPEC,
					     AVAHI_PROTO_UNSPEC,
					     0,
					     publisher->priv->name,
					     "_liferea._tcp",
					     NULL,
					     NULL,
					     publisher->priv->port,
					     "Password=false" /* txt_record */,
					     NULL);

	if (ret < 0) {
		g_warning ("Could not add service: %s", avahi_strerror (ret));
		return FALSE;
	}

	ret = avahi_entry_group_commit (publisher->priv->entry_group);

	if (ret < 0) {
		g_warning ("Could not commit service: %s", avahi_strerror (ret));
		return FALSE;
	}

	return TRUE;
}

gboolean
liferea_avahi_publisher_publish (LifereaAvahiPublisher *publisher,
				 gchar                 *name,
				 guint                 port)
{
	if (publisher->priv->client == NULL) {
		g_warning ("The avahi MDNS service is not running") ;
		return FALSE;
	}

	publisher->priv->port = port;
	publisher->priv->name = name;
	
	return create_service (publisher);
}

GType
liferea_avahi_publisher_get_type (void) 
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) 
	{
		static const GTypeInfo our_info = 
		{
			sizeof (LifereaAvahiPublisherClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) liferea_avahi_publisher_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (LifereaAvahiPublisher),
			0, /* n_preallocs */
			(GInstanceInitFunc) liferea_avahi_publisher_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "LifereaAvahiPublisher",
					       &our_info, 0);
	}

	return type;
}

static void
liferea_avahi_publisher_class_init (LifereaAvahiPublisherClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = liferea_avahi_publisher_finalize;

	g_type_class_add_private (object_class, sizeof(LifereaAvahiPublisherPrivate));
}

static void
liferea_avahi_publisher_init (LifereaAvahiPublisher *publisher)
{
	publisher->priv = LIFEREA_AVAHI_PUBLISHER_GET_PRIVATE (publisher);
	
	avahi_client_init (publisher);
}

static void
liferea_avahi_publisher_finalize (GObject *object)
{
	LifereaAvahiPublisher *publisher;
	
	publisher = LIFEREA_AVAHI_PUBLISHER (object);
	
	g_return_if_fail (publisher->priv != NULL);
	
	if (publisher->priv->entry_group)
		avahi_entry_group_free (publisher->priv->entry_group);
		
	if (publisher->priv->client)
		avahi_client_free (publisher->priv->client);
		
	if (publisher->priv->poll)
		avahi_glib_poll_free (publisher->priv->poll);
		
	g_free (publisher->priv->name);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

LifereaAvahiPublisher *
liferea_avahi_publisher_new (void)
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

	if (liferea_avahi_publisher) {
		g_object_ref (liferea_avahi_publisher);
	} else {
		liferea_avahi_publisher = g_object_new (LIFEREA_AVAHI_PUBLISHER_TYPE, NULL);
		g_object_add_weak_pointer (liferea_avahi_publisher, (gpointer *) &liferea_avahi_publisher);
	}
	
	return liferea_avahi_publisher;
}
