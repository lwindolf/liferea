/**
 * @file liferea_web_view.h  Webkit2 widget for Liferea
 *
 * Copyright (C) 2016 Leiaz <leiaz@mailbox.org>
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

#ifndef _LIFEREA_WEB_VIEW_H
#define _LIFEREA_WEB_VIEW_H

#include <webkit2/webkit2.h>

#define LIFEREA_TYPE_WEB_VIEW liferea_web_view_get_type ()

#define LIFEREA_WEB_VIEW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), LIFEREA_TYPE_WEB_VIEW, LifereaWebView))
#define IS_LIFEREA_WEB_VIEW(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIFEREA_TYPE_WEB_VIEW))
#define LIFEREA_WEB_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), LIFEREA_TYPE_WEB_VIEW, LifereaWebViewClass))
#define IS_LIFEREA_WEB_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), LIFEREA_TYPE_WEB_VIEW))
#define LIFEREA_WEB_VIEW_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), LIFEREA_TYPE_WEB_VIEW, LifereaWebViewClass))

typedef struct _LifereaWebView LifereaWebView;
typedef struct _LifereaWebViewClass LifereaWebViewClass;

GType liferea_web_view_get_type (void);

LifereaWebView *
liferea_web_view_new (void);

void
liferea_web_view_set_dbus_connection (LifereaWebView *self, GDBusConnection *connection);

void
liferea_web_view_scroll_pagedown (LifereaWebView *self);
#endif
