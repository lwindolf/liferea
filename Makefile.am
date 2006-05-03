## Process this file with automake to produce Makefile.in

SUBDIRS = doc man opml pixmaps po src

desktop_files=liferea.desktop

Applicationsdir = $(datadir)/applications
Applications_DATA = $(desktop_files)

schemadir   = @GCONF_SCHEMA_FILE_DIR@
schema_DATA = liferea.schemas

EXTRA_DIST = \
	feedbag.xpi \
	liferea.glade \
	liferea.gladep \
	liferea.spec \
	liferea-SuSE.spec \
	src/liferea \
	$(desktop_files) \
	$(schema_DATA)

install-data-local:
if GCONF_SCHEMAS_INSTALL
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_srcdir)/$(schema_DATA)
endif
	@$(NORMAL_INSTALL)
	if test -d $(srcdir)/css; then \
	  $(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/css; \
          $(INSTALL_DATA) $(srcdir)/css/liferea.css $(DESTDIR)$(pkgdatadir)/css; \
          $(INSTALL_DATA) $(srcdir)/css/liferea2.css $(DESTDIR)$(pkgdatadir)/css; \
          $(INSTALL_DATA) $(srcdir)/css/adblock.css $(DESTDIR)$(pkgdatadir)/css; \
	fi
	if test -d $(srcdir)/dtd; then \
	  $(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/dtd; \
          $(INSTALL_DATA) $(srcdir)/dtd/html.ent $(DESTDIR)$(pkgdatadir)/dtd; \
	fi

dist-hook:
	if test -d $(srcdir)/css; then \
	  $(mkinstalldirs) $(distdir)/css; \
          $(INSTALL_DATA) $(srcdir)/css/liferea.css $(distdir)/css; \
          $(INSTALL_DATA) $(srcdir)/css/liferea2.css $(distdir)/css; \
          $(INSTALL_DATA) $(srcdir)/css/adblock.css $(distdir)/css; \
	fi
	if test -d $(srcdir)/dtd; then \
	  $(mkinstalldirs) $(distdir)/dtd; \
          $(INSTALL_DATA) $(srcdir)/dtd/html.ent $(distdir)/dtd; \
	fi

DISTCLEANFILES = liferea-SuSE.spec liferea.spec
