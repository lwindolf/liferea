## Process this file with automake to produce Makefile.in

SUBDIRS = src po pixmaps

desktop_files=liferea.desktop

Applicationsdir = $(datadir)/applications
Applications_DATA = $(desktop_files)

EXTRA_DIST = \
	autogen.sh \
	.glade \
	.gladep \
	liferea.glade \
	liferea.gladep \
	liferea.spec \
	src/liferea \
	$(desktop_files)

install-data-local:
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

