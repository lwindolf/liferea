/* HTTP feed download, Liferea reuses the only slightly
 * adapted Snownews code:
 *
 * Snownews - A lightweight console RSS newsreader
 * 
 * Copyright 2003 Oliver Feiler <kiza@kcore.de>
 * http://kiza.kcore.de/software/snownews/
 *
 * netio.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _NETIO_H
#define _NETIO_H

#include "update.h"
#include "conversions.h"

/* returns the raw download data or NULL on error */
char * downloadURL(struct feed_request *request);

#endif
