/*
 *  Liferea GtkMozEmbed support
 *
 *  Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "mozilla.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMMouseEvent.h"

/**
 * Takes a pointer to a mouse event and returns the mouse
 *  button number or -1 on error.
 */
extern "C" 
gint mozilla_get_mouse_event_button(gpointer event) {
	gint	button = 0;
	
	g_return_val_if_fail (event, -1);

	/* the following lines were found in the Galeon source */	
	nsIDOMMouseEvent *aMouseEvent = (nsIDOMMouseEvent *) event;
	aMouseEvent->GetButton ((PRUint16 *) &button);

	/* for some reason we get different numbers on PPC, this fixes
	 * that up... -- MattA */
	if (button == 65536)
	{
		button = 1;
	}
	else if (button == 131072)
	{
		button = 2;
	}

	return button;
}
