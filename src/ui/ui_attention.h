/**
 * @file ui_attention.h  attention profile dialog
 * 
 * Copyright (C) 2008 Lars Lindner <lars.lindner@gmail.com>
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
 
#ifndef _UI_ATTENTION_H
#define _UI_ATTENTION_H

#include <gtk/gtk.h>
#include <glib-object.h>
#include <glib.h>

#include "attention.h"

G_BEGIN_DECLS

#define ATTENTION_PROFILE_DIALOG_TYPE		(attention_profile_dialog_get_type ())
#define ATTENTION_PROFILE_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), ATTENTION_PROFILE_DIALOG_TYPE, AttentionProfileDialog))
#define ATTENTION_PROFILE_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), ATTENTION_PROFILE_DIALOG_TYPE, AttentionProfileDialogClass))
#define IS_ATTENTION_PROFILE_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), ATTENTION_PROFILE_DIALOG_TYPE))
#define IS_ATTENTION_PROFILE_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), ATTENTION_PROFILE_DIALOG_TYPE))

typedef struct AttentionProfileDialog		AttentionProfileDialog;
typedef struct AttentionProfileDialogClass	AttentionProfileDialogClass;
typedef struct AttentionProfileDialogPrivate	AttentionProfileDialogPrivate;

struct AttentionProfileDialog
{
	GObject		parent;
	
	/*< private >*/
	AttentionProfileDialogPrivate	*priv;
};

struct AttentionProfileDialogClass 
{
	GtkObjectClass parent_class;
};

GType attention_profile_dialog_get_type	(void);

/**
 * Creates singleton attention profile dialog.
 *
 * @param ap 	attention profile to present
 *
 * @returns attention profile dialog
 */
AttentionProfileDialog * attention_profile_dialog_open (AttentionProfile *ap);
 
#endif
