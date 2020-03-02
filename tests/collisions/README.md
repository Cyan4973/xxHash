
__collisionsTest__ is a brute force hash analyzer
which will measure a 64-bit hash algorithm's collision rate
by generating billions of hashes,
and comparing the result to an "ideal" target.

The test requires a very large amount of memory.
By default, it will generate 24 billion of 64-bit hashes,
requiring __192 GB of RAM__ for their storage.
The number of hashes can be modified using command `--nbh=`.
be aware that testing the collision ratio of 64-bit hashes
requires a very large amount of hashes (several billions) for meaningful measurements.

To reduce RAM usage, an optional filter can be requested, with `--filter`.
It reduces the nb of candidates to analyze, hence associated RAM budget.
Note that the filter itself requires a lot of RAM
(32 GB by default, can be modified using `--filterlog=`,
a too small filter will not be efficient, aim at ~2 bytes per hash),
and reading and writing into filter cost a significant CPU budget,
so this method is slower.
It also doesn't allow advanced analysis of partial bitfields,
since most hashes will be discarded and not stored.

When using the filter, the RAM budget consists of the filter and a list of candidates,
which will be a fraction of original hash list.
Using default settings (24 billions hashes, 32 GB filter),
the number of potential candidates should be reduced to less than 2 billions,
requiring ~14 GB for their storage.
Such a result also depends on hash algorithm's efficiency.
The number of effective candidates is likely to be lower, at ~ 1 billion,
but storage must allocate an upper bound.

For the default test, the expected "optimal" collision rate for a 64-bit hash function is ~18 collisions.

#### How to build
```
make
```

Note : the code is a mix of C99 and C++14,
it's not compatible with a C90-only compiler.

#### Build modifier

- `SLAB5` : use alternative pattern generator, friendlier for weak hash algorithms
- `POOL_MT` : if  `=0`, disable multi-treading code (enabled by default)

#### How to integrate any hash in the tester

The build script is expecting to compile files in `./allcodecs`.
Put the source code here.
This also works if the hash is a single `*.h` files.

The glue happens in `hashes.h`.
In this file, there are 2 sections :
- Add the required `#include "header.h"`, and create a wrapper,
to respect the format expected by the function pointer.
- Add the wrapper, along with the name and an indication of the output width,
to the table, at the end of `hashed.h`

Build with `make`. Locate your new hash with `./collisionsTest -h`,
it should be listed.


#### Usage


#### Some advises on how to setup a collisions test

Most tests are primarily driven by the amount of RAM available.
Here's a method to decide the size of the test.

Presuming that RAM budget is not plentiful, for this example 32 GB,
the `--filter` mode is actually compulsory to measure anything meaningful.
Let's plan 50% of memory for the filter, that's 16 GB.
This will be good enough to filter about 10% less hashes than this size.
Let's round down to 14 G.

By requesting 14G, the expectation is that the program will automatically
size the filter to 16 GB, and expect to store ~1G candidates,
leaving enough room to breeze for the system.

The command line becomes :
```
./collisionsTest --nbh=14G --filter NameOfHash
```

#### Examples :

Here are a few results produced with this tester :

| Algorithm | Input Len | Nb Hashes | Expected | Nb Collisions | Notes |
| --- | --- | --- | --- | --- | --- |
| __XXH3__ | 256 | 100 Gi | 312.5 | 326 |  |
| __XXH64__ | 256 | 100 Gi | 312.5 | 294 |  |
| __XXH128__ | 256 | 100 Gi | 0.0 | 0 | As a 128-bit hash, we expect XXH128 to generate 0 hash |
| __XXH128__ low 64-bit | 512 | 100 Gi | 312.5 | 321 |  |
| __XXH128__ high 64-bit | 512 | 100 Gi | 312.5 | 325 |  |

Test on small inputs :

| Algorithm | Input Len | Nb Hashes | Expected | Nb Collisions | Notes |
| --- | --- | --- | --- | --- | --- |
| __XXH3__ | 8 |  |  |  | To be restarted |
| __XXH64__ | 8 | 100 Gi | 312.5 | __0__ | `XXH64` is bijective for `len==8` |
| __XXH128__ | 16 | 25 Gi | 0.0 | 0 | test range 9-16 |
| __XXH128__ | 32 | 25 Gi | 0.0 | 0 | test range 17-128 |
| __XXH128__ | 100 | 13 Gi | 0.0 | 0 | test range 17-128 |
| __XXH128__ | 200 | 13 Gi | 0.0 | 0 | test range 129-240 |
