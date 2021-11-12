#!/bin/sh

# Simple wrapper to valgrind/memcheck the test cases

# $@ 	binaries to check

error=0

if command -v valgrind >/dev/null; then
	for tool in $@; do
		details=$(
			valgrind -q --leak-check=full --error-markers=begin,end "./$tool" 2>&1#
		)
		output=$(
			echo "$details" |\
			grep -A1 "== begin" |\
			egrep -v "== begin|possibly lost|^--$"
		)
		if [ "$output" != "" ]; then
			error=1
			echo "ERROR: memcheck reports problems for '$tool'!"
			echo "$output"
			
			# When in github provide extra details
			if [ "$GITHUB_ACTION" != "" ]; then
				echo "::group:: $tool details"
				echo "$details"
				echo "::endgroup:"
			fi
		fi
	done
else
	echo "Skipping test as valgrind is not installed."
fi

exit $error
