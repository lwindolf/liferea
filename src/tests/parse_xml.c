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
	gchar	*name;
	gchar	*xml_string;
	gchar	*xpath_expression;
	gboolean result;
} *tcXPathPtr;

struct tcXPath tc_xpath[] = {
	{
		"/parse_xml/xpath_find_empty_doc",
		"<?xml version = \"1.0\"?>\n<nothing/>",
		"/html/body",
		TRUE		// counter-intuitive, but a body is automatically added by libxml2!
	},
	{
		"/parse_xml/xpath_find_empty_doc2",
		"<?xml version = \"1.0\"?>\n<html><body/></html>",
		"/html/body",
		TRUE
	},
	{
		"/parse_xml/xpath_find_real_doc",
		"<!DOCTYPE html>\n<html lang=\"de\"	data-responsive>\n<head><title>Title</title>\n<body>jssj</body></html>",
		"/html/body",
		TRUE
	},
	{
		"/parse_xml/xpath_find_atom_feed",
		"<html><head><link rel=\"alternate\" type=\"application/atom+xml\" title=\"Aktuelle News von heise online\" href=\"https://www.heise.de/rss/heise-atom.xml\"></head></html>",
		"/html/head/link[@rel='alternate' and @type='application/atom+xml']/@href",
		TRUE
	},
	NULL
};

typedef struct tcStripper {
	gchar	*name;
	gchar	*xml_string;
	gchar	*xpath_expression;	// expression that must not be found
} *tcStripperPtr;

struct tcStripper tc_strippers[] = {
	{
		.name = "/xhtml_strip/onload",
		.xml_string = "<div onload='alert(\"Hallo\");'></div>< div onload=\"alert('Hallo');\"></div>",
		.xpath_expression = "//div/@onload"
	},
	{
		"/xhtml_strip/meta",
		"<head><meta http-equiv='Refresh' content='5' /></head>",
		"//meta"
	},
	{
		"/xhtml_strip/wbr",
		"<div><wbr/></div>",
		"//wbr"
	},
	{
		"/xhtml_strip/extra_body",
		"<div><body>abc</body></div>",
		"//div/body"
	},
	{
		"/xhtml_strip/script",
		"<div><script type='text/javascript'>Some script\n</ script><script src='somewhere'/><script>alert('Hallo');</script></div>",
		"//script"
	},
	{
		"/xhtml_strip/iframe",
		"<div><iframe>Some iframe\n</iframe><iframe/><iframe>another iframe</iframe></div>",
		"//iframe"
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

	g_assert_true ((xpath_find (root, tc->xpath_expression) != NULL) == tc->result);

	xmlFreeDoc (doc);
}

static void
tc_strip (gconstpointer user_data)
{
	tcStripperPtr	tc = (tcStripperPtr)user_data;
	xmlDocPtr	doc;
	xmlNodePtr	root;
	gchar		*stripped;

	stripped = xhtml_strip_dhtml ((const gchar *)tc->xml_string);
	g_assert_true (stripped != NULL);

	doc = xhtml_parse (stripped, (size_t)strlen (stripped));
	g_free (stripped);

	g_assert_false (!doc);
	root = xmlDocGetRootElement (doc);
	g_assert_false (!root);

	g_assert_true (xpath_find (root, tc->xpath_expression) == NULL);

	xmlFreeDoc (doc);
}

int
main (int argc, char *argv[])
{
	gint result;

	xml_init ();

	g_test_init (&argc, &argv, NULL);

	for (int i = 0; tc_xpath[i].name != NULL; i++) {
		g_test_add_data_func (tc_xpath[i].name, &tc_xpath[i], &tc_xpath_find);
	}
	for (int i = 0; tc_strippers[i].name != NULL; i++) {
		g_test_add_data_func (tc_strippers[i].name, &tc_strippers[i], &tc_strip);
	}

	result = g_test_run();

	xml_deinit ();

	return result;
}
