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
import time

gi.require_version('Gtk', '3.0')

from gi.repository import GObject, GLib, Gtk, Gio, Liferea, Pango
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

class DownloadManagerPlugin(GObject.Object, Liferea.Activatable, Liferea.DownloadActivatable):
    __gtype_name__ = 'DownloadManagerPlugin'

    shell = GObject.property(type=Liferea.Shell)

    def __init__(self):
        super().__init__()
        self.window = None
        self.downloads = {}
        self.max_concurrent_downloads = 3
        self.download_dir = GLib.get_user_special_dir(GLib.UserDirectory.DIRECTORY_DOWNLOAD)
        # Allow for XDG not resolving the download directory
        if not self.download_dir:
            self.download_dir = os.path.expanduser("~")

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

        self.vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self.vbox.set_spacing(0)
        self.vbox.set_name("download-manager-vbox")

        self.scrollable_area = Gtk.ScrolledWindow()
        self.scrollable_area.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        self.scrollable_area.add(self.vbox)

        self.window.add(self.scrollable_area)
        self.window.set_default_size(640, 400)
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
        # Collect keys to delete
        keys_to_delete = [key for key, download in self.downloads.items() if download.get('finished')]

        # Delete the collected keys
        for key in keys_to_delete:
            self.vbox.remove(self.downloads[key].get('container').get_parent())
            del self.downloads[key]

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

    def on_open_file(self, button, download):
        os.system(f"xdg-open {download.get('filename')}")

    # Ideas:
    #
    # - Allow changing max concurrent downloads
    # - Allow canceling downloads
    # - Limit download speed
    # - Settings persistence (e.g. download dir)

    def do_download(self, url):       
        progress_bar = Gtk.ProgressBar()
        progress_bar.set_show_text(True)

        label = Gtk.Label(label=url)
        label.set_xalign(0)
        label.set_ellipsize(Pango.EllipsizeMode.MIDDLE)
        
        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        vbox.set_spacing(0)
        vbox.pack_start(label, False, False, 6)
        vbox.pack_start(progress_bar, False, False, 6)

        hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        hbox.pack_end(vbox, True, True, 0)

        separator = Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL)
        separator
        outerVBox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        outerVBox.set_spacing(0)
        outerVBox.pack_start(hbox, False, False, 3)
        outerVBox.pack_start(separator, False, False, 3)

        self.vbox.pack_start(outerVBox, False, False, 0)
        outerVBox.show_all()
        
        filename = self.safe_filename(url)
        self.downloads[filename] = download = {
            'progress_bar' : progress_bar,
            'container'    : hbox,
            'url'          : url,
            'filename'     : filename,
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
       
        threading.Thread(target=self.download_file, args=(download,)).start()       

    def do_show(self):
        self.window.show_all()
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
        try:
            # Wait until a slot is free
            GObject.idle_add(download.get('progress_bar').set_text, 'Pending')
            while threading.active_count() > self.max_concurrent_downloads + 1:
                time.sleep(5)

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
        except Exception as e:
            print(f"Download failed: {e}")
            GObject.idle_add(self.download_failed, download)

    def download_failed(self, download):
        download.get('progress_bar').set_text("Failed")
        download['finished'] = True

        icon = Gtk.Image.new_from_icon_name("dialog-error", Gtk.IconSize.LARGE_TOOLBAR)
        download.get('container').pack_start(icon, False, False, 5)
        icon.show()

    def download_complete(self, download):
        download.get('progress_bar').destroy()
        download['finished'] = True
        download['success'] = True
        
        open_button = Gtk.Button()
        open_button.set_image(Gtk.Image.new_from_icon_name("folder-symbolic", Gtk.IconSize.BUTTON))
        open_button.connect("clicked", self.on_open_file, download)
        download.get('container').pack_start(open_button, False, False, 5)
        open_button.show()
