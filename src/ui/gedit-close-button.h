/*
 * gedit-close-button.h
 * This file is part of gedit
 *
 * Copyright (C) 2010 - Paolo Borelli
 *
 * gedit is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * gedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __GEDIT_CLOSE_BUTTON_H__
#define __GEDIT_CLOSE_BUTTON_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_CLOSE_BUTTON			(gedit_close_button_get_type ())
#define GEDIT_CLOSE_BUTTON(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_CLOSE_BUTTON, GeditCloseButton))
#define GEDIT_CLOSE_BUTTON_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_CLOSE_BUTTON, GeditCloseButton const))
#define GEDIT_CLOSE_BUTTON_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GEDIT_TYPE_CLOSE_BUTTON, GeditCloseButtonClass))
#define GEDIT_IS_CLOSE_BUTTON(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_CLOSE_BUTTON))
#define GEDIT_IS_CLOSE_BUTTON_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_CLOSE_BUTTON))
#define GEDIT_CLOSE_BUTTON_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GEDIT_TYPE_CLOSE_BUTTON, GeditCloseButtonClass))

typedef struct _GeditCloseButton		GeditCloseButton;
typedef struct _GeditCloseButtonClass		GeditCloseButtonClass;
typedef struct _GeditCloseButtonClassPrivate	GeditCloseButtonClassPrivate;

struct _GeditCloseButton
{
	GtkButton parent;
};

struct _GeditCloseButtonClass
{
	GtkButtonClass parent_class;

	GeditCloseButtonClassPrivate *priv;
};

GType		  gedit_close_button_get_type (void) G_GNUC_CONST;

GtkWidget	 *gedit_close_button_new      (void);

G_END_DECLS

#endif /* __GEDIT_CLOSE_BUTTON_H__ */
/* ex:set ts=8 noet: */
