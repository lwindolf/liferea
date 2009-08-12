#!/bin/sh

autoreconf -i
intltoolize
./configure "$@"

