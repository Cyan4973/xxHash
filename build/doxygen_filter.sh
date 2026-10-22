#!/bin/sh
# Doxygen input filter (see FILTER_PATTERNS in Doxyfile and Doxyfile-internal).
# xxhash.h is distributed without comments: its documentation is generated
# from the source units in lib/, amalgamated with their comments.

set -eu

dir=$(dirname "$0")
case $1 in
    *xxhash.h) exec sh "$dir/amalgamate.sh" --comments all "$dir/../lib/xxhash_root.h" ;;
    *)         exec cat "$1" ;;
esac
