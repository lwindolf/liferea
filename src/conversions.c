/* 
 * gzip/zlib uncompression, Liferea reuses a stripped
 * version of the SnowNews code...
 *
 * Snownews - A lightweight console RSS newsreader
 * 
 * Copyright 2003 Oliver Feiler <kiza@kcore.de> and
 *                Rene Puls <rpuls@gmx.net>
 *
 * conversions.c
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <zlib.h>

#include "conversions.h"
#include "callbacks.h"

void *zlib_uncompress(void *in_buf, int in_size, int *out_size, int voodoo_magic) {
	char tmpstring[1024];
	z_stream stream;
	char *out_buf = NULL;
	int out_buf_bytes = 0;
	char tmp_buf[512];
	int result;
	int new_bytes;

	/* Prepare the stream structure. */
	stream.zalloc = NULL;
	stream.zfree = NULL;
	stream.opaque = NULL;
	stream.next_in = in_buf;	
	stream.avail_in = in_size;
	stream.next_out = tmp_buf;
	stream.avail_out = sizeof tmp_buf;
	
	if (out_size != NULL)
		*out_size = 0;
	
	/* Deflated data from GZIP files can only be decompressed with voodoo magic(tm)! */
	if (voodoo_magic)
		inflateInit2(&stream, -MAX_WBITS);
	else
		inflateInit(&stream);
	
	do {
		/* Should be Z_FINISH? */
		result = inflate(&stream, Z_NO_FLUSH);
		switch (result) {
		case Z_ERRNO:
		case Z_NEED_DICT:
		case Z_BUF_ERROR:
		case Z_MEM_ERROR:
		case Z_DATA_ERROR:
		case Z_VERSION_ERROR:
			inflateEnd(&stream);
			free(out_buf);
			snprintf (tmpstring, sizeof(tmpstring), _("ERROR: zlib_uncompress: %d %s\n"), result, stream.msg);
			UIStatus (tmpstring, 2);
			return NULL;
		}
		if (stream.avail_out < sizeof tmp_buf) {
			/* Add the new uncompressed data to our output buffer. */
			new_bytes = sizeof tmp_buf - stream.avail_out;
			out_buf = realloc(out_buf, out_buf_bytes + new_bytes);
			memcpy(out_buf + out_buf_bytes, tmp_buf, new_bytes);
			out_buf_bytes += new_bytes;
			stream.next_out = tmp_buf;
			stream.avail_out = sizeof tmp_buf;
		} else {
			/* For some reason, inflate() didn't write out a single byte. */
			inflateEnd(&stream);
			free(out_buf);
			UIStatus (_("ERROR: No output during decompression"), 2);
			return NULL;
		}
	} while (result != Z_STREAM_END);
	
	inflateEnd(&stream);
	
	/* Null-terminate the output buffer so it can be handled like a string. */
	out_buf = realloc(out_buf, out_buf_bytes + 1);
	out_buf[out_buf_bytes] = 0;
	
	/* The returned size does NOT include the additionall null byte! */
	if (out_size != NULL)
		*out_size = out_buf_bytes;
	
	return out_buf;
}

void *gzip_uncompress(void *in_buf, int in_size, int *out_size) {
	char tmpstring[1024];
	struct gzip_header *header;
	char *data_start;
	int offset = sizeof *header;
	
	header = in_buf;

	if (out_size != NULL)
		*out_size = 0;

	if ((header->magic[0] != 0x1F) || (header->magic[1] != 0x8B)) {
		UIStatus (_("ERROR: Invalid magic bytes for GZIP data"), 2);
		return NULL;
	}
	
	if (header->method != 8) {
		UIStatus (_("ERROR: Compression method is not deflate"), 2);
		return NULL;
	}
	
	if (header->flags != 0 && header->flags != 8) {
		snprintf (tmpstring, sizeof(tmpstring), _("ERROR: Unsupported flags %d\n"), header->flags);
		UIStatus (tmpstring, 2);
		return NULL;
	}
	
	if (header->flags & 8) {
		/* skip the file name */
		while (offset < in_size) {
			if (((char *)in_buf)[offset] == 0) {
				offset++;
				break;
			}
			offset++;
		}
	}
	
	data_start = (char *)in_buf + offset;
	
	return zlib_uncompress(data_start, in_size - offset - 8, out_size, 1);
}
