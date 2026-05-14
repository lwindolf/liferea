#!/bin/bash

# Simple wrapper to valgrind/memcheck the test cases
# To be run in the meson builddir

# $@ 	checks to run

error=0

if command -v valgrind >/dev/null; then
	for check in $@; do
		details=$(
			valgrind -q --enable-debuginfod=no --leak-check=full --gen-suppressions=all --suppressions="$(dirname "$0")/memcheck.supp" ./liferea --test "$check" 2>&1
		)
		output=$(
			echo "$details" | grep "definitely lost" | grep -v "0 bytes in 0 blocks"
		)
		if [ "$output" != "" ]; then
			error=1
			echo "ERROR: memcheck reports problems for '$check'!"
			echo "Relevant error lines are:"
			echo
			echo "$output"
			echo
			echo "Full valgrind details:"
			echo
			[ "$GITHUB_ACTION" != "" ] && echo "::group:: $check details"
			echo "$details"
			[ "$GITHUB_ACTION" != "" ] && echo "::endgroup::"
			echo
		else
			echo "memcheck '$check' OK"
		fi
	done
else
	echo "Skipping test as valgrind is not installed."
fi

exit $error
