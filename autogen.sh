#!/bin/sh

tmp=`which autoreconf`
if [ "$tmp" = "" ]; then
	echo "ERROR: You need to install autoconf!"
	exit 1
fi

tmp=`which intltoolize`
if [ "$tmp" = "" ]; then
	echo "ERROR: You need to install intltool!"
	exit 1
fi

tmp=`which libtoolize`
if [ "$tmp" = "" ]; then
	echo "ERROR: You need to install libtool!"
	exit 1
fi

rm -f configure aclocal.m4 autom4te.cache/*
rmdir --ignore-fail-on-non-empty autom4te.cache
find . -iname "Makefile" -o -iname "Makefile.in" -o -iname "Makefile.in.in" -type f  -exec rm -f '{}' +
autoreconf -i
intltoolize
if test -z "$NOCONFIGURE"; then
./configure "$@"
fi
