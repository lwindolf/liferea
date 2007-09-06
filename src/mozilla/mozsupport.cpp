/**
 * @file mozsupport.cpp C++ portion of GtkMozEmbed support
 *
 * Copyright (C) 2004-2007 Lars Lindner <lars.lindner@gmail.com>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 *
 * The preference handling was taken from the Galeon source
 *
 *  Copyright (C) 2000 Marco Pesenti Gritti 
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

#include "mozilla-config.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#define MOZILLA_INTERNAL_API

#include "mozsupport.h"
#include <gtk/gtk.h>
#include <gtkmozembed.h>
#include <gtkmozembed_internal.h>

#include "nsIWebBrowser.h"
#include "nsIDOMMouseEvent.h"
#include "dom/nsIDOMKeyEvent.h"
#include "nsIDOMWindow.h"
#include "nsIPrefService.h"
#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "necko/nsNetCID.h"

extern "C" {
#include "conf.h"
#include "ui/ui_itemlist.h"
}

extern "C" 
gint mozsupport_key_press_cb (GtkWidget *widget, gpointer ev)
{
	nsIDOMKeyEvent	*event = (nsIDOMKeyEvent*)ev;
	PRUint32	keyCode = 0;
	PRBool		alt, ctrl, shift;
	
	/* This key interception is necessary to catch
	   spaces which are an internal Mozilla key binding.
	   All other combinations like Ctrl-Space, Alt-Space
	   can be caught with the GTK code in ui_mainwindow.c.
	   This is a bad case of code duplication... */
	
	event->GetCharCode (&keyCode);
	if (keyCode == nsIDOMKeyEvent::DOM_VK_SPACE) {
     		event->GetShiftKey (&shift);
     		event->GetCtrlKey (&ctrl);
     		event->GetAltKey (&alt);

		/* Do trigger scrolling if the skimming hotkey is 
		  <Space> with a modifier. Other cases (Space+modifier)
		  are handled in src/ui/ui_mainwindow.c and if we
		  get <Space> with a modifier here it needs no extra
		  handling. */
		if ((0 == conf_get_int_value (BROWSE_KEY_SETTING)) &&
		    !(alt | shift | ctrl)) {
			if (mozsupport_scroll_pagedown(widget) == FALSE)
				on_next_unread_item_activate (NULL, NULL);
			return TRUE;
		}
	}	
	return FALSE;
}
/**
 * Takes a pointer to a mouse event and returns the mouse
 *  button number or -1 on error.
 */
extern "C" 
gint mozsupport_get_mouse_event_button(gpointer event) {
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

extern "C" void
mozsupport_set_zoom (GtkWidget *embed, gfloat aZoom) {
	nsCOMPtr<nsIWebBrowser>		mWebBrowser;	
	nsCOMPtr<nsIDOMWindow> 		mDOMWindow;
	
	gtk_moz_embed_get_nsIWebBrowser(GTK_MOZ_EMBED(embed), getter_AddRefs(mWebBrowser));
	mWebBrowser->GetContentDOMWindow(getter_AddRefs(mDOMWindow));	
	if(NULL == mDOMWindow) {
		g_warning("could not retrieve DOM window...");
		return;
	}
	mDOMWindow->SetTextZoom (aZoom);
}

extern "C" gfloat
mozsupport_get_zoom (GtkWidget *embed) {
	nsCOMPtr<nsIWebBrowser>		mWebBrowser;	
	nsCOMPtr<nsIDOMWindow> 		mDOMWindow;
	float zoom;
	
	gtk_moz_embed_get_nsIWebBrowser(GTK_MOZ_EMBED(embed), getter_AddRefs(mWebBrowser));
	mWebBrowser->GetContentDOMWindow(getter_AddRefs(mDOMWindow));	
	if(NULL == mDOMWindow) {
		g_warning("could not retrieve DOM window...");
		return 1.0;
	}
	mDOMWindow->GetTextZoom (&zoom);	
	return zoom;
}

extern "C" void mozsupport_scroll_to_top(GtkWidget *embed) {
	nsCOMPtr<nsIWebBrowser>		WebBrowser;	
	nsCOMPtr<nsIDOMWindow> 		DOMWindow;

	gtk_moz_embed_get_nsIWebBrowser(GTK_MOZ_EMBED(embed), getter_AddRefs(WebBrowser));
	WebBrowser->GetContentDOMWindow(getter_AddRefs(DOMWindow));	
	if(NULL == DOMWindow) {
		g_warning("could not retrieve DOM window...");
		return;
	}

	DOMWindow->ScrollTo(0, 0);
}

extern "C" gboolean mozsupport_scroll_pagedown(GtkWidget *embed) {
	gint initial_y, final_y;
	nsCOMPtr<nsIWebBrowser>		WebBrowser;	
	nsCOMPtr<nsIDOMWindow> 		DOMWindow;

	gtk_moz_embed_get_nsIWebBrowser(GTK_MOZ_EMBED(embed), getter_AddRefs(WebBrowser));
	WebBrowser->GetContentDOMWindow(getter_AddRefs(DOMWindow));	
	if(NULL == DOMWindow) {
		g_warning("could not retrieve DOM window...");
		return FALSE;
	}

	DOMWindow->GetScrollY(&initial_y);
	DOMWindow->ScrollByPages(1);
	DOMWindow->GetScrollY(&final_y);
	
	return initial_y != final_y;
}

/* the following code is from the Galeon source mozilla/mozilla.cpp */

extern "C" gboolean
mozsupport_save_prefs (void)
{
	nsCOMPtr<nsIPrefService> prefService = 
				 do_GetService (NS_PREFSERVICE_CONTRACTID);
	g_return_val_if_fail (prefService != nsnull, FALSE);

	nsresult rv = prefService->SavePrefFile (nsnull);
	return NS_SUCCEEDED (rv) ? TRUE : FALSE;
}

/**
 * mozsupport_preference_set: set a string mozilla preference
 */
extern "C" gboolean
mozsupport_preference_set(const char *preference_name, const char *new_value)
{
	g_return_val_if_fail (preference_name != NULL, FALSE);

	/*It is legitimate to pass in a NULL value sometimes. So let's not
	 *assert and just check and return.
	 */
	if (!new_value) return FALSE;

	nsCOMPtr<nsIPrefService> prefService = 
				do_GetService (NS_PREFSERVICE_CONTRACTID);
	nsCOMPtr<nsIPrefBranch> pref;
	prefService->GetBranch ("", getter_AddRefs(pref));

	if (pref)
	{
		nsresult rv = pref->SetCharPref (preference_name, new_value);            
		return NS_SUCCEEDED (rv) ? TRUE : FALSE;
	}

	return FALSE;
}

/**
 * mozsupport_preference_set_boolean: set a boolean mozilla preference
 */
extern "C" gboolean
mozsupport_preference_set_boolean (const char *preference_name,
				gboolean new_boolean_value)
{
	g_return_val_if_fail (preference_name != NULL, FALSE);
  
	nsCOMPtr<nsIPrefService> prefService = 
				do_GetService (NS_PREFSERVICE_CONTRACTID);
	nsCOMPtr<nsIPrefBranch> pref;
	prefService->GetBranch ("", getter_AddRefs(pref));
  
	if (pref)
	{
		nsresult rv = pref->SetBoolPref (preference_name,
				new_boolean_value ? PR_TRUE : PR_FALSE);
		return NS_SUCCEEDED (rv) ? TRUE : FALSE;
	}

	return FALSE;
}

/**
 * mozsupport_preference_set_int: set an integer mozilla preference
 */
extern "C" gboolean
mozsupport_preference_set_int (const char *preference_name, int new_int_value)
{
	g_return_val_if_fail (preference_name != NULL, FALSE);

	nsCOMPtr<nsIPrefService> prefService = 
				do_GetService (NS_PREFSERVICE_CONTRACTID);
	nsCOMPtr<nsIPrefBranch> pref;
	prefService->GetBranch ("", getter_AddRefs(pref));

	if (pref)
	{
		nsresult rv = pref->SetIntPref (preference_name, new_int_value);
		return NS_SUCCEEDED (rv) ? TRUE : FALSE;
	}

	return FALSE;
}

/**
 * Set Mozilla caching to on or off line mode
 */
extern "C" void
mozsupport_set_offline_mode (gboolean offline)
{
	nsresult rv;

	nsCOMPtr<nsIIOService> io = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
	if (NS_SUCCEEDED(rv))
	{
		rv = io->SetOffline(offline);
		//if (NS_SUCCEEDED(rv)) return TRUE;
	}
	//return FALSE;
}

