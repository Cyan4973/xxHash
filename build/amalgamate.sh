#!/bin/sh
# Generates xxhash.h from the source units in lib/.
#
# The root file is copied verbatim, except for lines of the form
#     #include "unit.h"
# which are replaced by the content of unit.h (path relative to the including
# file), minus its leading license block. Expansion is recursive.
# Each unit must start with a license block (/* ... */, mentioning Copyright
# and BSD 2-Clause License), followed by exactly one empty line.
# Each file must have balanced #if / #endif.
# All *.h files in the root's directory must be used (included or excluded).
#
# Requires only a POSIX shell and a POSIX awk.

set -eu

LC_ALL=C
export LC_ALL
AWK=${AWK:-awk}
prog=amalgamate.sh

usage() {
    cat >&2 <<EOF
Usage: $0 [-x UNIT]... [-o FILE | --check FILE | --sync STAMP -o FILE | --where N] ROOT

  (default)       write the result to stdout
  -o FILE         write the result to FILE (only touched if content changes)
  --check FILE    exit 1 if FILE differs from the result (never writes)
  --sync STAMP    with -o: update FILE, unless it was edited by hand since
                  last generation (recorded in STAMP); used by the Makefile
  --where N       tell which unit, and which line, line N of the result comes from
  -x UNIT         exclude UNIT (name as written in #include) from the result
EOF
    exit 2
}

die() {
    echo "$prog: error: $*" >&2
    exit 1
}

out='' check='' stamp='' where='' excludes='' root=''
while [ $# -gt 0 ]; do
    case $1 in
    -o)       [ $# -ge 2 ] || usage; out=$2; shift 2 ;;
    --check)  [ $# -ge 2 ] || usage; check=$2; shift 2 ;;
    --sync)   [ $# -ge 2 ] || usage; stamp=$2; shift 2 ;;
    --where)  [ $# -ge 2 ] || usage; where=$2; shift 2 ;;
    -x)       [ $# -ge 2 ] || usage; excludes="$excludes $2"; shift 2 ;;
    -h|--help) usage ;;
    -*)       die "unknown option: $1" ;;
    *)        [ -z "$root" ] || die "only one ROOT file can be given"; root=$1; shift ;;
    esac
done
[ -n "$root" ] || usage
[ -f "$root" ] || die "cannot read $root"
[ -z "$stamp" ] || [ -n "$out" ] || die "--sync requires -o"
[ -z "$check" ] || [ -z "$out" ] || die "--check and -o are mutually exclusive"
[ -z "$check" ] || [ -f "$check" ] || die "cannot read $check"
if [ -n "$where" ]; then
    [ -z "$out$check" ] || die "--where cannot be combined with -o or --check"
    case $where in
    ''|*[!0-9]*) die "--where expects a line number" ;;
    esac
fi

# list all units, using the same path syntax as the awk script
rootdir=$(dirname "$root")
units=
if [ "$rootdir" = "." ]; then
    for f in *.h; do [ ! -f "$f" ] || units="$units $f"; done
else
    for f in "$rootdir"/*.h; do [ ! -f "$f" ] || units="$units $f"; done
fi

generate() {
    $AWK -v prog="$prog" -v root="$root" -v excludes="$excludes" \
           -v allunits="$units" -v where="$where" '
    function fail(msg) {
        if (!err) print prog ": error: " msg | "cat 1>&2"
        err = 1
    }

    function emit(line, file, lineno) {
        outn++
        if (where == "") print line
        else if (outn == where) { wherefile = file; whereline = lineno; whatline = line }
    }

    function expand(file, depth,    dir, line, lineno, rc, state, hascopy, hasbsd, bal, name, rest, path, probe) {
        if (file in seen) { fail(file ": included more than once"); return }
        seen[file] = 1
        dir = file
        if (!sub(/\/[^\/]*$/, "", dir)) dir = "."
        state = (depth > 0)   # 1: expect license block start, 2: in license block, 3: expect empty line, 0: body
        lineno = 0; bal = 0; hascopy = 0; hasbsd = 0
        while ((rc = (getline line < file)) > 0) {
            lineno++
            sub(/\r$/, "", line)
            if (state == 1) {
                if (line != "/*") {
                    fail(file ":" lineno ": a unit must start with its license block")
                    break
                }
                state = 2
                continue
            }
            if (state == 2) {
                if (index(line, "Copyright")) hascopy = 1
                if (index(line, "BSD 2-Clause License")) hasbsd = 1
                if (line == " */") {
                    if (!hascopy || !hasbsd) {
                        fail(file ":" lineno ": license block lacks Copyright or BSD 2-Clause License")
                        break
                    }
                    state = 3
                }
                continue
            }
            if (state == 3) {
                if (line != "") {
                    fail(file ":" lineno ": license block must be followed by exactly one empty line")
                    break
                }
                state = 0
                continue
            }
            if (line ~ /^[ \t]*#[ \t]*if/) bal++
            if (line ~ /^[ \t]*#[ \t]*endif/ && --bal < 0) {
                fail(file ":" lineno ": #endif without #if")
                break
            }
            if (line ~ /^[ \t]*#[ \t]*include[ \t]*"/) {
                if (line !~ /^#include "[^"]+"/) {
                    fail(file ":" lineno ": unit inclusion must be written exactly as: #include \"unit.h\"")
                    break
                }
                name = line; sub(/^#include "/, "", name)
                rest = name; sub(/^[^"]*"/, "", rest); sub(/".*$/, "", name)
                if (rest !~ /^[ \t]*$/ && rest !~ /^[ \t]*\/\*.*\*\/[ \t]*$/) {
                    fail(file ":" lineno ": unexpected text after #include")
                    break
                }
                path = (dir == ".") ? name : dir "/" name
                if (name in excluded) {
                    exused[name] = 1
                    seen[path] = 1
                    continue
                }
                if ((getline probe < path) < 0) {
                    fail(file ":" lineno ": cannot read " path)
                    break
                }
                close(path)
                expand(path, depth + 1)
                if (err) break
                continue
            }
            emit(line, file, lineno)
        }
        if (rc < 0) fail("cannot read " file)
        close(file)
        if (!err && state) fail(file ": incomplete license block")
        if (!err && bal) fail(file ": " bal " unterminated #if")
    }

    BEGIN {
        err = 0; outn = 0; wherefile = ""
        n = split(excludes, list, " ")
        for (i = 1; i <= n; i++) excluded[list[i]] = 1
        if (where != "") where += 0
        expand(root, 0)
        for (u in excluded)
            if (!err && !(u in exused)) fail("-x " u ": no such #include")
        n = split(allunits, list, " ")
        for (i = 1; i <= n; i++)
            if (!err && !(list[i] in seen)) fail(list[i] ": unit is never included")
        if (!err && where != "") {
            if (wherefile == "") fail("line " where " is beyond the end of the result (" outn " lines)")
            else print wherefile ":" whereline ": " whatline
        }
        exit err
    }'
}

tmp=
cleanup() {
    [ -z "$tmp" ] || rm -f "$tmp"
}
trap cleanup EXIT
trap 'exit 2' HUP INT TERM

# record in the stamp whether FILE is the result of last generation
record() {
    mkdir -p "$(dirname "$stamp")"
    if [ "$1" = generated ]; then
        printf '# written by %s, do not edit\n# generated: %s\n' "$prog" "$(cksum < "$out")" > "$stamp"
    else
        printf '# written by %s, do not edit\n# divergent\n' "$prog" > "$stamp"
    fi
}

# succeed if files $1 and $2 are identical (cmp is not always available, e.g. on MSYS2)
same() {
    # shellcheck disable=SC2016 # awk script
    [ "$(wc -c < "$1")" = "$(wc -c < "$2")" ] &&
    $AWK 'NR == FNR { a[NR] = $0; next } a[FNR] "" != $0 "" { exit 1 }' "$1" "$2"
}

replace() {
    mv -f "$tmp" "$out"
    tmp=
    echo "$prog: updated $out" >&2
}

if [ -n "$check" ]; then
    tmp="$check.tmp.$$"
    generate > "$tmp"
    if ! same "$tmp" "$check"; then
        echo "$prog: $check is not up to date with $rootdir/" >&2
        echo "$prog: $check is generated: edit the units in $rootdir/, then run 'make amalgamate'" >&2
        if command -v diff > /dev/null 2>&1; then
            diff -u "$check" "$tmp" | head -n 40 >&2 || true
        fi
        exit 1
    fi
elif [ -n "$stamp" ]; then
    tmp="$out.tmp.$$"
    generate > "$tmp"
    newer=''
    # shellcheck disable=SC2086 # $units is a list
    [ ! -f "$out" ] || newer=$(find $units -newer "$out")
    if [ ! -f "$out" ]; then
        replace
        record generated
    elif same "$tmp" "$out"; then
        record generated
    elif [ -f "$stamp" ] && [ "$(sed -n 's/^# generated: //p' "$stamp")" = "$(cksum < "$out")" ]; then
        replace   # $out is the result of last generation: units were modified
        record generated
    elif [ ! -f "$stamp" ] && [ -n "$newer" ]; then
        replace   # no history (e.g. fresh checkout): units are more recent
        record generated
    else
        echo "$prog: warning: $out seems edited by hand, it differs from the result of $rootdir/" >&2
        echo "$prog: warning: $out kept as is: port the changes to $rootdir/," \
             "or discard them with 'make amalgamate'" >&2
        record divergent
    fi
elif [ -n "$out" ]; then
    tmp="$out.tmp.$$"
    generate > "$tmp"
    if [ -f "$out" ] && same "$tmp" "$out"; then
        rm -f "$tmp"
        tmp=
    else
        replace
    fi
else
    generate
fi
