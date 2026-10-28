#!/bin/sh
# Checks that the API of xxhash.h remains compatible with a reference version,
# by default the latest release (highest v* tag), or $XXH_COMPAT_REF.
#
# For each inclusion mode, declarations and macros of the reference must still
# be present, unchanged. Additions are allowed.
# - declarations: statements of the preprocessed xxhash.h (prototypes, types,
#   structures), whitespace normalized, regardless of order;
# - macros: names of XXH* macros defined after inclusion
#   (not with XXH_NAMESPACE, which renames even disabled functions).
# Intentional changes are listed in tests/api_compat.exceptions.
# Note: the declarations for static linking only (XXH_STATIC_LINKING_ONLY)
# are not guaranteed to remain stable, but their changes must be deliberate.

set -eu

cd "$(dirname "$0")/.."
CC=${CC:-cc}
tmp=tests/tmp_compat
exceptions=tests/api_compat.exceptions

if [ -n "${XXH_COMPAT_REF-}" ]; then
    ref=$XXH_COMPAT_REF
    git cat-file -e "$ref:xxhash.h" 2>/dev/null || { echo "error: cannot read xxhash.h from $ref" >&2; exit 1; }
else
    ref=$(git tag -l 'v[0-9]*' --sort=-version:refname 2>/dev/null | head -n 1)
    if [ -z "$ref" ] || ! git cat-file -e "$ref:xxhash.h" 2>/dev/null; then
        echo "note: no release tag available, skipping API compatibility check"
        exit 0
    fi
fi

rm -rf "$tmp"
mkdir -p "$tmp/ref" "$tmp/new"
git show "$ref:xxhash.h" > "$tmp/ref/xxhash.h"
cp xxhash.h "$tmp/new/xxhash.h"
printf '#include "xxhash.h"\n' > "$tmp/ref/t.c"
cp "$tmp/ref/t.c" "$tmp/new/t.c"
grep -v '^#' "$exceptions" 2>/dev/null | sed '/^$/d' > "$tmp/exceptions" || true

# declarations: statements coming from xxhash.h, one per line, in a canonical form
declarations() {
    $CC -E "$@" > "$tmp/raw"
    awk '/^# *[0-9]+ "/ { inside = ($0 ~ /xxhash\.h"/); next } inside' "$tmp/raw" \
    | tr '\n\t' '  ' | sed 's/[;{}]/&\
/g' \
    | sed -e 's/  */ /g' -e 's/ *\([^A-Za-z0-9_ ]\) */\1/g' -e 's/^ //' -e 's/ $//' \
    | grep '[A-Za-z_]' | sed 's/^/declaration: /' | sort -u
}

# macros: names of XXH* macros
macros() {
    $CC -E -dM "$@" > "$tmp/raw"
    awk '{ sub(/\(.*/, "", $2); print $2 }' "$tmp/raw" | grep -E '^(XXH|xxh)' \
    | sed 's/^/macro: /' | sort -u
}

status=0
for mode in "" "-DXXH_STATIC_LINKING_ONLY" "-DXXH_NAMESPACE=NS_" "-DXXH_NAMESPACE=NS_ -DXXH_STATIC_LINKING_ONLY"; do
    for option in "" "-DXXH_NO_STREAM" "-DXXH_NO_XXH3" "-DXXH_NO_LONG_LONG"; do
        for side in ref new; do
            # shellcheck disable=SC2086 # mode and option are lists
            declarations $mode $option "$tmp/$side/t.c" > "$tmp/$side.api"
            # shellcheck disable=SC2086
            case $mode in
            *XXH_NAMESPACE*) ;;
            *) macros $mode $option "$tmp/$side/t.c" >> "$tmp/$side.api" ;;
            esac
            sort -o "$tmp/$side.api" "$tmp/$side.api"
        done
        # removed or changed since the reference, and not listed as exceptions
        comm -23 "$tmp/ref.api" "$tmp/new.api" | grep -v -x -F -f "$tmp/exceptions" > "$tmp/missing" || true
        if [ -s "$tmp/missing" ]; then
            echo "FAIL: API differs from $ref with: ${mode:-(default)} ${option}" >&2
            sed 's/^/  only in '"$ref"': /' "$tmp/missing" >&2
            comm -13 "$tmp/ref.api" "$tmp/new.api" | sed 's/^/  only in current: /' >&2
            status=1
        fi
    done
done

if [ "$status" -eq 0 ]; then
    rm -rf "$tmp"
    echo "API compatible with $ref"
else
    echo "Intentional changes can be listed in $exceptions" >&2
fi
exit "$status"
