/* 
 * gzip/zlib uncompression, Liferea reuses a stripped
 * version of the SnowNews code...
 *
 * Copyright 2003 Oliver Feiler <kiza@kcore.de> 
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

#ifndef _CONVERSIONS_H
#define _CONVERSIONS_H

char * UIDejunk (char * feed_description);
char * WrapText (char * text, int width);
char *base64encode(char const *inbuf, unsigned int inbuf_size);
char* decodechunked(char * chunked, unsigned int *inputlen);
void CleanupString (char * string, int tidyness);

struct gzip_header {
	unsigned char magic[2];
	unsigned char method;
	unsigned char flags;
	unsigned char mtime[4];
	unsigned char xfl;
	unsigned char os;
};

struct gzip_footer {
	unsigned char crc32[4];
	unsigned char size[4];
};

#endif
