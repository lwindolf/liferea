/*
 *  $Id$
 *  JaguarFoundation
 *  Copyright © 2004 René Puls <http://purl.org/net/kianga/>
 *
 *  Latest version: <http://purl.org/net/kianga/latest/jaguartools>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License version 2.1, as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "zlib_interface.h"
#include <stdlib.h>
#include <stdio.h>
#include <zlib.h>
#include <string.h>

int JG_ZLIB_DEBUG = 0;

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

int jg_zlib_uncompress(void const *in_buf, int in_size, 
				       void **out_buf_ptr, int *out_size,
					   int gzip)
{
	char tmpstring[1024];
	z_stream stream;
	char *out_buf = NULL;
	int out_buf_bytes = 0;
	char tmp_buf[4096];
	int result;
	int new_bytes;
	
	/* Prepare the stream structure. */
	stream.zalloc = NULL;
	stream.zfree = NULL;
	stream.opaque = NULL;
	stream.next_in = (void *)in_buf;	
	stream.avail_in = in_size;
	stream.next_out = tmp_buf;
	stream.avail_out = sizeof tmp_buf;
	
	if (out_size != NULL)
		*out_size = 0;
	
	if (gzip)
		result = inflateInit2(&stream, MAX_WBITS + 32); /* UNTESTED */
	else
		result = inflateInit2(&stream, -MAX_WBITS);
	
	if (result != 0) {
		if (JG_ZLIB_DEBUG)
			fprintf(stderr, "inflateInit2 failed: %d\n", result);
		return JG_ZLIB_ERROR_OLDVERSION;
	}
	
	do {
		/* Should be Z_FINISH? */
		result = inflate(&stream, Z_NO_FLUSH);
		switch (result) {
			case Z_BUF_ERROR:
				if (stream.avail_in == 0)
					goto DONE; /* zlib bug */
			case Z_ERRNO:
			case Z_NEED_DICT:
			case Z_MEM_ERROR:
			case Z_DATA_ERROR:
			case Z_VERSION_ERROR:
				inflateEnd(&stream);
				free(out_buf);
				if (JG_ZLIB_DEBUG) {
					snprintf (tmpstring, sizeof(tmpstring), "ERROR: zlib_uncompress: %d %s\n", result, stream.msg);
					fprintf(stderr, tmpstring);
				}
				return JG_ZLIB_ERROR_UNCOMPRESS;
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
			if (JG_ZLIB_DEBUG)
				fprintf(stderr, "ERROR: No output during decompression\n");
			return JG_ZLIB_ERROR_NODATA;
		}
	} while (result != Z_STREAM_END);
	
DONE:
	
	inflateEnd(&stream);
	
	/* Null-terminate the output buffer so it can be handled like a string. */
	out_buf = realloc(out_buf, out_buf_bytes + 1);
	out_buf[out_buf_bytes] = 0;
	
	/* The returned size does NOT include the additionall null byte! */
	if (out_size != NULL)
		*out_size = out_buf_bytes;
	
	*out_buf_ptr = out_buf;

	return 0;
}

/* Decompressed gzip,deflate compressed data. This is what the webservers usually send. */

int jg_gzip_uncompress(void const *in_buf, int in_size, 
					   void **out_buf_ptr, int *out_size) 
{
	char tmpstring[1024];
	struct gzip_header const *header;
	char *data_start;
	int offset = sizeof *header;
	
	header = in_buf;
	
	if (out_size != NULL)
		*out_size = 0;
	
	if ((header->magic[0] != 0x1F) || (header->magic[1] != 0x8B)) {
		if (JG_ZLIB_DEBUG)
			fprintf(stderr, "ERROR: Invalid magic bytes for GZIP data\n");
		return JG_ZLIB_ERROR_BAD_MAGIC;
	}
	
	if (header->method != 8) {
		if (JG_ZLIB_DEBUG)
			fprintf(stderr, "ERROR: Compression method is not deflate\n");
		return JG_ZLIB_ERROR_BAD_METHOD;
	}
	
	if (header->flags != 0 && header->flags != 8) {
		if (JG_ZLIB_DEBUG) {
			snprintf (tmpstring, sizeof(tmpstring), "ERROR: Unsupported flags %d", header->flags);
			fprintf(stderr, "ERROR: %s\n", tmpstring);
		}
		return JG_ZLIB_ERROR_BAD_FLAGS;
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

	return jg_zlib_uncompress(data_start, in_size - offset - 8, 
							  out_buf_ptr, out_size, 0);
}
