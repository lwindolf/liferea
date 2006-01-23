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

#ifndef JG_ZLIB_INTERFACE
#define JG_ZLIB_INTERFACE

enum JG_ZLIB_ERROR {
	JG_ZLIB_ERROR_OLDVERSION = -1,
	JG_ZLIB_ERROR_UNCOMPRESS = -2,
	JG_ZLIB_ERROR_NODATA = -3,
	JG_ZLIB_ERROR_BAD_MAGIC = -4,
	JG_ZLIB_ERROR_BAD_METHOD = -5,
	JG_ZLIB_ERROR_BAD_FLAGS = -6
};

extern int JG_ZLIB_DEBUG;

int jg_zlib_uncompress(void const *in_buf, int in_size, 
                       void **out_buf_ptr, int *out_size,
                       int gzip);   

int jg_gzip_uncompress(char const *in_buf, int in_size,
                       void **out_buf_ptr, int *out_size);

#endif
