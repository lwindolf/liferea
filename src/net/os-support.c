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

#include <string.h>
#include "os-support.h"

/******************************************************************************
 * This is a replacement for strsep which is not portable (missing on Solaris).
 *
 * http://www.winehq.com/hypermail/wine-patches/2001/11/0024.html
 *
 * The following function is written by François Gouget
 */

#ifdef SUN
char* strsep(char** str, const char* delims)
{
    char* token;

    if (*str==NULL) {
        /* No more tokens */
        return NULL;
    }

    token=*str;
    while (**str!='\0') {
        if (strchr(delims,**str)!=NULL) {
            **str='\0';
            (*str)++;
            return token;
        }
        (*str)++;
    }
    /* There is no other token */
    *str=NULL;
   return token;
}
#endif


/* Private malloc wrapper. Aborts program execution if malloc fails. */
void * s_malloc (size_t size) {
	void *newmem;
	
	newmem = malloc (size);
	
	if (newmem == NULL) {
		MainQuit ("Allocating memory", strerror(errno));
	}
	
	return newmem;
}
