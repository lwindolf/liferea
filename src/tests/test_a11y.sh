#!/bin/bash

if command -vp gla11y >/dev/null; then
	gla11y ../../glade/*.ui | grep FATAL && exit 1
else
	printf "WARNING: gla11y is not installed, cannot test accessibility"
fi
