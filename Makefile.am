## Process this file with automake to produce Makefile.in

SUBDIRS = src po pixmaps

desktop_files=liferea.desktop

Applicationsdir = $(datadir)/applications
Applications_DATA = $(desktop_files)

man_MANS = liferea.1

schemadir   = @GCONF_SCHEMA_FILE_DIR@
schema_DATA = liferea.schemas

EXTRA_DIST = \
	autogen.sh \
	liferea.glade \
	liferea.gladep \
	liferea.spec \
	liferea-SuSE.spec \
	liferea.1 \
	src/liferea \
	$(desktop_files) \
	$(schema_DATA)

install-data-local:
if GCONF_SCHEMAS_INSTALL
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/$(schema_DATA)
endif
	@$(NORMAL_INSTALL)
	if test -d $(srcdir)/css; then \
	  $(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/css; \
          $(INSTALL_DATA) $(srcdir)/css/liferea.css $(DESTDIR)$(pkgdatadir)/css; \
	fi

dist-hook:
	if test -d $(srcdir)/css; then \
	  $(mkinstalldirs) $(distdir)/css; \
          $(INSTALL_DATA) $(srcdir)/css/liferea.css $(distdir)/css; \
	fi

DISTCLEANFILES = liferea-SuSE.spec liferea.spec
