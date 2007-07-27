/**
 * @file avahi_publisher.h  cache synchronization using AVAHI
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
#ifndef _AVAHI_H
#define _AVAHI_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define LIFEREA_AVAHI_PUBLISHER_TYPE		(liferea_avahi_publisher_get_type ())
#define LIFEREA_AVAHI_PUBLISHER(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LIFEREA_AVAHI_PUBLISHER_TYPE, LifereaAvahiPublisher))
#define LIFEREA_AVAHI_PUBLISHER_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), LIFEREA_AVAHI_PUBLISHER_TYPE, LifereaAvahiPublisherClass))
#define IS_LIFEREA_AVAHI_PUBLISHER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIFEREA_AVAHI_PUBLISHER_TYPE))
#define IS_LIFEREA_AVAHI_PUBLISHER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), LIFEREA_AVAHI_PUBLISHER_TYPE))
#define LIFEREA_AVAHI_PUBLISHER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), LIFEREA_AVAHI_PUBLISHER_TYPE, LifereaAvahiPublisherClass))

typedef struct LifereaAvahiPublisherPrivate LifereaAvahiPublisherPrivate;

typedef struct {
	GObject		object;
	
	LifereaAvahiPublisherPrivate	*priv;
} LifereaAvahiPublisher;

typedef struct {
	GObjectClass	parent_class;

} LifereaAvahiPublisherClass;

typedef enum
{
	LIFEREA_AVAHI_PUBLISHER_ERROR_NOT_RUNNING,
	LIFEREA_AVAHI_PUBLISHER_ERROR_FAILED,
} LifereaAvahiPublisherError;

#define LIFEREA_AVAHI_PUBLISHER_ERROR liferea_avahi_publisher_error_quark ()

GType	liferea_avahi_publisher_get_type (void);

/**
 * Set up a new AVAHI publisher 
 */
LifereaAvahiPublisher	*liferea_avahi_publisher_new (void);

/** 
 * Register sync service 
 *
 * @param publisher	Liferea service publisher object
 * @param name		service name
 * @param port		server port
 */
gboolean liferea_avahi_publisher_publish (LifereaAvahiPublisher *publisher, gchar *name, guint port);

G_END_DECLS

#endif
