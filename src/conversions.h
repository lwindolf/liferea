/* 
 * gzip/zlib uncompression, Liferea reuses a stripped
 * version of the SnowNews code...
 *
 * Copyright 2003 Oliver Feiler <kiza@kcore.de> 
 *
 *
 * FIXME: license
 *
 */

/*-----------------------------------------------------------------------*/
/* some Liferea specific adaptions					 */
 
#ifndef _CONVERSIONS_H
#define _CONVERSIONS_H

#include "support.h"

/* we redefine some SnowNews functions */
#define UIStatus(a, b)		ui_mainwindow_set_status_bar(a)
#define MainQuit(str, errno)	g_error(str);
#define	getch()			0
/*-----------------------------------------------------------------------*/

void *zlib_uncompress(void *in_buf, int in_size, int *out_size, int voodoo_magic);
void *gzip_uncompress(void *in_buf, int in_size, int *out_size);
char *base64encode(char const *inbuf, unsigned int inbuf_size);

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
