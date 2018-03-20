#!/usr/bin/python3

import os
import subprocess

datadir = os.path.join (os.environ['MESON_INSTALL_PREFIX'], 'share')
icondir = os.path.join(datadir, 'icons', 'hicolor')
schemadir = os.path.join(datadir, 'glib-2.0', 'schemas')
desktopdir = os.path.join(datadir, 'applications')

if not os.environ.get('DESTDIR'):
	print('Compiling gsettings schemas...')
	subprocess.call(['glib-compile-schemas', schemadir])

	print('Updating icon cache...')
	subprocess.call(['gtk-update-icon-cache', '-ft', icondir])

	print('Updating desktop database...')
	subprocess.call(['update-desktop-database', desktopdir])
else:
	print ('*** Icon cache not updated.  After install, run this:')
	print ('*** gtk-gtk_update_icon_cache -ft ' + icondir)
