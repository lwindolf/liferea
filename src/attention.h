/**
 * @file attention.h  attiontion statistics keeping
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

#ifndef _ATTENTION_H
#define _ATTENTION_H

/*
 * To track user reading preferences Liferea implements simple
 * category based statistics. For each item read each the statistic
 * counters of all categories the item belongs to is increased.
 * To relative significance of each category is determined by
 * also keeping an absolut item view counter.
 */

#include <glib-object.h>
#include <glib.h>

/** category statistics description */
typedef struct categoryStat {
	gulong		count;		/**< absolute count of items read with this category */
	gchar		*id;		/**< category id */
	gchar		*name;		/**< category name */
} *categoryStatPtr;

G_BEGIN_DECLS

#define ATTENTION_PROFILE_TYPE			(attention_profile_get_type ())
#define ATTENTION_PROFILE(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), ATTENTION_PROFILE_TYPE, AttentionProfile))
#define ATTENTION_PROFILE_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), ATTENTION_PROFILE_TYPE, AttentionProfileClass))
#define IS_ATTENTION_PROFILE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), ATTENTION_PROFILE_TYPE))
#define IS_ATTENTION_PROFILE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), ATTENTION_PROFILE_TYPE))

typedef struct AttentionProfile		AttentionProfile;
typedef struct AttentionProfileClass	AttentionProfileClass;
typedef struct AttentionProfilePrivate	AttentionProfilePrivate;

struct AttentionProfile
{
	GObject		parent;
	
	/*< private >*/
	AttentionProfilePrivate	*priv;
};

struct AttentionProfileClass 
{
	GObjectClass parent_class;
};

GType attention_profile_get_type	(void);

/**
 * Returns the singleton attention profile.
 *
 * @returns the attention profile
 */
AttentionProfile * attention_profile_get (void);

/**
 * Method to be called when an item is displayed to update the
 * items categories reading statistics. This method should not
 * be called when just marking items read. For mass-item viewing
 * it should also not be called for performance reasons.
 *
 * @param ap		the attention profile
 * @param categories	list of category names
 */
void attention_profile_add_read (AttentionProfile *ap, GSList *categories);

/**
 * Returns a list of all currently known category statistics.
 *
 * @param ap		the attention profile
 *
 * @returns list of category statistics
 */
GSList * attention_profile_get_categories (AttentionProfile *ap);

G_END_DECLS

#endif
