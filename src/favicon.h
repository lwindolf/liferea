/*
   favicon handling
   
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef _FAVICON_H
#define _FAVICON_H

#include "feed.h"

/* function reads the data of a MS Windows .ICO file referenced by 
   icondata with length datalen, converts the image into XPM 
   format and saves the result in the file outputfile. If the 
   conversion is successful TRUE is returned. */
gboolean convertIcoToXPM(gchar *outputfile, unsigned char *icondata, int datalen);

void favicon_download(feedPtr fp);
void loadFavIcon(feedPtr fp);
void removeFavIcon(feedPtr fp);

#endif
