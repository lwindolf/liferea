#!/bin/bash

# Simple wrapper to valgrind/memcheck the test cases

# $@ 	binaries to check

error=0

if command -v valgrind >/dev/null; then
	for tool in $@; do
		details=$(
			valgrind -q --leak-check=full --suppressions="$(dirname "$0")/memcheck.supp" --error-markers=begin,end "./$tool" 2>&1
		)
		output=$(
			echo "$details" |\
			grep -A1 "== begin" |\
			egrep -v "== begin|possibly lost|^--$"
		)
		if [ "$output" != "" ]; then
			error=1
			echo "ERROR: memcheck reports problems for '$tool'!"
			echo "Relevant error lines are:"
			echo
			echo "$output"
			echo
			echo "Full valgrind details:"
			echo
			[ "$GITHUB_ACTION" != "" ] && echo "::group:: $tool details"
			echo "$details"
			[ "$GITHUB_ACTION" != "" ] && echo "::endgroup::"
			echo
		else
			echo "memcheck '$tool' OK"
		fi
	done
else
	echo "Skipping test as valgrind is not installed."
fi

exit $error
