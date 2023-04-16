## Process this file with automake to produce Makefile.in

SUBDIRS = doc man opml pixmaps po src xslt glade

desktop_in_files = net.sourceforge.liferea.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

dbusservicedir = $(datadir)/dbus-1/services
dbusservice_DATA = net.sourceforge.liferea.service

net.sourceforge.liferea.service: Makefile
	$(AM_V_GEN) (echo '[D-BUS Service]'; \
		echo 'Name=net.sourceforge.liferea'; \
		echo 'Exec=${bindir}/liferea --gapplication-service') > $@.tmp && \
		mv $@.tmp $@


appdatadir = $(datadir)/metainfo
appdata_in_files = net.sourceforge.liferea.appdata.xml.in
appdata_DATA = $(appdata_in_files:.xml.in=.xml)

@INTLTOOL_XML_RULE@

cssdir = $(pkgdatadir)/css
css_DATA = \
	css/liferea.css \
	css/user.css

jsdir = $(pkgdatadir)/js
js_DATA = \
	js/gresource.xml \
	js/htmlview.js \
	js/Readability-readerable.js \
	js/Readability.js

dtddir = $(pkgdatadir)/dtd
dtd_DATA = dtd/html.ent

plugindir = $(pkglibdir)/plugins
plugin_DATA = \
	plugins/add-bookmark-site.py \
	plugins/add-bookmark-site.plugin \
	plugins/add-bookmark-site.ui \
	plugins/bold-unread.py \
	plugins/bold-unread.plugin \
	plugins/getfocus.py \
	plugins/getfocus.plugin \
	plugins/gnome-keyring.py \
	plugins/gnome-keyring.plugin \
	plugins/headerbar.py \
	plugins/headerbar.plugin \
	plugins/libnotify.py \
	plugins/libnotify.plugin \
	plugins/media-player.py \
	plugins/media-player.plugin \
	plugins/plugin-installer.py \
	plugins/plugin-installer.plugin \
	plugins/trayicon.py \
	plugins/trayicon.plugin

gsettings_SCHEMAS = net.sf.liferea.gschema.xml
@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

data_convertdir = $(datadir)/GConf/gsettings
dist_data_convert_DATA = liferea.convert

EXTRA_DIST = \
	net.sf.liferea.gschema.xml.in \
	po/liferea.pot \
	$(desktop_in_files) \
	$(desktop_DATA) \
	$(schema_DATA) \
	$(css_DATA) \
	$(js_DATA) \
	$(dtd_DATA) \
	$(plugin_DATA) \
	$(gsettings_SCHEMAS) \
	$(appdata_in_files) \
	$(appdata_DATA)

DISTCLEANFILES = \
	liferea.desktop \
	net.sourceforge.liferea.service \
	intltool-extract \
	intltool-merge \
	intltool-update

CLEANFILES = \
	$(gsettings_SCHEMAS) \
	$(appdata_DATA)

po/liferea.pot:
	cd po && $(MAKE) liferea.pot

test:
	cd src/tests && make test
