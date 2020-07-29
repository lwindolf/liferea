#!/bin/bash

if command -vp gla11y >/dev/null; then
	if gla11y ../../glade/*.ui | grep FATAL; then
		printf "ERROR: Fatal accessibility issues were found!\n"
		exit 1
	else
		printf "Accessibility looks fine.\n"
	fi
else
	printf "WARNING: gla11y is not installed, cannot test accessibility\n"
fi
