/**
 * @file attribs.c This file defines the attributes that items and
 * feeds can contain
 *
 * Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
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


#include <glib.h>
#include "support.h"
#include "metadata.h"
#include "common.h"
#include "htmlview.h"

typedef enum {
	POS_HEAD,
	POS_BODY,
	POS_FOOT
} output_position;

struct str_attrib {
	output_position pos;
	gchar *prompt;
};

static gpointer str_parser(gpointer prevData, const gchar *str, gpointer user_data) {
	g_free(prevData);
	return g_strdup(str);
}

static void str_render(gpointer data, struct displayset *displayset, gpointer user_data) {
	struct str_attrib *props = (struct str_attrib*)user_data;
	gchar *str;
	printf("Adding %s to %d\n", str, props->pos);
	printf("foot is %d\n", POS_FOOT);
	switch (props->pos) {
	case POS_HEAD:
		str = g_strdup_printf(HEAD_LINE, props->prompt, (gchar*)data);;
		addToHTMLBufferFast(&(displayset->head), str);
		g_free(str);
		break;
	case POS_BODY:
		addToHTMLBufferFast(&(displayset->body), str);
		break;
	case POS_FOOT:
		FEED_FOOT_WRITE(displayset->foot, props->prompt, (gchar*)data);
		break;
	}

}

static void str_free(gpointer data, gpointer user_data) {
	g_free(data);
}

#define REGISTER_STR_ATTRIB(position, strid, promptStr) do { \
 struct str_attrib *props = g_new(struct str_attrib, 1); \
 props->pos = (position); \
 props->prompt = _(promptStr); \
 metadata_register(strid, str_parser, str_render, str_free, props); \
} while (0);

void attribs_init() {
	REGISTER_STR_ATTRIB(POS_FOOT, "author", "author");
	REGISTER_STR_ATTRIB(POS_FOOT, "contributor", "contributors");
}
