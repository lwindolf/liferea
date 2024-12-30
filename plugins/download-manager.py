"""
Download Manager Plugin

Copyright (C) 2024 Lars Windolf <lars.lindner@gmx.de>

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

import threading
import requests
import gi
import os

gi.require_version('Peas', '1.0')
gi.require_version('PeasGtk', '1.0')
gi.require_version('Liferea', '3.0')

from gi.repository import GObject, GLib, Gdk, Gtk, Gio, Liferea, Pango
from urllib.parse import urlparse

def remove_menuitem(action, menus, level=0):
    """
    Given an action name such as app.function, remove it from a Gio.Menu
    """
    for i in range(menus.get_n_items()):
        link = menus.iterate_item_links(i)
        if link.next():
            remove_menuitem(action, link.get_value(), level+1)
        else:            
            attr = menus.iterate_item_attributes(i)
            while attr.next():
                if str(attr.get_name()) == "action":
                    value = str(attr.get_value()).strip("'")
                    if value == action:
                        menus.remove(i)

class DownloadManagerPlugin(GObject.Object, Liferea.DownloadActivatable):
    __gtype_name__ = 'DownloadManagerPlugin'

    shell = GObject.property(type=Liferea.Shell)
    download_dir = Gio.File.new_for_path(GLib.get_user_special_dir(GLib.UserDirectory.DIRECTORY_DOWNLOAD)).get_path()

    def __init__(self):
        super().__init__()
        self.window = None
        self.downloads = {}

    def do_activate(self):
        self.window = Gtk.Window(title="Liferea Download Manager")

        self.headerbar = Gtk.HeaderBar(title="Liferea Download Manager")
        self.headerbar.set_show_close_button(True)
        self.window.set_titlebar(self.headerbar)

        self.choose_dir_button = Gtk.Button(label="Download Location")
        self.choose_dir_button.connect("clicked", self.on_choose_directory)
        self.headerbar.pack_start(self.choose_dir_button)

        self.clear_button = Gtk.Button(label="Clear")
        self.clear_button.connect("clicked", self.on_clear_list)
        self.headerbar.pack_start(self.clear_button)

        self.vbox = Gtk.VBox()
        self.vbox.set_spacing(0)
        self.vbox.set_name("download-manager-vbox")
        self.window.add(self.vbox)
        self.window.set_default_size(640, 400)
        self.window.show_all()
        self.window.connect("delete-event", self.on_window_close)

        # Create action and "Tools" menu bar item
        action = Gio.SimpleAction.new('DownloadManager', None)
        action.connect("activate", self.show_menu_action, self.shell)

        self._app = Liferea.Shell.get_window().get_application ()
        self._app.add_action(action)

        self.toolsmenu = self.shell.get_property("builder").get_object("tools_menu")
        self.toolsmenu.append('Downloads', 'app.DownloadManager')

    def do_deactivate(self):
        if self.window:
            self.window.destroy()
            self.window = None
        
        self._app.remove_action('DownloadManager')
        remove_menuitem('app.DownloadManager', self.toolsmenu)

        # FIXME stop all threads

    def on_window_close(self, window, event):
        window.hide()
        return True

    def on_clear_list(self, button):
        # FIXME clear only non-running
        for child in self.vbox.get_children():
            self.vbox.remove(child)
        self.downloads.clear()

    def on_choose_directory(self, button):
        dialog = Gtk.FileChooserDialog(
            title="Select Download Directory",
            parent=self.window,
            action=Gtk.FileChooserAction.SELECT_FOLDER,
            buttons=(
            Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
            Gtk.STOCK_OPEN, Gtk.ResponseType.OK
            )
        )
        dialog.set_current_folder(self.download_dir)
        
        response = dialog.run()
        if response == Gtk.ResponseType.OK:
            self.download_dir = dialog.get_filename()
        dialog.destroy()

    # Add right-click context menu
    def on_popup_menu(self, widget, event, data=None):
        print("Popup menu")
        if event.button != 3:  # Right click
            return
        menu = Gtk.Menu()

        open_item = Gtk.MenuItem(label="Open")
        open_item.connect("activate", self.on_open_file, widget)
        menu.append(open_item)

        open_folder_item = Gtk.MenuItem(label="Open Folder")
        open_folder_item.connect("activate", self.on_open_folder, widget)
        menu.append(open_folder_item)

        menu.show_all()
        menu.popup_at_pointer(event)

    def on_open_file(self, widget):
        print("Open file")

    def on_open_folder(self, widget):
        print("Open folder")

    # TODOs:
    # - Allow canceling downloads
    # - Allow opening downloaded files
    # - Max concurrent downloads
    # - Settings persistence (e.g. download dir)
    #
    # Ideas:
    #
    # - Limit download speed

    def do_download(self, url):
        progress_bar = Gtk.ProgressBar()
        progress_bar.set_show_text(True)

        label = Gtk.Label(label=url)
        label.set_xalign(0)
        label.set_ellipsize(Pango.EllipsizeMode.MIDDLE)
        
        vbox = Gtk.VBox()
        hbox = Gtk.HBox()
        separator = Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL)
        
        hbox.pack_start(vbox, True, True, 0)
        
        vbox.set_spacing(0)
        vbox.pack_start(label, False, False, 6)
        vbox.pack_start(progress_bar, False, False, 6)
        
        self.downloads[url] = {
            'progress_bar' : progress_bar,
            'container'    : hbox,
            'url'          : url,
            'filename'     : self.safe_filename(url),
            'finished'     : False,
            'success'      : False
        }

        # Add custom CSS to the progress bar
        css_provider = Gtk.CssProvider()
        css_provider.load_from_data(b"""
        progressbar > trough, progressbar > trough > progress {
            min-height: 10px;
        }
        """)
        context = progress_bar.get_style_context()
        context.add_provider(css_provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
       
        self.vbox.pack_start(hbox, False, False, 0)
        self.vbox.pack_start(separator, False, False, 3)
        hbox.show_all()
        separator.show()

        threading.Thread(target=self.download_file, args=(self.downloads[url],)).start()       

    def do_show(self):
        self.window.present()

    def show_menu_action(self, action, variant, shell):
        self.do_show()

    # Prepare download file name from URL
    def safe_filename(self, url):
        parsed_url = urlparse(url)
        safe_filename = os.path.basename(parsed_url.path)
        if not safe_filename:
            return None

        safe_filepath = os.path.join(self.download_dir, safe_filename)
        name, ext = os.path.splitext(safe_filename)
        counter = 1
        while os.path.exists(safe_filepath):
            safe_filename = f"{name}_{counter}{ext}"
            safe_filepath = os.path.join(self.download_dir, safe_filename)
            counter += 1

        return safe_filepath

    def download_file(self, download):
        if not download.get('filename'):
            print("Could not determine filename from URL")
            GObject.idle_add(self.download_failed, download)
            return      

        response = requests.get(download.get('url'), stream=True)
        if response.status_code != 200:
            GObject.idle_add(self.download_failed, download)
            return
        
        total_length = response.headers.get('content-length')
        if total_length is None:
            GObject.idle_add(download.get('progress_bar').set_fraction, 1.0)
        else:
            total_length = int(total_length)
            downloaded = 0
            with open(download.get('filename'), 'ab') as f:
                for data in response.iter_content(chunk_size=4096):
                    f.write(data)
                    downloaded += len(data)
                    fraction = downloaded / total_length
                    GObject.idle_add(download.get('progress_bar').set_fraction, fraction)
                    GObject.idle_add(download.get('progress_bar').set_text, "{:.0%}".format(fraction))

        GObject.idle_add(self.download_complete, download)

    def download_failed(self, download):
        download.get('progress_bar').set_text("Failed")

    def download_complete(self, download):
        download.get('progress_bar').hide()
        
        open_button = Gtk.Button()
        open_button.set_image(Gtk.Image.new_from_icon_name("folder-symbolic", Gtk.IconSize.BUTTON))
        open_button.connect("clicked", self.on_open_file, download.get('filename'))
        download.get('container').pack_start(open_button, False, False, 5)
        open_button.show()
