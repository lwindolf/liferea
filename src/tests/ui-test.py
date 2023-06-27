#! /usr/bin/python3

# Wrapping copied from:
# https://salsa.debian.org/ci-team/autopkgtest-help/-/issues/6

# Copyright (c) 2021 Johannes Schauer Marin Rodrigues <josch@mister-muffin.de>
# Copyright (c) 2023 Paul Gevers <elbrus@debian.org>

# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public License
# along with this library; see the file COPYING.LIB.  If not, write to
# the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.


import atexit
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from time import sleep

debug = False
try:
    uname = os.uname()
    if uname.nodename == 'mulciber':
        debug = True
except:
    pass

def start_dbus():
    pid_r, pid_w = os.pipe()
    addr_r, addr_w = os.pipe()
    os.set_inheritable(pid_w, True)
    os.set_inheritable(addr_w, True)
    subprocess.check_call(
        [
            "dbus-daemon",
            "--fork",
            "--session",
            "--print-address=%d" % addr_w,
            "--print-pid=%d" % pid_w,
        ],
        close_fds=False,
    )
    os.close(pid_w)
    os.close(addr_w)
    dbuspid = int(os.read(pid_r, 4096).decode("ascii").strip())
 #   atexit.register(os.kill, dbuspid, signal.SIGTERM)
    dbusaddr = os.read(addr_r, 4096).decode("ascii").strip()
    os.close(pid_r)
    os.close(addr_r)
    os.environ["DBUS_SESSION_BUS_ADDRESS"] = dbusaddr


def setup_dirs():
    if "AUTOPKGTEST_TMP" not in os.environ:
        tmpdir = tempfile.mkdtemp()
#        atexit.register(shutil.rmtree, tmpdir)
        os.environ["AUTOPKGTEST_TMP"] = tmpdir
    home = os.environ["AUTOPKGTEST_TMP"] + "/home"
    for v, p in [
        ("HOME", home),
        ("XDG_CONFIG_HOME", home + "/.config"),
        ("XDG_DATA_HOME", home + "/.local/share"),
        ("XDG_CACHE_HOME", home + "/.cache"),
        ("XDG_RUNTIME_DIR", home + "/runtime"),
    ]:
        os.environ[v] = p
        os.makedirs(p)


def enable_a11y():
    subprocess.check_call(
        [
            "gsettings",
            "set",
            "org.gnome.desktop.interface",
            "toolkit-accessibility",
            "true",
        ]
    )


def main():
    os.environ["LC_ALL"] = 'C.UTF-8'
    os.environ['LANG'] = 'en_US.UTF-8'
    os.environ['QT_ACCESSIBILITY'] = '1'
    if "DISPLAY" not in os.environ:
        subprocess.check_call(
            ["xvfb-run", "--auto-servernum"] + sys.argv, env={"XAUTHORITY": "/dev/null"}
        )
        exit()
    setup_dirs()
    start_dbus()
    enable_a11y()
    from dogtail.config import config
    config.debugSleep = True
    config.debugSearching = True
    config.debugTranslation = True
    config.logDebugToFile = False
    from dogtail.utils import run
    from dogtail.procedural import focus, click
    from dogtail.tree import root
    run('liferea')
    sleep(2) # Just for safety; without this it works when triggered interactively, but inside autopkgtest things go wrong

    click('View')
    click('Fullscreen')

    if debug: sleep(2)
    click('Help')
    click('Contents')

    if debug: sleep(2)
    click('Help')
    click('Quick Reference')

    if debug: sleep(2)
    click('Help')
    click('FAQ')

    if debug: sleep(2)
    click('Help')
    click('About')
    if debug: sleep(2)
    click('Credits')
    if debug: sleep(2)
    click('Close')

    click('Item')
    click('Next Unread Item')

    click('Planet Debian')
    click('Feed')
    click('Properties')
    click('Source')
    focus.text()

    if debug: sleep(2)
    liferea = root.application('liferea')
    if os.uname().machine != "s390x":
        # Crashes on s390x (but who would be using liferea on s390x anyways)
        liferea.dump()

    # Quit
    if debug: sleep(2)
    click('Subscriptions')
    click('Quit')


if __name__ == "__main__":
    main()
