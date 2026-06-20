# -*- coding: utf-8 -*-
#
# pythonconsole.py
#
# Copyright (C) 2006 - Steve Frécinaux
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# The Rhythmbox authors hereby grant permission for non-GPL compatible
# GStreamer plugins to be used and distributed together with GStreamer
# and Rhythmbox. This permission is above and beyond the permissions granted
# by the GPL license by which Rhythmbox is covered. If you modify this code
# you may extend this exception to your version of the code, but you are not
# obligated to do so. If you do not wish to do so, delete this exception
# statement from your version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

# Parts from "Interactive Python-GTK Console" (stolen from epiphany's console.py)
#     Copyright (C), 1998 James Henstridge <james@daa.com.au>
#     Copyright (C), 2005 Adam Hooper <adamh@densi.com>
# Bits from gedit Python Console Plugin
#     Copyrignt (C), 2005 Raphaël Slinckx
# Adapted for Liferea
#     Copyright (C), 2018 Sven Arvidsson <sa@whiz.se>

import sys
import re
import traceback

from gi.repository import GLib, Gtk, Gdk, Pango

class PythonConsole(Gtk.ScrolledWindow):

    def get_end_iter(self):
        return self.view.get_buffer().get_end_iter()

    def get_iter_at_mark(self, mark):
        return self.view.get_buffer().get_iter_at_mark(mark)

    def __init__(self, namespace = {}, destroy_cb = None):
        Gtk.ScrolledWindow.__init__(self)

        self.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        self.view = Gtk.TextView()
        self.view.set_monospace(True)
        self.view.set_editable(True)
        self.view.set_wrap_mode(Gtk.WrapMode.CHAR)
        self.view.set_cursor_visible(True)
        self.set_child(self.view)

        buffer = self.view.get_buffer()
        self.normal = buffer.create_tag("normal")
        self.error  = buffer.create_tag("error")
        self.error.set_property("foreground", "red")
        self.command = buffer.create_tag("command")
        self.command.set_property("foreground", "blue")

        self.__spaces_pattern = re.compile(r'^\s+')     
        self.namespace = namespace

        self.destroy_cb = destroy_cb

        self.block_command = False

        # Init first line
        buffer.create_mark("input-line", self.get_end_iter(), True)
        buffer.insert(self.get_end_iter(), ">>> ")
        buffer.create_mark("input", self.get_end_iter(), True)

        # Init history
        self.history = ['']
        self.history_pos = 0
        self.current_command = ''
        self.namespace['__history__'] = self.history

        # Set up hooks for standard output.
        self.stdout = gtkoutfile(self, sys.stdout.fileno(), self.normal)
        self.stderr = gtkoutfile(self, sys.stderr.fileno(), self.error)

        # Signals
        key_controller = Gtk.EventControllerKey.new ()
        key_controller.connect("key-pressed", self.__key_pressed_cb)
        self.view.add_controller(key_controller)
        buffer.connect("mark-set", self.__mark_set_cb)


    def __key_pressed_cb(self, controller, keyval, keycode, state):
        view = self.view

        # FIXME: only run on blank line
        if keyval == Gdk.KEY_d and \
           (state & Gdk.ModifierType.CONTROL_MASK):
            self.destroy()
            return True

        # FIXME: Hacky, figure out how to insert the same amount of
        # new lines as are visible in TextView.
        elif keyval == Gdk.KEY_l and \
           (state & Gdk.ModifierType.CONTROL_MASK):
            buffer = view.get_buffer()

            inp_mark = buffer.get_mark("input")
            cur = self.get_end_iter()
            for i in range(0, 24):
                buffer.insert(cur, "\n")
            buffer.insert(cur, ">>> ")
            cur = self.get_end_iter()
            buffer.move_mark_by_name("input", cur)
            GLib.idle_add(self.scroll_to_end)
            return True

        # FIXME: Also Ctrl+E for end of line, but for now this works
        # fine if GTK is set to use Emacs bindings...
        elif keyval == Gdk.KEY_a and \
             (state & Gdk.ModifierType.CONTROL_MASK):
            # Go to the begin of the command instead of the begin of
            # the line
            buffer = view.get_buffer()
            inp = self.get_iter_at_mark(buffer.get_mark("input"))
            if state & Gdk.ModifierType.SHIFT_MASK:
                buffer.move_mark_by_name("insert", inp)
            else:
                buffer.place_cursor(inp)
            return True
        
        elif keyval == Gdk.KEY_Return and \
             (state & Gdk.ModifierType.CONTROL_MASK):
            # Get the command
            buffer = view.get_buffer()
            inp_mark = buffer.get_mark("input")
            inp = self.get_iter_at_mark(inp_mark)
            cur = self.get_end_iter()
            line = buffer.get_text(inp, cur, True)
            self.current_command = self.current_command + line + "\n"
            self.history_add(line)

            # Prepare the new line
            cur = self.get_end_iter()
            buffer.insert(cur, "\n... ")
            cur = self.get_end_iter()
            buffer.move_mark(inp_mark, cur)
            
            # Keep indentation of precendent line
            spaces = re.match(self.__spaces_pattern, line)
            if spaces is not None:
                buffer.insert(cur, line[spaces.start() : spaces.end()])
                cur = self.get_end_iter()
                
            buffer.place_cursor(cur)
            GLib.idle_add(self.scroll_to_end)
            return True
        
        elif keyval == Gdk.KEY_Return:
            # Get the marks
            buffer = view.get_buffer()
            lin_mark = buffer.get_mark("input-line")
            inp_mark = buffer.get_mark("input")

            # Get the command line
            inp = self.get_iter_at_mark(inp_mark)
            cur = self.get_end_iter()
            line = buffer.get_text(inp, cur, True)
            self.current_command = self.current_command + line + "\n"
            self.history_add(line)

            # Make the line blue
            lin = self.get_iter_at_mark(lin_mark)
            buffer.apply_tag(self.command, lin, cur)
            buffer.insert(cur, "\n")
            
            cur_strip = self.current_command.rstrip()

            if cur_strip.endswith(":") \
            or (self.current_command[-2:] != "\n\n" and self.block_command):
                # Unfinished block command
                self.block_command = True
                com_mark = "... "
            elif cur_strip.endswith("\\"):
                com_mark = "... "
            else:
                # Eval the command
                self.__run(self.current_command)
                self.current_command = ''
                self.block_command = False
                com_mark = ">>> "

            # Prepare the new line
            cur = self.get_end_iter()
            buffer.move_mark(lin_mark, cur)
            buffer.insert(cur, com_mark)
            cur = self.get_end_iter()
            buffer.move_mark(inp_mark, cur)
            buffer.place_cursor(cur)
            GLib.idle_add(self.scroll_to_end)
            return True

        elif keyval == Gdk.KEY_KP_Down or \
             keyval == Gdk.KEY_Down:
            # Next entry from history
            self.history_down()
            GLib.idle_add(self.scroll_to_end)
            return True

        elif keyval == Gdk.KEY_KP_Up or \
             keyval == Gdk.KEY_Up:
            # Previous entry from history
            self.history_up()
            GLib.idle_add(self.scroll_to_end)
            return True

        elif keyval == Gdk.KEY_KP_Left or \
             keyval == Gdk.KEY_Left or \
             keyval == Gdk.KEY_BackSpace:
            buffer = view.get_buffer()
            inp = self.get_iter_at_mark(buffer.get_mark("input"))
            cur = self.get_iter_at_mark(buffer.get_insert())
            return inp.compare(cur) == 0

        elif keyval == Gdk.KEY_Home:
            # Go to the begin of the command instead of the begin of
            # the line
            buffer = view.get_buffer()
            inp = self.get_iter_at_mark(buffer.get_mark("input"))
            if state & Gdk.ModifierType.SHIFT_MASK:
                buffer.move_mark_by_name("insert", inp)
            else:
                buffer.place_cursor(inp)
            return True

        return False
        
    def __mark_set_cb(self, buffer, iter, name):
        input = self.get_iter_at_mark(buffer.get_mark("input"))
        pos   = self.get_iter_at_mark(buffer.get_insert())
        self.view.set_editable(pos.compare(input) != -1)

    def get_command_line(self):
        buffer = self.view.get_buffer()
        inp = self.get_iter_at_mark(buffer.get_mark("input"))
        cur = self.get_end_iter()
        return buffer.get_text(inp, cur, True)
    
    def set_command_line(self, command):
        buffer = self.view.get_buffer()
        mark = buffer.get_mark("input")
        inp = self.get_iter_at_mark(mark)
        cur = self.get_end_iter()
        buffer.delete(inp, cur)
        buffer.insert(inp, command)
        buffer.select_range(self.get_iter_at_mark(mark),
                            self.get_end_iter())
        self.view.grab_focus()
    
    def history_add(self, line):
        if line.strip() != '':
            self.history_pos = len(self.history)
            self.history[self.history_pos - 1] = line
            self.history.append('')
    
    def history_up(self):
        if self.history_pos > 0:
            self.history[self.history_pos] = self.get_command_line()
            self.history_pos = self.history_pos - 1
            self.set_command_line(self.history[self.history_pos])
            
    def history_down(self):
        if self.history_pos < len(self.history) - 1:
            self.history[self.history_pos] = self.get_command_line()
            self.history_pos = self.history_pos + 1
            self.set_command_line(self.history[self.history_pos])
    
    def scroll_to_end(self):
        iter = self.get_end_iter()
        self.view.scroll_to_iter(iter, 0.0, False, 0.5, 0.5)
        return False

    def write(self, text, tag = None):
        buffer = self.view.get_buffer()
        if tag is None:
            buffer.insert(self.get_end_iter(), text)
        else:
            iter = self.get_end_iter()
            offset = iter.get_offset()
            buffer.insert(iter, text)

            start_iter = Gtk.TextIter()
            start_iter = buffer.get_iter_at_offset(offset)
            buffer.apply_tag(tag, start_iter, self.get_end_iter())
        GLib.idle_add(self.scroll_to_end)

    def eval(self, command, display_command = False):
        buffer = self.view.get_buffer()
        lin = buffer.get_mark("input-line")
        buffer.delete(self.get_iter_at_mark(lin),
                  self.get_end_iter())

        if isinstance(command, list) or isinstance(command, tuple):
            for c in command:
                if display_command:
                    self.write(">>> " + c + "\n", self.command)
                self.__run(c)
        else:
            if display_command:
                self.write(">>> " + c + "\n", self.command)
            self.__run(command)

        cur = self.get_end_iter()
        buffer.move_mark_by_name("input-line", cur)
        buffer.insert(cur, ">>> ")
        cur = self.get_end_iter()
        buffer.move_mark_by_name("input", cur)
        self.view.scroll_to_iter(self.get_end_iter(), 0.0, False, 0.5, 0.5)
    
    def __run(self, command):
        sys.stdout, self.stdout = self.stdout, sys.stdout
        sys.stderr, self.stderr = self.stderr, sys.stderr
        
        try:
            try:
                r = eval(command, self.namespace, self.namespace)
                if r is not None:
                    print(repr(r))
            except SyntaxError:
                exec(command, self.namespace)
        except:
            if hasattr(sys, 'last_type') and sys.last_type == SystemExit:
                self.destroy()
            else:
                traceback.print_exc()
        finally:
            sys.stdout, self.stdout = self.stdout, sys.stdout
            sys.stderr, self.stderr = self.stderr, sys.stderr


    def destroy(self):
        if self.destroy_cb is not None:
            self.destroy_cb()
        
class gtkoutfile:
    """A fake output file object.  It sends output to a TK test widget,
    and if asked for a file number, returns one set on instance creation"""
    def __init__(self, console, fn, tag):
        self.fn = fn
        self.console = console
        self.tag = tag
    def close(self):         pass
    def flush(self):         pass
    def fileno(self):        return self.fn
    def isatty(self):        return 0
    def read(self, a):       return ''
    def readline(self):      return ''
    def readlines(self):     return []
    def write(self, s):      self.console.write(s, self.tag)
    def writelines(self, l): self.console.write(l, self.tag)
    def seek(self, a):       raise IOError((29, 'Illegal seek'))
    def tell(self):          raise IOError((29, 'Illegal seek'))
    truncate = tell
