#!/bin/sh

set -eu

usage() {
    echo "Usage: $0 [--check] VERSION" >&2
    exit 2
}

mode=update
if [ "${1-}" = "--check" ]; then
    mode=check
    shift
fi
[ "$#" -eq 1 ] || usage
version=$1

if ! printf '%s\n' "$version" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
    echo "Invalid version: $version" >&2
    exit 2
fi

script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
repo_dir=$(CDPATH= cd "$script_dir/.." && pwd)
status=0
tmp_file=

cleanup() {
    [ -z "$tmp_file" ] || rm -f "$tmp_file"
}
trap cleanup EXIT
trap 'exit 2' HUP INT TERM

sync_file() {
    relative_file=$1
    kind=$2
    file="$repo_dir/$relative_file"
    tmp_file="${file}.tmp.$$"

    awk_status=0
    awk -v kind="$kind" -v version="$version" '
        {
            isVersion = (kind == "doxy" && $0 ~ /^PROJECT_NUMBER[[:space:]]*=/) \
                     || (kind == "clib" && $0 ~ /^[[:space:]]*"version"[[:space:]]*:/) \
                     || (kind == "man" && $0 ~ /^\.TH .*"xxhsum [0-9]/)
            if (isVersion) {
                original = $0
                matches++
                if (!sub(/[0-9]+\.[0-9]+\.[0-9]+/, version)) bad = 1
                if ($0 != original) changed = 1
            }
            print
        }
        END {
            if (matches != 1 || bad) exit 2
            if (!changed) exit 3
        }
    ' "$file" > "$tmp_file" || awk_status=$?

    if [ "$awk_status" -ne 0 ] && [ "$awk_status" -ne 3 ]; then
        echo "Unable to find exactly one version in $relative_file" >&2
        exit 1
    fi

    if [ "$awk_status" -eq 3 ]; then
        rm -f "$tmp_file"
    elif [ "$mode" = "check" ]; then
        echo "$relative_file is not synchronized with version $version" >&2
        rm -f "$tmp_file"
        status=1
    else
        mv "$tmp_file" "$file"
        echo "Updated $relative_file to $version"
    fi
    tmp_file=
}

sync_file Doxyfile doxy
sync_file Doxyfile-internal doxy
sync_file clib.json clib

if [ "$mode" = "check" ]; then
    sync_file cli/xxhsum.1 man
fi

exit "$status"
