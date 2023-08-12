#!/bin/bash

# Simple wrapper to valgrind/memcheck the test cases

# $@ 	binaries to check

error=0

if command -v valgrind >/dev/null; then
	for tool in $@; do
		details=$(
			valgrind -q --leak-check=full --suppressions=<(cat <<-EOT
			  {
			     selinuxfs_exists
			     Memcheck:Leak
			     match-leak-kinds: definite
			     fun:malloc
			     fun:initialise_tags
			     fun:_Z25semmle_read_configurationv
			     fun:semmle_init
			     fun:fopen
			     fun:selinuxfs_exists
			     obj:/lib/x86_64-linux-gnu/libselinux.so.1
			     fun:call_init
			     fun:_dl_init
			     obj:/lib/x86_64-linux-gnu/ld-2.27.so
			  }
EOT
			) --error-markers=begin,end "./$tool" 2>&1
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
				echo "::endgroup::"
			fi
		else
			echo "memcheck '$tool' OK"
		fi
	done
else
	echo "Skipping test as valgrind is not installed."
fi

exit $error
