"""
System Tray Icon Plugin

Copyright (C) 2013-2025 Lars Windolf <lars.windolf@gmx.de>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
"""

import gettext
import os
import pathlib
from pathlib import Path
from collections import namedtuple
import cairo
import gi
from gi.repository import Gio, GLib, GObject, Gtk, Liferea
from gi.repository import Gdk, GdkPixbuf

try:
    gi.require_version('AyatanaAppIndicator3', '0.1')
    from gi.repository import AyatanaAppIndicator3 as AppIndicator
    APPINDICATOR_AVAILABLE = True
except (ImportError, ValueError):
    try:
        gi.require_version('AppIndicator3', '0.1')
        from gi.repository import AppIndicator3 as AppIndicator
        APPINDICATOR_AVAILABLE = True
    except (ImportError, ValueError):
        APPINDICATOR_AVAILABLE = False

_ = lambda x: x
try:
    t = gettext.translation("liferea")
except FileNotFoundError:
    pass
else:
    _ = t.gettext

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


def get_config_path():
    """Return config file path"""
    trayicon_path = "liferea/plugins/trayicon"
    config_home = os.getenv('XDG_CONFIG_HOME',Path.joinpath(Path.home(), ".config"))
    config_dir = Path.joinpath(Path(config_home), trayicon_path)

    if not config_dir.exists():
        config_dir.mkdir(0o700, True, True)

    config_path = config_dir / "trayicon.conf"
    return config_path


def status_notifier_available():
    dbus = Gio.bus_get_sync(Gio.BusType.SESSION)
    result = dbus.call_sync(
        "org.freedesktop.DBus",  # bus name
        "/",  # object path
        "org.freedesktop.DBus",  # interface name
        "NameHasOwner",  # method name
        GLib.Variant.new_tuple(
            GLib.Variant.new_string("org.kde.StatusNotifierWatcher"),
        ),
        None,  # reply_type
        Gio.DBusCallFlags.NONE,
        -1,
    )

    if not result:
        return False

    result = result.unpack()
    return result and result[0]


class TrayiconPlugin (GObject.Object, Liferea.Activatable, Liferea.ShellActivatable):
    __gtype_name__ = 'TrayiconPlugin'

    object = GObject.property(type=GObject.Object)
    shell = GObject.property(type=Liferea.Shell)
    config_path = None
    read_pix = None
    unread_pix = None
    staticon = None
    menu = None
    min_enabled = None
    window = None
    delete_signal_id = None
    feedlist_new_items_cb_id = None
    feedlist = None
    use_appindicator = None

    def do_activate(self):
        self.use_appindicator = APPINDICATOR_AVAILABLE and status_notifier_available()

        if self.use_appindicator:
            self.indicator = AppIndicator.Indicator.new(
                "Liferea",
                Liferea.icon_find_pixmap_file("emblem-web.svg"),
                AppIndicator.IndicatorCategory.APPLICATION_STATUS
            )

            self.indicator.set_attention_icon_full(
                Liferea.icon_find_pixmap_file("unread.png"),
                _("Liferea unread icon")
            )
            self.indicator.set_status(AppIndicator.IndicatorStatus.ACTIVE)
            self.indicator.set_title("Liferea")
        else:
            self.read_pix = Liferea.icon_create_from_file("emblem-web.svg")
            # FIXME: Support a scalable image!
            self.unread_pix = Liferea.icon_create_from_file("unread.png")

            self.staticon = Gtk.StatusIcon ()
            self.staticon.connect("activate", self.trayicon_click)
            self.staticon.connect("popup_menu", self.trayicon_popup)
            self.staticon.connect("size-changed", self.trayicon_size_changed)
            self.staticon.set_visible(True)

            self.trayicon_set_pixbuf(self.read_pix)

        self.menu = Gtk.Menu()
        menuitem_toggle = Gtk.MenuItem(_("Show / Hide"))
        menuitem_close_behavior = Gtk.CheckMenuItem(_("Minimize to tray on close"))
        menuitem_quit = Gtk.MenuItem(_("Quit"))

        self.config_path = get_config_path()
        self.min_enabled = self.get_config()

        if self.min_enabled == "True":
            menuitem_close_behavior.set_active(True)
        else:
            menuitem_close_behavior.set_active(False)

        menuitem_toggle.connect("activate", self.trayicon_toggle)
        menuitem_close_behavior.connect("toggled", self.trayicon_close_behavior)
        menuitem_quit.connect("activate", self.trayicon_quit)
        self.menu.append(menuitem_toggle)
        self.menu.append(menuitem_close_behavior)
        self.menu.append(menuitem_quit)
        self.menu.show_all()

        if self.use_appindicator:
            self.indicator.set_menu(self.menu)
            self.indicator.set_secondary_activate_target(menuitem_toggle)

        self.window = self.shell.get_window()
        self.delete_signal_id = GObject.signal_lookup("delete_event", Gtk.Window)
        GObject.signal_handlers_block_matched (self.window,
                                               GObject.SignalMatchType.ID | GObject.SignalMatchType.DATA,
                                               self.delete_signal_id, 0, None, None, None)
        self.window.connect("delete_event", self.trayicon_close_action)
        self.window.connect("window-state-event", self.window_state_event_cb)

        # show the window if it is hidden when starting liferea
        self.window.deiconify()
        self.window.show()

        feedlist = self.shell.props.feed_list
        self.feedlist_new_items_cb(feedlist)
        sigid = feedlist.connect("new-items", self.feedlist_new_items_cb)
        self.feedlist_new_items_cb_id = sigid
        self.feedlist = feedlist

    def window_state_event_cb(self, widget, event):
        "Hide window when minimize"
        if event.changed_mask & event.new_window_state & Gdk.WindowState.ICONIFIED:
            self.window.deiconify()
            self.shell.save_position()
            self.window.hide()

    def get_config(self):
        """Load configuration file"""
        try:
            with open(self.config_path, "r") as f:
                setting = f.readline()
            if setting == "":
                setting = "True"
        except FileNotFoundError:
            self.save_config("True")
            setting = "True"
        return setting

    def save_config(self, minimize_setting):
        """Save configuration file"""
        with open(self.config_path, "w") as f:
            f.write(minimize_setting)

    def trayicon_click(self, widget, data = None):
        # Always show the window on click, as some window managers misbehave.
        self.shell.show_window()

    def trayicon_close_action(self, widget, event):
        self.shell.save_position()
        if self.min_enabled == "True":
            self.window.hide()
        else:
            Liferea.Application.shutdown()
        return True

    def trayicon_close_behavior(self, widget, data = None):
        if widget.get_active():
            self.min_enabled = "True"
        else:
            self.min_enabled = "False"
        self.save_config(self.min_enabled)

    def trayicon_toggle(self, widget, data = None):
        self.shell.toggle_visibility()

    def trayicon_quit(self, widget, data = None):
        Liferea.Application.shutdown()

    def trayicon_popup(self, widget, button, time, data = None):
        self.menu.popup(None, None, self.staticon.position_menu, self.staticon, 3, time)

    def trayicon_set_pixbuf(self, pix):
        if pix is None:
            return

        icon_size = self.staticon.props.size
        if icon_size == 0:
            return

        if pix.props.height != icon_size:
            pix = pix.scale_simple(icon_size, icon_size,
                GdkPixbuf.InterpType.HYPER)

        if self.staticon.props.pixbuf != pix:
            self.staticon.props.pixbuf = pix

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

        new_count = min(99, new_count) # show max 2 digit
        if self.use_appindicator:
            if new_count > 0:
                self.indicator.set_label(str(new_count), "99")
                self.indicator.set_status(AppIndicator.IndicatorStatus.ATTENTION)
            else:
                self.indicator.set_label("", "99")
                self.indicator.set_status(AppIndicator.IndicatorStatus.ACTIVE)
        else:
            if new_count > 0:
                pix = self.show_new_count(new_count)
            else:
                pix = self.read_pix

            self.trayicon_set_pixbuf(pix)

    def trayicon_size_changed(self, widget, size):
        self.feedlist_new_items_cb()
        return True

    def do_deactivate(self):
        if self.use_appindicator:
            self.indicator.set_status(AppIndicator.IndicatorStatus.PASSIVE)
        else:
            self.staticon.set_visible(False)

        self.window.disconnect_by_func(self.trayicon_close_action)
        GObject.signal_handlers_unblock_matched (self.window,
                                                 GObject.SignalMatchType.ID | GObject.SignalMatchType.DATA,
                                                 self.delete_signal_id, 0, None,None,None)
        self.window.disconnect_by_func(self.window_state_event_cb)

        self.feedlist.disconnect(self.feedlist_new_items_cb_id)

        # unhide the window when deactivating the plugin
        self.window.deiconify()
        self.window.show()

        if self.use_appindicator:
            del self.indicator
        else:
            del self.staticon
        del self.window
        del self.menu
