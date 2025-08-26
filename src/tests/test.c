#include <glib.h>

extern int test_parse_date (int argc, char *argv[]);
extern int test_parse_html (int argc, char *argv[]);
extern int test_parse_uri (int argc, char *argv[]);
extern int test_parse_xml (int argc, char *argv[]);
extern int test_parse_rss (int argc, char *argv[]);
extern int test_social (int argc, char *argv[]);
extern int test_update (int argc, char *argv[]);

int run_test (int argc, char *argv[]) {
        // We expect the test name to be in argv[2]
        if (argv[2]) {
                if (g_str_equal (argv[2], "parse_date"))
                        return test_parse_date (argc, argv);
                if (g_str_equal (argv[2], "parse_html"))
                        return test_parse_html (argc, argv);
                if (g_str_equal (argv[2], "parse_rss"))
                        return test_parse_rss (argc, argv);
                if (g_str_equal (argv[2], "parse_uri"))
                        return test_parse_uri (argc, argv);
                if (g_str_equal (argv[2], "parse_xml"))
                        return test_parse_xml (argc, argv);
                if (g_str_equal (argv[2], "social"))
                        return test_social (argc, argv);
                if (g_str_equal (argv[2], "update"))
                        return test_update (argc, argv);
        }

        g_printerr ("Unknown test '%s'\n", argv[2]);
	return 1;
}