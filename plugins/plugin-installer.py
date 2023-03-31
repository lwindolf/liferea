# -*- coding: utf-8 -*-
# vim: set ts=4 et sw=4 sts=4:

# Plugin browser plugin for Liferea
# Copyright (C) 2018 Lars Windolf <lars.windolf@gmx.de>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import urllib.request, json
import os, sys, glob, shutil, subprocess
from pathlib import Path
import gi

gi.require_version('Gtk', '3.0')

from gi.repository import GObject, Liferea, Gtk, Gio, PeasGtk

import gettext

_ = lambda x: x
try:
    t = gettext.translation("liferea")
except FileNotFoundError:
    pass
else:
    _ = t.gettext

class AppActivatable(GObject.Object, Liferea.ShellActivatable):
    __gtype_name__ = "PluginBrowserAppActivatable"

    shell = GObject.property(type=Liferea.Shell)

    def __init__(self):
        GObject.Object.__init__(self)

    def do_activate(self):
        action = Gio.SimpleAction.new ('InstallPlugins', None)
        action.connect("activate", self._run)

        self._app = self.shell.get_window().get_application ()
        self._app.add_action (action)

        toolsmenu = self.shell.get_property("builder").get_object ("tools_menu")
        toolsmenu.append (_('Plugins'), 'app.InstallPlugins')

    def do_deactivate(self):
        self._browser = None
        self._app.remove_action ('InstallPlugins')
        self.app = None

    def _run(self, action, data=None):
        self._browser = PluginBrowser()
        self._browser.show_all()

class PluginBrowser(Gtk.Window):
    SCHEMA_ID = "net.sf.liferea.plugins"
    SCHEMA_PATH = "glib-2.0/schemas/"
    PLUGIN_PATH = "liferea/plugins"

    def __init__(self):
        Gtk.Window.__init__(self, title=_("Plugin Installer"))

        self.data_home_path = os.getenv('XDG_DATA_HOME',Path.joinpath(Path.home(), ".local/share"))
        self.target_dir = Path.joinpath(Path(self.data_home_path), self.PLUGIN_PATH)
        self.schema_dir = Path.joinpath(Path(self.data_home_path), self.SCHEMA_PATH)

        self.set_border_width(10)
        self.set_default_size(600,300)

        self._grid = Gtk.Grid()
        self._grid.set_column_homogeneous(True)
        self._grid.set_row_homogeneous(True)

        self._notebook = Gtk.Notebook()
        self._notebook.append_page(PeasGtk.PluginManager(None), Gtk.Label(_("Activate Plugins")))
        self._notebook.append_page(self._grid, Gtk.Label(_("Download Plugins")))

        self.add(self._notebook)

        self._liststore = Gtk.ListStore(bool, str, str, str, str)
        self._plugin_list = self.fetch_list()
        for ref in self._plugin_list['plugins']:
            try:
                name = next(iter(ref))
                installed = False
                if(os.path.isfile('%s/%s.plugin' % (self.target_dir, ref[name]['module'])) or
                   os.path.isdir('%s/%s' % (self.target_dir, ref[name]['module']))):
                   installed = True
                if not 'icon' in ref[name]:
                   ref[name]['icon'] = 'libpeas-plugin'

                self._liststore.append((installed, ref[name]['icon'], name, ref[name]['category'], ('<b>%s</b>\n%s') % (name, ref[name]['description'])))
            except:
                print(_("Bad fields for plugin entry %s") % name)

        self.current_filter_category = None
        self.category_filter = self._liststore.filter_new()
        self.category_filter.set_visible_func(self.category_filter_func)

        # creating the treeview, making it use the filter as a model, and adding the columns
        self.treeview = Gtk.TreeView.new_with_model(self.category_filter)
        self.treeview.get_selection().connect("changed", self.on_selection_changed)
        for i, column_title in enumerate(["Inst.", "Icon", "Description"]):
            if column_title == 'Inst.':
                renderer = Gtk.CellRendererToggle()
                column = Gtk.TreeViewColumn(column_title, renderer, active=i)
            elif column_title == 'Icon':
                renderer = Gtk.CellRendererPixbuf()
                column = Gtk.TreeViewColumn(column_title, renderer, icon_name=i)
            else:
                renderer = Gtk.CellRendererText()
                column = Gtk.TreeViewColumn(column_title, renderer, markup=4)
            self.treeview.append_column(column)
            column.set_sort_column_id(i)

        self._categories = Gtk.ListStore(str)
        for category in [_("All"), _("Advanced"), _("Menu"), _("Notifications")]:
            self._categories.append([category])

        self._catcombo = Gtk.ComboBox.new_with_model(self._categories)
        renderer_text = Gtk.CellRendererText()
        self._catcombo.pack_start(renderer_text, True)
        self._catcombo.add_attribute(renderer_text, "text", 0)
        self._catcombo.connect("changed", self.on_catcombo_changed)
        self._catlabel = Gtk.Label(_("Filter by category"))
        self._catcombo.set_active(0)

        # Setting up the layout, putting the treeview in a scrollwindow, and the buttons in a row
        self.scrollable_treelist = Gtk.ScrolledWindow()
        self.scrollable_treelist.set_vexpand(True)

        self._installButton = Gtk.Button.new_with_mnemonic(_("_Install"))
        self._installButton.connect("clicked", self.install)
        self._installButton.set_sensitive(False)

        self._uninstallButton = Gtk.Button.new_with_mnemonic(_("_Uninstall"))
        self._uninstallButton.connect("clicked", self.uninstall)
        self._uninstallButton.set_sensitive(False)

        self._grid.attach(self.scrollable_treelist, 0, 0, 8, 10)
        self._grid.attach_next_to(self._catlabel, self.scrollable_treelist, Gtk.PositionType.TOP, 1, 1)
        self._grid.attach_next_to(self._catcombo, self._catlabel, Gtk.PositionType.RIGHT, 2, 1)
        self._grid.attach_next_to(self._installButton, self.scrollable_treelist, Gtk.PositionType.BOTTOM, 1, 1)
        self._grid.attach_next_to(self._uninstallButton, self._installButton, Gtk.PositionType.RIGHT, 1, 1)

        self.scrollable_treelist.add(self.treeview)

        self.show_all()

    def fetch_list(self):
        """Fetch list from github project repo and parse JSON"""

        if True == Liferea.NetworkMonitor.is_online():
            list_url = "https://raw.githubusercontent.com/lwindolf/liferea/master/plugins/plugin-list.json"
            data = None
            req = urllib.request.Request(list_url)
            resp = urllib.request.urlopen(req).read()
            return json.loads(resp.decode('utf-8'))
        else:
            return {'plugins': []}

    def category_filter_func(self, model, iter, data):
        """Tests if the category in the row is the one in the filter"""
        if self.current_filter_category is None or self.current_filter_category == "All":
            return True
        else:
            return model[iter][3] == self.current_filter_category

    def on_catcombo_changed(self, combo):
        active_row_id = combo.get_active()
        if active_row_id != -1:
            self.current_filter_category = ("All", "Advanced", "Menu", "Notifications")[active_row_id]
        else:
            self.current_filter_category = None
        self.category_filter.refilter()

    def on_selection_changed(self, selection):
        self.update_buttons()

    def update_buttons(self):
        model, treeiter = self.treeview.get_selection().get_selected()
        if treeiter != None:
            self._installButton.set_sensitive(model[treeiter][0] == 0)
            self._uninstallButton.set_sensitive(model[treeiter][0] != 0)

    def get_plugin_by_name(self, plugin):
        plugin_info = None

        # Get infos on selected plugin
        for ref in self._plugin_list['plugins']:
            if plugin == next(iter(ref)):
                plugin_info = ref[plugin]

        if plugin_info == None:
            raise Exception("Fatal: Failed to get plugin infos from tree list!")

        return plugin_info

    def install(self, path=None, column=None, user_data=None):
        selection = self.treeview.get_selection()
        model, treeiter = selection.get_selected()
        if treeiter != None:
            model[treeiter][0] = self.install_plugin(self.get_plugin_by_name(model[treeiter][2]))

        self.update_buttons()

    def uninstall(self, path=None, column=None, user_data=None):
        selection = self.treeview.get_selection()
        model, treeiter = selection.get_selected()
        if treeiter != None:
            model[treeiter][0] = self.uninstall_plugin(self.get_plugin_by_name(model[treeiter][2]))

        self.update_buttons()

    def show_message(self, message, error = False, buttons = Gtk.ButtonsType.CLOSE):
        dialog = Gtk.MessageDialog(self, Gtk.DialogFlags.DESTROY_WITH_PARENT, (Gtk.MessageType.ERROR if error else Gtk.MessageType.INFO), buttons, message)
        response = Gtk.Dialog.run(dialog)
        Gtk.Widget.destroy(dialog)
        return response

    def install_plugin(self, plugin_info):
        """Fetches github repo for a plugin and tries to install the plugin"""
        DIR_NAME = "/tmp/liferea-pluginbrowser-%s" % plugin_info['module']
        if os.path.isdir(DIR_NAME):
            shutil.rmtree(DIR_NAME)
        os.mkdir(DIR_NAME)
        os.chdir(DIR_NAME)

        # Check and install dependencies
        if 'deps' in plugin_info:
            # First check if package manager is available
            try:
                # Run package manager check command
                p = subprocess.Popen(' '.join(plugin_info['deps']['pkgmgr']['check']), shell=True)
                p.wait()
                pkg_mgr_missing = p.returncode
            except:
                pkg_mgr_missing = 0

            if pkg_mgr_missing != 0:
                self.show_message(_("Missing package manager '%s'. Cannot check nor install necessary dependencies!") % plugin_info['deps']['pkgmgr']['name'], True)
                return False

            try:

                # For each package run package check command
                for pkg in plugin_info['deps']['packages']:
                    print("Checking for %s..."%pkg)
                    cmd = plugin_info['deps']['pkgmgr']['checkPkg'][:]
                    cmd.append(pkg)
                    print("   -> %s"%' '.join(cmd))
                    p = subprocess.Popen(cmd)
                    p.wait()
                    if p.returncode != 0:
                        cmd = plugin_info['deps']['pkgmgr']['installPkg'][:]
                        cmd.append(pkg)
                        response = self.show_message(_("Missing package '%s'. Do you want to install it? (Will run '%s')") % (pkg, ' '.join(cmd)), False, Gtk.ButtonsType.OK_CANCEL)
                        if Gtk.ResponseType.OK != response:
                            return False

                        p = subprocess.Popen(cmd)
                        p.wait()
                        if p.returncode != 0:
                            self.show_message(_("Package installation failed (%s)! Check console output for further problem details!") % sys.exc_info()[0], True)
                            return False
            except:
                self.show_message(_("Failed to check plugin dependencies (%s)!") % sys.exc_info()[0], True)
                return False

        # Git checkout
        try:
            p = subprocess.Popen(["git", "clone", "https://github.com/%s" % plugin_info['source'], "."])
            p.wait()
            # FIXME: error checking
        except FileNotFoundError:
            self.show_message(_("Command \"git\" not found, please install it!"), True)
            return False

        # Now copy the plugin source, there are 2 variants:
        # - either there is a subdir named after the module   <module>/
        # - or there is a module file with language extension <module>.py
        try:
            src_dir = '%s/%s' % (DIR_NAME, plugin_info['module'])
            if os.path.isdir(src_dir):
                print(_("Copying %s to %s") % (src_dir, self.target_dir))
                shutil.copytree(src_dir, "%s/%s" % (self.target_dir, plugin_info['module']))
        except:
            self.show_message(_("Failed to copy plugin directory (%s)!") % sys.exc_info()[0], True)
            return False

        # FIXME: support other plugin languages besides Python
        try:
            src_file = '%s/%s.py' % (DIR_NAME, plugin_info['module'])
            if os.path.isfile(src_file):
                shutil.copy(src_file, self.target_dir)
        except:
            self.show_message(_("Failed to copy plugin .py file (%s)!") % sys.exc_info()[0], True)
            return False

        # Copy .plugin file if it is not inside the plugin source itself
        try:
            # Do not copy .plugin file if it is inside plugin source
            src_file = '%s/%s/%s.plugin' % (DIR_NAME, plugin_info['module'], plugin_info['module'])
            if not os.path.isfile(src_file):
                shutil.copy('%s/%s.plugin' % (DIR_NAME, plugin_info['module']), self.target_dir)
        except:
            self.show_message(_("Failed to copy .plugin file (%s)!") % sys.exc_info()[0], True)
            return False

        # Optional: find and install schemata
        try:
            schema_found = False

            # We allow for schema either at top level or in first subdir
            for schema_file in glob.iglob('%s/**/*.gschema.xml' % (DIR_NAME), recursive = True):
                schema_found = True
                if not os.path.isdir(self.schema_dir):
                    print(_('Creating schema directory %s') % self.schema_dir)
                    os.makedirs(self.schema_dir)
                print(_('Installing schema %s') % schema_file)
                shutil.copy(schema_file,self.schema_dir)

            if schema_found:
                print(_('Compiling schemas...'))
                p = subprocess.Popen(["glib-compile-schemas", self.schema_dir])
                p.wait()
	        	# FIXME: error checking
        except:
            self.show_message(_("Failed to install schema files (%s)!") % sys.exc_info()[0], True)
            return False

        # Enable plugin (for next restart)
        try:
            settings = Gio.Settings.new(self.SCHEMA_ID)
            current_plugins = settings.get_strv('active-plugins')
            current_plugins.append(plugin_info['module'])
            settings.set_strv('active-plugins', current_plugins)
        except:
            self.show_message(_("Failed to enable plugin (%s)!") % sys.exc_info()[0], True)
            return False

        # Cleanup
        shutil.rmtree(DIR_NAME)

        self.show_message(_("Plugin '%s' is now installed. Ensure to restart Liferea!") % plugin_info['module'])
        return True

    def uninstall_plugin(self, plugin_info):
        """Deletes all possible files and directories that might belong to the plugin"""
        error = False

        # Remove plugin from active-plugins
        try:
            settings = Gio.Settings.new(self.SCHEMA_ID)
            current_plugins = settings.get_strv('active-plugins')
            current_plugins.remove(plugin_info['module'])
            settings.set_strv('active-plugins', current_plugins)
        except:
            print(_("Failed to disable plugin (%s)!") % sys.exc_info()[0])
            error = True

        # Drop plugin dir (for directory plugins)
        src_dir = '%s/%s' % (self.target_dir, plugin_info['module'])
        try:
            if os.path.isdir(src_dir):
                print(_("Deleting '%s'") % src_dir)
                shutil.rmtree(src_dir)
        except:
            print(_("Failed to remove directory '%s' (%s)!") % (src_dir, sys.exc_info()[0]))
            error = True

        # Drop plugin source file (for single file plugins)
        src_file = '%s/%s.py' % (self.target_dir, plugin_info['module'])
        try:
            if os.path.isfile(src_file):
                print("Deleting '%s'" % src_file)
                os.remove(src_file)
        except:
            print(_("Failed to remove .py file!"))
            error = True

        # Drop plugin file
        src_file = '%s/%s/%s.plugin' % (self.target_dir, plugin_info['module'], plugin_info['module'])
        try:
            if os.path.isfile(src_file):
                print(_("Deleting '%s'") % src_file)
                os.remove(src_file)
        except:
            print(_("Failed to remove .plugin file!"))
            error = True

        # FIXME: Drop plugin schema files
        #
        # Removing schemas does not really work, as the schema files can have
        # arbitrary names...

        if error:
            self.show_message(_("Sorry! Plugin removal failed!."), True)
        else:
            self.show_message(_("Plugin was removed. Please restart Liferea once for it to take full effect!."), False)
