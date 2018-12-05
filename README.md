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

The benchmark uses SMHasher speed test, compiled with Visual 2010 on a Windows Seven 32-bit box.
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

A more recent version, XXH64, has been created thanks to [Mathias Westerdahl](https://github.com/JCash),
which offers superior speed and dispersion for 64-bit systems.
Note however that 32-bit applications will still run faster using the 32-bit version.

Two more versions, XXH32a and XXH64a, which differ only in the initialization and finalization, use SIMD
optimizations via GCC/Clang extensions to compute two XXH32 hashes at once, then either joining or merging
them together to create a 32-bit or 64-bit hash. There is also a normal version which does not use extensions.

Each hash has its usages. 

First of all, GCC and Clang have different results with the alternative hashes.

On x86/x86_64 targets, as of 2018, GCC will produce noticeably faster code than Clang.
Note that on macOS, `gcc` is a symlink to `clang` by default. 

These are the results for the xxhsum 100kb benchmark, compiled with both Clang 7.0.0 and GCC 8.1.0 on macOS 10.13.6,
on an Intel Core 2 Duo P7450 (Penryn, SSE4.1) @2.13 GHz, with `-msse4.1 -O2`:

**64-bit**
  
| Type   | Aligned (GCC) | Unaligned (GCC) | Aligned (Clang) | Unaligned (Clang) |
|--------|--------------:|----------------:|----------------:|------------------:|
| XXH32  |     3.91 GB/s |       2.97 GB/s |       3.89 GB/s |         2.98 GB/s |
| XXH64  |     3.98 GB/s |       2.87 GB/s |       4.00 GB/s |         2.73 GB/s |
| XXH32a |     4.95 GB/s |       3.14 GB/s |       4.43 GB/s |         2.86 GB/s |
| XXH64a |     4.94 GB/s |       3.13 GB/s |       4.45 GB/s |         2.89 GB/s |

**32-bit**

| Type   | Aligned (GCC) | Unaligned (GCC) | Aligned (Clang) | Unaligned (Clang) |
|--------|--------------:|----------------:|----------------:|------------------:|
| XXH32  |     3.91 GB/s |       2.97 GB/s |       3.91 GB/s |         2.93 GB/s |
| XXH64  |     2.03 GB/s |       1.12 GB/s |       2.03 GB/s |         1.33 GB/s |
| XXH32a |     4.84 GB/s |       2.96 GB/s |       4.48 GB/s |         2.63 GB/s |
| XXH64a |     4.87 GB/s |       2.85 GB/s |       4.47 GB/s |         2.63 GB/s |

However, on ARM NEON targets, Clang will produce significantly faster code than GCC.
If you are compiling for iOS or using a recent NDK, you are using Clang.

On an LG G3 on Android 9.0 (LineageOS 16) with a Qualcomm Snapdragon 801 (Cortex-A15, ARMv7a/ve, quad-core)
@1.7/1.7/2.45/2.45 GHz, compiled with ARM GCC 8.2.0 and Clang 7.0.0 with `-O2 -march=native`, measured in Termux:

**32-bit**

| Type   | Aligned (GCC) | Unaligned (GCC) | Aligned (Clang) | Unaligned (Clang) |
|--------|--------------:|----------------:|----------------:|------------------:|
| XXH32  |     4.14 GB/s |       4.14 GB/s |       4.14 GB/s |         4.14 GB/s |
| XXH64  |     0.96 GB/s |       0.98 GB/s |       0.89 GB/s |         0.89 GB/s |
| XXH32a |     4.97 GB/s |       4.97 GB/s |       5.72 GB/s |         5.72 GB/s |
| XXH64a |     4.94 GB/s |       4.94 GB/s |       5.70 GB/s |         5.70 GB/s |

Note: The device was charging and the screen was tapped repeatedly to keep the CPU at
a high, stable frequency. Without doing this, the CPU will often throttle within the
benchmark to save power, causing inconsistent results which vary up to 50%.


Additionally, the alternative hashes have more warmup time than the basic ones, as
creating and destroying SIMD vectors is expensive.

The performance of the hashes would be graded as follows, assuming aligned data:

Note: Short input is assumed to be shorter than ~256 bytes.

| Version | 32-bit, short | 64-bit, short | 32-bit, long | 64-bit, long |
|---------|---------------|---------------|--------------|--------------|
| XXH32   | A             | A             | A            | B            |
| XXH64   | F             | B             | F            | A+           |
| XXH32a  | B             | B             | A+           | A            |
| XXH64a  | D             | B             | A            | A            |

Note: This assumes that XXH32a/XXH64a uses vectorization.
XXH32a and XXH64a will not be vectorized if...
* Not using GCC or Clang and not overridden with XXH_VECTORIZE=1
* Not targeting either SSE4.1 (or later) or NEON and not overridden with XXH_VECTORIZE=1
* XXH_VECTORIZE=0
* On a big endian systems without XXH32
* When explicitly targeting SSE4.1, not targeting AVX, and not using Clang in 32-bit mode,
  and using unaligned data. The loops aren't properly vectorized for unaligned access on
  SSE4.1 this way, and the normal way is slightly faster.

In addition, on older Core 2 processors, XXH32a and XXH64a will be faster than XXH64.


For maximum portability, use XXH32. XXH32 is fully C89 conformant, and
will work on any machine.

For short inputs shorter than a few hundred bytes, use XXH32 as well.

For longer inputs, the options open up.

If you know for sure that your code will be compiled on a modern 64-bit target,
XXH64 is recommended. Keep in mind that 32-bit targets typically have results
50-75% slower than XXH32.

XXH32a and XXH64a mostly rely on GCC/Clang's `__attribute__((__vector_size__(16)))`
extension to use clean vectors at the cost of requiring GCC or Clang. To disable
this behavior and use a plain array, define `XXH_VECTORIZE=0`. Similarly, to forcibly
enable it, define `XXH_VECTORIZE=1`. If GCC or Clang is detected and the compiler is
targeting ARM NEON or SSE4.1, this will be automatically enabled.

The reason for this choice instead of using all intrinsics, is to improve readability
(intrinsics aren't very expressive), improve portability (GCC and Clang will convert
the vector code to normal code if the target does not support it), and reduce code
blocks like this as much as possible.

```c
#ifdef __SSE4_1__
    __m128i inp = _mm_loadu_si128(p);
    ...
#elif defined(__ARM_NEON__)
    uint32x4_t inp = vld1q_u32(p);
    ...
#else
    v[0] = XXH32_round(v[0], XXH_get32bits(p)); p+=4;
    ...
#endif
```

There are already enough duplicated code blocks (mostly to bypass checking for alignment
and endianness).

Some intrinsics are used for when there are faster options that the compiler will
not recognize, such as forcing the "rotate left" code to produce a `vsliq_n_u32`
instruction instead of `vshlq_n_u32` followed by `vorrq_u32` on ARM NEON, however, they
are only to be used when there is a benefit. 

Using XXH32a or XXH64a with `XXH_VECTORIZE` set to zero will be slower than if it is
enabled, but faster than `XXH_VECTORIZE=1` if the target does not support SIMD.

If you want a consistent 64-bit hash that is fast on 32-bit and 64-bit, use
XXH64a. XXH64a will not see massive performance gains over XXH32 like XXH64 sees,
but it is one of the only 64-bit hashes that is actually fast on 32-bit systems, and
is still going to be faster than XXH32 on longer inputs with supported vectorization.

XXH64a, even without vectorization, will always be faster than XXH64 on a
32-bit target.

XXH32a is also a good option for longer inputs. 

SMHasher speed test, compiled using GCC 4.8.2, on Linux Mint 64-bit.
The reference system uses a Core i5-3340M @2.7GHz

| Version    | Speed on 64-bit | Speed on 32-bit |
|------------|------------------|------------------|
| XXH64      | 13.8 GB/s        |  1.9 GB/s        |
| XXH32      |  6.8 GB/s        |  6.0 GB/s        |

This project also includes a command line utility, named `xxhsum`, offering similar features as `md5sum`,
thanks to [Takayuki Matsuoka](https://github.com/t-mat) contributions.


### License

The library files `xxhash.c` and `xxhash.h` are BSD licensed.
The utility `xxhsum` is GPL licensed.


### Build modifiers

The following macros can be set at compilation time,
they modify xxhash behavior. They are all disabled by default.

- `XXH_INLINE_ALL` : Make all functions `inline`, with bodies directly included within `xxhash.h`.
                     There is no need for an `xxhash.o` module in this case.
                     Inlining functions is generally beneficial for speed on small keys.
                     It's especially effective when key length is a compile time constant,
                     with observed performance improvement in the +200% range .
                     See [this article](https://fastcompression.blogspot.com/2018/03/xxhash-for-small-keys-impressive-power.html) for details.
- `XXH_ACCEPT_NULL_INPUT_POINTER` : if set to `1`, when input is a null-pointer,
                                    xxhash result is the same as a zero-length key
                                    (instead of a dereference segfault).
- `XXH_FORCE_MEMORY_ACCESS` : default method `0` uses a portable `memcpy()` notation.
                              Method `1` uses a gcc-specific `packed` attribute, which can provide better performance for some targets.
                              Method `2` forces unaligned reads, which is not standard compliant, but might sometimes be the only way to extract better performance.
- `XXH_CPU_LITTLE_ENDIAN` : by default, endianess is determined at compile time.
                            It's possible to skip auto-detection and force format to little-endian, by setting this macro to 1.
                            Setting it to 0 forces big-endian.
- `XXH_FORCE_NATIVE_FORMAT` : on big-endian systems : use native number representation.
                              Breaks consistency with little-endian results.
- `XXH_PRIVATE_API` : same impact as `XXH_INLINE_ALL`.
                      Name underlines that symbols will not be published on library public interface.
- `XXH_NAMESPACE` : prefix all symbols with the value of `XXH_NAMESPACE`.
                    Useful to evade symbol naming collisions,
                    in case of multiple inclusions of xxHash source code.
                    Client applications can still use regular function name,
                    symbols are automatically translated through `xxhash.h`.
- `XXH_STATIC_LINKING_ONLY` : gives access to state declaration for static allocation.
                              Incompatible with dynamic linking, due to risks of ABI changes.
- `XXH_NO_LONG_LONG` : removes support for XXH64,
                       for targets without 64-bit support.
- `XXH_VECTORIZE` : Set to zero or one, forces manual vectorization of the XXH32a and XXH64a
                    hashes. This is automatically detected for SSE4.1 and NEON.

### Example

Calling xxhash 64-bit variant from a C program :

```c
#include "xxhash.h"

unsigned long long calcul_hash(const void* buffer, size_t length)
{
    unsigned long long const seed = 0;   /* or any other value */
    unsigned long long const hash = XXH64(buffer, length, seed);
    return hash;
}
```

Using streaming variant is more involved, but makes it possible to provide data in multiple rounds :
```c
#include "stdlib.h"   /* abort() */
#include "xxhash.h"


unsigned long long calcul_hash_streaming(someCustomType handler)
{
    XXH64_state_t* const state = XXH64_createState();
    if (state==NULL) abort();

    size_t const bufferSize = SOME_VALUE;
    void* const buffer = malloc(bufferSize);
    if (buffer==NULL) abort();

    unsigned long long const seed = 0;   /* or any other value */
    XXH_errorcode const resetResult = XXH64_reset(state, seed);
    if (resetResult == XXH_ERROR) abort();

    (...)
    while ( /* any condition */ ) {
        size_t const length = get_more_data(buffer, bufferSize, handler);   /* undescribed */
        XXH_errorcode const addResult = XXH64_update(state, buffer, length);
        if (addResult == XXH_ERROR) abort();
        (...)
    }

    (...)
    unsigned long long const hash = XXH64_digest(state);

    free(buffer);
    XXH64_freeState(state);

    return hash;
}
```


### Other programming languages

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
