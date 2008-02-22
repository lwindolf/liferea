/**
 * @file attention.c  attiontion statistics keeping 
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include "attention.h"
#include "db.h"

static void attention_profile_class_init	(AttentionProfileClass *klass);
static void attention_profile_init		(AttentionProfile *ld);

#define ATTENTION_PROFILE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), ATTENTION_PROFILE_TYPE, AttentionProfilePrivate))

struct AttentionProfilePrivate {
	gulong		totalCount;	/**< absolute count of registered item selections */

	GHashTable	*categoryStats;	/**< per category item views (key = upper case category name, value = counter) */
};

static GObjectClass *parent_class = NULL;

GType
attention_profile_get_type (void) 
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) 
	{
		static const GTypeInfo our_info = 
		{
			sizeof (AttentionProfileClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) attention_profile_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (AttentionProfile),
			0, /* n_preallocs */
			(GInstanceInitFunc) attention_profile_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "AttentionProfile",
					       &our_info, 0);
	}

	return type;
}

static void
attention_profile_finalize (GObject *object)
{
	AttentionProfile *ap = ATTENTION_PROFILE (object);
	
	g_hash_table_destroy (ap->priv->categoryStats);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
attention_profile_class_init (AttentionProfileClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = attention_profile_finalize;

	g_type_class_add_private (object_class, sizeof(AttentionProfilePrivate));
}

static void
attention_profile_init (AttentionProfile *ap)
{
	ap->priv = ATTENTION_PROFILE_GET_PRIVATE (ap);	
}

AttentionProfile *
attention_profile_get (void)
{
	AttentionProfile *ap = NULL;
	
	ap = ATTENTION_PROFILE (g_object_new (ATTENTION_PROFILE_TYPE, NULL));
	
	ap->priv->totalCount = 0;
	ap->priv->categoryStats = db_attention_stats_load ();
	
	return ap;
}

void
attention_profile_add_read (AttentionProfile *ap, GSList *categories)
{
	while (categories) {
		gchar *id = g_utf8_casefold (categories->data, -1);
		categoryStatPtr stat = (categoryStatPtr)g_hash_table_lookup (ap->priv->categoryStats, id);
		if (stat) {
			stat->count++;
			g_free (id);
		} else {
			stat = g_new0 (struct categoryStat, 1);
			stat->count = 0;
			stat->id = id;
			stat->name = g_strdup ((gchar *)categories->data);
			g_hash_table_insert (ap->priv->categoryStats, id, (gpointer)stat);
		}
		db_attention_stat_save (stat);
		categories = g_slist_next (categories);
	}
}
