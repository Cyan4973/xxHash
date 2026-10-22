#!/bin/sh
# Checks headers generated from lib/ against xxhash.h:
# - lib/xxhash_root.h, used directly as a multi-file header,
#   must preprocess exactly like xxhash.h;
# - the variant excluding XXH3 units must preprocess exactly like
#   xxhash.h with XXH_NO_XXH3, and must compile into an XXH3-free library.

set -eu

cd "$(dirname "$0")/.."
CC=${CC:-cc}
CXX=${CXX:-c++}
NM=${NM:-nm}
tmp=tests/tmp_amalg
status=0

rm -rf "$tmp"
mkdir -p "$tmp/noxxh3"
sh build/amalgamate.sh -x xxh3_api.h -x xxh3_static.h -x xxh3_impl.h \
    -o "$tmp/noxxh3/xxhash.h" lib/xxhash_root.h 2>/dev/null

# includes the header several times, in different modes
cat > "$tmp/multi.c" <<EOF
#include XXH_HEADER
#ifndef XXH_STATIC_LINKING_ONLY
#  define XXH_STATIC_LINKING_ONLY
#endif
#include XXH_HEADER
#ifndef XXH_INLINE_ALL
#  define XXH_INLINE_ALL
#endif
#include XXH_HEADER
EOF

# preprocessors may lay out whitespace differently across file boundaries
# (e.g. clang -P), so compare token streams modulo whitespace
normalize() {
    tr '\t' ' ' | sed -e 's/^  *//' -e 's/   */ /g' -e '/^$/d'
}

# compare_pp NAME REFERENCE_HEADER TESTED_HEADER COMPILER CPPFLAGS...
compare_pp() {
    name=$1; ref=$2; new=$3; cc=$4
    shift 4
    if [ "$cc" = "$CXX" ]; then lang=c++; else lang=c; fi
    for side in ref new; do
        if [ "$side" = ref ]; then header=$ref; else header=$new; fi
        # no pipe from the compiler: its failures must stop the script
        $cc -x $lang -E -P "$@" "$header" > "$tmp/raw"
        normalize < "$tmp/raw" > "$tmp/$side.E"
        $cc -x $lang -E -dM "$@" "$header" > "$tmp/raw"
        sort < "$tmp/raw" > "$tmp/$side.dM"
        $cc -x $lang -E -P "$@" -DXXH_HEADER="\"$PWD/$header\"" "$tmp/multi.c" > "$tmp/raw"
        normalize < "$tmp/raw" > "$tmp/$side.multi"
    done
    for mode in E dM multi; do
        if ! cmp -s "$tmp/ref.$mode" "$tmp/new.$mode"; then
            echo "FAIL: $name ($cc $*): $mode output differs from $ref" >&2
            status=1
        fi
    done
}

# note: XXH_IMPLEMENTATION is defined empty, as xxhash.h redefines it in INLINE_ALL mode
for flags in \
    "" \
    "-DXXH_STATIC_LINKING_ONLY" \
    "-DXXH_STATIC_LINKING_ONLY -DXXH_IMPLEMENTATION=" \
    "-DXXH_INLINE_ALL" \
    "-DXXH_PRIVATE_API" \
    "-DXXH_NAMESPACE=NS_ -DXXH_STATIC_LINKING_ONLY -DXXH_IMPLEMENTATION=" \
    "-DXXH_NO_STREAM -DXXH_INLINE_ALL" \
    "-DXXH_NO_LONG_LONG -DXXH_INLINE_ALL"
do
    # shellcheck disable=SC2086 # flags are meant to be split
    compare_pp root xxhash.h lib/xxhash_root.h "$CC" $flags
    # shellcheck disable=SC2086
    compare_pp noxxh3 xxhash.h "$tmp/noxxh3/xxhash.h" "$CC" -DXXH_NO_XXH3 $flags
done
compare_pp root xxhash.h lib/xxhash_root.h "$CXX" -DXXH_INLINE_ALL
compare_pp noxxh3 xxhash.h "$tmp/noxxh3/xxhash.h" "$CXX" -DXXH_NO_XXH3 -DXXH_INLINE_ALL

# the variant must be byte-identical to xxhash.h with XXH_NO_XXH3 sections removed
if command -v unifdef >/dev/null 2>&1; then
    unifdef -DXXH_NO_XXH3 xxhash.h > "$tmp/unifdef.h" || [ $? -eq 1 ]
    if ! cmp -s "$tmp/unifdef.h" "$tmp/noxxh3/xxhash.h"; then
        echo "FAIL: noxxh3 variant differs from: unifdef -DXXH_NO_XXH3 xxhash.h" >&2
        status=1
    fi
else
    echo "note: unifdef not found, skipping unifdef comparison"
fi

# the variant must compile cleanly, and contain no XXH3 symbol
cp xxhash.c "$tmp/noxxh3/"
$CC -std=c99 -pedantic -Wall -Wextra -Werror -c "$tmp/noxxh3/xxhash.c" -o "$tmp/noxxh3/xxhash.o"
if $NM "$tmp/noxxh3/xxhash.o" | grep -q 'XXH3_\|XXH128'; then
    echo "FAIL: noxxh3 variant still defines XXH3 symbols" >&2
    status=1
fi
if ! $NM "$tmp/noxxh3/xxhash.o" | grep -q 'XXH64_digest'; then
    echo "FAIL: noxxh3 variant lacks XXH64 symbols" >&2
    status=1
fi

if [ "$status" -eq 0 ]; then
    rm -rf "$tmp"
    echo "amalgamation variants: OK"
fi
exit "$status"
