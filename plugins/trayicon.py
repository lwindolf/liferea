#
# System Tray Icon Plugin
#
# Copyright (C) 2013 Lars Windolf <lars.lindner@gmail.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public License
# along with this library; see the file COPYING.LIB.  If not, write to
# the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
#

import gi
gi.require_version('Peas', '1.0')
gi.require_version('PeasGtk', '1.0')
gi.require_version('Liferea', '3.0')
from gi.repository import GObject, Peas, PeasGtk, Gtk, Liferea
from gi.repository import Gdk, GdkPixbuf
import cairo
from collections import namedtuple

# Cairo text extents
Extents = namedtuple("Extents", [
    "x_bearing", "y_bearing",
    "width", "height",
    "x_advance", "y_advance"
    ])

def pixbuf_text(width, height, text, font_size=16, bg_pix=None):
    """Draw text to pixbuf at lower right corner

        @width: canvas width
        @height: canvas height
        @text: UTF-8 text to draw
        @font_size: font size
        @bg_pix: pixbuf to draw as background

        @return: a new pixbuf with the given canvas width and height
    """
    bg_color = (0.8, 0.8, 0.8, 1)
    text_color = (0.8, 0.2, 0, 1)

    surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, width, height)
    context = cairo.Context(surface)

    if bg_pix is not None:
        bg_w, bg_h = bg_pix.get_width(), bg_pix.get_height()
        Gdk.cairo_set_source_pixbuf(context, bg_pix,
                max(width - bg_w, 0)/2, max(height - bg_h, 0)/2)
        context.paint() #paint the pixbuf

    context.set_font_size(font_size)
    context.select_font_face("condensed")

    # extents: (x_bearing, y_bearing, width, height, x_advance, y_advance)
    extents = Extents._make(context.text_extents(text))

    # draw text with a background color
    x_off = width - extents.width - 1
    y_off = height - extents.height - 1
    context.set_source_rgba(*bg_color)
    context.rectangle(x_off, y_off, extents.width + 2, extents.height + 2)
    context.fill()

    x_off = max(width - extents.x_bearing - extents.width, 1)
    y_off = height - (extents.height + extents.y_bearing) - 1
    context.move_to(x_off, y_off)
    context.set_source_rgba(*text_color)
    context.show_text(text)

    pixbuf= Gdk.pixbuf_get_from_surface(surface, 0, 0, width, height)
    return pixbuf

class TrayiconPlugin (GObject.Object, Liferea.ShellActivatable):
    __gtype_name__ = 'TrayiconPlugin'

    object = GObject.property(type=GObject.Object)
    shell = GObject.property(type=Liferea.Shell)

    def do_activate(self):
        self.staticon = Gtk.StatusIcon ()
        # FIXME: Support a scalable image!
        self.read_pix = Liferea.icon_create_from_file ("available.png")
        self.unread_pix = Liferea.icon_create_from_file ("unread.png")

        self.staticon.set_from_pixbuf(Liferea.icon_create_from_file("unread.png"))
        self.staticon.connect("activate", self.trayicon_click)
        self.staticon.connect("popup_menu", self.trayicon_popup)
        self.staticon.connect("size-changed", self.trayicon_size_changed)
        self.staticon.set_visible(True)

        self.menu = Gtk.Menu()
        menuitem_toggle = Gtk.MenuItem("Show / Hide")
        menuitem_quit = Gtk.MenuItem("Quit")
        menuitem_toggle.connect("activate", self.trayicon_toggle)
        menuitem_quit.connect("activate", self.trayicon_quit)
        self.menu.append(menuitem_toggle)
        self.menu.append(menuitem_quit)
        self.menu.show_all()

        self.window = self.shell.get_window()
        self.minimize_to_tray_delete_handler = self.window.connect("delete_event",
                                                                   self.trayicon_minimize_on_close)
        self.minimize_to_tray_minimize_handler = self.window.connect("window-state-event",
                                                                     self.window_state_event_cb)

        feedlist = self.shell.props.feed_list
        self.feedlist_new_items_cb(feedlist)
        sigid = feedlist.connect("new-items", self.feedlist_new_items_cb)
        self.feedlist_new_items_cb_id = sigid
        self.feedlist = feedlist

    def window_state_event_cb(self, widget, event):
        "Hide window when minimize"
        if event.changed_mask & event.new_window_state & Gdk.WindowState.ICONIFIED:
            self.window.hide()
            self.window.deiconify()

    def trayicon_click(self, widget, data = None):
        self.shell.toggle_visibility()

    def trayicon_minimize_on_close(self, widget, data = None):
        self.window.hide()
        return True

    def trayicon_toggle(self, widget, data = None):
        self.shell.toggle_visibility()

    def trayicon_quit(self, widget, data = None):
        Liferea.shutdown()

    def trayicon_popup(self, widget, button, time, data = None):
        self.menu.popup(None, None, self.staticon.position_menu, self.staticon, 3, time)

    def show_new_count(self, new_count):
        """display new count on status icon"""
        pix_size = self.staticon.props.size
        font_size = max(10, pix_size/4*2)
        #print(pix_size, font_size, double_figure)

        pix = pixbuf_text(pix_size, pix_size,
                "{}".format(new_count),
                font_size, self.unread_pix)
        self.staticon.props.pixbuf = pix
        return pix

    def feedlist_new_items_cb(self, feedlist=None, new_count=-1):
        if new_count < 0:
            if feedlist is None:
                feedlist = self.shell.props.feed_list
            new_count = feedlist.get_new_item_count()
        if new_count > 0:
            double_figure = min(99, new_count) # show max 2 digit
            pix = self.show_new_count(double_figure)
        else:
            pix = self.read_pix

        icon_size = self.staticon.props.size
        if pix.props.height < icon_size:
            pix = pix.scale_simple(icon_size, icon_size,
                    GdkPixbuf.InterpType.HYPER)

        if  self.staticon.props.pixbuf != pix:
            self.staticon.props.pixbuf = pix

    def trayicon_size_changed(self, widget, size):
        self.feedlist_new_items_cb()
        return True

    def do_deactivate(self):
        self.staticon.set_visible(False)
        self.window.disconnect(self.minimize_to_tray_delete_handler)
        self.window.disconnect(self.minimize_to_tray_minimize_handler)

        self.feedlist.disconnect(self.feedlist_new_items_cb_id)

        # unhide the window when deactivating the plugin
        self.window.deiconify()
        self.window.show()

        del self.staticon
        del self.window
        del self.menu
