xxHash source units
===================

`xxhash.h`, at the root of the repository, is generated from the files in this
directory. They are the reference source: edit them, not `xxhash.h`.

- `xxhash_root.h` : the skeleton of `xxhash.h`, including the units below
  at their insertion points. It is a valid header, usable directly.
- `xxh3_api.h` : `XXH3` and `XXH128` public declarations.
- `xxh3_static.h` : `XXH3` and `XXH128` declarations reserved to static linking.
- `xxh3_impl.h` : `XXH3` and `XXH128` implementation.

Units are fragments of `xxhash.h`, not standalone headers:
they are only meaningful at their insertion point,
and are not meant to be included directly by user programs.
Each unit is wrapped into its own `#ifndef XXH_NO_*` guard,
so that removing a unit is equivalent to defining the corresponding build macro.


Workflow
--------

- Edit the units, then run `make` as usual:
  `xxhash.h` is regenerated automatically before anything else is built.
- Commit both the units and the regenerated `xxhash.h`.
  CI fails if `xxhash.h` is not up to date with `lib/`.
- `make amalgamate` regenerates `xxhash.h` unconditionally.
  If `xxhash.h` was edited directly, `make` keeps it and issues a warning:
  port the changes to `lib/`, or discard them with `make amalgamate`.
- `make check-amalgamation` verifies that `xxhash.h` is up to date, without writing it.
- `make test-amalgamation` checks that headers generated from `lib/` are equivalent to `xxhash.h`.

Bug reports and debuggers generally reference lines in `xxhash.h`.
To find the corresponding source line:
```sh
sh build/amalgamate.sh --where 4242 lib/xxhash_root.h
```

History of code moved from `xxhash.h` into units can be tracked with `git blame -C -C`.


Generator
---------

`build/amalgamate.sh` only requires a POSIX shell and a POSIX `awk`.
It copies `xxhash_root.h` verbatim, replacing each line `#include "unit.h"`
(exactly this form, at the beginning of the line)
by the content of the unit, minus its leading license block.
Expansion is recursive. It also verifies that:
- each unit starts with a license block (`/*` ... ` */`, mentioning `Copyright`
  and `BSD 2-Clause License`), followed by exactly one empty line;
- each file has balanced `#if` / `#endif`;
- each unit is included exactly once;
- all `*.h` files in this directory are used.

Option `-x unit.h` generates a variant without the selected unit.
For example, a header without `XXH3`:
```sh
sh build/amalgamate.sh -x xxh3_api.h -x xxh3_static.h -x xxh3_impl.h lib/xxhash_root.h > xxhash.h
```

New unit names must be unique across the repository,
and must differ from installed headers (such as `xxh3.h`).
