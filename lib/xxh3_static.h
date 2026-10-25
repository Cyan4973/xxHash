/*
 * xxHash - Extremely Fast Hash algorithm
 * XXH3 and XXH128 advanced declarations (static linking only)
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

#if defined(__cplusplus) && (__cplusplus >= 201103L) /* >= C++11 */
/* In C++ alignas() is a keyword */
#  define XXH_ALIGN(n)      alignas(n)
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) /* > C11 */
#  define XXH_ALIGN(n)      _Alignas(n)
#elif defined(__GNUC__)
#  define XXH_ALIGN(n)      __attribute__ ((aligned(n)))
#elif defined(_MSC_VER)
#  define XXH_ALIGN(n)      __declspec(align(n))
#else
#  define XXH_ALIGN(n)   /* disabled */
#endif

/* Old GCC versions only accept the attribute after the type in structures. */
#if !(defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L))   /* C11+ */ \
    && ! (defined(__cplusplus) && (__cplusplus >= 201103L)) /* >= C++11 */ \
    && defined(__GNUC__)
#   define XXH_ALIGN_MEMBER(align, type) type XXH_ALIGN(align)
#else
#   define XXH_ALIGN_MEMBER(align, type) XXH_ALIGN(align) type
#endif

/*!
 * @internal
 * @brief The size of the internal XXH3 buffer.
 *
 * This is the optimal update size for incremental hashing.
 *
 * @see XXH3_64b_update(), XXH3_128b_update().
 */
#define XXH3_INTERNALBUFFER_SIZE 256

/*!
 * @def XXH3_SECRET_DEFAULT_SIZE
 * @brief Default Secret's size
 *
 * This is the size of internal XXH3_kSecret
 * and is needed by XXH3_generateSecret_fromSeed().
 *
 * Not to be confused with @ref XXH3_SECRET_SIZE_MIN.
 */
#define XXH3_SECRET_DEFAULT_SIZE 192

/*!
 * @internal
 * @brief Structure for XXH3 streaming API.
 *
 * @note This is only defined when @ref XXH_STATIC_LINKING_ONLY,
 * @ref XXH_INLINE_ALL, or @ref XXH_IMPLEMENTATION is defined.
 * Otherwise it is an opaque type.
 * Never use this definition in combination with dynamic library.
 * This allows fields to safely be changed in the future.
 *
 * @note ** This structure has a strict alignment requirement of 64 bytes!! **
 * Do not allocate this with `malloc()` or `new`,
 * it will not be sufficiently aligned.
 * Use @ref XXH3_createState() and @ref XXH3_freeState(), or stack allocation.
 *
 * Typedef'd to @ref XXH3_state_t.
 * Do never access the members of this struct directly.
 *
 * @see XXH3_INITSTATE() for stack initialization.
 * @see XXH3_createState(), XXH3_freeState().
 * @see XXH32_state_s, XXH64_state_s
 */
struct XXH3_state_s {
   XXH_ALIGN_MEMBER(64, XXH64_hash_t acc[8]);
       /*!< The 8 accumulators. See @ref XXH32_state_s::acc and @ref XXH64_state_s::acc */
   XXH_ALIGN_MEMBER(64, unsigned char customSecret[XXH3_SECRET_DEFAULT_SIZE]);
       /*!< Used to store a custom secret generated from a seed. */
   XXH_ALIGN_MEMBER(64, unsigned char buffer[XXH3_INTERNALBUFFER_SIZE]);
       /*!< The internal buffer. @see XXH32_state_s::mem32 */
   XXH32_hash_t bufferedSize;
       /*!< The amount of memory in @ref buffer, @see XXH32_state_s::memsize */
   XXH32_hash_t useSeed;
       /*!< Reserved field. Needed for padding on 64-bit. */
   size_t nbStripesSoFar;
       /*!< Number or stripes processed. */
   XXH64_hash_t totalLen;
       /*!< Total length hashed. 64-bit even on 32-bit targets. */
   size_t nbStripesPerBlock;
       /*!< Number of stripes per block. */
   size_t secretLimit;
       /*!< Size of @ref customSecret or @ref extSecret */
   XXH64_hash_t seed;
       /*!< Seed for _withSeed variants. Must be zero otherwise, @see XXH3_INITSTATE() */
   XXH64_hash_t reserved64;
       /*!< Reserved field. */
   const unsigned char* extSecret;
       /*!< Reference to an external secret for the _withSecret variants, NULL
        *   for other variants. */
   /* note: there may be some padding at the end due to alignment on 64 bytes */
}; /* typedef'd to XXH3_state_t */

#undef XXH_ALIGN_MEMBER

/*!
 * @brief Initializes a stack-allocated `XXH3_state_s`.
 *
 * When the @ref XXH3_state_t structure is merely emplaced on stack,
 * it should be initialized with XXH3_INITSTATE() or a memset()
 * in case its first reset uses XXH3_NNbits_reset_withSeed().
 * This init can be omitted if the first reset uses default or _withSecret mode.
 * This operation isn't necessary when the state is created with XXH3_createState().
 * Note that this doesn't prepare the state for a streaming operation,
 * it's still necessary to use XXH3_NNbits_reset*() afterwards.
 */
#define XXH3_INITSTATE(XXH3_state_ptr)                       \
    do {                                                     \
        XXH3_state_t* tmp_xxh3_state_ptr = (XXH3_state_ptr); \
        tmp_xxh3_state_ptr->seed = 0;                        \
        tmp_xxh3_state_ptr->extSecret = NULL;                \
    } while(0)


/*!
 * @brief Calculates the 128-bit hash of @p data using XXH3.
 *
 * @param data The block of data to be hashed, at least @p len bytes in size.
 * @param len  The length of @p data, in bytes.
 * @param seed The 64-bit seed to alter the hash's output predictably.
 *
 * @pre
 *   The memory between @p data and @p data + @p len must be valid,
 *   readable, contiguous memory. However, if @p len is `0`, @p data may be
 *   `NULL`. In C++, this also must be *TriviallyCopyable*.
 *
 * @return The calculated 128-bit XXH3 value.
 *
 * @see @ref single_shot_example "Single Shot Example" for an example.
 */
XXH_PUBLIC_API XXH_PUREF XXH128_hash_t XXH128(XXH_NOESCAPE const void* data, size_t len, XXH64_hash_t seed);


/* ===   Experimental API   === */
/* Symbols defined below must be considered tied to a specific library version. */

/*!
 * @brief Derive a high-entropy secret from any user-defined content, named customSeed.
 *
 * @param secretBuffer    A writable buffer for derived high-entropy secret data.
 * @param secretSize      Size of secretBuffer, in bytes.  Must be >= XXH3_SECRET_SIZE_MIN.
 * @param customSeed      A user-defined content.
 * @param customSeedSize  Size of customSeed, in bytes.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * The generated secret can be used in combination with `*_withSecret()` functions.
 * The `_withSecret()` variants are useful to provide a higher level of protection
 * than 64-bit seed, as it becomes much more difficult for an external actor to
 * guess how to impact the calculation logic.
 *
 * The function accepts as input a custom seed of any length and any content,
 * and derives from it a high-entropy secret of length @p secretSize into an
 * already allocated buffer @p secretBuffer.
 *
 * The generated secret can then be used with any `*_withSecret()` variant.
 * The functions @ref XXH3_128bits_withSecret(), @ref XXH3_64bits_withSecret(),
 * @ref XXH3_128bits_reset_withSecret() and @ref XXH3_64bits_reset_withSecret()
 * are part of this list. They all accept a `secret` parameter
 * which must be large enough for implementation reasons (>= @ref XXH3_SECRET_SIZE_MIN)
 * _and_ feature very high entropy (consist of random-looking bytes).
 * These conditions can be a high bar to meet, so @ref XXH3_generateSecret() can
 * be employed to ensure proper quality.
 *
 * @p customSeed can be anything. It can have any size, even small ones,
 * and its content can be anything, even "poor entropy" sources such as a bunch
 * of zeroes. The resulting `secret` will nonetheless provide all required qualities.
 *
 * @pre
 *   - @p secretSize must be >= @ref XXH3_SECRET_SIZE_MIN
 *   - When @p customSeedSize > 0, supplying NULL as customSeed is undefined behavior.
 *
 * Example code:
 * @code{.c}
 *    #include <stdio.h>
 *    #include <stdlib.h>
 *    #include <string.h>
 *    #define XXH_STATIC_LINKING_ONLY // expose unstable API
 *    #include "xxhash.h"
 *    // Hashes argv[2] using the entropy from argv[1].
 *    int main(int argc, char* argv[])
 *    {
 *        char secret[XXH3_SECRET_SIZE_MIN];
 *        if (argv != 3) { return 1; }
 *        XXH3_generateSecret(secret, sizeof(secret), argv[1], strlen(argv[1]));
 *        XXH64_hash_t h = XXH3_64bits_withSecret(
 *             argv[2], strlen(argv[2]),
 *             secret, sizeof(secret)
 *        );
 *        printf("%016llx\n", (unsigned long long) h);
 *    }
 * @endcode
 */
XXH_PUBLIC_API XXH_errorcode XXH3_generateSecret(XXH_NOESCAPE void* secretBuffer, size_t secretSize, XXH_NOESCAPE const void* customSeed, size_t customSeedSize);

/*!
 * @brief Generate the same secret as the _withSeed() variants.
 *
 * @param secretBuffer A writable buffer of @ref XXH3_SECRET_DEFAULT_SIZE bytes
 * @param seed         The 64-bit seed to alter the hash result predictably.
 *
 * The generated secret can be used in combination with
 *`*_withSecret()` and `_withSecretandSeed()` variants.
 *
 * Example C++ `std::string` hash class:
 * @code{.cpp}
 *    #include <string>
 *    #define XXH_STATIC_LINKING_ONLY // expose unstable API
 *    #include "xxhash.h"
 *    // Slow, seeds each time
 *    class HashSlow {
 *        XXH64_hash_t seed;
 *    public:
 *        HashSlow(XXH64_hash_t s) : seed{s} {}
 *        size_t operator()(const std::string& x) const {
 *            return size_t{XXH3_64bits_withSeed(x.c_str(), x.length(), seed)};
 *        }
 *    };
 *    // Fast, caches the seeded secret for future uses.
 *    class HashFast {
 *        unsigned char secret[XXH3_SECRET_DEFAULT_SIZE];
 *    public:
 *        HashFast(XXH64_hash_t s) {
 *            XXH3_generateSecret_fromSeed(secret, seed);
 *        }
 *        size_t operator()(const std::string& x) const {
 *            return size_t{
 *                XXH3_64bits_withSecret(x.c_str(), x.length(), secret, sizeof(secret))
 *            };
 *        }
 *    };
 * @endcode
 */
XXH_PUBLIC_API void XXH3_generateSecret_fromSeed(XXH_NOESCAPE void* secretBuffer, XXH64_hash_t seed);

/*!
 * @brief Maximum size of "short" key in bytes.
 */
#define XXH3_MIDSIZE_MAX 240

/*!
 * @brief Calculates 64/128-bit seeded variant of XXH3 hash of @p data.
 *
 * @param data       The block of data to be hashed, at least @p len bytes in size.
 * @param len        The length of @p data, in bytes.
 * @param secret     The secret data.
 * @param secretSize The length of @p secret, in bytes.
 * @param seed       The 64-bit seed to alter the hash result predictably.
 *
 * These variants generate hash values using either:
 * - @p seed for "short" keys (< @ref XXH3_MIDSIZE_MAX = 240 bytes)
 * - @p secret for "large" keys (>= @ref XXH3_MIDSIZE_MAX).
 *
 * This generally benefits speed, compared to `_withSeed()` or `_withSecret()`.
 * `_withSeed()` has to generate the secret on the fly for "large" keys.
 * It's fast, but can be perceptible for "not so large" keys (< 1 KB).
 * `_withSecret()` has to generate the masks on the fly for "small" keys,
 * which requires more instructions than _withSeed() variants.
 * Therefore, _withSecretandSeed variant combines the best of both worlds.
 *
 * When @p secret has been generated by XXH3_generateSecret_fromSeed(),
 * this variant produces *exactly* the same results as `_withSeed()` variant,
 * hence offering only a pure speed benefit on "large" input,
 * by skipping the need to regenerate the secret for every large input.
 *
 * Another usage scenario is to hash the secret to a 64-bit hash value,
 * for example with XXH3_64bits(), which then becomes the seed,
 * and then employ both the seed and the secret in _withSecretandSeed().
 * On top of speed, an added benefit is that each bit in the secret
 * has a 50% chance to swap each bit in the output, via its impact to the seed.
 *
 * This is not guaranteed when using the secret directly in "small data" scenarios,
 * because only portions of the secret are employed for small data.
 */
XXH_PUBLIC_API XXH_PUREF XXH64_hash_t
XXH3_64bits_withSecretandSeed(XXH_NOESCAPE const void* data, size_t len,
                              XXH_NOESCAPE const void* secret, size_t secretSize,
                              XXH64_hash_t seed);

/*!
 * @brief Calculates 128-bit seeded variant of XXH3 hash of @p data.
 *
 * @param input      The memory segment to be hashed, at least @p len bytes in size.
 * @param length     The length of @p data, in bytes.
 * @param secret     The secret used to alter hash result predictably.
 * @param secretSize The length of @p secret, in bytes (must be >= XXH3_SECRET_SIZE_MIN)
 * @param seed64     The 64-bit seed to alter the hash result predictably.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @see XXH3_64bits_withSecretandSeed(): contract is the same.
 */
XXH_PUBLIC_API XXH_PUREF XXH128_hash_t
XXH3_128bits_withSecretandSeed(XXH_NOESCAPE const void* input, size_t length,
                               XXH_NOESCAPE const void* secret, size_t secretSize,
                               XXH64_hash_t seed64);

#endif  /* !XXH_NO_XXH3 */
