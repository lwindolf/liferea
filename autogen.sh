#!/bin/sh

autoreconf -i || echo "ERROR: You need to install autoconf!" && exit 1
intltoolize || echo "ERROR: You need to install intltool!" && exit 1
./configure "$@"

