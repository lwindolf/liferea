#!/bin/sh

# Simple wrapper to valgrind/memcheck the test cases

# $@ 	binaries to check

error=0

if command -v valgrind >/dev/null; then
	for tool in $@; do
		output=$(
			valgrind -q --leak-check=full --error-markers=begin,end "./$tool" 2>&1 |\
			grep -A1 "== begin" |\
			egrep -v "== begin|possibly lost|^--$"
		)
		if [ "$output" != "" ]; then
			error=1
			echo "ERROR: memcheck reports problems for '$tool'!"
			echo "$output"
		fi
	done
else
	echo "Skipping test as valgrind is not installed."
fi

exit $error
