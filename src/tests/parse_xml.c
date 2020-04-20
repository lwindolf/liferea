/**
 * @file parse_xml.c  Test cases for XML helpers
 *
 * Copyright (C) 2020 Lars Windolf <lars.windolf@gmx.de>
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

#include "xml.h"

typedef struct tcXPath {
	gchar	*xml_string;
	gchar	*xpath_expression;
	gboolean result;
} *tcXPathPtr;

struct tcXPath tc_xpath[][3] = {
	{
		"<?xml version = \"1.0\"?>\n<nothing/>",
		"/html/body",
		TRUE		// counter-intuitive, but a body is automatically added by libxml2!
	},
	{
		"<?xml version = \"1.0\"?>\n<html><body/></html>",
		"/html/body",
		TRUE
	},
	{
		"<!DOCTYPE html>\n<html lang=\"de\"	data-responsive>\n<head><title>Title</title>\n<body>jssj</body></html>",
		"/html/body",
		TRUE
	},
	{
		"<html><head><link rel=\"alternate\" type=\"application/atom+xml\" title=\"Aktuelle News von heise online\" href=\"https://www.heise.de/rss/heise-atom.xml\"></head></html>",
		"/html/head/link[@rel='alternate' and @type='application/atom+xml']/@href",
		TRUE
	},
	NULL
};


static void
tc_xpath_find (gconstpointer user_data)
{
	tcXPathPtr	tc = (tcXPathPtr)user_data;
	xmlDocPtr	doc = xhtml_parse ((gchar *)tc->xml_string, (size_t)strlen (tc->xml_string));
	xmlNodePtr	root;

	g_assert_false (!doc);

	root = xmlDocGetRootElement (doc);
	g_assert_false (!root);

	g_assert_true ((xpath_find (root, g_strdup (tc->xpath_expression)) != NULL) == tc->result);
}

int
main (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	for (int i = 0; tc_xpath[i]->xml_string != NULL; i++) {
		g_test_add_data_func (g_strdup_printf ("/parse_xml/%d", i), &tc_xpath[i], &tc_xpath_find);
	}

	return g_test_run();
}
