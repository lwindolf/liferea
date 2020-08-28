#!/bin/bash

if command -vp gla11y >/dev/null; then
	output=$(gla11y ../../glade/*.ui)
	echo "$output"

	# For now lets prevent only fatals
	if echo "$output" | grep -q FATAL; then
		printf "ERROR: Fatal accessibility issues were found!\n"
		exit 1
	else
		printf "Accessibility looks fine.\n"
	fi
else
	printf "WARNING: gla11y is not installed, cannot test accessibility\n"
fi
