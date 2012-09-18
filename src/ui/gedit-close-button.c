/*
 * gedit-close-button.c
 * This file is part of gedit
 *
 * Copyright (C) 2010 - Paolo Borelli
 * Copyright (C) 2011 - Ignacio Casal Quinteiro
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

#include "gedit-close-button.h"

struct _GeditCloseButtonClassPrivate
{
	GtkCssProvider *css;
};

G_DEFINE_TYPE_WITH_CODE (GeditCloseButton, gedit_close_button, GTK_TYPE_BUTTON,
                         g_type_add_class_private (g_define_type_id, sizeof (GeditCloseButtonClassPrivate)))

static void
gedit_close_button_class_init (GeditCloseButtonClass *klass)
{
	static const gchar button_style[] =
		"* {\n"
		  "-GtkButton-default-border : 0;\n"
		  "-GtkButton-default-outside-border : 0;\n"
		  "-GtkButton-inner-border: 0;\n"
		  "-GtkWidget-focus-line-width : 0;\n"
		  "-GtkWidget-focus-padding : 0;\n"
		  "padding: 0;\n"
		"}";

	klass->priv = G_TYPE_CLASS_GET_PRIVATE (klass, GEDIT_TYPE_CLOSE_BUTTON, GeditCloseButtonClassPrivate);

	klass->priv->css = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (klass->priv->css, button_style, -1, NULL);
}

static void
gedit_close_button_init (GeditCloseButton *button)
{
	GtkStyleContext *context;
	GtkWidget *image;
	GIcon *icon;

	icon = g_themed_icon_new_with_default_fallbacks ("window-close-symbolic");
	image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
	gtk_widget_show (image);
	g_object_unref (icon);

	gtk_container_add (GTK_CONTAINER (button), image);

	/* make it small */
	context = gtk_widget_get_style_context (GTK_WIDGET (button));
	gtk_style_context_add_provider (context,
	                                GTK_STYLE_PROVIDER (GEDIT_CLOSE_BUTTON_GET_CLASS (button)->priv->css),
	                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

GtkWidget *
gedit_close_button_new ()
{
	return GTK_WIDGET (g_object_new (GEDIT_TYPE_CLOSE_BUTTON,
	                                 "relief", GTK_RELIEF_NONE,
	                                 "focus-on-click", FALSE,
	                                 NULL));
}

/* ex:set ts=8 noet: */
