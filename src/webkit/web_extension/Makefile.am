webextension_LTLIBRARIES = liblifereawebextension.la

webextensiondir = $(pkglibdir)/web-extension

liblifereawebextension_la_SOURCES = web_extension_main.c liferea_web_extension.c liferea_web_extension.h liferea_web_extension_names.h
liblifereawebextension_la_CFLAGS = $(WEB_EXTENSION_CFLAGS)
liblifereawebextension_la_LIBADD = $(WEB_EXTENSION_LIBS)
liblifereawebextension_la_LDFLAGS = -module -avoid-version -no-undefined
