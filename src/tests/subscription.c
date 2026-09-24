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
	gint    feedInterval;		/* feed specific update interval as configured feed author / webmaster */
	gint    propsInterval;		/* feed specific update interval configured in subscription properties by user */
	gint    globalInterval;		/* global update interval as configured in preferences */
	guint   effectiveInterval;	/* resulting interval */
} *tcPtr;

struct tc tc_intervals[] = {
	{
		.name = "/subscription/limit-to-feed-interval",
		.feedInterval = 30,
		.propsInterval = 60,
		.globalInterval = 120,
		.effectiveInterval = 60
	},
	{
		.name = "/subscription/props-interval-wins1",
		.feedInterval = 65,
		.propsInterval = 60,
		.globalInterval = 120,
		.effectiveInterval = 65
	},
	{
		.name = "/subscription/props-interval-wins2",
		.feedInterval = 65,
		.propsInterval = 60,
		.globalInterval = 20,
		.effectiveInterval = 65
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
		.effectiveInterval = 60
	},
	{
		.name = "/subscription/never-update-by-props",
		.feedInterval = 30,
		.propsInterval = -2,
		.globalInterval = 120,
		.effectiveInterval = 0
	},
	{
		.name = "/subscription/never-update-by-both",
		.feedInterval = 30,
		.propsInterval = -2,
		.globalInterval = -2,
		.effectiveInterval = 0
	},
	{
		.name = "/subscription/pref-interval-wins",
		.feedInterval = 0,
		.propsInterval = 240,
		.globalInterval = 120,
		.effectiveInterval = 240
	},
	{ NULL }
};

// update cache age test cases
typedef struct tcCache {
	const gchar	*name;
	gint    	maxage;		/* largest cache age in [min] (i.e. minimum interval between updates) required by feed */
	gint    	nowDiff;	/* difference of lastPoll in [s] to current timestamp */
	gboolean	canUpdate;
	fetchError	error;
} *tcCachePtr;

struct tcCache tc_cache_ages[] = {
	{
		.name = "/subscription/cache-age-none-uninitialized",
		.maxage = -1,
		.nowDiff = 1234567890,	// just a very large diff	
		.canUpdate = TRUE
	},
	{
		.name = "/subscription/cache-age-none",
		.maxage = -1,
		.nowDiff = 1000,	// in the past
		.canUpdate = TRUE
	},
	{
		.name = "/subscription/cache-age-blocks-update",
		.maxage = 1440,		// 1 day
		.nowDiff = 60 * 60 * 2,	// 2 hours later
		.canUpdate = FALSE
	},
	{
		.name = "/subscription/cache-age-allows-update",
		.maxage = 45,			// 45min
		.nowDiff = 60 * 60 * 24,	// 1 day later
		.canUpdate = TRUE
	},
	{
		.name = "/subscription/fetch-error-net-allows-update",
		.maxage = 45,			// 45min
		.nowDiff = 5,			// 5min later
		.error = FETCH_ERROR_NET,
		.canUpdate = TRUE
	},
	{
		.name = "/subscription/fetch-error-auth-allows-update",
		.maxage = 45,			// 45min
		.nowDiff = 5,			// 5min later
		.error = FETCH_ERROR_AUTH,
		.canUpdate = TRUE
	},
	{
		.name = "/subscription/fetch-error-xml-disallows-update",
		.maxage = 45,			// 45min
		.nowDiff = 5,			// 5min later
		.error = FETCH_ERROR_XML,
		.canUpdate = FALSE
	},
	{
		.name = "/subscription/fetch-error-discovery-disallows-update",
		.maxage = 45,			// 45min
		.nowDiff = 5,			// 5min later
		.error = FETCH_ERROR_DISCOVER,
		.canUpdate = FALSE
	},
	{ NULL }
};

typedef struct tcMinInterval {
	const gchar *name;
	gint maxAgeMinutes;
	gint ttl;
	gint synFrequency;
	gint synPeriod;
	gint expected;
} *tcMinIntervalPtr;

static struct tcMinInterval tc_min_intervals[] = {
	{
		.name = "/update-state/min-interval/none-set",
		.maxAgeMinutes = 0,
		.ttl = 0,
		.synFrequency = 0,
		.synPeriod = 0,
		.expected = -1
	},
	{
		.name = "/update-state/min-interval/max-age-only",
		.maxAgeMinutes = 45,
		.ttl = 0,
		.synFrequency = 0,
		.synPeriod = 0,
		.expected = 45
	},
	{
		.name = "/update-state/min-interval/ttl-wins",
		.maxAgeMinutes = 30,
		.ttl = 90,
		.synFrequency = 0,
		.synPeriod = 0,
		.expected = 90
	},
	{
		.name = "/update-state/min-interval/syndication-wins",
		.maxAgeMinutes = 30,
		.ttl = 0,
		.synFrequency = 2,
		.synPeriod = 60,
		.expected = 120
	},
	{
		.name = "/update-state/min-interval/largest-of-all-wins",
		.maxAgeMinutes = 120,
		.ttl = 90,
		.synFrequency = 3,
		.synPeriod = 60,
		.expected = 180
	},
	{ NULL }
};

typedef struct tcCanBeUpdated {
	const gchar	*name;
	gboolean	discontinued;	/* TRUE if HTTP 410 */
	gboolean	alreadyRunning;	/* TRUE if update is pending */
	const gchar	*source;	/* source URI of the feed */
	gboolean	canUpdate;	/* result */
} *tcCanBeUpdatedPtr;

static struct tcCanBeUpdated tc_can_be_updated[] = {
	{
		.name = "/subscription/can-be-updated-yes",
		.source = "http://example.com/feed",	
		.canUpdate = TRUE
	},
	{
		.name = "/subscription/can-be-updated-no-discontinued",
		.discontinued = TRUE,
		.source = "http://example.com/feed",	
		.canUpdate = FALSE
	},
	{
		.name = "/subscription/can-be-updated-no-already-running",
		.alreadyRunning = TRUE,
		.source = "http://example.com/feed",	
		.canUpdate = FALSE
	},
	{
		.name = "/subscription/can-be-updated-command",
		.source = "| curl http://example.com/feed",	
		.canUpdate = TRUE
	},
	{
		.name = "/subscription/can-be-updated-file",
		.source = "/home/jane/myrss.xml",	
		.canUpdate = TRUE
	},
	{
		.name = "/subscription/can-be-updated-no-invalid-uri",
		.source = "://exa|mple.com/feed",
		.canUpdate = FALSE
	},
	{
		.name = "/subscription/can-be-updated-no-invalid-uri2",
		.source = "abc",
		.canUpdate = FALSE
	},
	{
		.name = "/subscription/can-be-updated-no-empty-uri",
		.source = "",
		.canUpdate = FALSE
	},
	{
		.name = "/subscription/can-be-updated-short-domain",
		.source = "abc.com",
		.canUpdate = FALSE	// FIXME: this is a regression, should work and default to https://
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
	update_state_set_cache_maxage (s->updateState, tc->feedInterval);
	conf_set_int_value (DEFAULT_UPDATE_INTERVAL, tc->globalInterval);

	if (subscription_get_effective_update_interval (s) != tc->effectiveInterval)
		g_print ("Effective interval mismatch: actual %d expected %d\n", subscription_get_effective_update_interval (s), tc->effectiveInterval);
	g_assert_true (subscription_get_effective_update_interval (s) == tc->effectiveInterval);

	subscription_free (s);
}

static void
tc_cache_age (gconstpointer user_data)
{
	tcCachePtr	tc = (tcCachePtr)user_data;
	subscriptionPtr s = subscription_new (NULL, NULL, NULL);

	s->error = tc->error;
	s->updateState->lastPoll = g_get_real_time () - (gint64)tc->nowDiff * G_USEC_PER_SEC;
	update_state_set_cache_maxage (s->updateState, tc->maxage);

	g_assert_true (subscription_can_update_now (s) == tc->canUpdate);

	subscription_free (s);
}

static void
tc_min_interval (gconstpointer user_data)
{
	tcMinIntervalPtr tc = (tcMinIntervalPtr)user_data;
	updateStatePtr state = update_state_new ();

	update_state_set_cache_maxage (state, tc->maxAgeMinutes);
	update_state_set_ttl (state, tc->ttl);
	update_state_set_syn_frequency (state, tc->synFrequency);
	update_state_set_syn_period (state, tc->synPeriod);

	g_assert_true (update_state_get_min_interval (state) == tc->expected);

	update_state_free (state);
}

static void
tc_can_be_updated_func (gconstpointer user_data)
{
	tcCanBeUpdatedPtr	tc = (tcCanBeUpdatedPtr)user_data;
	subscriptionPtr 	s = subscription_new (NULL, NULL, NULL);

	s->updateJob = tc->alreadyRunning ? (UpdateJob *)1 : NULL;	// set invalid pointer "1", do not actually create a job (as it would crash due to missing job queue)
	s->source = g_strdup (tc->source);
	s->discontinued = tc->discontinued;

	g_assert_true (subscription_can_be_updated (s, 0) == tc->canUpdate);

	s->updateJob = NULL;	// avoid freeing bogus pointer
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
	for (int i = 0; tc_cache_ages[i].name != NULL; i++) {
		g_test_add_data_func (tc_cache_ages[i].name, &tc_cache_ages[i], &tc_cache_age);
	}
	for (int i = 0; tc_min_intervals[i].name != NULL; i++) {
		g_test_add_data_func (tc_min_intervals[i].name, &tc_min_intervals[i], &tc_min_interval);
	}
	for (int i = 0; tc_can_be_updated[i].name != NULL; i++) {
		g_test_add_data_func (tc_can_be_updated[i].name, &tc_can_be_updated[i], &tc_can_be_updated_func);
	}

	result = g_test_run();

	return result;
}
