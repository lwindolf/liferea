/**
 * @file avahi_browser.h  cache synchronization using AVAHI
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

#define LIFEREA_AVAHI_BROWSER_TYPE		(liferea_avahi_browser_get_type ())
#define LIFEREA_AVAHI_BROWSER(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LIFEREA_AVAHI_BROWSER_TYPE, LifereaAvahiBrowser))
#define LIFEREA_AVAHI_BROWSER_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), LIFEREA_AVAHI_BROWSER_TYPE, LifereaAvahiBrowserClass))
#define IS_LIFEREA_AVAHI_BROWSER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIFEREA_AVAHI_BROWSER_TYPE))
#define IS_LIFEREA_AVAHI_BROWSER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), LIFEREA_AVAHI_BROWSER_TYPE))
#define LIFEREA_AVAHI_BROWSER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), LIFEREA_AVAHI_BROWSER_TYPE, LifereaAvahiBrowserClass))

typedef struct LifereaAvahiBrowserPrivate LifereaAvahiBrowserPrivate;

typedef struct {
	GObject		object;
	
	LifereaAvahiBrowserPrivate	*priv;
} LifereaAvahiBrowser;

typedef struct {
	GObjectClass	parent_class;

} LifereaAvahiBrowserClass;

typedef enum
{
	LIFEREA_AVAHI_BROWSER_ERROR_NOT_RUNNING,
	LIFEREA_AVAHI_BROWSER_ERROR_FAILED,
} LifereaAvahiBrowserError;

#define LIFEREA_AVAHI_BROWSER_ERROR liferea_avahi_browser_error_quark ()

GType	liferea_avahi_browser_get_type (void);

/**
 * Set up a new AVAHI browser
 */
LifereaAvahiBrowser	*liferea_avahi_browser_new (void);

/** 
 * Start browser searching for sync peers.
 *
 * @param browser	Liferea service browser object
 */
void liferea_avahi_browser_start (LifereaAvahiBrowser *browser);

/**
 * Stop browsing for sync peers.
 *
 * @param browser	Liferea service browser object
 */
void liferea_avahi_browser_stop (LifereaAvahiBrowser *browser);

G_END_DECLS

#endif
