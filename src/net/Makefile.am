## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@
	
noinst_LIBRARIES = liblinet.a

liblinet_a_CFLAGS = @PACKAGE_CFLAGS@ -Wall
liblinet_a_SOURCES = \
	compat.h \
	conversions.c conversions.h \
	digcalc.c digcalc.h \
	md5.c md5.h \
	netio.c netio.h \
	net-support.c net-support.h \
	os-support.c os-support.h
