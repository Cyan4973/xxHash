#!/bin/sh
# Generates a limited variant of xxhash.h, with only the selected hash families,
# from the source units in lib/ (see lib/README.md).
#
# Units of excluded families (and of streaming, with --no-stream) are removed,
# and the corresponding build macros are defined, so that the variant behaves
# like xxhash.h compiled with these macros.
#
# Requires only a POSIX shell and a POSIX awk.

set -eu

usage() {
    cat >&2 <<EOF
Usage: $0 [--no-stream] [--comments MODE] [-o FILE] FAMILY...

  FAMILY          xxh32, xxh64 or xxh3 (xxh3 requires xxh32 and xxh64,
                  which are then included)
  --no-stream     remove the streaming API (one-shot functions only)
  --comments MODE api (default), none or all: see build/amalgamate.sh
  -o FILE         write the result to FILE (default: stdout)
EOF
    exit 2
}

die() {
    echo "variant.sh: error: $*" >&2
    exit 1
}

dir=$(dirname "$0")
lib="$dir/../lib"
xxh32=0 xxh64=0 xxh3=0 stream=1 comments=api out=''
while [ $# -gt 0 ]; do
    case $1 in
    xxh32)       xxh32=1; shift ;;
    xxh64)       xxh64=1; shift ;;
    xxh3)        xxh3=1; shift ;;
    --no-stream) stream=0; shift ;;
    --comments)  [ $# -ge 2 ] || usage; comments=$2; shift 2 ;;
    -o)          [ $# -ge 2 ] || usage; out=$2; shift 2 ;;
    -h|--help)   usage ;;
    *)           die "unknown family or option: $1" ;;
    esac
done
[ $((xxh32 + xxh64 + xxh3)) -gt 0 ] || usage
if [ "$xxh3" = 1 ]; then xxh32=1; xxh64=1; fi

# exclude the units matching a pattern
exclude() {
    for unit in "$lib"/$1; do
        [ ! -f "$unit" ] || options="$options -x ${unit##*/}"
    done
}

options="--comments $comments"
if [ "$xxh32" = 0 ]; then options="$options -D XXH_NO_XXH32"; exclude 'xxh32_*.h'; fi
if [ "$xxh64" = 0 ]; then options="$options -D XXH_NO_LONG_LONG"; exclude 'xxh64_*.h'; fi
if [ "$xxh3" = 0 ];  then options="$options -D XXH_NO_XXH3"; exclude 'xxh3_*.h'; fi
if [ "$stream" = 0 ]; then options="$options -D XXH_NO_STREAM"; exclude '*_stream_*.h'; fi
# shellcheck disable=SC2086 # options is a list (unit names contain no space)
exec sh "$dir/amalgamate.sh" $options ${out:+-o "$out"} "$lib/xxhash_root.h"
