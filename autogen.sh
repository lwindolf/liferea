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

autoreconf -i
intltoolize
if test -z "$NOCONFIGURE"; then
./configure "$@"
fi
