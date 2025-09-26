/**
 * @file update.c  generic update request and state processing
 *
 * Copyright (C) 2003-2024 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2009 Adrian Bunk <bunk@users.sourceforge.net>
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

#include "update.h"

/* update state interface */

updateStatePtr
update_state_new (void)
{
	return g_new0 (struct updateState, 1);
}

glong
update_state_get_lastmodified (updateStatePtr state)
{
	return state->lastModified;
}

void
update_state_set_lastmodified (updateStatePtr state, glong lastModified)
{
	state->lastModified = lastModified;
}

const gchar *
update_state_get_etag (updateStatePtr state)
{
	return state->etag;
}

void
update_state_set_etag (updateStatePtr state, const gchar *etag)
{
	g_free (state->etag);
	state->etag = NULL;
	if (etag)
		state->etag = g_strdup(etag);
}

void
update_state_set_cache_maxage (updateStatePtr state, const gint maxage)
{
	if (0 < maxage)
		state->maxAgeMinutes = maxage;
	else
		state->maxAgeMinutes = -1;
}

gint
update_state_get_cache_maxage (updateStatePtr state)
{
	return state->maxAgeMinutes;
}

const gchar *
update_state_get_cookies (updateStatePtr state)
{
	return state->cookies;
}

void
update_state_set_cookies (updateStatePtr state, const gchar *cookies)
{
	g_free (state->cookies);
	state->cookies = NULL;
	if (cookies)
		state->cookies = g_strdup (cookies);
}

updateStatePtr
update_state_copy (updateStatePtr state)
{
	updateStatePtr newState;

	newState = update_state_new ();
	update_state_set_lastmodified (newState, update_state_get_lastmodified (state));
	update_state_set_cookies (newState, update_state_get_cookies (state));
	update_state_set_etag (newState, update_state_get_etag (state));

	return newState;
}

void
update_state_free (updateStatePtr updateState)
{
	if (!updateState)
		return;

	g_free (updateState->cookies);
	g_free (updateState->etag);
	g_free (updateState);
}

/* update options */

updateOptionsPtr
update_options_copy (updateOptionsPtr options)
{
	updateOptionsPtr newOptions;
	newOptions = g_new0 (struct updateOptions, 1);
	newOptions->username = g_strdup (options->username);
	newOptions->password = g_strdup (options->password);
	newOptions->dontUseProxy = options->dontUseProxy;
	return newOptions;
}
void
update_options_free (updateOptionsPtr options)
{
	if (!options)
		return;

	g_free (options->username);
	g_free (options->password);
	g_free (options);
}

/* update request object */

G_DEFINE_TYPE (UpdateRequest, update_request, G_TYPE_OBJECT);

static void
update_request_finalize (GObject *obj)
{
	UpdateRequest *request = UPDATE_REQUEST (obj);

	update_state_free (request->updateState);
	update_options_free (request->options);

	g_free (request->postdata);
	g_free (request->source);
	g_free (request->filtercmd);

	G_OBJECT_CLASS (update_request_parent_class)->finalize (obj);
}

static void
update_request_class_init (UpdateRequestClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = update_request_finalize;
}

static void
update_request_init (UpdateRequest *request)
{
}

UpdateRequest *
update_request_new (const gchar *source, updateStatePtr state, updateOptionsPtr options)
{
	UpdateRequest *request = UPDATE_REQUEST (g_object_new (UPDATE_REQUEST_TYPE, NULL));

	request->source = g_strdup (source);

	if (state)
		request->updateState = update_state_copy (state);
	else
		request->updateState = update_state_new ();


	if (options)
		request->options = update_options_copy (options);
	else
		request->options = g_new0 (struct updateOptions, 1);

	return request;
}

void
update_request_set_source(UpdateRequest *request, const gchar* source)
{
	g_free (request->source);
	request->source = g_strdup (source);
}

void
update_request_set_auth_value (UpdateRequest *request, const gchar* authValue)
{
	g_free (request->authValue);
	request->authValue = g_strdup (authValue);
}

void
update_request_allow_commands (UpdateRequest *request, gboolean allowCommands)
{
	request->allowCommands = allowCommands;
}


/* update result */

G_DEFINE_TYPE (UpdateResult, update_result, G_TYPE_OBJECT)

static void update_result_finalize (GObject *object) {
	UpdateResult *self = UPDATE_RESULT(object);

	g_free(self->source);
	g_free(self->data);
	g_free(self->contentType);
	g_free(self->filterErrors);
	if (self->updateState) {
		update_state_free(self->updateState);
	}

	G_OBJECT_CLASS(update_result_parent_class)->finalize(object);
}

static void update_result_class_init (UpdateResultClass *klass) {
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	object_class->finalize = update_result_finalize;
}

static void update_result_init (UpdateResult *self) {
	self->source = NULL;
	self->httpstatus = 0;
	self->data = NULL;
	self->size = 0;
	self->contentType = NULL;
	self->filterErrors = NULL;
	self->updateState = update_state_new ();
}