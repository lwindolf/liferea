## Process this file with automake to produce Makefile.in

SUBDIRS = doc man opml pixmaps po src xslt

ACLOCAL_AMFLAGS = -I m4

desktop_in_files = liferea.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

gladedir = $(pkgdatadir)
glade_DATA = liferea.glade

schemadir   = @GCONF_SCHEMA_FILE_DIR@
schema_DATA = liferea.schemas

cssdir = $(pkgdatadir)/css
css_DATA = \
	css/liferea.css \
	css/adblock.css

mediadir = $(pkgdatadir)/media
media_DATA = \
	media/xspf_player_slim.swf \
	media/xspf_license.txt

dtddir = $(pkgdatadir)/dtd
dtd_DATA = dtd/html.ent

EXTRA_DIST = \
	$(glade_DATA) \
	liferea.gladep \
	liferea.spec \
	src/liferea \
	src/liferea-add-feed \
	po/liferea.pot \
	$(desktop_in_files) \
	$(desktop_DATA) \
	$(schema_DATA) \
	$(css_DATA) \
	$(dtd_DATA) \
	$(media_DATA)

install-data-local:
if GCONF_SCHEMAS_INSTALL
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_srcdir)/$(schema_DATA)
endif
	@$(NORMAL_INSTALL)


DISTCLEANFILES = \
	liferea.spec \
	liferea.desktop \
	intltool-extract \
	intltool-merge \
	intltool-update
