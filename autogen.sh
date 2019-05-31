#!/bin/sh

if ! command autoreconf; then
	echo "ERROR: You need to install autoconf!"
	exit 1
fi

if ! command intltoolize; then
	echo "ERROR: You need to install intltool!"
	exit 1
fi

if ! command libtoolize; then
	echo "ERROR: You need to install libtool!"
	exit 1
fi

autoreconf -i
intltoolize
if test -z "$NOCONFIGURE"; then
./configure "$@"
fi
