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


/* UIDejunk: remove crap (=html tags) from feed description and convert
 * html entities to something useful if we hit them.
 * This function took almost forever to get right, but at least I learned
 * that html entity &hellip; has nothing to do with Lucifer's ISP, but
 * instead means "..." (3 dots, "and so on...").
 */
char * UIDejunk (char * feed_description) {
	char *start;				/* Points to first char everytime. Need to free this. */
	char *text;					/* = feed_description.
	                               Well, at least at the beginning of the func. */
	char *newtext;				/* Detag'ed *text. */
	char *detagged;				/* Returned value from strsep. This is what we want. */
	char *entity;				/* Which HTML crap did we hit now? */
/*	struct entity *cur_entity;*/
	int found = 0;				/* User defined entity matched? */
	
	/* Gracefully handle passed NULL ptr. */
	if (feed_description == NULL) {
		newtext = strdup("(null)");
		return newtext;
	}
	
	/* Make a copy and point *start to it so we can free the stuff again! */
	text = strdup (feed_description);
	start = text;
	
	
	/* If *text begins with a tag, discard all of them. */
	while (1) {
		if (text[0] == '<') {
			strsep (&text, "<");
			strsep (&text, ">");
		} else
			break;
		if (text == NULL) {
			newtext = strdup (_("No description available."));
			free (start);
			return newtext;
		}
	}
	newtext = malloc (1);
	newtext[0] = '\0';
	
	while (1) {
		/* Strip tags... tagsoup mode. */
		/* strsep puts everything before "<" into detagged. */
		detagged = strsep (&text, "<");
		if (detagged == NULL)
			break;
		
		/* Replace <p> and <br> (in all incarnations) with newlines, but only
		   if there isn't already a following newline. */
		if (text != NULL) {
			if ((strncasecmp (text, "p", 1) == 0) ||
				(strncasecmp (text, "br", 2) == 0)) {
				if ((strncasecmp (text, "br>\n", 4) != 0) &&
					(strncasecmp (text, "br/>\n", 5) != 0) &&
					(strncasecmp (text, "br />\n", 6) != 0) &&
					(strncasecmp (text, "p>\n", 3) != 0)) {
					newtext = realloc (newtext, strlen(newtext)+2);
					strcat (newtext, "\n");
				}
			}
		}
		newtext = realloc (newtext, strlen(newtext)+strlen(detagged)+1);

		/* Now append detagged to newtext. */
		strcat (newtext, detagged);
		
		/* Advance *text to next position after the closed tag. */
		if ((strsep (&text, ">")) == NULL)
			break;
	}
	free (start);
	
	CleanupString (newtext, 0);
	
	/* See if there are any entities in the string at all. */
	if (strchr(newtext, '&') != NULL) {
		text = strdup (newtext);
		start = text;
		free (newtext);
	
		newtext = malloc (1);
		newtext[0] = '\0';
		
		while (1) {
			/* Strip HTML entities. */
			detagged = strsep (&text, "&");
			if (detagged == NULL)
				break;
			
			if (detagged != '\0') {
				newtext = realloc (newtext, strlen(newtext)+strlen(detagged)+1);
				strcat (newtext, detagged);
			}
			/* Expand newtext by one char. */
			newtext = realloc (newtext, strlen(newtext)+2);
			/* This might break if there is an & sign in the text. */
			entity = strsep (&text, ";");
			if (entity != NULL) {
				/* XML defined entities. */
				if (strcmp(entity, "amp") == 0) {
					strcat (newtext, "&");
					continue;
				} else if (strcmp(entity, "lt") == 0) {
					strcat (newtext, "<");
					continue;
				} else if (strcmp(entity, "gt") == 0) {
					strcat (newtext, ">");
					continue;
				} else if (strcmp(entity, "quot") == 0) {
					strcat (newtext, "\"");
					continue;
				} else if (strcmp(entity, "apos") == 0) {
					strcat (newtext, "'");
					continue;
				}
				/*				
//				/ * Decode user defined entities. * /
//				found = 0;
//				for (cur_entity = first_entity; cur_entity != NULL; cur_entity = cur_entity->next_ptr) {
//					if (strcmp (entity, cur_entity->entity) == 0) {
//						/ * We have found a matching entity. * /
//						
//						/ * If entity_length is more than 1 char we need to realloc
//						   more space in newtext. * /
//						if (cur_entity->entity_length > 1)
//							newtext = realloc (newtext,  strlen(newtext)+cur_entity->entity_length+1);
//						
//						/ * Append new entity. * /
//						strcat (newtext, cur_entity->converted_entity);
//						
//						/ * Set found flag. * /
//						found = 1;
//						
//						/ * We can now leave the for loop. * /
//						break;
//					}
//				}
*/
				/* If nothing matched so far, put text back in. */
				if (!found) {
					/* Changed into &+entity to avoid stray semicolons
					   at the end of wrapped text if no entity matches. */
					newtext = realloc (newtext, strlen(newtext)+strlen(entity)+2);
					strcat (newtext, "&");
					strcat (newtext, entity);
				}
			} else
				break;
		}
		free (start);
	}	
	return newtext;
}

/* 5th try at a wrap text functions.
 * My first version was broken, my second one sucked, my third try was
 * so overcomplicated I didn't understand it anymore... Kianga tried
 * the 4th version which corrupted some random memory unfortunately...
 * but this one works. Heureka!
 */
char * WrapText (char * text, int width) {
	char *newtext;
	char *textblob;			/* Working copy of text. */
	char *chapter;
	char *line;				/* One line of text with max width. */
	char *savepos;			/* Saved position pointer so we can go back in the string. */
	char *chunk;
	char *start;
	/*
	char *p;
	int lena, lenb;
	*/
	
	textblob = strdup (text);
	start = textblob;
	

	line = malloc (1);
	line[0] = '\0';
	
	newtext = malloc(1);
	memset (newtext, 0, 1);

	while (1) {
		/* First, cut at \n. */
		chapter = strsep (&textblob, "\n");
		if (chapter == NULL)
			break;
		while (1) {
			savepos = chapter;
			chunk = strsep (&chapter, " ");
			
			/* Last chunk. */
			if (chunk == NULL) {
				if (line != NULL) {
					newtext = realloc (newtext, strlen(newtext)+strlen(line)+2);
					strcat (newtext, line);
					strcat (newtext, "\n");
					
					/* Faster replacement with memcpy.
					 * Overkill, overcomplicated and smelling of bugs all over.
					 */
					/*
					lena = strlen(newtext);
					lenb = strlen(line);
					newtext = realloc (newtext, lena+lenb+2);
					p = newtext+lena;
					memcpy (p, line, lenb);
					p += lenb;
					*p = '\n';
					p++;
					*p=0;
					*/
					
					line[0] = '\0';
				}
				break;
			}
			
			if (strlen(chunk) > width) {
				/* First copy remaining stuff in line to newtext. */
				newtext = realloc (newtext, strlen(newtext)+strlen(line)+2);
				strcat (newtext, line);
				strcat (newtext, "\n");
				
				free (line);
				line = malloc (1);
				line[0] = '\0';
				
				/* Then copy chunk with max length of line to newtext. */
				line = realloc (line, width+1);
				strncat (line, chunk, width-5);
				strcat (line, "...");
				newtext = realloc (newtext, strlen(newtext)+width+2);
				strcat (newtext, line);
				strcat (newtext, "\n");
				free (line);
				line = malloc (1);
				line[0] = '\0';
				continue;
			}

			if (strlen(line)+strlen(chunk) <= width) {
				line = realloc (line, strlen(line)+strlen(chunk)+2);
				strcat (line, chunk);
				strcat (line, " ");
			} else {
				/* Why the fuck can chapter be NULL here anyway? */
				if (chapter != NULL) {
					chapter--;
					chapter[0] = ' ';
				}
				chapter = savepos;
				newtext = realloc (newtext, strlen(newtext)+strlen(line)+2);
				strcat (newtext, line);
				strcat (newtext, "\n");
				free (line);
				line = malloc (1);
				line[0] = '\0';
			}
		}
	}
	
	//if (line != NULL)
		free (line);
	
	free (start);	
	
	return newtext;
}

/* Decompresses deflate compressed data. Probably unneeded,
 * see gzip_uncompress below.
 */
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

/* Decompressed gzip,deflate compressed data. This is what the webservers
 * usually send.
 */
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


char *base64encode(char const *inbuf, unsigned int inbuf_size) {
	static unsigned char const alphabet[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	char *outbuf = NULL;
	unsigned int inbuf_pos = 0;
	unsigned int outbuf_pos = 0;
	unsigned int outbuf_size = 0;
	int bits = 0;
	int char_count = 0;
	
	outbuf = malloc(1);
	
	while (inbuf_pos < inbuf_size) {
	
		bits |= *inbuf;
		char_count++;
		
		if (char_count == 3) {
			outbuf = realloc(outbuf, outbuf_size+4);
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
		outbuf = realloc(outbuf, outbuf_size+4);
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
	
	outbuf = realloc(outbuf, outbuf_size+1);
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
