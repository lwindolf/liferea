/* ico2xpm.c -- Convert icons to pixmaps
 * Copyright (C) 1998 Philippe Martin
 * Modified by Brion Vibber
 * 
 * Modified to be suitable for Liferea
 * Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "support.h"
#include "feed.h"
#include "common.h"

#ifdef STDC_HEADERS
#include <stdlib.h>
#endif

/* The maximum number of icons we'll try to handle in any one file */
#define MAXICONS 32

/* Header of a single icon image in an icon file */
typedef struct IconDirectoryEntry {
	unsigned char  bWidth;
	unsigned char  bHeight;
	unsigned char  bColorCount;
	unsigned char  bReserved;
	unsigned short  wPlanes;
	unsigned short  wBitCount;
	unsigned long  dwBytesInRes;
	unsigned long  dwImageOffset;
} ICONDIRENTRY;

/* Header of an icon file */
typedef struct ICONDIR {
	 unsigned short          idReserved;
	 unsigned short          idType;
	 unsigned short          idCount;
/*    ICONDIRENTRY  idEntries[1];*/
	 ICONDIRENTRY  idEntries[MAXICONS];
} ICONHEADER;

/* Bitmap header - this is on the images themselves */
typedef struct tagBITMAPINFOHEADER{
        unsigned long	biSize;
        long		biWidth;
        long		biHeight;
        unsigned short	biPlanes;
        unsigned short	biBitCount;
        unsigned long	biCompression;
        unsigned long	biSizeImage;
        long		biXPelsPerMeter;
        long		biYPelsPerMeter;
        unsigned long	biClrUsed;
        unsigned long	biClrImportant;
} BITMAPINFOHEADER;

/* Magic number for an icon file */
/* This is the the idReserved and idType fields of the header */
static char ico_magic_number[4] = {0, 0, 1, 0};

static int do_verbose = 0;

/* void convert __P((void)); */

/* For keeping track of which icon image we're on */
static unsigned short whichimage = 0;

/*
 * Conversion
 */

#define XPM_HEAD "/* XPM */\n\
static char *pixmap[] = {\n\
/* width height ncols cpp */\n\
\"%d %d %d 2\",\n  /* Colors */\n"

/* The smallest number of double words containing c bytes */
#define DWORD_ALIGN_BYTES(b) (((b) / 4) + (((b) % 4 > 0) ? 1 : 0))

/* The smallest number of double words containing c bits */
#define DWORD_ALIGN_BITS(b) (((b) / 32) + (((b) % 32 > 0) ? 1 : 0))

/* The different possible number of colors */
#define LOW_COLOR 16
#define HIGH_COLOR 0
/*#define NCOLORS (color_level == LOW_COLOR ? 16 : 256)*/
#define NCOLORS ncolors

/* The color index of a pixel in the icon colormap */
#define PIXEL_INDEX(x,y) \
     (color_level == LOW_COLOR \
      ? ((x) % 2 == 0 \
	 ? (image [bytes_per_line * (height - 1 - (y)) / 2 + (x) / 2] & 0xF0) >> 4 \
	 : image [bytes_per_line * (height - 1 - (y)) / 2 + (x) / 2] & 0x0F) \
      : image [bytes_per_line * (height - 1 - (y)) + (x)])
     
/* The color index of a pixel in the (reduced) pixmap colormap */
#define PIXEL_REDUCED_INDEX(x,y) \
     (reduced_colormap_index [PIXEL_INDEX((x),(y))])
  
/* The RGB value of a colormap entry */
#define INDEX_VALUE(index,c) (colormap[(c) + (index) * 4])
#define INDEX_R_VALUE(index) (INDEX_VALUE((index), 2))
#define INDEX_G_VALUE(index) (INDEX_VALUE((index), 1))
#define INDEX_B_VALUE(index) (INDEX_VALUE((index), 0))
  
/* The transparency of a pixel */
#define MASK_BYTE(x,y) (bytes_per_mask_line * (height - 1 - (y)) + (x) / 8)
#define MASK_BIT(x,y) (7 - (x) % 8)
#define IS_TRANSPARENT(x,y) \
     (mask[MASK_BYTE((x),(y))] & (1 << MASK_BIT((x),(y))))

static FILE *xpm_stream; 

/* Values read from icon file */
static ICONHEADER iconheader;
static BITMAPINFOHEADER bitmapinfoheader;
/*static unsigned char magic_number [sizeof(ico_magic_number)];*/
static unsigned char *magic_number;
static unsigned char color_level;
static unsigned char colormap[256 * 4];
static unsigned char image[256 * 256];
static unsigned char mask[256 * 32];

/* Different variables computed from the read ones */
static unsigned ncolors;
static unsigned char width;
static unsigned char height;
static unsigned char bytes_per_line;
static unsigned char bytes_per_mask_line;
static int image_length;
static int mask_length;
static int i, x, y;

/* Variables for the reduced colormap */
static int color_used[256];
static int nb_color_used;
static unsigned char reduced_colormap_index [256];

/* Does the icon have transparent pixels ? */
static int have_transparent_pixels;

gboolean
convertIcoToXPM(gchar *outputfile, unsigned char *icondata, int datalen)
{
  int offset = 0;

  /* Check the magic number & header */
  /*fread (magic_number, 1, sizeof(ico_magic_number), ico_stream);*/
  //fread (&iconheader, 1, 6/*sizeof(unsigned short)*3*/, ico_stream);
  memcpy(&iconheader, icondata, 6);
  offset += 6;
  magic_number = (unsigned char *)&iconheader;
  for (i = 0 ; i < sizeof(ico_magic_number) ; i ++)
    {    
      if (magic_number [i] != ico_magic_number [i])
	{
	  g_warning("favicon.ico data is not a recognized icon file.\n");
	  return FALSE;
	}
    }
  
  /* Output some stats */
  if (do_verbose)
    g_print("favicon.ico data contains %d icon images\n", GUINT16_FROM_LE(iconheader.idCount));

  /* Read in the rest of the icon directory entries */
  //fread(&iconheader.idEntries[0],iconheader.idCount,
  //  /*sizeof(ICONDIRENTRY)*/16,ico_stream);
  memcpy(&iconheader.idEntries[0], icondata + offset, GUINT16_FROM_LE(iconheader.idCount)*16);
  offset += GUINT16_FROM_LE(iconheader.idCount)*16;
  
  /* Cycle through each icon image */
  for(whichimage = 0; whichimage < GUINT16_FROM_LE(iconheader.idCount); whichimage++) {
    // * For debugging
    if(do_verbose) {
      g_print("(%d) identry: %d %d %d %d %d %d %d %d\n",
        whichimage,
        (int)iconheader.idEntries[whichimage].bWidth,
        (int)iconheader.idEntries[whichimage].bHeight,
        (int)iconheader.idEntries[whichimage].bColorCount,
        (int)iconheader.idEntries[whichimage].bReserved,
        (int)GUINT16_FROM_LE(iconheader.idEntries[whichimage].wPlanes),
        (int)GUINT16_FROM_LE(iconheader.idEntries[whichimage].wBitCount),
        (int)GULONG_FROM_LE(iconheader.idEntries[whichimage].dwBytesInRes),
        (int)GULONG_FROM_LE(iconheader.idEntries[whichimage].dwImageOffset));
    }
    //*/
    
    /* Update the output file name */
    

    /* Read the icon size */
    width = iconheader.idEntries[whichimage].bWidth;
    height = iconheader.idEntries[whichimage].bWidth;

    /* Determine the number of bytes defining a line of the icon. */
    bytes_per_line = 4 * DWORD_ALIGN_BYTES (width);
    bytes_per_mask_line = 4 * DWORD_ALIGN_BITS (width);

    /* Let's surf on over to the bitmap image & read the BMIH. */
    //fseek (ico_stream, GULONG_FROM_LEiconheader.idEntries[whichimage].dwImageOffset),SEEK_SET);
    //fread (&bitmapinfoheader, 1, sizeof(bitmapinfoheader), ico_stream);
    offset = GULONG_FROM_LE(iconheader.idEntries[whichimage].dwImageOffset);
    memcpy(&bitmapinfoheader, icondata + offset, sizeof(bitmapinfoheader));

    // * For debugging
    if(do_verbose) {
      g_print("(%d) bmih: %d %d %d %d %d %d %d %d %d %d %d\n",
        whichimage,
        (int)GULONG_FROM_LE(bitmapinfoheader.biSize),
        (int)GLONG_FROM_LE(bitmapinfoheader.biWidth),
        (int)GLONG_FROM_LE(bitmapinfoheader.biHeight),
        (int)GUINT16_FROM_LE(bitmapinfoheader.biPlanes),
        (int)GUINT16_FROM_LE(bitmapinfoheader.biBitCount),
        (int)GULONG_FROM_LE(bitmapinfoheader.biCompression),
        (int)GULONG_FROM_LE(bitmapinfoheader.biSizeImage),
        (int)GLONG_FROM_LE(bitmapinfoheader.biXPelsPerMeter),
        (int)GLONG_FROM_LE(bitmapinfoheader.biYPelsPerMeter),
        (int)GULONG_FROM_LE(bitmapinfoheader.biClrUsed),
        (int)GULONG_FROM_LE(bitmapinfoheader.biClrImportant));
    }//*/
    
    /* Read the number of colors.
     * TODO: add support for monochrome, 24-bit icons
     */
    switch(GUINT16_FROM_LE(bitmapinfoheader.biPlanes)
         * GUINT16_FROM_LE(bitmapinfoheader.biBitCount)) {
      case 4: /* 2^4 = 14 */ color_level = LOW_COLOR; break;
      case 8: /* 2^8 = 256 */ color_level = HIGH_COLOR; break;
      default:
        g_warning("Unsupported number of colors in favicon.ico image! Skipping. (%d planes, %d bpp)\n",
          (int)GUINT16_FROM_LE(bitmapinfoheader.biPlanes),
          (int)GUINT16_FROM_LE(bitmapinfoheader.biBitCount));
        continue;
    }
    
    /* Read the colormap */
    if(GULONG_FROM_LE(bitmapinfoheader.biClrImportant) != 0)
      ncolors = GULONG_FROM_LE(bitmapinfoheader.biClrImportant);
    else if(GULONG_FROM_LE(bitmapinfoheader.biClrUsed) != 0)
      ncolors = GULONG_FROM_LE(bitmapinfoheader.biClrUsed);
    else
      ncolors = 1 << (GUINT16_FROM_LE(bitmapinfoheader.biPlanes)*GUINT16_FROM_LE(bitmapinfoheader.biBitCount));

    /*fseek (ico_stream, iconheader.idEntries[whichimage].dwImageOffset
      + bitmapinfoheader.biSize, SEEK_SET);
    fread (colormap, 1,ncolors * 4, ico_stream);*/
    offset += GULONG_FROM_LE(bitmapinfoheader.biSize);
    memcpy(&colormap, icondata + offset, ncolors*4);
    offset += ncolors*4;

    /* Read the image */
    if (color_level == LOW_COLOR)
        image_length = bytes_per_line * height / 2;
    else
        image_length = bytes_per_line * height;
    /*fread (image, 1, image_length, ico_stream);*/
    memcpy(&image, icondata + offset, image_length);
    offset += image_length;

    /* Read the mask */
    mask_length = bytes_per_mask_line * height;
    /*fread (mask, 1, mask_length, ico_stream);*/
    memcpy(&mask, icondata + offset, mask_length);
    offset += mask_length;

    /* Read error / prematured EOF  occured ? */
    if((mask + mask_length) > (icondata + datalen))
      g_warning("Premature end of favicon.ico data.\n");
    /*if (ferror (ico_stream))
      fprintf (stderr, "%s: %s: Read error.\n",
  	     program_name, input_file);*/
    /*if (feof (ico_stream) || ferror (ico_stream))
      {
        fclose (ico_stream);
        return;
      }*/

    /* Reduce the number of colors */
    for (i = 0 ; i < ncolors ; i++)
      color_used [i] = 0;
  
    for (y = 0 ; y <height ; y++)
      for (x = 0 ; x < width ; x++)
        color_used [PIXEL_INDEX(x,y)] = 1;
  
    nb_color_used = 0;
    for (i = 0 ; i < ncolors ; i++)
      if (color_used [i])
        reduced_colormap_index[i] = nb_color_used ++;

    /* Check for at least one transparent pixel */
    have_transparent_pixels = 0;
    for (y=0; y<height && have_transparent_pixels == 0 ; y++)
      for (x=0; x<width; x++)
        if (IS_TRANSPARENT(x,y))
	  {
	    have_transparent_pixels = 1;
	    break;
	  }
  
    /*
     * Write the pixmap file
     */
    if ((xpm_stream = fopen (outputfile, "w")) == NULL)
      {
        perror (outputfile);	// FIXME: use glib
        return FALSE;
      }

    /* Write the header */
    fprintf (xpm_stream, XPM_HEAD, 
	     width, height, 
	     nb_color_used + have_transparent_pixels);

    /* Write the colors */
    x = 0;
    for (i=0; i<NCOLORS; i++)
      {
        if (color_used [i])
	  {
	    fprintf (xpm_stream, "  \"%.2X c #%.2X%.2X%.2X\",\n", x++, 
		    INDEX_R_VALUE(i), INDEX_G_VALUE(i), INDEX_B_VALUE(i));
	  }
      }
    if (have_transparent_pixels)
      fprintf (xpm_stream, "  \".. s None c None\",\n");

    /* Write the image */
    for (y=0; y<height; y++)
      {
        fprintf (xpm_stream, "  \"");
        for (x=0; x<width; x++)
	  {
	    if (IS_TRANSPARENT(x,y))
	      fprintf (xpm_stream, "..");
	    else
	      fprintf (xpm_stream, "%.2X", PIXEL_REDUCED_INDEX(x,y));
	  }
        fprintf (xpm_stream, "\",\n");
      }
    fprintf (xpm_stream, "};\n");

    /* Write error occured ? */
    if (ferror (xpm_stream))
        g_warning("Error writing %s.\n", outputfile);
  
    fclose (xpm_stream);
  
    /* Write info about xpm file written */
    if (do_verbose)
        g_print("Wrote %s %dx%dx%d (%d)\n",
	       outputfile, width, height,
	       nb_color_used, NCOLORS);
	
    /* we intentionally return after the first retrieved pixmap */       
    return TRUE;
  }
  return FALSE;
}

/* Liferea specific wrapper functions */

void loadFavIcon(feedPtr fp) {
	gchar		*filename, *tmp;
	GdkPixbuf	*pixbuf;
	
	/* try to load a saved favicon */
	filename = getCacheFileName(fp->id, "xpm");
	if(g_file_test(filename, G_FILE_TEST_EXISTS)) {
		/* remove path, because create_pixbuf allows no absolute pathnames */
		tmp = strrchr(filename, '/');
		pixbuf = create_pixbuf(++tmp);
		fp->icon = gdk_pixbuf_scale_simple(pixbuf, 16, 16, GDK_INTERP_BILINEAR);
		g_object_unref(pixbuf);
	}
	g_free(filename);
}

void removeFavIcon(feedPtr fp) {
	gchar		*filename;
	
	/* try to load a saved favicon */
	filename = getCacheFileName( fp->id, "xpm");
	if(g_file_test(filename, G_FILE_TEST_EXISTS)) {
		if(0 != unlink(filename)) {
			g_warning(_("Could not delete icon file %s! Please remove manually!"), filename);
		}	
	}
	g_free(filename);
}
