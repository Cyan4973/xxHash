# Instructions for coding agents

These instructions are meant for automated contributors (coding agents, bots),
but apply to every contributor.

## `xxhash.h` is generated: do not edit it

`xxhash.h` is generated from the source units in `lib/` (see [`lib/README.md`](lib/README.md)).
CI fails if the committed `xxhash.h` differs from the result of `lib/`.

1. Make changes in the units of `lib/`, never in `xxhash.h`.
   To find which unit, and which line, line `N` of `xxhash.h` comes from:
   `sh build/amalgamate.sh --where N lib/xxhash_root.h`
2. Regenerate `xxhash.h` with `make amalgamate`
   (other `make` targets, such as `make test`, also regenerate it when units change).
3. Commit the modified units and the regenerated `xxhash.h` together.

Each regeneration overwrites `xxhash.h`: changes made directly in `xxhash.h` are lost,
move them into `lib/` first.

## Before submitting

- Run `make test`. It also checks that `xxhash.h` is up to date (`make check-amalgamation`),
  and that the API remains compatible with the latest release (`make test-api-compat`).
- If units of `lib/` were modified, also run `make test-amalgamation`.
- Hash values must not change.
- Code must remain compatible with C90, except for `long long` (`make c90test`).
- Pull requests target the `dev` branch.
