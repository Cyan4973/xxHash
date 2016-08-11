xxHash - Extremely fast hash algorithm
======================================

xxHash is an Extremely fast Hash algorithm, running at RAM speed limits.
It successfully completes the [SMHasher](http://code.google.com/p/smhasher/wiki/SMHasher) test suite
which evaluates collision, dispersion and randomness qualities of hash functions.
Code is highly portable, and hashes are identical on all platforms (little / big endian).

|Branch      |Status   |
|------------|---------|
|master      | [![Build Status](https://travis-ci.org/Cyan4973/xxHash.svg?branch=master)](https://travis-ci.org/Cyan4973/xxHash?branch=master) |
|dev         | [![Build Status](https://travis-ci.org/Cyan4973/xxHash.svg?branch=dev)](https://travis-ci.org/Cyan4973/xxHash?branch=dev) |



Benchmarks
-------------------------

The benchmark uses SMHasher speed test, compiled with Visual 2010 on a Windows Seven 32-bits box.
The reference system uses a Core 2 Duo @3GHz


| Name          |   Speed  | Quality | Author           |
|---------------|----------|:-------:|------------------|
| [xxHash]      | 5.4 GB/s |   10    | Y.C.             |
| MurmurHash 3a | 2.7 GB/s |   10    | Austin Appleby   |
| SBox          | 1.4 GB/s |    9    | Bret Mulvey      |
| Lookup3       | 1.2 GB/s |    9    | Bob Jenkins      |
| CityHash64    | 1.05 GB/s|   10    | Pike & Alakuijala|
| FNV           | 0.55 GB/s|    5    | Fowler, Noll, Vo |
| CRC32         | 0.43 GB/s|    9    |                  |
| MD5-32        | 0.33 GB/s|   10    | Ronald L.Rivest  |
| SHA1-32       | 0.28 GB/s|   10    |                  |

[xxHash]: http://www.xxhash.com

Q.Score is a measure of quality of the hash function.
It depends on successfully passing SMHasher test set.
10 is a perfect score.
Algorithms with a score < 5 are not listed on this table.

A new version, XXH64, has been created thanks to [Mathias Westerdahl]'s contribution,
which offers superior speed and dispersion for 64-bits systems.
Note however that 32-bits applications will still run faster using the 32-bits version.
[Mathias Westerdahl]: https://github.com/JCash

SMHasher speed test, compiled using GCC 4.8.2, on Linux Mint 64-bits.
The reference system uses a Core i5-3340M @2.7GHz

| Version    | Speed on 64-bits | Speed on 32-bits |
|------------|------------------|------------------|
| XXH64      | 13.8 GB/s        |  1.9 GB/s        |
| XXH32      |  6.8 GB/s        |  6.0 GB/s        |

This project also includes a command line utility, named `xxhsum`, offering similar features as `md5sum`, thanks to [Takayuki Matsuoka](https://github.com/t-mat) contributions.


### License

The library files `xxhash.c` and `xxhash.h` are BSD licensed.
The utility `xxhsum` is GPL licensed.


### Build modifiers

The following macros influence xxhash behavior. They are all disabled by default.

- `XXH_FORCE_NATIVE_FORMAT` : on big-endian systems : use native number representation,
                              resulting in system-specific results.
                              Breaks consistency with little-endian results.
- `XXH_ACCEPT_NULL_INPUT_POINTER` : if presented with a null-pointer,
                              xxhash result is the same as a null-length key,
                              instead of a dereference segfault.
- `XXH_NO_LONG_LONG` : removes support for XXH64,
                       useful for targets without 64-bits support.
- `XXH_STATIC_LINKING_ONLY` : gives access to state definition for static allocation.
                      Incompatible with dynamic linking, due to risks of ABI changes.
- `XXH_PRIVATE_API` : Make all functions `static` and accessible through `xxhash.h` for inlining.
                      Do not compile `xxhash.c` as a separate module in this case.
- `XXH_NAMESPACE` : prefix all symbols with the value of `XXH_NAMESPACE`,
                    in order to evade symbol naming collisions,
                    in case of multiple inclusions of xxHash library
                    (typically via intermediate libraries).


### Other languages

Beyond the C reference version,
xxHash is also available on many programming languages,
thanks to great contributors.
They are [listed here](http://www.xxhash.com/#other-languages).


### Branch Policy

> - The "master" branch is considered stable, at all times.
> - The "dev" branch is the one where all contributions must be merged
    before being promoted to master.
>   + If you plan to propose a patch, please commit into the "dev" branch,
      or its own feature branch.
      Direct commit to "master" are not permitted.
