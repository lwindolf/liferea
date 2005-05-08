## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	$(PACKAGE_CFLAGS) \
	$(DBUS_CFLAGS)

noinst_LIBRARIES = liblinet.a


# if USE_LIBCURL
# liblinet_a_CFLAGS = @PACKAGE_CFLAGS@ \
# 	@CURL_CFLAGS@
# liblinet_a_SOURCES = curl.c downloadlib.h
# else
liblinet_a_CFLAGS = $(PACKAGE_CFLAGS)
liblinet_a_SOURCES = \
	compat.h \
	conversions.c conversions.h \
	cookies.c cookies.h \
	digcalc.c digcalc.h \
	md5.c md5.h \
	netio.c netio.h \
	net-support.c net-support.h \
	os-support.c os-support.h \
	zlib_interface.c zlib_interface.h \
	downloadlib.h
# endif
