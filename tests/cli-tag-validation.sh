#!/bin/sh
set -eu

XXHSUM=${XXHSUM:-"$(pwd)/xxhsum"}
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cd "$test_dir"
printf 'tag validation\n' > input

for algorithm in 32 64 128 3; do
    for endian in default little; do
        set --
        if [ "$endian" = little ]; then set -- --little-endian; fi
        "$XXHSUM" --tag -H"$algorithm" "$@" input > valid.xxh
        "$XXHSUM" --check --strict valid.xxh

        # Reject unknown names, prefixes, and trailing garbage, even when the
        # digest would match the algorithm selected by the old prefix parser.
        for tag in BOGUS X XXH XXH3garbage XXH32garbage XXH64garbage XXH128garbage \
                   XXH3_LEgarbage XXH32_LEgarbage XXH64_LEgarbage XXH128_LEgarbage; do
            sed "s/^[^ ]*/$tag/" valid.xxh > invalid.xxh
            if "$XXHSUM" --check --strict invalid.xxh; then
                echo "Accepted invalid algorithm tag: $tag" >&2
                exit 1
            fi
        done

        # A valid line must not hide a malformed tag in strict mode.
        cat valid.xxh invalid.xxh > mixed.xxh
        if "$XXHSUM" --check --strict mixed.xxh; then
            echo 'Accepted malformed tag in a mixed checksum file' >&2
            exit 1
        fi
    done
done

echo 'Algorithm tag validation: OK'
