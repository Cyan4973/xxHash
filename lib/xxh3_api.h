/*
 * xxHash - Extremely Fast Hash algorithm
 * XXH3 and XXH128 public declarations
 * Copyright (C) 2012-2023 Yann Collet
 *
 * BSD 2-Clause License (https://www.opensource.org/licenses/bsd-license.php)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You can contact the author at:
 *   - xxHash homepage: https://www.xxhash.com
 *   - xxHash source repository: https://github.com/Cyan4973/xxHash
 *
 * This file is a fragment of xxhash.h, not a standalone header:
 * it is only meaningful at its insertion point in lib/xxhash_root.h.
 * xxhash.h is generated from lib/ by build/amalgamate.sh, see lib/README.md.
 */

#if !defined(XXH_NO_XXH3) && !defined(XXH_NO_XXH32) && !defined(XXH_NO_LONG_LONG)
/* XXH3 depends on XXH32 and XXH64 */

#if defined(XXH_INLINE_ALL) || defined(XXH_PRIVATE_API)
   /* see INLINE mode: undefine names, in case they were defined by a previous inclusion */
#  undef XXH3_64bits
#  undef XXH3_64bits_withSecret
#  undef XXH3_64bits_withSeed
#  undef XXH3_64bits_withSecretandSeed
#  undef XXH3_generateSecret
#  undef XXH3_generateSecret_fromSeed
#  undef XXH128
#  undef XXH3_128bits
#  undef XXH3_128bits_withSeed
#  undef XXH3_128bits_withSecret
#  undef XXH3_128bits_withSecretandSeed
#  undef XXH128_isEqual
#  undef XXH128_cmp
#  undef XXH128_canonicalFromHash
#  undef XXH128_hashFromCanonical
#  define XXH128_canonical_t XXH_IPREF(XXH128_canonical_t)
#  define XXH3_state_s  XXH_IPREF(XXH3_state_s)
#  define XXH3_state_t  XXH_IPREF(XXH3_state_t)
#  define XXH128_hash_t XXH_IPREF(XXH128_hash_t)
#endif
#ifdef XXH_NAMESPACE
#  define XXH3_64bits XXH_IPREF(XXH3_64bits)
#  define XXH3_64bits_withSecret XXH_IPREF(XXH3_64bits_withSecret)
#  define XXH3_64bits_withSeed XXH_IPREF(XXH3_64bits_withSeed)
#  define XXH3_64bits_withSecretandSeed XXH_IPREF(XXH3_64bits_withSecretandSeed)
#  define XXH3_generateSecret XXH_IPREF(XXH3_generateSecret)
#  define XXH3_generateSecret_fromSeed XXH_IPREF(XXH3_generateSecret_fromSeed)
/* XXH3_128bits */
#  define XXH128 XXH_IPREF(XXH128)
#  define XXH3_128bits XXH_IPREF(XXH3_128bits)
#  define XXH3_128bits_withSeed XXH_IPREF(XXH3_128bits_withSeed)
#  define XXH3_128bits_withSecret XXH_IPREF(XXH3_128bits_withSecret)
#  define XXH3_128bits_withSecretandSeed XXH_IPREF(XXH3_128bits_withSecretandSeed)
#  define XXH128_isEqual XXH_IPREF(XXH128_isEqual)
#  define XXH128_cmp     XXH_IPREF(XXH128_cmp)
#  define XXH128_canonicalFromHash XXH_IPREF(XXH128_canonicalFromHash)
#  define XXH128_hashFromCanonical XXH_IPREF(XXH128_hashFromCanonical)
#endif

/*!
 * @}
 * ************************************************************************
 * @defgroup XXH3_family XXH3 family
 * @ingroup public
 * @{
 *
 * XXH3 is a more recent hash algorithm featuring:
 *  - Improved speed for both small and large inputs
 *  - True 64-bit and 128-bit outputs
 *  - SIMD acceleration
 *  - Improved 32-bit viability
 *
 * Speed analysis methodology is explained here:
 *
 *    https://fastcompression.blogspot.com/2019/03/presenting-xxh3.html
 *
 * Compared to XXH64, expect XXH3 to run approximately
 * ~2x faster on large inputs and >3x faster on small ones,
 * exact differences vary depending on platform.
 *
 * XXH3's speed benefits greatly from SIMD and 64-bit arithmetic,
 * but does not require it.
 * Most 32-bit and 64-bit targets that can run XXH32 smoothly can run XXH3
 * at competitive speeds, even without vector support. Further details are
 * explained in the implementation.
 *
 * XXH3 has a fast scalar implementation, but it also includes accelerated SIMD
 * implementations for many common platforms:
 *   - AVX512
 *   - AVX2
 *   - SSE2
 *   - ARM NEON
 *   - WebAssembly SIMD128
 *   - POWER8 VSX
 *   - s390x ZVector
 * This can be controlled via the @ref XXH_VECTOR macro, but it automatically
 * selects the best version according to predefined macros. For the x86 family, an
 * automatic runtime dispatcher is included separately in @ref xxh_x86dispatch.c.
 *
 * XXH3 implementation is portable:
 * it has a generic C90 formulation that can be compiled on any platform,
 * all implementations generate exactly the same hash value on all platforms.
 * Starting from v0.8.0, it's also labelled "stable", meaning that
 * any future version will also generate the same hash value.
 *
 * XXH3 offers 2 variants, _64bits and _128bits.
 *
 * When only 64 bits are needed, prefer invoking the _64bits variant, as it
 * reduces the amount of mixing, resulting in faster speed on small inputs.
 * It's also generally simpler to manipulate a scalar return type than a struct.
 *
 * The API supports one-shot hashing, streaming mode, and custom secrets.
 */

/*!
 * @ingroup tuning
 * @brief Possible values for @ref XXH_VECTOR.
 *
 * Unless set explicitly, determined automatically.
 */
#  define XXH_SCALAR 0 /*!< Portable scalar version */
#  define XXH_SSE2   1 /*!< SSE2 for Pentium 4, Opteron, all x86_64. */
#  define XXH_AVX2   2 /*!< AVX2 for Haswell and Bulldozer */
#  define XXH_AVX512 3 /*!< AVX512 for Skylake and Icelake */
#  define XXH_NEON   4 /*!< NEON for most ARMv7-A, all AArch64, and WASM SIMD128 */
#  define XXH_VSX    5 /*!< VSX and ZVector for POWER8/z13 (64-bit) */
#  define XXH_SVE    6 /*!< SVE for some ARMv8-A and ARMv9-A */
#  define XXH_LSX    7 /*!< LSX (128-bit SIMD) for LoongArch64 */
#  define XXH_LASX   8 /*!< LASX (256-bit SIMD) for LoongArch64 */
#  define XXH_RVV    9 /*!< RVV (RISC-V Vector) for RISC-V */

/*-**********************************************************************
*  XXH3 64-bit variant
************************************************************************/

/*!
 * @brief Calculates 64-bit unseeded variant of XXH3 hash of @p input.
 *
 * @param input  The block of data to be hashed, at least @p length bytes in size.
 * @param length The length of @p input, in bytes.
 *
 * @pre
 *   The memory between @p input and @p input + @p length must be valid,
 *   readable, contiguous memory. However, if @p length is `0`, @p input may be
 *   `NULL`. In C++, this also must be *TriviallyCopyable*.
 *
 * @return The calculated 64-bit XXH3 hash value.
 *
 * @note
 *   This is equivalent to @ref XXH3_64bits_withSeed() with a seed of `0`, however
 *   it may have slightly better performance due to constant propagation of the
 *   defaults.
 *
 * @see
 *    XXH3_64bits_withSeed(), XXH3_64bits_withSecret(): other seeding variants
 * @see @ref single_shot_example "Single Shot Example" for an example.
 */
XXH_PUBLIC_API XXH_PUREF XXH64_hash_t XXH3_64bits(XXH_NOESCAPE const void* input, size_t length);

/*!
 * @brief Calculates 64-bit seeded variant of XXH3 hash of @p input.
 *
 * @param input  The block of data to be hashed, at least @p length bytes in size.
 * @param length The length of @p input, in bytes.
 * @param seed   The 64-bit seed to alter the hash result predictably.
 *
 * @pre
 *   The memory between @p input and @p input + @p length must be valid,
 *   readable, contiguous memory. However, if @p length is `0`, @p input may be
 *   `NULL`. In C++, this also must be *TriviallyCopyable*.
 *
 * @return The calculated 64-bit XXH3 hash value.
 *
 * @note
 *    seed == 0 produces the same results as @ref XXH3_64bits().
 *
 * This variant generates a custom secret on the fly based on default secret
 * altered using the @p seed value.
 *
 * While this operation is decently fast, note that it's not completely free.
 *
 * @see @ref single_shot_example "Single Shot Example" for an example.
 */
XXH_PUBLIC_API XXH_PUREF XXH64_hash_t XXH3_64bits_withSeed(XXH_NOESCAPE const void* input, size_t length, XXH64_hash_t seed);

/*!
 * The bare minimum size for a custom secret.
 *
 * @see
 *  XXH3_64bits_withSecret(), XXH3_64bits_reset_withSecret(),
 *  XXH3_128bits_withSecret(), XXH3_128bits_reset_withSecret().
 */
#define XXH3_SECRET_SIZE_MIN 136

/*!
 * @brief Calculates 64-bit variant of XXH3 with a custom "secret".
 *
 * @param data       The block of data to be hashed, at least @p len bytes in size.
 * @param len        The length of @p data, in bytes.
 * @param secret     The secret data.
 * @param secretSize The length of @p secret, in bytes.
 *
 * @return The calculated 64-bit XXH3 hash value.
 *
 * @pre
 *   The memory between @p data and @p data + @p len must be valid,
 *   readable, contiguous memory. However, if @p length is `0`, @p data may be
 *   `NULL`. In C++, this also must be *TriviallyCopyable*.
 *
 * It's possible to provide any blob of bytes as a "secret" to generate the hash.
 * This makes it more difficult for an external actor to prepare an intentional collision.
 * The main condition is that @p secretSize *must* be large enough (>= @ref XXH3_SECRET_SIZE_MIN).
 * However, the quality of the secret impacts the dispersion of the hash algorithm.
 * Therefore, the secret _must_ look like a bunch of random bytes.
 * Avoid "trivial" or structured data such as repeated sequences or a text document.
 * Whenever in doubt about the "randomness" of the blob of bytes,
 * consider employing @ref XXH3_generateSecret() instead (see below).
 * It will generate a proper high entropy secret derived from the blob of bytes.
 * Another advantage of using XXH3_generateSecret() is that
 * it guarantees that all bits within the initial blob of bytes
 * will impact every bit of the output.
 * This is not necessarily the case when using the blob of bytes directly
 * because, when hashing _small_ inputs, only a portion of the secret is employed.
 *
 * @see @ref single_shot_example "Single Shot Example" for an example.
 */
XXH_PUBLIC_API XXH_PUREF XXH64_hash_t XXH3_64bits_withSecret(XXH_NOESCAPE const void* data, size_t len, XXH_NOESCAPE const void* secret, size_t secretSize);


/* note : canonical representation of XXH3 is the same as XXH64
 * since they both produce XXH64_hash_t values */


/*-**********************************************************************
*  XXH3 128-bit variant
************************************************************************/

/*!
 * @brief The return value from 128-bit hashes.
 *
 * Stored in little endian order, although the fields themselves are in native
 * endianness.
 */
typedef struct {
    XXH64_hash_t low64;   /*!< `value & 0xFFFFFFFFFFFFFFFF` */
    XXH64_hash_t high64;  /*!< `value >> 64` */
} XXH128_hash_t;

/*!
 * @brief Calculates 128-bit unseeded variant of XXH3 of @p data.
 *
 * @param data The block of data to be hashed, at least @p length bytes in size.
 * @param len  The length of @p data, in bytes.
 *
 * @return The calculated 128-bit variant of XXH3 value.
 *
 * The 128-bit variant of XXH3 has more strength, but it has a bit of overhead
 * for shorter inputs.
 *
 * This is equivalent to @ref XXH3_128bits_withSeed() with a seed of `0`, however
 * it may have slightly better performance due to constant propagation of the
 * defaults.
 *
 * @see XXH3_128bits_withSeed(), XXH3_128bits_withSecret(): other seeding variants
 * @see @ref single_shot_example "Single Shot Example" for an example.
 */
XXH_PUBLIC_API XXH_PUREF XXH128_hash_t XXH3_128bits(XXH_NOESCAPE const void* data, size_t len);
/*! @brief Calculates 128-bit seeded variant of XXH3 hash of @p data.
 *
 * @param data The block of data to be hashed, at least @p length bytes in size.
 * @param len  The length of @p data, in bytes.
 * @param seed The 64-bit seed to alter the hash result predictably.
 *
 * @return The calculated 128-bit variant of XXH3 value.
 *
 * @note
 *    seed == 0 produces the same results as @ref XXH3_64bits().
 *
 * This variant generates a custom secret on the fly based on default secret
 * altered using the @p seed value.
 *
 * While this operation is decently fast, note that it's not completely free.
 *
 * @see XXH3_128bits(), XXH3_128bits_withSecret(): other seeding variants
 * @see @ref single_shot_example "Single Shot Example" for an example.
 */
XXH_PUBLIC_API XXH_PUREF XXH128_hash_t XXH3_128bits_withSeed(XXH_NOESCAPE const void* data, size_t len, XXH64_hash_t seed);
/*!
 * @brief Calculates 128-bit variant of XXH3 with a custom "secret".
 *
 * @param data       The block of data to be hashed, at least @p len bytes in size.
 * @param len        The length of @p data, in bytes.
 * @param secret     The secret data.
 * @param secretSize The length of @p secret, in bytes.
 *
 * @return The calculated 128-bit variant of XXH3 value.
 *
 * It's possible to provide any blob of bytes as a "secret" to generate the hash.
 * This makes it more difficult for an external actor to prepare an intentional collision.
 * The main condition is that @p secretSize *must* be large enough (>= @ref XXH3_SECRET_SIZE_MIN).
 * However, the quality of the secret impacts the dispersion of the hash algorithm.
 * Therefore, the secret _must_ look like a bunch of random bytes.
 * Avoid "trivial" or structured data such as repeated sequences or a text document.
 * Whenever in doubt about the "randomness" of the blob of bytes,
 * consider employing @ref XXH3_generateSecret() instead (see below).
 * It will generate a proper high entropy secret derived from the blob of bytes.
 * Another advantage of using XXH3_generateSecret() is that
 * it guarantees that all bits within the initial blob of bytes
 * will impact every bit of the output.
 * This is not necessarily the case when using the blob of bytes directly
 * because, when hashing _small_ inputs, only a portion of the secret is employed.
 *
 * @see @ref single_shot_example "Single Shot Example" for an example.
 */
XXH_PUBLIC_API XXH_PUREF XXH128_hash_t XXH3_128bits_withSecret(XXH_NOESCAPE const void* data, size_t len, XXH_NOESCAPE const void* secret, size_t secretSize);

/* Following helper functions make it possible to compare XXH128_hast_t values.
 * Since XXH128_hash_t is a structure, this capability is not offered by the language.
 * Note: For better performance, these functions can be inlined using XXH_INLINE_ALL */

/*!
 * @brief Check equality of two XXH128_hash_t values
 *
 * @param h1 The 128-bit hash value.
 * @param h2 Another 128-bit hash value.
 *
 * @return `1` if `h1` and `h2` are equal.
 * @return `0` if they are not.
 */
XXH_PUBLIC_API XXH_PUREF int XXH128_isEqual(XXH128_hash_t h1, XXH128_hash_t h2);

/*!
 * @brief Compares two @ref XXH128_hash_t
 *
 * This comparator is compatible with stdlib's `qsort()`/`bsearch()`.
 *
 * @param h128_1 Left-hand side value
 * @param h128_2 Right-hand side value
 *
 * @return >0 if @p h128_1  > @p h128_2
 * @return =0 if @p h128_1 == @p h128_2
 * @return <0 if @p h128_1  < @p h128_2
 */
XXH_PUBLIC_API XXH_PUREF int XXH128_cmp(XXH_NOESCAPE const void* h128_1, XXH_NOESCAPE const void* h128_2);


/*******   Canonical representation   *******/
typedef struct { unsigned char digest[sizeof(XXH128_hash_t)]; } XXH128_canonical_t;


/*!
 * @brief Converts an @ref XXH128_hash_t to a big endian @ref XXH128_canonical_t.
 *
 * @param dst  The @ref XXH128_canonical_t pointer to be stored to.
 * @param hash The @ref XXH128_hash_t to be converted.
 *
 * @pre
 *   @p dst must not be `NULL`.
 * @see @ref canonical_representation_example "Canonical Representation Example"
 */
XXH_PUBLIC_API void XXH128_canonicalFromHash(XXH_NOESCAPE XXH128_canonical_t* dst, XXH128_hash_t hash);

/*!
 * @brief Converts an @ref XXH128_canonical_t to a native @ref XXH128_hash_t.
 *
 * @param src The @ref XXH128_canonical_t to convert.
 *
 * @pre
 *   @p src must not be `NULL`.
 *
 * @return The converted hash.
 * @see @ref canonical_representation_example "Canonical Representation Example"
 */
XXH_PUBLIC_API XXH_PUREF XXH128_hash_t XXH128_hashFromCanonical(XXH_NOESCAPE const XXH128_canonical_t* src);


#endif  /* !XXH_NO_XXH3 */
