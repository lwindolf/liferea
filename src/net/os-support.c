/* 
 * Liferea reuses a stripped version of the SnowNews code...
 *
 * Snownews - A lightweight console RSS newsreader
 * 
 * Copyright 2003-2004 Oliver Feiler <kiza@kcore.de>
 * http://kiza.kcore.de/software/snownews/
 *
 * os-support.c
 *
 * Library support functions.
 *
 * Please read the file README.patching before changing any code in this file!
 *
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <stdlib.h>
#include "os-support.h"
#include "../common.h" /* provides strsep */

/* Private malloc wrapper. Aborts program execution if malloc fails. */
void * s_malloc (size_t size) {
	void *newmem;
	
	newmem = g_malloc (size);
	
	if (newmem == NULL) {
		MainQuit ("Allocating memory", strerror(errno));
	}
	
	return newmem;
}
