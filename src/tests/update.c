/**
 * @file update.c  Test cases for update processing
 * 
 * Copyright (C) 2024 Lars Windolf <lars.windolf@gmx.de>
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

#include "debug.h"
#include "conf.h"
#include "net.h"
#include "net_monitor.h"
#include "update.h"

typedef struct tc {
	gchar	        *url;
        gchar           *filtercmd;
        gboolean        emptyBody;      // TRUE if empty body expected
        gboolean        filterErrors;   // TRUE if filter errors expected
        int             httpStatus;     // expected HTTP status (or 0 to disable check)
        gchar           *testString;    // expected string in body
        UpdateJob       *job;
} *tcPtr;
struct tc tc_url                = { .url = "https://github.com/", .testString = "github" };
//struct tc tc_dns_fail           = { .url = "https://doesnotexist.local", .emptyBody = TRUE };   // FIXME: cannot be tested due to missing error callback causing memory leak
struct tc tc_port_fail          = { .url = "http://localhost:6666", .emptyBody = TRUE };
struct tc tc_proto_fail         = { .url = "htps://localhost", .emptyBody = TRUE };
struct tc tc_local_file         = { .url = "file:///etc/hosts", .testString = "localhost" };
struct tc tc_http_301           = { .url = "https://lzone.de/http/301", .httpStatus = 200 };     // We expect HTTP 200 because the networking code is supposed to do the redirect
struct tc tc_http_308           = { .url = "https://lzone.de/http/308", .httpStatus = 200 };     // We expect HTTP 200 because the networking code is supposed to do the redirect
struct tc tc_http_404           = { .url = "https://lzone.de/http/404", .httpStatus = 404 };
struct tc tc_http_410           = { .url = "https://lzone.de/http/410", .httpStatus = 410 };
struct tc tc_script_ok          = { .url = "|./test-feed-script.sh",    .httpStatus = 200, .testString = "TEST STRING" }; // Run an existing script
struct tc tc_script_fail        = { .url = "|./test-feed-script-fail.sh",  .emptyBody = TRUE, .httpStatus = 500 };        // Run an existing script that fails
struct tc tc_script_missing     = { .url = "|./test-missing-script.sh",    .emptyBody = TRUE, .httpStatus = 500 };        // Run a missing script
struct tc tc_script_notallowed  = { .url = "|./test-notallowed-script.sh", .emptyBody = TRUE, .httpStatus = 403 };        // Run a script where it is not allowed
struct tc tc_filter_ok          = { .url = "https://lzone.de/", .filtercmd = "./test-feed-script.sh", .httpStatus = 200, .testString = "TEST STRING" }; // Run a filter that works
struct tc tc_filter_fail        = { .url = "https://lzone.de/", .filtercmd = "./test-feed-script-fail.sh", .emptyBody = TRUE, .filterErrors = TRUE };      // Run a filter that fails
struct tc tc_filter_missing     = { .url = "https://lzone.de/", .filtercmd = "./test-feed-script-missing.sh", .emptyBody = TRUE, .filterErrors = TRUE };   // Run a filter that is missing
// FIXME: XSL filter test case

// result is global because we run async and exit the main loop upon test end
static int result = 1;
static GMainLoop *loop = NULL;

static void
tc_update_cb (const UpdateResult * const result, gpointer user_data, updateFlags flags)
{
        tcPtr   tc = (tcPtr)user_data;

        g_print("update result %s (HTTP %d, size=%ld, body=%p, state=%d)\n", result->source, result->httpstatus, result->size, result->data, tc->job->state);
}

static void
tc_update_job_new (gpointer user_data)
{
	tcPtr			tc = (tcPtr)user_data;
        UpdateRequest	        *request;

	request = update_request_new (tc->url, NULL, NULL);
        request->filtercmd = g_strdup(tc->filtercmd);

        // script handling
        if (g_str_has_prefix (tc->url, "|") && !strstr (tc->url, "notallowed"))
                request->allowCommands = TRUE;

        tc->job = update_job_new (tc, request, tc_update_cb, user_data, 0);
	g_assert (tc->job != NULL);
        g_object_ref (tc->job);
}

static void
tc_update_job_check_result (gconstpointer user_data) {
        tcPtr			tc = (tcPtr)user_data;
        UpdateJob               *job = tc->job;
        UpdateResult            *result = job->result;
        gint state              = update_job_get_state (job);

        if (state != JOB_STATE_FINISHED)
                g_error ("Unexpected state %d != expected %d", state, JOB_STATE_FINISHED);
        if ((result->size == 0) && !tc->emptyBody)
                g_error ("Result data is unexpectedly empty!");
        if (tc->testString && !strstr (result->data, tc->testString))
                g_error ("Test string '%s' not found in result data!", tc->testString);
        if (tc->httpStatus && (tc->httpStatus != result->httpstatus))
                g_error ("HTTP status %d != expected %d", result->httpstatus, tc->httpStatus);
        if (tc->filterErrors && !result->filterErrors)
                g_error ("Expected filter errors not found!");

        g_object_unref (job);
}

// step 2: after some time to start test requests and check their results
gboolean
check_updates (gpointer user_data)
{
        g_print("check updates\n") ;

        g_test_add_data_func ("/update_job/github.com",	        &tc_url,	&tc_update_job_check_result);
        //g_test_add_data_func ("/update_job/dns-fail",	        &tc_dns_fail,	&tc_update_job_check_result);
        g_test_add_data_func ("/update_job/port-fail",	        &tc_port_fail,	&tc_update_job_check_result);
        g_test_add_data_func ("/update_job/proto-fail", 	&tc_proto_fail,	&tc_update_job_check_result);
        g_test_add_data_func ("/update_job/local-file",  	&tc_local_file,	&tc_update_job_check_result);
        g_test_add_data_func ("/update_job/http-301",     	&tc_http_301,	&tc_update_job_check_result);
        g_test_add_data_func ("/update_job/http-308",     	&tc_http_308,	&tc_update_job_check_result);
        g_test_add_data_func ("/update_job/http-404",     	&tc_http_404,	&tc_update_job_check_result);
        g_test_add_data_func ("/update_job/http-410",     	&tc_http_410,	&tc_update_job_check_result);
        g_test_add_data_func ("/update_job/script-ok",     	&tc_script_ok,	        &tc_update_job_check_result);
        g_test_add_data_func ("/update_job/script-fail",     	&tc_script_fail,	&tc_update_job_check_result);
        g_test_add_data_func ("/update_job/script-missing",     &tc_script_missing,	&tc_update_job_check_result);
        g_test_add_data_func ("/update_job/script-notallowed",  &tc_script_notallowed,  &tc_update_job_check_result);
        g_test_add_data_func ("/update_job/filter-ok",          &tc_filter_ok,          &tc_update_job_check_result);
        g_test_add_data_func ("/update_job/filter-fail",        &tc_filter_fail,        &tc_update_job_check_result);
        g_test_add_data_func ("/update_job/filter-missing",     &tc_filter_missing,     &tc_update_job_check_result);

        result = g_test_run();

        g_main_loop_quit (loop);

        return FALSE;
}

// step 1: setup update jobs and see if they can be started
gboolean
start_updates (gpointer user_data)
{
        guint max, nr = 0;

        tc_update_job_new (&tc_url);
        //tc_update_job_new (&tc_dns_fail);
        tc_update_job_new (&tc_port_fail);
        tc_update_job_new (&tc_proto_fail);
        tc_update_job_new (&tc_local_file);
        tc_update_job_new (&tc_http_301);
        tc_update_job_new (&tc_http_308);
        tc_update_job_new (&tc_http_404);
        tc_update_job_new (&tc_http_410);
        tc_update_job_new (&tc_script_ok);
        tc_update_job_new (&tc_script_fail);
        tc_update_job_new (&tc_script_missing);
        tc_update_job_new (&tc_script_notallowed);
        tc_update_job_new (&tc_filter_ok);
        tc_update_job_new (&tc_filter_fail);
        tc_update_job_new (&tc_filter_missing);
        update_job_queue_get_count (&nr, &max);
        g_assert (0 != nr);
        g_assert (10 < max);

        return FALSE;
}

int
main (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	if (argv[1] && g_str_equal (argv[1], "--debug"))
		debug_set_flags (DEBUG_UPDATE | DEBUG_NET | DEBUG_CONF);

        conf_init ();
	update_init ();
        network_init ();

        g_timeout_add_seconds (1, start_updates, NULL);
        g_timeout_add_seconds (15, check_updates, NULL);
        loop = g_main_loop_new (NULL, FALSE);
        g_main_loop_run (loop);

        // network_deinit ();      // causes invalid point on cancellable free :-(
        update_deinit ();
        conf_deinit ();

	return result;
}
