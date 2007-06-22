/* 
 * gzip/zlib uncompression, Liferea reuses a stripped
 * version of the SnowNews code...
 *
 * Snownews - A lightweight console RSS newsreader
 * 
 * Copyright 2003-2004 Oliver Feiler <kiza@kcore.de> and
 *                     Rene Puls <rpuls@gmx.net>
 *
 * conversions.c
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
#include <iconv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <zlib.h>

#include "compat.h"
#include "conversions.h"

extern struct entity *first_entity;

char *base64encode(char const *inbuf, unsigned int inbuf_size) {
	static unsigned char const alphabet[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	char *outbuf = NULL;
	unsigned int inbuf_pos = 0;
	unsigned int outbuf_pos = 0;
	unsigned int outbuf_size = 0;
	int bits = 0;
	int char_count = 0;
	
	outbuf = g_malloc(1);
	
	while (inbuf_pos < inbuf_size) {
	
		bits |= *inbuf;
		char_count++;
		
		if (char_count == 3) {
			outbuf = g_realloc(outbuf, outbuf_size+4);
			outbuf_size += 4;
			outbuf[outbuf_pos+0] = alphabet[bits >> 18];
			outbuf[outbuf_pos+1] = alphabet[(bits >> 12) & 0x3f];
			outbuf[outbuf_pos+2] = alphabet[(bits >> 6) & 0x3f];
			outbuf[outbuf_pos+3] = alphabet[bits & 0x3f];
			outbuf_pos += 4;
			bits = 0;
			char_count = 0;
		}
		
		inbuf++;
		inbuf_pos++;
		bits <<= 8;
	}
	
	if (char_count > 0) {
		bits <<= 16 - (8 * char_count);
		outbuf = g_realloc(outbuf, outbuf_size+4);
		outbuf_size += 4;
		outbuf[outbuf_pos+0] = alphabet[bits >> 18];
		outbuf[outbuf_pos+1] = alphabet[(bits >> 12) & 0x3f];
		if (char_count == 1) {
			outbuf[outbuf_pos+2] = '=';
			outbuf[outbuf_pos+3] = '=';
		} else {
			outbuf[outbuf_pos+2] = alphabet[(bits >> 6) & 0x3f];
			outbuf[outbuf_pos+3] = '=';
		}
		outbuf_pos += 4;
	}
	
	outbuf = g_realloc(outbuf, outbuf_size+1);
	outbuf[outbuf_pos] = 0;
	
	return outbuf;
}

/* Returns NULL on invalid input */
char* decodechunked(char * chunked, unsigned int *inputlen) {
	/* We can reuse the same buffer to dechunkify it:
	 * the data size will never increase. */
	char *orig = chunked, *dest = chunked;
	unsigned long chunklen;
	while((chunklen = strtoul(orig, &orig, 16))) {
		/* process one more chunk: */
		/* skip chunk-extension part */
		while(*orig && (*orig != '\r'))
			orig++;
		/* skip '\r\n' after chunk length */
		orig += 2;
		if(( chunklen > (chunked + *inputlen - orig)))
			/* insane chunk length. Well... */
			return NULL;
		memmove(dest, orig, chunklen);
		dest += chunklen;
		orig += chunklen;
		/* and go to the next chunk */
	}
	*dest = '\0';
	*inputlen = dest - chunked;
	
	return chunked;
}

/* Remove leading whitspaces, newlines, tabs.
 * This function should be safe for working on UTF-8 strings.
 * tidyness: 0 = only suck chars from beginning of string
 *           1 = extreme, vacuum everything along the string.
 */
void CleanupString (char * string, int tidyness) {
	int len, i;
	
	/* If we are passed a NULL pointer, leave it alone and return. */
	if (string == NULL)
		return;
	
	len = strlen(string);
	
	while ((string[0] == '\n' || string [0] == ' ' || string [0] == '\t') &&
			(len > 0)) {
		/* len=strlen(string) does not include \0 of string.
		   But since we copy from *string+1 \0 gets included.
		   Delicate code. Think twice before it ends in buffer overflows. */
		memmove (string, string+1, len);
		len--;
	}
	
	len = strlen(string);
	/* Eat newlines and tabs along the whole string. */
	if (tidyness == 1) {
		for (i = 0; i < len; i++) {
			if ((string[i] == '\t') || (string[i] == '\n'))
				string[i] = ' ';
		}
	}
}
