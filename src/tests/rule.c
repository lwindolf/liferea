/**
 * @file rule.c  Test cases for search folder rules
 *
 * Copyright (C) 2026 Lars Windolf <lars.windolf@gmx.de>
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

#include "enclosure.h"
#include "item.h"
#include "metadata.h"
#include "node.h"
#include "rule.h"
#include "subscription.h"

static gboolean
rule_eval (const gchar *ruleId, const gchar *value, itemPtr item)
{
	rulePtr rule = rule_new (ruleId, value ? value : "", TRUE);
	ruleCheckFunc check;
	gboolean result;

	g_assert_nonnull (rule);
	check = (ruleCheckFunc)rule->ruleInfo->checkFunc;
	result = check (rule, item);
	rule_free (rule);

	return result;
}

static itemPtr
make_item (void)
{
	itemPtr item = item_new ();

	item_set_title (item, "Mixed Case Title");
	item_set_description (item, "Body with KEYWORD and details");
	item->readStatus = FALSE;
	item->flagStatus = TRUE;
	item->hasEnclosure = TRUE;

	item->metadata = metadata_list_append (item->metadata, "author", "Jane Writer");
	item->metadata = metadata_list_append (item->metadata, "creator", "John Creator");
	item->metadata = metadata_list_append (item->metadata, "category", "Tech");

	return item;
}

static void
add_enclosure_metadata (itemPtr item, const gchar *mime)
{
	enclosurePtr enclosure = enclosure_new ("https://example.com/media", mime, 123, -1, -1);
	g_autofree gchar *enclosureStr = enclosure_to_string (enclosure);

	item->metadata = metadata_list_append (item->metadata, "enclosure", enclosureStr);
	enclosure_free (enclosure);
}

static void
tc_item_rules (void)
{
	itemPtr item = make_item ();

	g_assert_true (rule_eval ("exact", "keyword", item));
	g_assert_true (rule_eval ("exact_title", "mixed", item));
	g_assert_true (rule_eval ("exact_desc", "details", item));
	g_assert_false (rule_eval ("exact_desc", "notfound", item));

	g_object_unref (item);
}

static void
tc_author_rules (void)
{
	itemPtr item = make_item ();

	g_assert_true (rule_eval ("exact_author", "writer", item));
	g_assert_true (rule_eval ("exact_author", "creator", item));
	g_assert_false (rule_eval ("exact_author", "nobody", item));

	g_object_unref (item);
}

static void
tc_status_rules (void)
{
	itemPtr item = make_item ();

	g_assert_true (rule_eval ("unread", "", item));
	g_assert_true (rule_eval ("flagged", "", item));
	g_assert_true (rule_eval ("enclosure", "", item));

	item->readStatus = TRUE;
	item->flagStatus = FALSE;
	item->hasEnclosure = FALSE;

	g_assert_false (rule_eval ("unread", "", item));
	g_assert_false (rule_eval ("flagged", "", item));
	g_assert_false (rule_eval ("enclosure", "", item));

	g_object_unref (item);
}

static void
tc_category_rule (void)
{
	itemPtr item = make_item ();

	g_assert_true (rule_eval ("category", "Tech", item));
	g_assert_false (rule_eval ("category", "News", item));

	g_object_unref (item);
}

static void
tc_podcast_rule (void)
{
	itemPtr item = make_item ();

	add_enclosure_metadata (item, "audio/ogg");
	g_assert_true (rule_eval ("podcast", "", item));

	g_object_unref (item);

	item = make_item ();
	add_enclosure_metadata (item, "video/mp4");
	g_assert_false (rule_eval ("podcast", "", item));

	g_object_unref (item);
}

static void
tc_feed_rules (void)
{
	itemPtr item = make_item ();
	Node *folder = node_new ("folder");
	Node *feed = node_new ("feed");
	subscriptionPtr subscription = subscription_new (NULL, NULL, NULL);

	/* Avoid subscription_set_source() because it schedules feedlist saves. */
	subscription->source = g_strdup ("https://example.com/feed.xml");
	subscription->origSource = g_strdup ("https://example.com/feed.xml");

	node_set_title (folder, "Tech Folder");
	node_set_parent (feed, folder, -1);
	node_set_title (feed, "Planet Example");
	node_set_subscription (feed, subscription);

	item->parentNodeId = g_strdup (feed->id);

	g_assert_true (rule_eval ("feed_title", "planet", item));
	g_assert_true (rule_eval ("feed_source", "example.com", item));
	g_assert_true (rule_eval ("parent_folder", "tech", item));

	g_assert_false (rule_eval ("feed_title", "different", item));
	g_assert_false (rule_eval ("feed_source", "other-domain", item));
	g_assert_false (rule_eval ("parent_folder", "other-folder", item));

	/* Break parent-child relation before unref to satisfy node finalizers. */
	folder->children = g_slist_remove (folder->children, feed);
	feed->parent = NULL;

	g_object_unref (feed);
	g_object_unref (folder);
	g_object_unref (item);
}

int
test_rule (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/rule/item/basic", &tc_item_rules);
	g_test_add_func ("/rule/item/author", &tc_author_rules);
	g_test_add_func ("/rule/item/status", &tc_status_rules);
	g_test_add_func ("/rule/item/category", &tc_category_rule);
	g_test_add_func ("/rule/item/podcast", &tc_podcast_rule);
	g_test_add_func ("/rule/feed/rules", &tc_feed_rules);
        

	return g_test_run ();
}
