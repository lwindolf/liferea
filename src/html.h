/**
 * @file html.h HTML file handling / feed auto discovery
 * 
 * Copyright (C) 2004 ahmed el-helw <ahmedre@cc.gatech.edu>
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

#ifndef _HTML_H
#define _HTML_H

#include <glib.h>

/**
 * HTML feed auto discovery function. Searches the
 * passed HTML document for feed links and returns
 * one if at least one link could be found.
 *
 * @parm data	HTML source
 * @param baseUri URI that relative links will be based off of
 * @returns	feed URL or NULL. Must be freed by caller.
 */
gchar * html_auto_discover_feed(const gchar* data, const gchar *baseUri);

/**
 * Search for favicon links in a HTML file's head section
 * @param data HTML source
 * @param baseUri URI of the downloaded HTML used to resolve relative URIs
 * @returns URL of the favicon, or NULL. Must be freed by caller.
 */
gchar * html_discover_favicon(const gchar* data, const gchar *baseUri);
#endif
