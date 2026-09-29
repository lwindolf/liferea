#include <glib.h>

#include "itemlist.h"

static void
tc_itemlist_remove_null (void)
{
	ItemList *list = g_object_new (ITEMLIST_TYPE, NULL);

	g_assert_nonnull (list);
	itemlist_remove_item (NULL);
	g_object_unref (list);
}

int
test_itemlist (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/itemlist/remove_null", tc_itemlist_remove_null);
	return g_test_run ();
}
