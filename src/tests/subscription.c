/**
 * @file subscription.c  Test cases for subscription logic
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

#include "conf.h"
#include "subscription.h"

// update interval test cases
typedef struct tc {
        const gchar *name;
	gint    feedInterval;           /* feed specific update interval as configured feed author / webmaster */
        gint    propsInterval;          /* feed specific update interval configured in subscription properties by user */
        gint    globalInterval;         /* global update interval as configured in preferences */
	guint   effectiveInterval;      /* resulting interval */
} *tcPtr;

struct tc tc_intervals[] = {
        {
                .name = "/subscription/limit-to-feed-interval",
                .feedInterval = 30,
                .propsInterval = 60,
                .globalInterval = 120,
                .effectiveInterval = 30
        },
        {
                .name = "/subscription/props-interval-wins1",
                .feedInterval = 65,
                .propsInterval = 60,
                .globalInterval = 120,
                .effectiveInterval = 60
        },
        {
                .name = "/subscription/props-interval-wins2",
                .feedInterval = 65,
                .propsInterval = 60,
                .globalInterval = 20,
                .effectiveInterval = 60
        },
        {
                .name = "/subscription/props-interval-wins3",
                .feedInterval = 0,
                .propsInterval = 60,
                .globalInterval = 120,
                .effectiveInterval = 60
        },
        {
                .name = "/subscription/never-update-by-prefs",
                .feedInterval = 30,
                .propsInterval = 60,
                .globalInterval = -2,
                .effectiveInterval = 0
        },
        {
                .name = "/subscription/never-update-by-props",
                .feedInterval = 30,
                .propsInterval = -2,
                .globalInterval = 120,
                .effectiveInterval = 0
        },
        {
                .name = "/subscription/pref-interval-wins",
                .feedInterval = 0,
                .propsInterval = 240,
                .globalInterval = 120,
                .effectiveInterval = 120
        },
        { NULL } 
};

static void
tc_interval (gconstpointer user_data)
{
	tcPtr		tc = (tcPtr)user_data;
	subscriptionPtr s = subscription_new (NULL, NULL, NULL);

        // Do not use subscription_set_update_interval (s, tc->propsInterval); as it triggers feed list saving
       	s->updateInterval = tc->propsInterval;
        subscription_set_default_update_interval (s, tc->feedInterval);
        conf_set_int_value (DEFAULT_UPDATE_INTERVAL, tc->globalInterval);

        if (subscription_get_effective_update_interval (s) != tc->effectiveInterval)
                g_print ("Effective interval mismatch: actual %d expected %d\n", subscription_get_effective_update_interval (s), tc->effectiveInterval);
	g_assert_true (subscription_get_effective_update_interval (s) == tc->effectiveInterval);

	subscription_free (s);
}

int
test_subscription (int argc, char *argv[])
{
	gint result;

        conf_init ();

	g_test_init (&argc, &argv, NULL);

	for (int i = 0; tc_intervals[i].name != NULL; i++) {
		g_test_add_data_func (tc_intervals[i].name, &tc_intervals[i], &tc_interval);
	}

	result = g_test_run();

	return result;
}
