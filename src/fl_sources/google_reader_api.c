/**
 * @file google_reader_api->c  Helpers for implementing the Google Reader API
 * 
 * Copyright (C) 2022 Lars Windolf <lars.windolf@gmx.de>
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

#include "fl_sources/google_reader_api.h"

void
google_reader_api_check (googleReaderApi *api)
{
	g_assert (api->unread_count);
	g_assert (api->subscription_list);
	g_assert (api->add_subscription);
	g_assert (api->add_subscription_post);
	g_assert (api->remove_subscription);
	g_assert (api->remove_subscription_post);
	g_assert (api->edit_label);
	g_assert (api->edit_add_label_post);
	g_assert (api->edit_remove_label_post);
	g_assert (api->edit_tag);
	g_assert (api->edit_tag_add_post);
	g_assert (api->edit_tag_remove_post);
	g_assert (api->edit_tag_ar_tag_post);
	g_assert (api->token);
}

void
google_reader_api_free (googleReaderApi *api)
{
	g_free (api->login);
	g_free (api->login_post);
	g_free (api->unread_count);
	g_free (api->subscription_list);
	g_free (api->add_subscription);
	g_free (api->add_subscription_post);
	g_free (api->remove_subscription);
	g_free (api->remove_subscription_post);
	g_free (api->edit_label);
	g_free (api->edit_add_label_post);
	g_free (api->edit_remove_label_post);
	g_free (api->edit_tag);
	g_free (api->edit_tag_add_post);
	g_free (api->edit_tag_remove_post);
	g_free (api->edit_tag_ar_tag_post);
	g_free (api->token);
}

