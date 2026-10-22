/*
 * xxHash - Extremely Fast Hash algorithm
 * Header File
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
 */

/*!
 * @mainpage xxHash
 *
 * xxHash is an extremely fast non-cryptographic hash algorithm, working at RAM speed
 * limits.
 *
 * It is proposed in four flavors, in three families:
 * 1. @ref XXH32_family
 *   - Classic 32-bit hash function. Simple, compact, and runs on almost all
 *     32-bit and 64-bit systems.
 * 2. @ref XXH64_family
 *   - Classic 64-bit adaptation of XXH32. Just as simple, and runs well on most
 *     64-bit systems (but _not_ 32-bit systems).
 * 3. @ref XXH3_family
 *   - Modern 64-bit and 128-bit hash function family which features improved
 *     strength and performance across the board, especially on smaller data.
 *     It benefits greatly from SIMD and 64-bit without requiring it.
 *
 * Benchmarks
 * ---
 * The reference system uses an Intel i7-9700K CPU, and runs Ubuntu x64 20.04.
 * The open source benchmark program is compiled with clang v10.0 using -O3 flag.
 *
 * | Hash Name            | ISA ext | Width | Large Data Speed | Small Data Velocity |
 * | -------------------- | ------- | ----: | ---------------: | ------------------: |
 * | XXH3_64bits()        | @b AVX2 |    64 |        59.4 GB/s |               133.1 |
 * | MeowHash             | AES-NI  |   128 |        58.2 GB/s |                52.5 |
 * | XXH3_128bits()       | @b AVX2 |   128 |        57.9 GB/s |               118.1 |
 * | CLHash               | PCLMUL  |    64 |        37.1 GB/s |                58.1 |
 * | XXH3_64bits()        | @b SSE2 |    64 |        31.5 GB/s |               133.1 |
 * | XXH3_128bits()       | @b SSE2 |   128 |        29.6 GB/s |               118.1 |
 * | RAM sequential read  |         |   N/A |        28.0 GB/s |                 N/A |
 * | ahash                | AES-NI  |    64 |        22.5 GB/s |               107.2 |
 * | City64               |         |    64 |        22.0 GB/s |                76.6 |
 * | T1ha2                |         |    64 |        22.0 GB/s |                99.0 |
 * | City128              |         |   128 |        21.7 GB/s |                57.7 |
 * | FarmHash             | AES-NI  |    64 |        21.3 GB/s |                71.9 |
 * | XXH64()              |         |    64 |        19.4 GB/s |                71.0 |
 * | SpookyHash           |         |    64 |        19.3 GB/s |                53.2 |
 * | Mum                  |         |    64 |        18.0 GB/s |                67.0 |
 * | CRC32C               | SSE4.2  |    32 |        13.0 GB/s |                57.9 |
 * | XXH32()              |         |    32 |         9.7 GB/s |                71.9 |
 * | City32               |         |    32 |         9.1 GB/s |                66.0 |
 * | Blake3*              | @b AVX2 |   256 |         4.4 GB/s |                 8.1 |
 * | Murmur3              |         |    32 |         3.9 GB/s |                56.1 |
 * | SipHash*             |         |    64 |         3.0 GB/s |                43.2 |
 * | Blake3*              | @b SSE2 |   256 |         2.4 GB/s |                 8.1 |
 * | HighwayHash          |         |    64 |         1.4 GB/s |                 6.0 |
 * | FNV64                |         |    64 |         1.2 GB/s |                62.7 |
 * | Blake2*              |         |   256 |         1.1 GB/s |                 5.1 |
 * | SHA1*                |         |   160 |         0.8 GB/s |                 5.6 |
 * | MD5*                 |         |   128 |         0.6 GB/s |                 7.8 |
 * @note
 *   - Hashes which require a specific ISA extension are noted. SSE2 is also noted,
 *     even though it is mandatory on x64.
 *   - Hashes with an asterisk are cryptographic. Note that MD5 is non-cryptographic
 *     by modern standards.
 *   - Small data velocity is a rough average of algorithm's efficiency for small
 *     data. For more accurate information, see the wiki.
 *   - More benchmarks and strength tests are found on the wiki:
 *         https://github.com/Cyan4973/xxHash/wiki
 *
 * Usage
 * ------
 * All xxHash variants use a similar API. Changing the algorithm is a trivial
 * substitution.
 *
 * @pre
 *    For functions which take an input and length parameter, the following
 *    requirements are assumed:
 *    - The range from [`input`, `input + length`) is valid, readable memory.
 *      - The only exception is if the `length` is `0`, `input` may be `NULL`.
 *    - For C++, the objects must have the *TriviallyCopyable* property, as the
 *      functions access bytes directly as if it was an array of `unsigned char`.
 *
 * @anchor single_shot_example
 * **Single Shot**
 *
 * These functions are stateless functions which hash a contiguous block of memory,
 * immediately returning the result. They are the easiest and usually the fastest
 * option.
 *
 * XXH32(), XXH64(), XXH3_64bits(), XXH3_128bits()
 *
 * @code{.c}
 *   #include <string.h>
 *   #include "xxhash.h"
 *
 *   // Example for a function which hashes a null terminated string with XXH32().
 *   XXH32_hash_t hash_string(const char* string, XXH32_hash_t seed)
 *   {
 *       // NULL pointers are only valid if the length is zero
 *       size_t length = (string == NULL) ? 0 : strlen(string);
 *       return XXH32(string, length, seed);
 *   }
 * @endcode
 *
 *
 * @anchor streaming_example
 * **Streaming**
 *
 * These groups of functions allow incremental hashing of unknown size, even
 * more than what would fit in a size_t.
 *
 * XXH32_reset(), XXH64_reset(), XXH3_64bits_reset(), XXH3_128bits_reset()
 *
 * @code{.c}
 *   #include <stdio.h>
 *   #include <assert.h>
 *   #include "xxhash.h"
 *   // Example for a function which hashes a FILE incrementally with XXH3_64bits().
 *   XXH64_hash_t hashFile(FILE* f)
 *   {
 *       // Allocate a state struct. Do not just use malloc() or new.
 *       XXH3_state_t* state = XXH3_createState();
 *       assert(state != NULL && "Out of memory!");
 *       // Reset the state to start a new hashing session.
 *       XXH3_64bits_reset(state);
 *       char buffer[4096];
 *       size_t count;
 *       // Read the file in chunks
 *       while ((count = fread(buffer, 1, sizeof(buffer), f)) != 0) {
 *           // Run update() as many times as necessary to process the data
 *           XXH3_64bits_update(state, buffer, count);
 *       }
 *       // Retrieve the finalized hash. This will not change the state.
 *       XXH64_hash_t result = XXH3_64bits_digest(state);
 *       // Free the state. Do not use free().
 *       XXH3_freeState(state);
 *       return result;
 *   }
 * @endcode
 *
 * Streaming functions generate the xxHash value from an incremental input.
 * This method is slower than single-call functions, due to state management.
 * For small inputs, prefer `XXH32()` and `XXH64()`, which are better optimized.
 *
 * An XXH state must first be allocated using `XXH*_createState()`.
 *
 * Start a new hash by initializing the state with a seed using `XXH*_reset()`.
 *
 * Then, feed the hash state by calling `XXH*_update()` as many times as necessary.
 *
 * The function returns an error code, with 0 meaning OK, and any other value
 * meaning there is an error.
 *
 * Finally, a hash value can be produced anytime, by using `XXH*_digest()`.
 * This function returns the nn-bits hash as an int or long long.
 *
 * It's still possible to continue inserting input into the hash state after a
 * digest, and generate new hash values later on by invoking `XXH*_digest()`.
 *
 * When done, release the state using `XXH*_freeState()`.
 *
 *
 * @anchor canonical_representation_example
 * **Canonical Representation**
 *
 * The default return values from XXH functions are unsigned 32, 64 and 128 bit
 * integers.
 * This the simplest and fastest format for further post-processing.
 *
 * However, this leaves open the question of what is the order on the byte level,
 * since little and big endian conventions will store the same number differently.
 *
 * The canonical representation settles this issue by mandating big-endian
 * convention, the same convention as human-readable numbers (large digits first).
 *
 * When writing hash values to storage, sending them over a network, or printing
 * them, it's highly recommended to use the canonical representation to ensure
 * portability across a wider range of systems, present and future.
 *
 * The following functions allow transformation of hash values to and from
 * canonical format.
 *
 * XXH32_canonicalFromHash(), XXH32_hashFromCanonical(),
 * XXH64_canonicalFromHash(), XXH64_hashFromCanonical(),
 * XXH128_canonicalFromHash(), XXH128_hashFromCanonical(),
 *
 * @code{.c}
 *   #include <stdio.h>
 *   #include "xxhash.h"
 *
 *   // Example for a function which prints XXH32_hash_t in human readable format
 *   void printXxh32(XXH32_hash_t hash)
 *   {
 *       XXH32_canonical_t cano;
 *       XXH32_canonicalFromHash(&cano, hash);
 *       size_t i;
 *       for(i = 0; i < sizeof(cano.digest); ++i) {
 *           printf("%02x", cano.digest[i]);
 *       }
 *       printf("\n");
 *   }
 *
 *   // Example for a function which converts XXH32_canonical_t to XXH32_hash_t
 *   XXH32_hash_t convertCanonicalToXxh32(XXH32_canonical_t cano)
 *   {
 *       XXH32_hash_t hash = XXH32_hashFromCanonical(&cano);
 *       return hash;
 *   }
 * @endcode
 *
 *
 * @file xxhash.h
 * xxHash prototypes and implementation
 */

#if defined(__cplusplus) && !defined(XXH_NO_EXTERNC_GUARD)
extern "C" {
#endif

/* ****************************
 *  INLINE mode
 ******************************/
/*!
 * @defgroup public Public API
 * Contains details on the public xxHash functions.
 * @{
 */
#ifdef XXH_DOXYGEN
/*!
 * @brief Gives access to internal state declaration, required for static allocation.
 *
 * Incompatible with dynamic linking, due to risks of ABI changes.
 *
 * Usage:
 * @code{.c}
 *     #define XXH_STATIC_LINKING_ONLY
 *     #include "xxhash.h"
 * @endcode
 */
#  define XXH_STATIC_LINKING_ONLY
/* Do not undef XXH_STATIC_LINKING_ONLY for Doxygen */

/*!
 * @brief Gives access to internal definitions.
 *
 * Usage:
 * @code{.c}
 *     #define XXH_STATIC_LINKING_ONLY
 *     #define XXH_IMPLEMENTATION
 *     #include "xxhash.h"
 * @endcode
 */
#  define XXH_IMPLEMENTATION
/* Do not undef XXH_IMPLEMENTATION for Doxygen */

/*!
 * @brief Exposes the implementation and marks all functions as `inline`.
 *
 * Use these build macros to inline xxhash into the target unit.
 * Inlining improves performance on small inputs, especially when the length is
 * expressed as a compile-time constant:
 *
 *  https://fastcompression.blogspot.com/2018/03/xxhash-for-small-keys-impressive-power.html
 *
 * It also keeps xxHash symbols private to the unit, so they are not exported.
 *
 * Usage:
 * @code{.c}
 *     #define XXH_INLINE_ALL
 *     #include "xxhash.h"
 * @endcode
 * Do not compile and link xxhash.o as a separate object, as it is not useful.
 */
#  define XXH_INLINE_ALL
#  undef XXH_INLINE_ALL
/*!
 * @brief Exposes the implementation without marking functions as inline.
 */
#  define XXH_PRIVATE_API
#  undef XXH_PRIVATE_API
/*!
 * @brief Emulate a namespace by transparently prefixing all symbols.
 *
 * If you want to include _and expose_ xxHash functions from within your own
 * library, but also want to avoid symbol collisions with other libraries which
 * may also include xxHash, you can use @ref XXH_NAMESPACE to automatically prefix
 * any public symbol from xxhash library with the value of @ref XXH_NAMESPACE
 * (therefore, avoid empty or numeric values).
 *
 * Note that no change is required within the calling program as long as it
 * includes `xxhash.h`: Regular symbol names will be automatically translated
 * by this header.
 */
#  define XXH_NAMESPACE /* YOUR NAME HERE */
#  undef XXH_NAMESPACE
#endif

#define XXH_CAT(A,B) A##B
#define XXH_NAME2(A,B) XXH_CAT(A,B)
#define XXH_IPREF(Id) XXH_NAME2(XXH_NAMESPACE, Id)

#if (defined(XXH_INLINE_ALL) || defined(XXH_PRIVATE_API)) \
    && !defined(XXH_INLINE_ALL_31684351384)
   /* this section should be traversed only once */
#  define XXH_INLINE_ALL_31684351384
   /* give access to the advanced API, required to compile implementations */
#  undef XXH_STATIC_LINKING_ONLY   /* avoid macro redef */
#  define XXH_STATIC_LINKING_ONLY
   /* make all functions private */
#  undef XXH_PUBLIC_API
#  if defined(__GNUC__)
#    define XXH_PUBLIC_API static __inline __attribute__((__unused__))
#  elif defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
#    define XXH_PUBLIC_API static inline
#  elif defined(_MSC_VER)
#    define XXH_PUBLIC_API static __inline
#  else
     /* note: this version may generate warnings for unused static functions */
#    define XXH_PUBLIC_API static
#  endif

   /*
    * This part deals with the special case where a unit wants to inline xxHash,
    * but "xxhash.h" has previously been included without XXH_INLINE_ALL,
    * such as part of some previously included *.h header file.
    * Without further action, the new include would just be ignored,
    * and functions would effectively _not_ be inlined (silent failure).
    * The following macros solve this situation by prefixing all inlined names,
    * avoiding naming collision with previous inclusions.
    */
   /* Before that, we unconditionally #undef all symbols,
    * in case they were already defined with XXH_NAMESPACE.
    * They will then be redefined for XXH_INLINE_ALL
    */
#  undef XXH_versionNumber
    /* XXH32 */
#  undef XXH32
#  undef XXH32_createState
#  undef XXH32_freeState
#  undef XXH32_reset
#  undef XXH32_update
#  undef XXH32_digest
#  undef XXH32_copyState
#  undef XXH32_canonicalFromHash
#  undef XXH32_hashFromCanonical
    /* XXH64 */
#  undef XXH64
#  undef XXH64_createState
#  undef XXH64_freeState
#  undef XXH64_reset
#  undef XXH64_update
#  undef XXH64_digest
#  undef XXH64_copyState
#  undef XXH64_canonicalFromHash
#  undef XXH64_hashFromCanonical
    /* XXH3_64bits */
#  undef XXH3_64bits
#  undef XXH3_64bits_withSecret
#  undef XXH3_64bits_withSeed
#  undef XXH3_64bits_withSecretandSeed
#  undef XXH3_createState
#  undef XXH3_freeState
#  undef XXH3_copyState
#  undef XXH3_64bits_reset
#  undef XXH3_64bits_reset_withSeed
#  undef XXH3_64bits_reset_withSecret
#  undef XXH3_64bits_update
#  undef XXH3_64bits_digest
#  undef XXH3_generateSecret
    /* XXH3_128bits */
#  undef XXH128
#  undef XXH3_128bits
#  undef XXH3_128bits_withSeed
#  undef XXH3_128bits_withSecret
#  undef XXH3_128bits_reset
#  undef XXH3_128bits_reset_withSeed
#  undef XXH3_128bits_reset_withSecret
#  undef XXH3_128bits_reset_withSecretandSeed
#  undef XXH3_128bits_update
#  undef XXH3_128bits_digest
#  undef XXH128_isEqual
#  undef XXH128_cmp
#  undef XXH128_canonicalFromHash
#  undef XXH128_hashFromCanonical
    /* Finally, free the namespace itself */
#  undef XXH_NAMESPACE

    /* employ the namespace for XXH_INLINE_ALL */
#  define XXH_NAMESPACE XXH_INLINE_
   /*
    * Some identifiers (enums, type names) are not symbols,
    * but they must nonetheless be renamed to avoid redeclaration.
    * Alternative solution: do not redeclare them.
    * However, this requires some #ifdefs, and has a more dispersed impact.
    * Meanwhile, renaming can be achieved in a single place.
    */
#  define XXH_OK XXH_IPREF(XXH_OK)
#  define XXH_ERROR XXH_IPREF(XXH_ERROR)
#  define XXH_errorcode XXH_IPREF(XXH_errorcode)
#  define XXH32_canonical_t  XXH_IPREF(XXH32_canonical_t)
#  define XXH64_canonical_t  XXH_IPREF(XXH64_canonical_t)
#  define XXH128_canonical_t XXH_IPREF(XXH128_canonical_t)
#  define XXH32_state_s XXH_IPREF(XXH32_state_s)
#  define XXH32_state_t XXH_IPREF(XXH32_state_t)
#  define XXH64_state_s XXH_IPREF(XXH64_state_s)
#  define XXH64_state_t XXH_IPREF(XXH64_state_t)
#  define XXH3_state_s  XXH_IPREF(XXH3_state_s)
#  define XXH3_state_t  XXH_IPREF(XXH3_state_t)
#  define XXH128_hash_t XXH_IPREF(XXH128_hash_t)
   /* Ensure the header is parsed again, even if it was previously included */
#  undef XXHASH_H_5627135585666179
#  undef XXHASH_H_STATIC_13879238742
#endif /* XXH_INLINE_ALL || XXH_PRIVATE_API */

/* ****************************************************************
 *  Stable API
 *****************************************************************/
#ifndef XXHASH_H_5627135585666179
#define XXHASH_H_5627135585666179 1

/*! @brief Marks a global symbol. */
#if !defined(XXH_INLINE_ALL) && !defined(XXH_PRIVATE_API)
#  if defined(_WIN32) && defined(_MSC_VER) && (defined(XXH_IMPORT) || defined(XXH_EXPORT))
#    ifdef XXH_EXPORT
#      define XXH_PUBLIC_API __declspec(dllexport)
#    elif XXH_IMPORT
#      define XXH_PUBLIC_API __declspec(dllimport)
#    endif
#  else
#    define XXH_PUBLIC_API   /* do nothing */
#  endif
#endif

#ifdef XXH_NAMESPACE
#  define XXH_versionNumber XXH_IPREF(XXH_versionNumber)
/* XXH32 */
#  define XXH32 XXH_IPREF(XXH32)
#  define XXH32_createState XXH_IPREF(XXH32_createState)
#  define XXH32_freeState XXH_IPREF(XXH32_freeState)
#  define XXH32_reset XXH_IPREF(XXH32_reset)
#  define XXH32_update XXH_IPREF(XXH32_update)
#  define XXH32_digest XXH_IPREF(XXH32_digest)
#  define XXH32_copyState XXH_IPREF(XXH32_copyState)
#  define XXH32_canonicalFromHash XXH_IPREF(XXH32_canonicalFromHash)
#  define XXH32_hashFromCanonical XXH_IPREF(XXH32_hashFromCanonical)
/* XXH64 */
#  define XXH64 XXH_IPREF(XXH64)
#  define XXH64_createState XXH_IPREF(XXH64_createState)
#  define XXH64_freeState XXH_IPREF(XXH64_freeState)
#  define XXH64_reset XXH_IPREF(XXH64_reset)
#  define XXH64_update XXH_IPREF(XXH64_update)
#  define XXH64_digest XXH_IPREF(XXH64_digest)
#  define XXH64_copyState XXH_IPREF(XXH64_copyState)
#  define XXH64_canonicalFromHash XXH_IPREF(XXH64_canonicalFromHash)
#  define XXH64_hashFromCanonical XXH_IPREF(XXH64_hashFromCanonical)
/* XXH3_64bits */
#  define XXH3_64bits XXH_IPREF(XXH3_64bits)
#  define XXH3_64bits_withSecret XXH_IPREF(XXH3_64bits_withSecret)
#  define XXH3_64bits_withSeed XXH_IPREF(XXH3_64bits_withSeed)
#  define XXH3_64bits_withSecretandSeed XXH_IPREF(XXH3_64bits_withSecretandSeed)
#  define XXH3_createState XXH_IPREF(XXH3_createState)
#  define XXH3_freeState XXH_IPREF(XXH3_freeState)
#  define XXH3_copyState XXH_IPREF(XXH3_copyState)
#  define XXH3_64bits_reset XXH_IPREF(XXH3_64bits_reset)
#  define XXH3_64bits_reset_withSeed XXH_IPREF(XXH3_64bits_reset_withSeed)
#  define XXH3_64bits_reset_withSecret XXH_IPREF(XXH3_64bits_reset_withSecret)
#  define XXH3_64bits_reset_withSecretandSeed XXH_IPREF(XXH3_64bits_reset_withSecretandSeed)
#  define XXH3_64bits_update XXH_IPREF(XXH3_64bits_update)
#  define XXH3_64bits_digest XXH_IPREF(XXH3_64bits_digest)
#  define XXH3_generateSecret XXH_IPREF(XXH3_generateSecret)
#  define XXH3_generateSecret_fromSeed XXH_IPREF(XXH3_generateSecret_fromSeed)
/* XXH3_128bits */
#  define XXH128 XXH_IPREF(XXH128)
#  define XXH3_128bits XXH_IPREF(XXH3_128bits)
#  define XXH3_128bits_withSeed XXH_IPREF(XXH3_128bits_withSeed)
#  define XXH3_128bits_withSecret XXH_IPREF(XXH3_128bits_withSecret)
#  define XXH3_128bits_withSecretandSeed XXH_IPREF(XXH3_128bits_withSecretandSeed)
#  define XXH3_128bits_reset XXH_IPREF(XXH3_128bits_reset)
#  define XXH3_128bits_reset_withSeed XXH_IPREF(XXH3_128bits_reset_withSeed)
#  define XXH3_128bits_reset_withSecret XXH_IPREF(XXH3_128bits_reset_withSecret)
#  define XXH3_128bits_reset_withSecretandSeed XXH_IPREF(XXH3_128bits_reset_withSecretandSeed)
#  define XXH3_128bits_update XXH_IPREF(XXH3_128bits_update)
#  define XXH3_128bits_digest XXH_IPREF(XXH3_128bits_digest)
#  define XXH128_isEqual XXH_IPREF(XXH128_isEqual)
#  define XXH128_cmp     XXH_IPREF(XXH128_cmp)
#  define XXH128_canonicalFromHash XXH_IPREF(XXH128_canonicalFromHash)
#  define XXH128_hashFromCanonical XXH_IPREF(XXH128_hashFromCanonical)
#endif


/* *************************************
*  Compiler specifics
***************************************/

/* specific declaration modes for Windows */
#if !defined(XXH_INLINE_ALL) && !defined(XXH_PRIVATE_API)
#  if defined(_WIN32) && defined(_MSC_VER) && (defined(XXH_IMPORT) || defined(XXH_EXPORT))
#    ifdef XXH_EXPORT
#      define XXH_PUBLIC_API __declspec(dllexport)
#    elif XXH_IMPORT
#      define XXH_PUBLIC_API __declspec(dllimport)
#    endif
#  else
#    define XXH_PUBLIC_API   /* do nothing */
#  endif
#endif

#if defined (__GNUC__)
# define XXH_CONSTF  __attribute__((__const__))
# define XXH_PUREF   __attribute__((__pure__))
# define XXH_MALLOCF __attribute__((__malloc__))
#else
# define XXH_CONSTF  /* disable */
# define XXH_PUREF
# define XXH_MALLOCF
#endif

/* *************************************
*  Version
***************************************/
#define XXH_VERSION_MAJOR    0
#define XXH_VERSION_MINOR    8
#define XXH_VERSION_RELEASE  4
/*! @brief Version number, encoded as two digits each */
#define XXH_VERSION_NUMBER  (XXH_VERSION_MAJOR *100*100 + XXH_VERSION_MINOR *100 + XXH_VERSION_RELEASE)

/*!
 * @brief Obtains the xxHash version.
 *
 * This is mostly useful when xxHash is compiled as a shared library,
 * since the returned value comes from the library, as opposed to header file.
 *
 * @return @ref XXH_VERSION_NUMBER of the invoked library.
 */
XXH_PUBLIC_API XXH_CONSTF unsigned XXH_versionNumber (void);


/* ****************************
*  Common basic types
******************************/
#include <stddef.h>   /* size_t */
/*!
 * @brief Exit code for the streaming API.
 */
typedef enum {
    XXH_OK = 0, /*!< OK */
    XXH_ERROR   /*!< Error */
} XXH_errorcode;


/*-**********************************************************************
*  32-bit hash
************************************************************************/
#if defined(XXH_DOXYGEN) /* Don't show <stdint.h> include */
/*!
 * @brief An unsigned 32-bit integer.
 *
 * Not necessarily defined to `uint32_t` but functionally equivalent.
 */
typedef uint32_t XXH32_hash_t;

#elif !defined (__VMS) \
  && (defined (__cplusplus) \
  || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */) )
#   ifdef _AIX
#     include <inttypes.h>
#   else
#     include <stdint.h>
#   endif
    typedef uint32_t XXH32_hash_t;

#else
#   include <limits.h>
#   if UINT_MAX == 0xFFFFFFFFUL
      typedef unsigned int XXH32_hash_t;
#   elif ULONG_MAX == 0xFFFFFFFFUL
      typedef unsigned long XXH32_hash_t;
#   else
#     error "unsupported platform: need a 32-bit type"
#   endif
#endif

/*!
 * @}
 *
 * @defgroup XXH32_family XXH32 family
 * @ingroup public
 * Contains functions used in the classic 32-bit xxHash algorithm.
 *
 * @note
 *   XXH32 is useful for older platforms, with no or poor 64-bit performance.
 *   Note that the @ref XXH3_family provides competitive speed for both 32-bit
 *   and 64-bit systems, and offers true 64/128 bit hash results.
 *
 * @see @ref XXH64_family, @ref XXH3_family : Other xxHash families
 * @see @ref XXH32_impl for implementation details
 * @{
 */

/*!
 * @brief Calculates the 32-bit hash of @p input using xxHash32.
 *
 * @param input The block of data to be hashed, at least @p length bytes in size.
 * @param length The length of @p input, in bytes.
 * @param seed The 32-bit seed to alter the hash's output predictably.
 *
 * @pre
 *   The memory between @p input and @p input + @p length must be valid,
 *   readable, contiguous memory. However, if @p length is `0`, @p input may be
 *   `NULL`. In C++, this also must be *TriviallyCopyable*.
 *
 * @return The calculated 32-bit xxHash32 value.
 *
 * @see @ref single_shot_example "Single Shot Example" for an example.
 */
XXH_PUBLIC_API XXH_PUREF XXH32_hash_t XXH32 (const void* input, size_t length, XXH32_hash_t seed);

#ifndef XXH_NO_STREAM
/*!
 * @typedef struct XXH32_state_s XXH32_state_t
 * @brief The opaque state struct for the XXH32 streaming API.
 *
 * @see XXH32_state_s for details.
 * @see @ref streaming_example "Streaming Example"
 */
typedef struct XXH32_state_s XXH32_state_t;

/*!
 * @brief Allocates an @ref XXH32_state_t.
 *
 * @return An allocated pointer of @ref XXH32_state_t on success.
 * @return `NULL` on failure.
 *
 * @note Must be freed with XXH32_freeState().
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_MALLOCF XXH32_state_t* XXH32_createState(void);
/*!
 * @brief Frees an @ref XXH32_state_t.
 *
 * @param statePtr A pointer to an @ref XXH32_state_t allocated with @ref XXH32_createState().
 *
 * @return @ref XXH_OK.
 *
 * @note @p statePtr must be allocated with XXH32_createState().
 *
 * @see @ref streaming_example "Streaming Example"
 *
 */
XXH_PUBLIC_API XXH_errorcode  XXH32_freeState(XXH32_state_t* statePtr);
/*!
 * @brief Copies one @ref XXH32_state_t to another.
 *
 * @param dst_state The state to copy to.
 * @param src_state The state to copy from.
 * @pre
 *   @p dst_state and @p src_state must not be `NULL` and must not overlap.
 */
XXH_PUBLIC_API void XXH32_copyState(XXH32_state_t* dst_state, const XXH32_state_t* src_state);

/*!
 * @brief Resets an @ref XXH32_state_t to begin a new hash.
 *
 * @param statePtr The state struct to reset.
 * @param seed The 32-bit seed to alter the hash result predictably.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @note This function resets and seeds a state. Call it before @ref XXH32_update().
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_errorcode XXH32_reset  (XXH32_state_t* statePtr, XXH32_hash_t seed);

/*!
 * @brief Consumes a block of @p input to an @ref XXH32_state_t.
 *
 * @param statePtr The state struct to update.
 * @param input The block of data to be hashed, at least @p length bytes in size.
 * @param length The length of @p input, in bytes.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 * @pre
 *   The memory between @p input and @p input + @p length must be valid,
 *   readable, contiguous memory. However, if @p length is `0`, @p input may be
 *   `NULL`. In C++, this also must be *TriviallyCopyable*.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @note Call this to incrementally consume blocks of data.
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_errorcode XXH32_update (XXH32_state_t* statePtr, const void* input, size_t length);

/*!
 * @brief Returns the calculated hash value from an @ref XXH32_state_t.
 *
 * @param statePtr The state struct to calculate the hash from.
 *
 * @pre
 *  @p statePtr must not be `NULL`.
 *
 * @return The calculated 32-bit xxHash32 value from that state.
 *
 * @note
 *   Calling XXH32_digest() will not affect @p statePtr, so you can update,
 *   digest, and update again.
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_PUREF XXH32_hash_t XXH32_digest (const XXH32_state_t* statePtr);
#endif /* !XXH_NO_STREAM */

/*******   Canonical representation   *******/

/*!
 * @brief Canonical (big endian) representation of @ref XXH32_hash_t.
 */
typedef struct {
    unsigned char digest[4]; /*!< Hash bytes, big endian */
} XXH32_canonical_t;

/*!
 * @brief Converts an @ref XXH32_hash_t to a big endian @ref XXH32_canonical_t.
 *
 * @param dst  The @ref XXH32_canonical_t pointer to be stored to.
 * @param hash The @ref XXH32_hash_t to be converted.
 *
 * @pre
 *   @p dst must not be `NULL`.
 *
 * @see @ref canonical_representation_example "Canonical Representation Example"
 */
XXH_PUBLIC_API void XXH32_canonicalFromHash(XXH32_canonical_t* dst, XXH32_hash_t hash);

/*!
 * @brief Converts an @ref XXH32_canonical_t to a native @ref XXH32_hash_t.
 *
 * @param src The @ref XXH32_canonical_t to convert.
 *
 * @pre
 *   @p src must not be `NULL`.
 *
 * @return The converted hash.
 *
 * @see @ref canonical_representation_example "Canonical Representation Example"
 */
XXH_PUBLIC_API XXH_PUREF XXH32_hash_t XXH32_hashFromCanonical(const XXH32_canonical_t* src);


/*! @cond Doxygen ignores this part */
#ifdef __has_attribute
# define XXH_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
# define XXH_HAS_ATTRIBUTE(x) 0
#endif
/*! @endcond */

/*! @cond Doxygen ignores this part */
/* C-language Attributes are added in C23. */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 202311L) && defined(__has_c_attribute)
# define XXH_HAS_C_ATTRIBUTE(x) __has_c_attribute(x)
#else
# define XXH_HAS_C_ATTRIBUTE(x) 0
#endif
/*! @endcond */

/*! @cond Doxygen ignores this part */
#if defined(__cplusplus) && defined(__has_cpp_attribute)
# define XXH_HAS_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#else
# define XXH_HAS_CPP_ATTRIBUTE(x) 0
#endif
/*! @endcond */

/*! @cond Doxygen ignores this part */
/*
 * Define XXH_FALLTHROUGH macro for annotating switch case with the 'fallthrough' attribute
 * introduced in CPP17 and C23.
 * CPP17 : https://en.cppreference.com/w/cpp/language/attributes/fallthrough
 * C23   : https://en.cppreference.com/w/c/language/attributes/fallthrough
 */
#if XXH_HAS_C_ATTRIBUTE(fallthrough) || XXH_HAS_CPP_ATTRIBUTE(fallthrough)
# define XXH_FALLTHROUGH [[fallthrough]]
#elif XXH_HAS_ATTRIBUTE(__fallthrough__)
# define XXH_FALLTHROUGH __attribute__ ((__fallthrough__))
#else
# define XXH_FALLTHROUGH /* fallthrough */
#endif
/*! @endcond */

/*! @cond Doxygen ignores this part */
/*
 * Define XXH_NOESCAPE for annotated pointers in public API.
 * https://clang.llvm.org/docs/AttributeReference.html#noescape
 * As of writing this, only supported by clang.
 */
#if XXH_HAS_ATTRIBUTE(noescape)
# define XXH_NOESCAPE __attribute__((__noescape__))
#else
# define XXH_NOESCAPE
#endif
/*! @endcond */


/*!
 * @}
 * @ingroup public
 * @{
 */

#ifndef XXH_NO_LONG_LONG
/*-**********************************************************************
*  64-bit hash
************************************************************************/
#if defined(XXH_DOXYGEN) /* don't include <stdint.h> */
/*!
 * @brief An unsigned 64-bit integer.
 *
 * Not necessarily defined to `uint64_t` but functionally equivalent.
 */
typedef uint64_t XXH64_hash_t;
#elif !defined (__VMS) \
  && (defined (__cplusplus) \
  || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */) )
#   ifdef _AIX
#     include <inttypes.h>
#   else
#     include <stdint.h>
#   endif
   typedef uint64_t XXH64_hash_t;
#else
#  include <limits.h>
#  if defined(__LP64__) && ULONG_MAX == 0xFFFFFFFFFFFFFFFFULL
     /* LP64 ABI says uint64_t is unsigned long */
     typedef unsigned long XXH64_hash_t;
#  else
     /* the following type must have a width of 64-bit */
     typedef unsigned long long XXH64_hash_t;
#  endif
#endif

/*!
 * @}
 *
 * @defgroup XXH64_family XXH64 family
 * @ingroup public
 * @{
 * Contains functions used in the classic 64-bit xxHash algorithm.
 *
 * @note
 *   XXH3 provides competitive speed for both 32-bit and 64-bit systems,
 *   and offers true 64/128 bit hash results.
 *   It provides better speed for systems with vector processing capabilities.
 */

/*!
 * @brief Calculates the 64-bit hash of @p input using xxHash64.
 *
 * @param input The block of data to be hashed, at least @p length bytes in size.
 * @param length The length of @p input, in bytes.
 * @param seed The 64-bit seed to alter the hash's output predictably.
 *
 * @pre
 *   The memory between @p input and @p input + @p length must be valid,
 *   readable, contiguous memory. However, if @p length is `0`, @p input may be
 *   `NULL`. In C++, this also must be *TriviallyCopyable*.
 *
 * @return The calculated 64-bit xxHash64 value.
 *
 * @see @ref single_shot_example "Single Shot Example" for an example.
 */
XXH_PUBLIC_API XXH_PUREF XXH64_hash_t XXH64(XXH_NOESCAPE const void* input, size_t length, XXH64_hash_t seed);

/*******   Streaming   *******/
#ifndef XXH_NO_STREAM
/*!
 * @brief The opaque state struct for the XXH64 streaming API.
 *
 * @see XXH64_state_s for details.
 * @see @ref streaming_example "Streaming Example"
 */
typedef struct XXH64_state_s XXH64_state_t;   /* incomplete type */

/*!
 * @brief Allocates an @ref XXH64_state_t.
 *
 * @return An allocated pointer of @ref XXH64_state_t on success.
 * @return `NULL` on failure.
 *
 * @note Must be freed with XXH64_freeState().
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_MALLOCF XXH64_state_t* XXH64_createState(void);

/*!
 * @brief Frees an @ref XXH64_state_t.
 *
 * @param statePtr A pointer to an @ref XXH64_state_t allocated with @ref XXH64_createState().
 *
 * @return @ref XXH_OK.
 *
 * @note @p statePtr must be allocated with XXH64_createState().
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_errorcode  XXH64_freeState(XXH64_state_t* statePtr);

/*!
 * @brief Copies one @ref XXH64_state_t to another.
 *
 * @param dst_state The state to copy to.
 * @param src_state The state to copy from.
 * @pre
 *   @p dst_state and @p src_state must not be `NULL` and must not overlap.
 */
XXH_PUBLIC_API void XXH64_copyState(XXH_NOESCAPE XXH64_state_t* dst_state, const XXH64_state_t* src_state);

/*!
 * @brief Resets an @ref XXH64_state_t to begin a new hash.
 *
 * @param statePtr The state struct to reset.
 * @param seed The 64-bit seed to alter the hash result predictably.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @note This function resets and seeds a state. Call it before @ref XXH64_update().
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_errorcode XXH64_reset  (XXH_NOESCAPE XXH64_state_t* statePtr, XXH64_hash_t seed);

/*!
 * @brief Consumes a block of @p input to an @ref XXH64_state_t.
 *
 * @param statePtr The state struct to update.
 * @param input The block of data to be hashed, at least @p length bytes in size.
 * @param length The length of @p input, in bytes.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 * @pre
 *   The memory between @p input and @p input + @p length must be valid,
 *   readable, contiguous memory. However, if @p length is `0`, @p input may be
 *   `NULL`. In C++, this also must be *TriviallyCopyable*.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @note Call this to incrementally consume blocks of data.
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_errorcode XXH64_update (XXH_NOESCAPE XXH64_state_t* statePtr, XXH_NOESCAPE const void* input, size_t length);

/*!
 * @brief Returns the calculated hash value from an @ref XXH64_state_t.
 *
 * @param statePtr The state struct to calculate the hash from.
 *
 * @pre
 *  @p statePtr must not be `NULL`.
 *
 * @return The calculated 64-bit xxHash64 value from that state.
 *
 * @note
 *   Calling XXH64_digest() will not affect @p statePtr, so you can update,
 *   digest, and update again.
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_PUREF XXH64_hash_t XXH64_digest (XXH_NOESCAPE const XXH64_state_t* statePtr);
#endif /* !XXH_NO_STREAM */
/*******   Canonical representation   *******/

/*!
 * @brief Canonical (big endian) representation of @ref XXH64_hash_t.
 */
typedef struct { unsigned char digest[sizeof(XXH64_hash_t)]; } XXH64_canonical_t;

/*!
 * @brief Converts an @ref XXH64_hash_t to a big endian @ref XXH64_canonical_t.
 *
 * @param dst The @ref XXH64_canonical_t pointer to be stored to.
 * @param hash The @ref XXH64_hash_t to be converted.
 *
 * @pre
 *   @p dst must not be `NULL`.
 *
 * @see @ref canonical_representation_example "Canonical Representation Example"
 */
XXH_PUBLIC_API void XXH64_canonicalFromHash(XXH_NOESCAPE XXH64_canonical_t* dst, XXH64_hash_t hash);

/*!
 * @brief Converts an @ref XXH64_canonical_t to a native @ref XXH64_hash_t.
 *
 * @param src The @ref XXH64_canonical_t to convert.
 *
 * @pre
 *   @p src must not be `NULL`.
 *
 * @return The converted hash.
 *
 * @see @ref canonical_representation_example "Canonical Representation Example"
 */
XXH_PUBLIC_API XXH_PUREF XXH64_hash_t XXH64_hashFromCanonical(XXH_NOESCAPE const XXH64_canonical_t* src);

#include "xxh3_api.h"
#endif  /* XXH_NO_LONG_LONG */

/*!
 * @}
 */
#endif /* XXHASH_H_5627135585666179 */



#if defined(XXH_STATIC_LINKING_ONLY) && !defined(XXHASH_H_STATIC_13879238742)
#define XXHASH_H_STATIC_13879238742
/* ****************************************************************************
 * This section contains declarations which are not guaranteed to remain stable.
 * They may change in future versions, becoming incompatible with a different
 * version of the library.
 * These declarations should only be used with static linking.
 * Never use them in association with dynamic linking!
 ***************************************************************************** */

/*
 * These definitions are only present to allow static allocation
 * of XXH states, on stack or in a struct, for example.
 * Never **ever** access their members directly.
 */

/*!
 * @internal
 * @brief Structure for XXH32 streaming API.
 *
 * @note This is only defined when @ref XXH_STATIC_LINKING_ONLY,
 * @ref XXH_INLINE_ALL, or @ref XXH_IMPLEMENTATION is defined. Otherwise it is
 * an opaque type. This allows fields to safely be changed.
 *
 * Typedef'd to @ref XXH32_state_t.
 * Do not access the members of this struct directly.
 * @see XXH64_state_s, XXH3_state_s
 */
struct XXH32_state_s {
   XXH32_hash_t total_len_32; /*!< Total length hashed, modulo 2^32 */
   XXH32_hash_t large_len;    /*!< Whether the hash is >= 16 (handles @ref total_len_32 overflow) */
   XXH32_hash_t acc[4];       /*!< Accumulator lanes */
   unsigned char buffer[16];  /*!< Internal buffer for partial reads. */
   XXH32_hash_t bufferedSize; /*!< Amount of data in @ref buffer */
   XXH32_hash_t reserved;     /*!< Reserved field. Do not read nor write to it. */
};   /* typedef'd to XXH32_state_t */


#ifndef XXH_NO_LONG_LONG  /* defined when there is no 64-bit support */

/*!
 * @internal
 * @brief Structure for XXH64 streaming API.
 *
 * @note This is only defined when @ref XXH_STATIC_LINKING_ONLY,
 * @ref XXH_INLINE_ALL, or @ref XXH_IMPLEMENTATION is defined. Otherwise it is
 * an opaque type. This allows fields to safely be changed.
 *
 * Typedef'd to @ref XXH64_state_t.
 * Do not access the members of this struct directly.
 * @see XXH32_state_s, XXH3_state_s
 */
struct XXH64_state_s {
   XXH64_hash_t total_len;    /*!< Total length hashed. This is always 64-bit. */
   XXH64_hash_t acc[4];       /*!< Accumulator lanes */
   unsigned char buffer[32];  /*!< Internal buffer for partial reads.. */
   XXH32_hash_t bufferedSize; /*!< Amount of data in @ref buffer */
   XXH32_hash_t reserved32;   /*!< Reserved field, needed for padding anyways*/
   XXH64_hash_t reserved64;   /*!< Reserved field. Do not read or write to it. */
};   /* typedef'd to XXH64_state_t */

#include "xxh3_static.h"
#endif  /* XXH_NO_LONG_LONG */
#if defined(XXH_INLINE_ALL) || defined(XXH_PRIVATE_API)
#  define XXH_IMPLEMENTATION
#endif

#endif  /* defined(XXH_STATIC_LINKING_ONLY) && !defined(XXHASH_H_STATIC_13879238742) */

#if defined (__cplusplus) && !defined(XXH_NO_EXTERNC_GUARD)
} /* extern "C" */
#endif


/* ======================================================================== */
/* ======================================================================== */
/* ======================================================================== */


/*-**********************************************************************
 * xxHash implementation
 *-**********************************************************************
 * xxHash's implementation used to be hosted inside xxhash.c.
 *
 * However, inlining requires implementation to be visible to the compiler,
 * hence be included alongside the header.
 * Previously, implementation was hosted inside xxhash.c,
 * which was then #included when inlining was activated.
 * This construction created issues with a few build and install systems,
 * as it required xxhash.c to be stored in /include directory.
 *
 * xxHash implementation is now directly integrated within xxhash.h.
 * As a consequence, xxhash.c is no longer needed in /include.
 *
 * xxhash.c is still available and is still useful.
 * In a "normal" setup, when xxhash is not inlined,
 * xxhash.h only exposes the prototypes and public symbols,
 * while xxhash.c can be built into an object file xxhash.o
 * which can then be linked into the final binary.
 ************************************************************************/

#if ( defined(XXH_INLINE_ALL) || defined(XXH_PRIVATE_API) \
   || defined(XXH_IMPLEMENTATION) ) && !defined(XXH_IMPLEM_13a8737387)
#  define XXH_IMPLEM_13a8737387

/* *************************************
*  Tuning parameters
***************************************/

/*!
 * @defgroup tuning Tuning parameters
 * @{
 *
 * Various macros to control xxHash's behavior.
 */
#ifdef XXH_DOXYGEN
/*!
 * @brief Define this to disable 64-bit code.
 *
 * Useful if only using the @ref XXH32_family and you have a strict C90 compiler.
 */
#  define XXH_NO_LONG_LONG
#  undef XXH_NO_LONG_LONG /* don't actually */
/*!
 * @brief Controls how unaligned memory is accessed.
 *
 * By default, access to unaligned memory is controlled by `memcpy()`, which is
 * safe and portable.
 *
 * Unfortunately, on some target/compiler combinations, the generated assembly
 * is sub-optimal.
 *
 * The below switch allow selection of a different access method
 * in the search for improved performance.
 *
 * @par Possible options:
 *
 *  - `XXH_FORCE_MEMORY_ACCESS=0` (default): `memcpy`
 *   @par
 *     Use `memcpy()`. Safe and portable. Note that most modern compilers will
 *     eliminate the function call and treat it as an unaligned access.
 *
 *  - `XXH_FORCE_MEMORY_ACCESS=1`: `__attribute__((aligned(1)))`
 *   @par
 *     Depends on compiler extensions and is therefore not portable.
 *     This method is safe _if_ your compiler supports it,
 *     and *generally* as fast or faster than `memcpy`.
 *
 *  - `XXH_FORCE_MEMORY_ACCESS=2`: Direct cast
 *  @par
 *     Casts directly and dereferences. This method doesn't depend on the
 *     compiler, but it violates the C standard as it directly dereferences an
 *     unaligned pointer. It can generate buggy code on targets which do not
 *     support unaligned memory accesses, but in some circumstances, it's the
 *     only known way to get the most performance.
 *
 *  - `XXH_FORCE_MEMORY_ACCESS=3`: Byteshift
 *  @par
 *     Also portable. This can generate the best code on old compilers which don't
 *     inline small `memcpy()` calls, and it might also be faster on big-endian
 *     systems which lack a native byteswap instruction. However, some compilers
 *     will emit literal byteshifts even if the target supports unaligned access.
 *
 *
 * @warning
 *   Methods 1 and 2 rely on implementation-defined behavior. Use these with
 *   care, as what works on one compiler/platform/optimization level may cause
 *   another to read garbage data or even crash.
 *
 * See https://fastcompression.blogspot.com/2015/08/accessing-unaligned-memory.html for details.
 *
 * Prefer these methods in priority order (0 > 3 > 1 > 2)
 */
#  define XXH_FORCE_MEMORY_ACCESS 0

/*!
 * @def XXH_SIZE_OPT
 * @brief Controls how much xxHash optimizes for size.
 *
 * xxHash, when compiled, tends to result in a rather large binary size. This
 * is mostly due to heavy usage to forced inlining and constant folding of the
 * @ref XXH3_family to increase performance.
 *
 * However, some developers prefer size over speed. This option can
 * significantly reduce the size of the generated code. When using the `-Os`
 * or `-Oz` options on GCC or Clang, this is defined to 1 by default,
 * otherwise it is defined to 0.
 *
 * Most of these size optimizations can be controlled manually.
 *
 * This is a number from 0-2.
 *  - `XXH_SIZE_OPT` == 0: Default. xxHash makes no size optimizations. Speed
 *    comes first.
 *  - `XXH_SIZE_OPT` == 1: Default for `-Os` and `-Oz`. xxHash is more
 *    conservative and disables hacks that increase code size. It implies the
 *    options @ref XXH_NO_INLINE_HINTS == 1, @ref XXH_FORCE_ALIGN_CHECK == 0,
 *    and @ref XXH3_NEON_LANES == 8 if they are not already defined.
 *  - `XXH_SIZE_OPT` == 2: xxHash tries to make itself as small as possible.
 *    Performance may cry. For example, the single shot functions just use the
 *    streaming API.
 */
#  define XXH_SIZE_OPT 0

/*!
 * @def XXH_FORCE_ALIGN_CHECK
 * @brief If defined to non-zero, adds a special path for aligned inputs (XXH32()
 * and XXH64() only).
 *
 * This is an important performance trick for architectures without decent
 * unaligned memory access performance.
 *
 * It checks for input alignment, and when conditions are met, uses a "fast
 * path" employing direct 32-bit/64-bit reads, resulting in _dramatically
 * faster_ read speed.
 *
 * The check costs one initial branch per hash, which is generally negligible,
 * but not zero.
 *
 * Moreover, it's not useful to generate an additional code path if memory
 * access uses the same instruction for both aligned and unaligned
 * addresses (e.g. x86 and aarch64).
 *
 * In these cases, the alignment check can be removed by setting this macro to 0.
 * Then the code will always use unaligned memory access.
 * Align check is automatically disabled on x86, x64, ARM64, and some ARM chips
 * which are platforms known to offer good unaligned memory accesses performance.
 *
 * It is also disabled by default when @ref XXH_SIZE_OPT >= 1.
 *
 * This option does not affect XXH3 (only XXH32 and XXH64).
 */
#  define XXH_FORCE_ALIGN_CHECK 0

/*!
 * @def XXH_NO_INLINE_HINTS
 * @brief When non-zero, sets all functions to `static`.
 *
 * By default, xxHash tries to force the compiler to inline almost all internal
 * functions.
 *
 * This can usually improve performance due to reduced jumping and improved
 * constant folding, but significantly increases the size of the binary which
 * might not be favorable.
 *
 * Additionally, sometimes the forced inlining can be detrimental to performance,
 * depending on the architecture.
 *
 * XXH_NO_INLINE_HINTS marks all internal functions as static, giving the
 * compiler full control on whether to inline or not.
 *
 * When not optimizing (-O0), using `-fno-inline` with GCC or Clang, or if
 * @ref XXH_SIZE_OPT >= 1, this will automatically be defined.
 */
#  define XXH_NO_INLINE_HINTS 0

/*!
 * @def XXH3_INLINE_SECRET
 * @brief Determines whether to inline the XXH3 withSecret code.
 *
 * When the secret size is known, the compiler can improve the performance
 * of XXH3_64bits_withSecret() and XXH3_128bits_withSecret().
 *
 * However, if the secret size is not known, it doesn't have any benefit. This
 * happens when xxHash is compiled into a global symbol. Therefore, if
 * @ref XXH_INLINE_ALL is *not* defined, this will be defined to 0.
 *
 * Additionally, this defaults to 0 on GCC 12+, which has an issue with function pointers
 * that are *sometimes* force inline on -Og, and it is impossible to automatically
 * detect this optimization level.
 */
#  define XXH3_INLINE_SECRET 0

/*!
 * @def XXH32_ENDJMP
 * @brief Whether to use a jump for `XXH32_finalize`.
 *
 * For performance, `XXH32_finalize` uses multiple branches in the finalizer.
 * This is generally preferable for performance,
 * but depending on exact architecture, a jmp may be preferable.
 *
 * This setting is only possibly making a difference for very small inputs.
 */
#  define XXH32_ENDJMP 0

/*!
 * @internal
 * @brief Redefines old internal names.
 *
 * For compatibility with code that uses xxHash's internals before the names
 * were changed to improve namespacing. There is no other reason to use this.
 */
#  define XXH_OLD_NAMES
#  undef XXH_OLD_NAMES /* don't actually use, it is ugly. */

/*!
 * @def XXH_NO_STREAM
 * @brief Disables the streaming API.
 *
 * When xxHash is not inlined and the streaming functions are not used, disabling
 * the streaming functions can improve code size significantly, especially with
 * the @ref XXH3_family which tends to make constant folded copies of itself.
 */
#  define XXH_NO_STREAM
#  undef XXH_NO_STREAM /* don't actually */
#endif /* XXH_DOXYGEN */
/*!
 * @}
 */

#ifndef XXH_FORCE_MEMORY_ACCESS   /* can be defined externally, on command line for example */
   /* prefer __packed__ structures (method 1) for GCC
    * < ARMv7 with unaligned access (e.g. Raspbian armhf) still uses byte shifting, so we use memcpy
    * which for some reason does unaligned loads. */
#  if defined(__GNUC__) && !(defined(__ARM_ARCH) && __ARM_ARCH < 7 && defined(__ARM_FEATURE_UNALIGNED))
#    define XXH_FORCE_MEMORY_ACCESS 1
#  endif
#endif

#ifndef XXH_SIZE_OPT
   /* default to 1 for -Os or -Oz */
#  if (defined(__GNUC__) || defined(__clang__)) && defined(__OPTIMIZE_SIZE__)
#    define XXH_SIZE_OPT 1
#  else
#    define XXH_SIZE_OPT 0
#  endif
#endif

#ifndef XXH_FORCE_ALIGN_CHECK  /* can be defined externally */
   /* don't check on sizeopt, x86, aarch64, or arm when unaligned access is available */
#  if XXH_SIZE_OPT >= 1 || \
      defined(__i386)  || defined(__x86_64__) || defined(__aarch64__) || defined(__ARM_FEATURE_UNALIGNED) \
   || defined(_M_IX86) || defined(_M_X64)     || defined(_M_ARM64)    || defined(_M_ARM) /* visual */
#    define XXH_FORCE_ALIGN_CHECK 0
#  else
#    define XXH_FORCE_ALIGN_CHECK 1
#  endif
#endif

#ifndef XXH_NO_INLINE_HINTS
#  if XXH_SIZE_OPT >= 1 || defined(__NO_INLINE__)  /* -O0, -fno-inline */
#    define XXH_NO_INLINE_HINTS 1
#  else
#    define XXH_NO_INLINE_HINTS 0
#  endif
#endif

#ifndef XXH3_INLINE_SECRET
#  if (defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 12) \
     || !defined(XXH_INLINE_ALL)
#    define XXH3_INLINE_SECRET 0
#  else
#    define XXH3_INLINE_SECRET 1
#  endif
#endif

#ifndef XXH32_ENDJMP
/* generally preferable for performance */
#  define XXH32_ENDJMP 0
#endif

/*!
 * @defgroup impl Implementation
 * @{
 */


/* *************************************
*  Includes & Memory related functions
***************************************/
#if defined(XXH_NO_STREAM)
/* nothing */
#elif defined(XXH_NO_STDLIB)

/* When requesting to disable any mention of stdlib,
 * the library loses the ability to invoked malloc / free.
 * In practice, it means that functions like `XXH*_createState()`
 * will always fail, and return NULL.
 * This flag is useful in situations where
 * xxhash.h is integrated into some kernel, embedded or limited environment
 * without access to dynamic allocation.
 */

static XXH_CONSTF void* XXH_malloc(size_t s) { (void)s; return NULL; }
static void XXH_free(void* p) { (void)p; }

#else

/*
 * Modify the local functions below should you wish to use
 * different memory routines for malloc() and free()
 */
#include <stdlib.h>

/*!
 * @internal
 * @brief Modify this function to use a different routine than malloc().
 */
static XXH_MALLOCF void* XXH_malloc(size_t s) { return malloc(s); }

/*!
 * @internal
 * @brief Modify this function to use a different routine than free().
 */
static void XXH_free(void* p) { free(p); }

#endif  /* XXH_NO_STDLIB */

#ifndef XXH_memcpy
/*!
 * @internal
 * @brief XXH_memcpy() macro can be redirected at compile time
 */
#  include <string.h>
#  define XXH_memcpy memcpy
#endif

#ifndef XXH_memset
/*!
 * @internal
 * @brief XXH_memset() macro can be redirected at compile time
 */
#  include <string.h>
#  define XXH_memset memset
#endif

#ifndef XXH_memcmp
/*!
 * @internal
 * @brief XXH_memcmp() macro can be redirected at compile time
 * Note: only needed by XXH128.
 */
#  include <string.h>
#  define XXH_memcmp memcmp
#endif



#include <limits.h>   /* ULLONG_MAX */


/* *************************************
*  Compiler Specific Options
***************************************/
#ifdef _MSC_VER /* Visual Studio warning fix */
#  pragma warning(disable : 4127) /* disable: C4127: conditional expression is constant */
#endif

#if XXH_NO_INLINE_HINTS  /* disable inlining hints */
#  if defined(__GNUC__) || defined(__clang__)
#    define XXH_FORCE_INLINE static __attribute__((__unused__))
#  else
#    define XXH_FORCE_INLINE static
#  endif
#  define XXH_NO_INLINE static
/* enable inlining hints */
#elif defined(__GNUC__) || defined(__clang__)
#  define XXH_FORCE_INLINE static __inline__ __attribute__((__always_inline__, __unused__))
#  define XXH_NO_INLINE static __attribute__((__noinline__))
#elif defined(_MSC_VER)  /* Visual Studio */
#  define XXH_FORCE_INLINE static __forceinline
#  define XXH_NO_INLINE static __declspec(noinline)
#elif defined (__cplusplus) \
  || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L))   /* C99 */
#  define XXH_FORCE_INLINE static inline
#  define XXH_NO_INLINE static
#else
#  define XXH_FORCE_INLINE static
#  define XXH_NO_INLINE static
#endif

#if defined(XXH_INLINE_ALL)
#  define XXH_STATIC XXH_FORCE_INLINE
#else
#  define XXH_STATIC static
#endif

#if XXH3_INLINE_SECRET
#  define XXH3_WITH_SECRET_INLINE XXH_FORCE_INLINE
#else
#  define XXH3_WITH_SECRET_INLINE XXH_NO_INLINE
#endif

/* Solaris includes __STDC_VERSION__ with C++. Tested with GCC 5.5 */
#if ((defined(sun) || defined(__sun)) && defined(__cplusplus))
#  define XXH_RESTRICT   /* disable */
#elif defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L   /* >= C99 */
#  define XXH_RESTRICT   restrict
#elif (defined (__GNUC__) && ((__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))) \
   || (defined (__clang__)) \
   || (defined (_MSC_VER) && (_MSC_VER >= 1400)) \
   || (defined (__INTEL_COMPILER) && (__INTEL_COMPILER >= 1300))
/*
 * There are a LOT more compilers that recognize __restrict but this
 * covers the major ones.
 */
#  define XXH_RESTRICT   __restrict
#else
#  define XXH_RESTRICT   /* disable */
#endif

/* *************************************
*  Debug
***************************************/
/*!
 * @ingroup tuning
 * @def XXH_DEBUGLEVEL
 * @brief Sets the debugging level.
 *
 * XXH_DEBUGLEVEL is expected to be defined externally, typically via the
 * compiler's command line options. The value must be a number.
 */
#ifndef XXH_DEBUGLEVEL
#  ifdef DEBUGLEVEL /* backwards compat */
#    define XXH_DEBUGLEVEL DEBUGLEVEL
#  else
#    define XXH_DEBUGLEVEL 0
#  endif
#endif

#if (XXH_DEBUGLEVEL>=1)
#  include <assert.h>   /* note: can still be disabled with NDEBUG */
#  define XXH_ASSERT(c)   assert(c)
#else
#  if defined(__INTEL_COMPILER)
#    define XXH_ASSERT(c)   XXH_ASSUME((unsigned char) (c))
#  else
#    define XXH_ASSERT(c)   XXH_ASSUME(c)
#  endif
#endif

/* note: use after variable declarations */
#ifndef XXH_STATIC_ASSERT
#  if defined(__cplusplus) && (__cplusplus >= 201103L)            /* C++11 */
#    define XXH_STATIC_ASSERT_WITH_MESSAGE(c,m) do { static_assert((c),m); } while(0)
#  elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)    /* C11 */
#    define XXH_STATIC_ASSERT_WITH_MESSAGE(c,m) do { _Static_assert((c),m); } while(0)
#  else
#    define XXH_STATIC_ASSERT_WITH_MESSAGE(c,m) do { struct xxh_sa { char x[(c) ? 1 : -1]; }; } while(0)
#  endif
#  define XXH_STATIC_ASSERT(c) XXH_STATIC_ASSERT_WITH_MESSAGE((c),#c)
#endif

/*!
 * @internal
 * @def XXH_COMPILER_GUARD(var)
 * @brief Used to prevent unwanted optimizations for @p var.
 *
 * It uses an empty GCC inline assembly statement with a register constraint
 * which forces @p var into a general purpose register (eg eax, ebx, ecx
 * on x86) and marks it as modified.
 *
 * This is used in a few places to avoid unwanted autovectorization (e.g.
 * XXH32_round()). All vectorization we want is explicit via intrinsics,
 * and _usually_ isn't wanted elsewhere.
 *
 * We also use it to prevent unwanted constant folding for AArch64 in
 * XXH3_initCustomSecret_scalar().
 */
#if defined(__GNUC__) || defined(__clang__)
#  define XXH_COMPILER_GUARD(var) __asm__("" : "+r" (var))
#else
#  define XXH_COMPILER_GUARD(var) ((void)0)
#endif

/* Specifically for NEON vectors which use the "w" constraint, on
 * Clang. */
#if defined(__clang__) && defined(__ARM_ARCH) && !defined(__wasm__)
#  define XXH_COMPILER_GUARD_CLANG_NEON(var) __asm__("" : "+w" (var))
#else
#  define XXH_COMPILER_GUARD_CLANG_NEON(var) ((void)0)
#endif

/* *************************************
*  Basic Types
***************************************/
#if !defined (__VMS) \
 && (defined (__cplusplus) \
 || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */) )
#   ifdef _AIX
#     include <inttypes.h>
#   else
#     include <stdint.h>
#   endif
    typedef uint8_t xxh_u8;
#else
    typedef unsigned char xxh_u8;
#endif
typedef XXH32_hash_t xxh_u32;

#ifdef XXH_OLD_NAMES
#  warning "XXH_OLD_NAMES is planned to be removed starting v0.9. If the program depends on it, consider moving away from it by employing newer type names directly"
#  define BYTE xxh_u8
#  define U8   xxh_u8
#  define U32  xxh_u32
#endif

/* ***   Memory access   *** */

/*!
 * @internal
 * @fn xxh_u32 XXH_read32(const void* ptr)
 * @brief Reads an unaligned 32-bit integer from @p ptr in native endianness.
 *
 * Affected by @ref XXH_FORCE_MEMORY_ACCESS.
 *
 * @param ptr The pointer to read from.
 * @return The 32-bit native endian integer from the bytes at @p ptr.
 */

/*!
 * @internal
 * @fn xxh_u32 XXH_readLE32(const void* ptr)
 * @brief Reads an unaligned 32-bit little endian integer from @p ptr.
 *
 * Affected by @ref XXH_FORCE_MEMORY_ACCESS.
 *
 * @param ptr The pointer to read from.
 * @return The 32-bit little endian integer from the bytes at @p ptr.
 */

/*!
 * @internal
 * @fn xxh_u32 XXH_readBE32(const void* ptr)
 * @brief Reads an unaligned 32-bit big endian integer from @p ptr.
 *
 * Affected by @ref XXH_FORCE_MEMORY_ACCESS.
 *
 * @param ptr The pointer to read from.
 * @return The 32-bit big endian integer from the bytes at @p ptr.
 */

/*!
 * @internal
 * @fn xxh_u32 XXH_readLE32_align(const void* ptr, XXH_alignment align)
 * @brief Like @ref XXH_readLE32(), but has an option for aligned reads.
 *
 * Affected by @ref XXH_FORCE_MEMORY_ACCESS.
 * Note that when @ref XXH_FORCE_ALIGN_CHECK == 0, the @p align parameter is
 * always @ref XXH_alignment::XXH_unaligned.
 *
 * @param ptr The pointer to read from.
 * @param align Whether @p ptr is aligned.
 * @pre
 *   If @p align == @ref XXH_alignment::XXH_aligned, @p ptr must be 4 byte
 *   aligned.
 * @return The 32-bit little endian integer from the bytes at @p ptr.
 */

#if (defined(XXH_FORCE_MEMORY_ACCESS) && (XXH_FORCE_MEMORY_ACCESS==3))
/*
 * Manual byteshift. Best for old compilers which don't inline memcpy.
 * We actually directly use XXH_readLE32 and XXH_readBE32.
 */
#elif (defined(XXH_FORCE_MEMORY_ACCESS) && (XXH_FORCE_MEMORY_ACCESS==2))

/*
 * Force direct memory access. Only works on CPU which support unaligned memory
 * access in hardware.
 */
static xxh_u32 XXH_read32(const void* memPtr) { return *(const xxh_u32*) memPtr; }

#elif (defined(XXH_FORCE_MEMORY_ACCESS) && (XXH_FORCE_MEMORY_ACCESS==1))

/*
 * __attribute__((aligned(1))) is supported by gcc and clang. Originally the
 * documentation claimed that it only increased the alignment, but actually it
 * can decrease it on gcc, clang, and icc:
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=69502,
 * https://gcc.godbolt.org/z/xYez1j67Y.
 */
#ifdef XXH_OLD_NAMES
typedef union { xxh_u32 u32; } __attribute__((__packed__)) unalign;
#endif
static xxh_u32 XXH_read32(const void* ptr)
{
    typedef __attribute__((__aligned__(1))) __attribute__((__may_alias__)) xxh_u32 xxh_unalign32;
    return *((const xxh_unalign32*)ptr);
}

#else

/*
 * Portable and safe solution. Generally efficient.
 * see: https://fastcompression.blogspot.com/2015/08/accessing-unaligned-memory.html
 */
static xxh_u32 XXH_read32(const void* memPtr)
{
    xxh_u32 val;
    XXH_memcpy(&val, memPtr, sizeof(val));
    return val;
}

#endif   /* XXH_FORCE_DIRECT_MEMORY_ACCESS */


/* ***   Endianness   *** */

/*!
 * @ingroup tuning
 * @def XXH_CPU_LITTLE_ENDIAN
 * @brief Whether the target is little endian.
 *
 * Defined to 1 if the target is little endian, or 0 if it is big endian.
 * It can be defined externally, for example on the compiler command line.
 *
 * If it is not defined,
 * a runtime check (which is usually constant folded) is used instead.
 *
 * @note
 *   This is not necessarily defined to an integer constant.
 *
 * @see XXH_isLittleEndian() for the runtime check.
 */
#ifndef XXH_CPU_LITTLE_ENDIAN
/*
 * Try to detect endianness automatically, to avoid the nonstandard behavior
 * in `XXH_isLittleEndian()`
 */
#  if defined(_WIN32) /* Windows is always little endian */ \
     || defined(__LITTLE_ENDIAN__) \
     || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#    define XXH_CPU_LITTLE_ENDIAN 1
#  elif defined(__BIG_ENDIAN__) \
     || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#    define XXH_CPU_LITTLE_ENDIAN 0
#  else
/*!
 * @internal
 * @brief Runtime check for @ref XXH_CPU_LITTLE_ENDIAN.
 *
 * Most compilers will constant fold this.
 */
static int XXH_isLittleEndian(void)
{
    /*
     * Portable and well-defined behavior.
     * Don't use static: it is detrimental to performance.
     */
    const union { xxh_u32 u; xxh_u8 c[4]; } one = { 1 };
    return one.c[0];
}
#   define XXH_CPU_LITTLE_ENDIAN   XXH_isLittleEndian()
#  endif
#endif




/* ****************************************
*  Compiler-specific Functions and Macros
******************************************/
#define XXH_GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)

#ifdef __has_builtin
#  define XXH_HAS_BUILTIN(x) __has_builtin(x)
#else
#  define XXH_HAS_BUILTIN(x) 0
#endif



/*
 * C23 and future versions have standard "unreachable()".
 * Once it has been implemented reliably we can add it as an
 * additional case:
 *
 * ```
 * #if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 202311L)
 * #  include <stddef.h>
 * #  ifdef unreachable
 * #    define XXH_UNREACHABLE() unreachable()
 * #  endif
 * #endif
 * ```
 *
 * Note C++23 also has std::unreachable() which can be detected
 * as follows:
 * ```
 * #if defined(__cpp_lib_unreachable) && (__cpp_lib_unreachable >= 202202L)
 * #  include <utility>
 * #  define XXH_UNREACHABLE() std::unreachable()
 * #endif
 * ```
 * NB: `__cpp_lib_unreachable` is defined in the `<version>` header.
 * We don't use that as including `<utility>` in `extern "C"` blocks
 * doesn't work on GCC12
 */

#if XXH_HAS_BUILTIN(__builtin_unreachable)
#  define XXH_UNREACHABLE() __builtin_unreachable()

#elif defined(_MSC_VER)
#  define XXH_UNREACHABLE() __assume(0)

#else
#  define XXH_UNREACHABLE()
#endif

#if XXH_HAS_BUILTIN(__builtin_assume)
#  define XXH_ASSUME(c) __builtin_assume(c)
#else
#  define XXH_ASSUME(c) if (!(c)) { XXH_UNREACHABLE(); }
#endif

#if defined(_MSC_VER)
   /* declares the intrinsics _rotl(), _rotl64(), _byteswap_ulong() and _byteswap_uint64(),
    * also needed with XXH_NO_STREAM or XXH_NO_STDLIB */
#  include <stdlib.h>
#endif

/*!
 * @internal
 * @def XXH_rotl32(x,r)
 * @brief 32-bit rotate left.
 *
 * @param x The 32-bit integer to be rotated.
 * @param r The number of bits to rotate.
 * @pre
 *   @p r > 0 && @p r < 32
 * @note
 *   @p x and @p r may be evaluated multiple times.
 * @return The rotated result.
 */
#if !defined(NO_CLANG_BUILTIN) && XXH_HAS_BUILTIN(__builtin_rotateleft32) \
                               && XXH_HAS_BUILTIN(__builtin_rotateleft64)
#  define XXH_rotl32 __builtin_rotateleft32
#  define XXH_rotl64 __builtin_rotateleft64
#elif XXH_HAS_BUILTIN(__builtin_stdc_rotate_left)
#  define XXH_rotl32 __builtin_stdc_rotate_left
#  define XXH_rotl64 __builtin_stdc_rotate_left
/* Note: although _rotl exists for minGW (GCC under windows), performance seems poor */
#elif defined(_MSC_VER)
#  define XXH_rotl32(x,r) _rotl(x,r)
#  define XXH_rotl64(x,r) _rotl64(x,r)
#else
#  define XXH_rotl32(x,r) (((x) << (r)) | ((x) >> (32 - (r))))
#  define XXH_rotl64(x,r) (((x) << (r)) | ((x) >> (64 - (r))))
#endif

/*!
 * @internal
 * @fn xxh_u32 XXH_swap32(xxh_u32 x)
 * @brief A 32-bit byteswap.
 *
 * @param x The 32-bit integer to byteswap.
 * @return @p x, byteswapped.
 */
#if defined(_MSC_VER)     /* Visual Studio */
#  define XXH_swap32 _byteswap_ulong
#elif XXH_GCC_VERSION >= 403
#  define XXH_swap32 __builtin_bswap32
#else
static xxh_u32 XXH_swap32 (xxh_u32 x)
{
    return  ((x << 24) & 0xff000000 ) |
            ((x <<  8) & 0x00ff0000 ) |
            ((x >>  8) & 0x0000ff00 ) |
            ((x >> 24) & 0x000000ff );
}
#endif


/* ***************************
*  Memory reads
*****************************/

/*!
 * @internal
 * @brief Enum to indicate whether a pointer is aligned.
 */
typedef enum {
    XXH_aligned,  /*!< Aligned */
    XXH_unaligned /*!< Possibly unaligned */
} XXH_alignment;

/*
 * XXH_FORCE_MEMORY_ACCESS==3 is an endian-independent byteshift load.
 *
 * This is ideal for older compilers which don't inline memcpy.
 */
#if (defined(XXH_FORCE_MEMORY_ACCESS) && (XXH_FORCE_MEMORY_ACCESS==3))

XXH_FORCE_INLINE xxh_u32 XXH_readLE32(const void* memPtr)
{
    const xxh_u8* bytePtr = (const xxh_u8 *)memPtr;
    return bytePtr[0]
         | ((xxh_u32)bytePtr[1] << 8)
         | ((xxh_u32)bytePtr[2] << 16)
         | ((xxh_u32)bytePtr[3] << 24);
}

XXH_FORCE_INLINE xxh_u32 XXH_readBE32(const void* memPtr)
{
    const xxh_u8* bytePtr = (const xxh_u8 *)memPtr;
    return bytePtr[3]
         | ((xxh_u32)bytePtr[2] << 8)
         | ((xxh_u32)bytePtr[1] << 16)
         | ((xxh_u32)bytePtr[0] << 24);
}

#else
XXH_FORCE_INLINE xxh_u32 XXH_readLE32(const void* ptr)
{
    return XXH_CPU_LITTLE_ENDIAN ? XXH_read32(ptr) : XXH_swap32(XXH_read32(ptr));
}

static xxh_u32 XXH_readBE32(const void* ptr)
{
    return XXH_CPU_LITTLE_ENDIAN ? XXH_swap32(XXH_read32(ptr)) : XXH_read32(ptr);
}
#endif

XXH_FORCE_INLINE xxh_u32
XXH_readLE32_align(const void* ptr, XXH_alignment align)
{
    if (align==XXH_unaligned) {
        return XXH_readLE32(ptr);
    } else {
        return XXH_CPU_LITTLE_ENDIAN ? *(const xxh_u32*)ptr : XXH_swap32(*(const xxh_u32*)ptr);
    }
}


/* *************************************
*  Misc
***************************************/
/*! @ingroup public */
XXH_PUBLIC_API unsigned XXH_versionNumber (void) { return XXH_VERSION_NUMBER; }


/* *******************************************************************
*  32-bit hash functions
*********************************************************************/
/*!
 * @}
 * @defgroup XXH32_impl XXH32 implementation
 * @ingroup impl
 *
 * Details on the XXH32 implementation.
 * @{
 */
 /* #define instead of static const, to be used as initializers */
#define XXH_PRIME32_1  0x9E3779B1U  /*!< 0b10011110001101110111100110110001 */
#define XXH_PRIME32_2  0x85EBCA77U  /*!< 0b10000101111010111100101001110111 */
#define XXH_PRIME32_3  0xC2B2AE3DU  /*!< 0b11000010101100101010111000111101 */
#define XXH_PRIME32_4  0x27D4EB2FU  /*!< 0b00100111110101001110101100101111 */
#define XXH_PRIME32_5  0x165667B1U  /*!< 0b00010110010101100110011110110001 */

#ifdef XXH_OLD_NAMES
#  define PRIME32_1 XXH_PRIME32_1
#  define PRIME32_2 XXH_PRIME32_2
#  define PRIME32_3 XXH_PRIME32_3
#  define PRIME32_4 XXH_PRIME32_4
#  define PRIME32_5 XXH_PRIME32_5
#endif

/*!
 * @internal
 * @brief Whether XXH32 code uses compiler fences against autovectorization.
 *
 * Autovectorization is detrimental to XXH32 on these targets,
 * see XXH32_round() and XXH32_mergeAccs().
 */
#if (defined(__SSE2__) || defined(__aarch64__) || defined(__wasm_simd128__)) && !defined(XXH_ENABLE_AUTOVECTORIZE)
#  define XXH32_PREVENT_AUTOVEC 1
#else
#  define XXH32_PREVENT_AUTOVEC 0
#endif

/*!
 * @internal
 * @brief Normal stripe processing routine.
 *
 * This shuffles the bits so that any bit from @p input impacts several bits in
 * @p acc.
 *
 * @param acc The accumulator lane.
 * @param input The stripe of input to mix.
 * @return The mixed accumulator lane.
 */
static xxh_u32 XXH32_round(xxh_u32 acc, xxh_u32 input)
{
    acc += input * XXH_PRIME32_2;
    acc  = XXH_rotl32(acc, 13);
    acc *= XXH_PRIME32_1;
#if XXH32_PREVENT_AUTOVEC
    /*
     * UGLY HACK:
     * A compiler fence is used to prevent GCC and Clang from
     * autovectorizing the XXH32 loop (pragmas and attributes don't work for some
     * reason) without globally disabling SSE4.1.
     *
     * The reason we want to avoid vectorization is because despite working on
     * 4 integers at a time, there are multiple factors slowing XXH32 down on
     * SSE4:
     * - There's a ridiculous amount of lag from pmulld (10 cycles of latency on
     *   newer chips!) making it slightly slower to multiply four integers at
     *   once compared to four integers independently. Even when pmulld was
     *   fastest, Sandy/Ivy Bridge, it is still not worth it to go into SSE
     *   just to multiply unless doing a long operation.
     *
     * - Four instructions are required to rotate,
     *      movqda tmp,  v // not required with VEX encoding
     *      pslld  tmp, 13 // tmp <<= 13
     *      psrld  v,   19 // x >>= 19
     *      por    v,  tmp // x |= tmp
     *   compared to one for scalar:
     *      roll   v, 13    // reliably fast across the board
     *      shldl  v, v, 13 // Sandy Bridge and later prefer this for some reason
     *
     * - Instruction level parallelism is actually more beneficial here because
     *   the SIMD actually serializes this operation: While v1 is rotating, v2
     *   can load data, while v3 can multiply. SSE forces them to operate
     *   together.
     *
     * This is also enabled on baseline SSE2 (no SSE4.1), where it matters even
     * more: without pmulld, each 32-bit vector multiply is emulated with
     * pmuludq + shuffles, or with long shift/add chains for constants.
     * GCC 16 does this on XXH32_update(), whose accumulators live in memory
     * (state->acc), making XXH32 streaming ~6x slower than single-shot.
     *
     * This is also enabled on AArch64, as Clang is *very aggressive* in vectorizing
     * the loop. NEON is only faster on the A53, and with the newer cores, it is less
     * than half the speed.
     *
     * Additionally, this is used on WASM SIMD128 because it JITs to the same
     * SIMD instructions and has the same issue.
     */
    XXH_COMPILER_GUARD(acc);
#endif
    return acc;
}

/*!
 * @internal
 * @brief Mixes all bits to finalize the hash.
 *
 * The final mix ensures that all input bits have a chance to impact any bit in
 * the output digest, resulting in an unbiased distribution.
 *
 * @param hash The hash to avalanche.
 * @return The avalanched hash.
 */
static xxh_u32 XXH32_avalanche(xxh_u32 hash)
{
    hash ^= hash >> 15;
    hash *= XXH_PRIME32_2;
    hash ^= hash >> 13;
    hash *= XXH_PRIME32_3;
    hash ^= hash >> 16;
    return hash;
}

#define XXH_get32bits(p) XXH_readLE32_align(p, align)

/*!
 * @internal
 * @brief Sets up the initial accumulator state for XXH32().
 */
XXH_FORCE_INLINE void
XXH32_initAccs(xxh_u32 *acc, xxh_u32 seed)
{
    XXH_ASSERT(acc != NULL);
    acc[0] = seed + XXH_PRIME32_1 + XXH_PRIME32_2;
    acc[1] = seed + XXH_PRIME32_2;
    acc[2] = seed + 0;
    acc[3] = seed - XXH_PRIME32_1;
}

/*!
 * @internal
 * @brief Consumes a block of data for XXH32().
 *
 * @return the end input pointer.
 */
XXH_FORCE_INLINE const xxh_u8 *
XXH32_consumeLong(
    xxh_u32 *XXH_RESTRICT acc,
    xxh_u8 const *XXH_RESTRICT input,
    size_t len,
    XXH_alignment align
)
{
    const xxh_u8* const bEnd = input + len;
    const xxh_u8* const limit = bEnd - 15;
    XXH_ASSERT(acc != NULL);
    XXH_ASSERT(input != NULL);
    XXH_ASSERT(len >= 16);
    /*
     * Accumulators are copied into locals, so that they stay in registers
     * for the whole loop. When the loop works directly on @p acc,
     * which is state->acc in streaming mode, Clang stores them back
     * to memory on every iteration (XXH_COMPILER_GUARD prevents promotion),
     * costing ~10% on XXH32_update() and XXH64_update().
     */
    {   xxh_u32 a0 = acc[0], a1 = acc[1], a2 = acc[2], a3 = acc[3];
        do {
            a0 = XXH32_round(a0, XXH_get32bits(input)); input += 4;
            a1 = XXH32_round(a1, XXH_get32bits(input)); input += 4;
            a2 = XXH32_round(a2, XXH_get32bits(input)); input += 4;
            a3 = XXH32_round(a3, XXH_get32bits(input)); input += 4;
        } while (input < limit);
        acc[0] = a0; acc[1] = a1; acc[2] = a2; acc[3] = a3;
    }

    return input;
}

/*!
 * @internal
 * @brief Merges the accumulator lanes together for XXH32()
 */
XXH_FORCE_INLINE XXH_PUREF xxh_u32
XXH32_mergeAccs(const xxh_u32 *acc)
{
    XXH_ASSERT(acc != NULL);
    {   xxh_u32 a0 = acc[0], a1 = acc[1], a2 = acc[2], a3 = acc[3];
#if XXH32_PREVENT_AUTOVEC
        /*
         * Prevent vectorization of this merge (GCC and Clang do it with AVX2).
         * In XXH32_digest(), it results in a single 16-byte load of state->acc,
         * which was just written by 4-byte stores: store-to-load forwarding fails,
         * making XXH32 streaming of small inputs up to ~2x slower.
         */
        XXH_COMPILER_GUARD(a0); XXH_COMPILER_GUARD(a1);
        XXH_COMPILER_GUARD(a2); XXH_COMPILER_GUARD(a3);
#endif
        return XXH_rotl32(a0, 1)  + XXH_rotl32(a1, 7)
             + XXH_rotl32(a2, 12) + XXH_rotl32(a3, 18);
    }
}

/*!
 * @internal
 * @brief Processes the last 0-15 bytes of @p ptr.
 *
 * There may be up to 15 bytes remaining to consume from the input.
 * This final stage will digest them to ensure that all input bytes are present
 * in the final mix.
 *
 * @param hash The hash to finalize.
 * @param ptr The pointer to the remaining input.
 * @param len The remaining length, modulo 16.
 * @param align Whether @p ptr is aligned.
 * @return The finalized hash.
 * @see XXH64_finalize().
 */
static XXH_PUREF xxh_u32
XXH32_finalize(xxh_u32 hash, const xxh_u8* ptr, size_t len, XXH_alignment align)
{
#define XXH_PROCESS1 do {                             \
    hash += (*ptr++) * XXH_PRIME32_5;                 \
    hash = XXH_rotl32(hash, 11) * XXH_PRIME32_1;      \
} while (0)

#define XXH_PROCESS4 do {                             \
    hash += XXH_get32bits(ptr) * XXH_PRIME32_3;       \
    ptr += 4;                                         \
    hash  = XXH_rotl32(hash, 17) * XXH_PRIME32_4;     \
} while (0)

    if (ptr==NULL) XXH_ASSERT(len == 0);

    /* Compact rerolled version; generally faster */
    if (!XXH32_ENDJMP) {
        len &= 15;
        while (len >= 4) {
            XXH_PROCESS4;
            len -= 4;
        }
        while (len > 0) {
            XXH_PROCESS1;
            --len;
        }
        return XXH32_avalanche(hash);
    } else {
         switch(len&15) /* or switch(bEnd - p) */ {
           case 12:      XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 8:       XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 4:       XXH_PROCESS4;
                         return XXH32_avalanche(hash);

           case 13:      XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 9:       XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 5:       XXH_PROCESS4;
                         XXH_PROCESS1;
                         return XXH32_avalanche(hash);

           case 14:      XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 10:      XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 6:       XXH_PROCESS4;
                         XXH_PROCESS1;
                         XXH_PROCESS1;
                         return XXH32_avalanche(hash);

           case 15:      XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 11:      XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 7:       XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 3:       XXH_PROCESS1;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 2:       XXH_PROCESS1;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 1:       XXH_PROCESS1;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 0:       return XXH32_avalanche(hash);
        }
        XXH_ASSERT(0);
        return hash;   /* reaching this point is deemed impossible */
    }
}

#ifdef XXH_OLD_NAMES
#  define PROCESS1 XXH_PROCESS1
#  define PROCESS4 XXH_PROCESS4
#else
#  undef XXH_PROCESS1
#  undef XXH_PROCESS4
#endif

/*!
 * @internal
 * @brief The implementation for @ref XXH32().
 *
 * @param input , len , seed Directly passed from @ref XXH32().
 * @param align Whether @p input is aligned.
 * @return The calculated hash.
 */
#if defined(_MSC_VER)
#  pragma warning(push)
/* MSVC's LTCG reports valid branches eliminated for constant input lengths. */
#  pragma warning(disable : 4702)  /* C4702: unreachable code */
#endif
XXH_FORCE_INLINE XXH_PUREF xxh_u32
XXH32_endian_align(const xxh_u8* input, size_t len, xxh_u32 seed, XXH_alignment align)
{
    xxh_u32 h32;

    if (input==NULL) XXH_ASSERT(len == 0);

    if (len>=16) {
        xxh_u32 acc[4];
        XXH32_initAccs(acc, seed);

        input = XXH32_consumeLong(acc, input, len, align);

        h32 = XXH32_mergeAccs(acc);
    } else {
        h32  = seed + XXH_PRIME32_5;
    }

    h32 += (xxh_u32)len;

    return XXH32_finalize(h32, input, len&15, align);
}
#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

/*! @ingroup XXH32_family */
XXH_PUBLIC_API XXH32_hash_t XXH32 (const void* input, size_t len, XXH32_hash_t seed)
{
#if !defined(XXH_NO_STREAM) && XXH_SIZE_OPT >= 2
    /* Simple version, good for code maintenance, but unfortunately slow for small inputs */
    XXH32_state_t state;
    XXH32_reset(&state, seed);
    XXH32_update(&state, (const xxh_u8*)input, len);
    return XXH32_digest(&state);
#else
    if (XXH_FORCE_ALIGN_CHECK) {
        if ((((size_t)input) & 3) == 0) {   /* Input is 4-bytes aligned, leverage the speed benefit */
            return XXH32_endian_align((const xxh_u8*)input, len, seed, XXH_aligned);
    }   }

    return XXH32_endian_align((const xxh_u8*)input, len, seed, XXH_unaligned);
#endif
}



/*******   Hash streaming   *******/
#ifndef XXH_NO_STREAM
/*! @ingroup XXH32_family */
XXH_PUBLIC_API XXH32_state_t* XXH32_createState(void)
{
    return (XXH32_state_t*)XXH_malloc(sizeof(XXH32_state_t));
}
/*! @ingroup XXH32_family */
XXH_PUBLIC_API XXH_errorcode XXH32_freeState(XXH32_state_t* statePtr)
{
    XXH_free(statePtr);
    return XXH_OK;
}

/*! @ingroup XXH32_family */
XXH_PUBLIC_API void XXH32_copyState(XXH32_state_t* dstState, const XXH32_state_t* srcState)
{
    XXH_memcpy(dstState, srcState, sizeof(*dstState));
}

/*! @ingroup XXH32_family */
XXH_PUBLIC_API XXH_errorcode XXH32_reset(XXH32_state_t* statePtr, XXH32_hash_t seed)
{
    XXH_ASSERT(statePtr != NULL);
    XXH_memset(statePtr, 0, sizeof(*statePtr));
    XXH32_initAccs(statePtr->acc, seed);
    return XXH_OK;
}


/*! @ingroup XXH32_family */
XXH_PUBLIC_API XXH_errorcode
XXH32_update(XXH32_state_t* state, const void* input, size_t len)
{
    if (input==NULL) {
        XXH_ASSERT(len == 0);
        return XXH_OK;
    }

    state->total_len_32 += (XXH32_hash_t)len;
    state->large_len |= (XXH32_hash_t)((len>=16) | (state->total_len_32>=16));

    XXH_ASSERT(state->bufferedSize < sizeof(state->buffer));
    if (len < sizeof(state->buffer) - state->bufferedSize)  {   /* fill in tmp buffer */
        XXH_memcpy(state->buffer + state->bufferedSize, input, len);
        state->bufferedSize += (XXH32_hash_t)len;
        return XXH_OK;
    }

    {   const xxh_u8* xinput = (const xxh_u8*)input;
        const xxh_u8* const bEnd = xinput + len;

        if (state->bufferedSize) {   /* non-empty buffer: complete first */
            XXH_memcpy(state->buffer + state->bufferedSize, xinput, sizeof(state->buffer) - state->bufferedSize);
            xinput += sizeof(state->buffer) - state->bufferedSize;
            /* then process one round */
            (void)XXH32_consumeLong(state->acc, state->buffer, sizeof(state->buffer), XXH_aligned);
            state->bufferedSize = 0;
        }

        XXH_ASSERT(xinput <= bEnd);
        if ((size_t)(bEnd - xinput) >= sizeof(state->buffer)) {
            /* Process the remaining data */
            xinput = XXH32_consumeLong(state->acc, xinput, (size_t)(bEnd - xinput), XXH_unaligned);
        }

        if (xinput < bEnd) {
            /* Copy the leftover to the tmp buffer */
            XXH_memcpy(state->buffer, xinput, (size_t)(bEnd-xinput));
            state->bufferedSize = (unsigned)(bEnd-xinput);
        }
    }

    return XXH_OK;
}


/*! @ingroup XXH32_family */
XXH_PUBLIC_API XXH32_hash_t XXH32_digest(const XXH32_state_t* state)
{
    xxh_u32 h32;

    if (state->large_len) {
        h32 = XXH32_mergeAccs(state->acc);
    } else {
        h32 = state->acc[2] /* == seed */ + XXH_PRIME32_5;
    }

    h32 += state->total_len_32;

    return XXH32_finalize(h32, state->buffer, state->bufferedSize, XXH_aligned);
}
#endif /* !XXH_NO_STREAM */

/*******   Canonical representation   *******/

/*! @ingroup XXH32_family */
XXH_PUBLIC_API void XXH32_canonicalFromHash(XXH32_canonical_t* dst, XXH32_hash_t hash)
{
    XXH_STATIC_ASSERT(sizeof(XXH32_canonical_t) == sizeof(XXH32_hash_t));
    if (XXH_CPU_LITTLE_ENDIAN) hash = XXH_swap32(hash);
    XXH_memcpy(dst, &hash, sizeof(*dst));
}
/*! @ingroup XXH32_family */
XXH_PUBLIC_API XXH32_hash_t XXH32_hashFromCanonical(const XXH32_canonical_t* src)
{
    return XXH_readBE32(src);
}


#ifndef XXH_NO_LONG_LONG

/* *******************************************************************
*  64-bit hash functions
*********************************************************************/
/*!
 * @}
 * @ingroup impl
 * @{
 */
/*******   Memory access   *******/

typedef XXH64_hash_t xxh_u64;

#ifdef XXH_OLD_NAMES
#  define U64 xxh_u64
#endif

#if (defined(XXH_FORCE_MEMORY_ACCESS) && (XXH_FORCE_MEMORY_ACCESS==3))
/*
 * Manual byteshift. Best for old compilers which don't inline memcpy.
 * We actually directly use XXH_readLE64 and XXH_readBE64.
 */
#elif (defined(XXH_FORCE_MEMORY_ACCESS) && (XXH_FORCE_MEMORY_ACCESS==2))

/* Force direct memory access. Only works on CPU which support unaligned memory access in hardware */
static xxh_u64 XXH_read64(const void* memPtr)
{
    return *(const xxh_u64*) memPtr;
}

#elif (defined(XXH_FORCE_MEMORY_ACCESS) && (XXH_FORCE_MEMORY_ACCESS==1))

/*
 * __attribute__((aligned(1))) is supported by gcc and clang. Originally the
 * documentation claimed that it only increased the alignment, but actually it
 * can decrease it on gcc, clang, and icc:
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=69502,
 * https://gcc.godbolt.org/z/xYez1j67Y.
 */
#ifdef XXH_OLD_NAMES
typedef union { xxh_u32 u32; xxh_u64 u64; } __attribute__((__packed__)) unalign64;
#endif
static xxh_u64 XXH_read64(const void* ptr)
{
    typedef __attribute__((__aligned__(1))) __attribute__((__may_alias__)) xxh_u64 xxh_unalign64;
    return *((const xxh_unalign64*)ptr);
}

#else

/*
 * Portable and safe solution. Generally efficient.
 * see: https://fastcompression.blogspot.com/2015/08/accessing-unaligned-memory.html
 */
static xxh_u64 XXH_read64(const void* memPtr)
{
    xxh_u64 val;
    XXH_memcpy(&val, memPtr, sizeof(val));
    return val;
}

#endif   /* XXH_FORCE_DIRECT_MEMORY_ACCESS */

#if defined(_MSC_VER)     /* Visual Studio */
#  define XXH_swap64 _byteswap_uint64
#elif XXH_GCC_VERSION >= 403
#  define XXH_swap64 __builtin_bswap64
#else
static xxh_u64 XXH_swap64(xxh_u64 x)
{
    return  ((x << 56) & 0xff00000000000000ULL) |
            ((x << 40) & 0x00ff000000000000ULL) |
            ((x << 24) & 0x0000ff0000000000ULL) |
            ((x << 8)  & 0x000000ff00000000ULL) |
            ((x >> 8)  & 0x00000000ff000000ULL) |
            ((x >> 24) & 0x0000000000ff0000ULL) |
            ((x >> 40) & 0x000000000000ff00ULL) |
            ((x >> 56) & 0x00000000000000ffULL);
}
#endif


/* XXH_FORCE_MEMORY_ACCESS==3 is an endian-independent byteshift load. */
#if (defined(XXH_FORCE_MEMORY_ACCESS) && (XXH_FORCE_MEMORY_ACCESS==3))

XXH_FORCE_INLINE xxh_u64 XXH_readLE64(const void* memPtr)
{
    const xxh_u8* bytePtr = (const xxh_u8 *)memPtr;
    return bytePtr[0]
         | ((xxh_u64)bytePtr[1] << 8)
         | ((xxh_u64)bytePtr[2] << 16)
         | ((xxh_u64)bytePtr[3] << 24)
         | ((xxh_u64)bytePtr[4] << 32)
         | ((xxh_u64)bytePtr[5] << 40)
         | ((xxh_u64)bytePtr[6] << 48)
         | ((xxh_u64)bytePtr[7] << 56);
}

XXH_FORCE_INLINE xxh_u64 XXH_readBE64(const void* memPtr)
{
    const xxh_u8* bytePtr = (const xxh_u8 *)memPtr;
    return bytePtr[7]
         | ((xxh_u64)bytePtr[6] << 8)
         | ((xxh_u64)bytePtr[5] << 16)
         | ((xxh_u64)bytePtr[4] << 24)
         | ((xxh_u64)bytePtr[3] << 32)
         | ((xxh_u64)bytePtr[2] << 40)
         | ((xxh_u64)bytePtr[1] << 48)
         | ((xxh_u64)bytePtr[0] << 56);
}

#else
XXH_FORCE_INLINE xxh_u64 XXH_readLE64(const void* ptr)
{
    return XXH_CPU_LITTLE_ENDIAN ? XXH_read64(ptr) : XXH_swap64(XXH_read64(ptr));
}

static xxh_u64 XXH_readBE64(const void* ptr)
{
    return XXH_CPU_LITTLE_ENDIAN ? XXH_swap64(XXH_read64(ptr)) : XXH_read64(ptr);
}
#endif

XXH_FORCE_INLINE xxh_u64
XXH_readLE64_align(const void* ptr, XXH_alignment align)
{
    if (align==XXH_unaligned)
        return XXH_readLE64(ptr);
    else
        return XXH_CPU_LITTLE_ENDIAN ? *(const xxh_u64*)ptr : XXH_swap64(*(const xxh_u64*)ptr);
}


/*******   xxh64   *******/
/*!
 * @}
 * @defgroup XXH64_impl XXH64 implementation
 * @ingroup impl
 *
 * Details on the XXH64 implementation.
 * @{
 */
/* #define rather that static const, to be used as initializers */
#define XXH_PRIME64_1  0x9E3779B185EBCA87ULL  /*!< 0b1001111000110111011110011011000110000101111010111100101010000111 */
#define XXH_PRIME64_2  0xC2B2AE3D27D4EB4FULL  /*!< 0b1100001010110010101011100011110100100111110101001110101101001111 */
#define XXH_PRIME64_3  0x165667B19E3779F9ULL  /*!< 0b0001011001010110011001111011000110011110001101110111100111111001 */
#define XXH_PRIME64_4  0x85EBCA77C2B2AE63ULL  /*!< 0b1000010111101011110010100111011111000010101100101010111001100011 */
#define XXH_PRIME64_5  0x27D4EB2F165667C5ULL  /*!< 0b0010011111010100111010110010111100010110010101100110011111000101 */

#ifdef XXH_OLD_NAMES
#  define PRIME64_1 XXH_PRIME64_1
#  define PRIME64_2 XXH_PRIME64_2
#  define PRIME64_3 XXH_PRIME64_3
#  define PRIME64_4 XXH_PRIME64_4
#  define PRIME64_5 XXH_PRIME64_5
#endif

/*! @copydoc XXH32_round */
static xxh_u64 XXH64_round(xxh_u64 acc, xxh_u64 input)
{
    acc += input * XXH_PRIME64_2;
    acc  = XXH_rotl64(acc, 31);
    acc *= XXH_PRIME64_1;
#if (defined(__SSE2__)) && !defined(XXH_ENABLE_AUTOVECTORIZE)
    /*
     * DISABLE AUTOVECTORIZATION:
     * A compiler fence is used to prevent GCC and Clang from
     * autovectorizing the XXH64 loop (pragmas and attributes don't work for some
     * reason) without globally disabling AVX512.
     *
     * Autovectorization of XXH64 tends to be detrimental,
     * though the exact outcome may change depending on exact cpu and compiler version.
     * For information, it has been reported as detrimental for Skylake-X and Zen5,
     * but possibly beneficial for Zen4.
     *
     * This is also enabled below AVX512: without vpmullq,
     * each 64-bit vector multiply is emulated with 3x pmuludq + shifts.
     * Clang 22 vectorizes XXH64_update() this way with AVX2,
     * making XXH64 streaming ~3x slower.
     *
     * The default is to disable auto-vectorization,
     * but you can select to enable it instead using `XXH_ENABLE_AUTOVECTORIZE` build variable.
     */
    XXH_COMPILER_GUARD(acc);
#endif
    return acc;
}

static xxh_u64 XXH64_mergeRound(xxh_u64 acc, xxh_u64 val)
{
    val  = XXH64_round(0, val);
    acc ^= val;
    acc  = acc * XXH_PRIME64_1 + XXH_PRIME64_4;
    return acc;
}

/*! @copydoc XXH32_avalanche */
static xxh_u64 XXH64_avalanche(xxh_u64 hash)
{
    hash ^= hash >> 33;
    hash *= XXH_PRIME64_2;
    hash ^= hash >> 29;
    hash *= XXH_PRIME64_3;
    hash ^= hash >> 32;
    return hash;
}


#define XXH_get64bits(p) XXH_readLE64_align(p, align)

/*!
 * @internal
 * @brief Sets up the initial accumulator state for XXH64().
 */
XXH_FORCE_INLINE void
XXH64_initAccs(xxh_u64 *acc, xxh_u64 seed)
{
    XXH_ASSERT(acc != NULL);
    acc[0] = seed + XXH_PRIME64_1 + XXH_PRIME64_2;
    acc[1] = seed + XXH_PRIME64_2;
    acc[2] = seed + 0;
    acc[3] = seed - XXH_PRIME64_1;
}

/*!
 * @internal
 * @brief Consumes a block of data for XXH64().
 *
 * @return the end input pointer.
 */
XXH_FORCE_INLINE const xxh_u8 *
XXH64_consumeLong(
    xxh_u64 *XXH_RESTRICT acc,
    xxh_u8 const *XXH_RESTRICT input,
    size_t len,
    XXH_alignment align
)
{
    const xxh_u8* const bEnd = input + len;
    const xxh_u8* const limit = bEnd - 31;
    XXH_ASSERT(acc != NULL);
    XXH_ASSERT(input != NULL);
    XXH_ASSERT(len >= 32);
    if (sizeof(void *) < sizeof(xxh_u64)) {
        /* reroll on 32-bit */
        do {
            size_t i;
            for (i = 0; i < 4; i++) {
                acc[i] = XXH64_round(acc[i], XXH_get64bits(input));
                input += 8;
            }
        } while (input < limit);
    } else {
        /* see XXH32_consumeLong() */
        xxh_u64 a0 = acc[0], a1 = acc[1], a2 = acc[2], a3 = acc[3];
        do {
            a0 = XXH64_round(a0, XXH_get64bits(input)); input += 8;
            a1 = XXH64_round(a1, XXH_get64bits(input)); input += 8;
            a2 = XXH64_round(a2, XXH_get64bits(input)); input += 8;
            a3 = XXH64_round(a3, XXH_get64bits(input)); input += 8;
        } while (input < limit);
        acc[0] = a0; acc[1] = a1; acc[2] = a2; acc[3] = a3;
    }

    return input;
}

/*!
 * @internal
 * @brief Merges the accumulator lanes together for XXH64()
 */
XXH_FORCE_INLINE XXH_PUREF xxh_u64
XXH64_mergeAccs(const xxh_u64 *acc)
{
    XXH_ASSERT(acc != NULL);
    {
        xxh_u64 h64 = XXH_rotl64(acc[0], 1) + XXH_rotl64(acc[1], 7)
                    + XXH_rotl64(acc[2], 12) + XXH_rotl64(acc[3], 18);
        /* reroll on 32-bit */
        if (sizeof(void *) < sizeof(xxh_u64)) {
            size_t i;
            for (i = 0; i < 4; i++) {
                h64 = XXH64_mergeRound(h64, acc[i]);
            }
        } else {
            h64 = XXH64_mergeRound(h64, acc[0]);
            h64 = XXH64_mergeRound(h64, acc[1]);
            h64 = XXH64_mergeRound(h64, acc[2]);
            h64 = XXH64_mergeRound(h64, acc[3]);
        }
        return h64;
    }
}

/*!
 * @internal
 * @brief Processes the last 0-31 bytes of @p ptr.
 *
 * There may be up to 31 bytes remaining to consume from the input.
 * This final stage will digest them to ensure that all input bytes are present
 * in the final mix.
 *
 * @param hash The hash to finalize.
 * @param ptr The pointer to the remaining input.
 * @param len The remaining length, modulo 32.
 * @param align Whether @p ptr is aligned.
 * @return The finalized hash
 * @see XXH32_finalize().
 */
XXH_STATIC XXH_PUREF xxh_u64
XXH64_finalize(xxh_u64 hash, const xxh_u8* ptr, size_t len, XXH_alignment align)
{
    if (ptr==NULL) XXH_ASSERT(len == 0);
    len &= 31;
    while (len >= 8) {
        xxh_u64 const k1 = XXH64_round(0, XXH_get64bits(ptr));
        ptr += 8;
        hash ^= k1;
        hash  = XXH_rotl64(hash,27) * XXH_PRIME64_1 + XXH_PRIME64_4;
        len -= 8;
    }
    if (len >= 4) {
        hash ^= (xxh_u64)(XXH_get32bits(ptr)) * XXH_PRIME64_1;
        ptr += 4;
        hash = XXH_rotl64(hash, 23) * XXH_PRIME64_2 + XXH_PRIME64_3;
        len -= 4;
    }
    while (len > 0) {
        hash ^= (*ptr++) * XXH_PRIME64_5;
        hash = XXH_rotl64(hash, 11) * XXH_PRIME64_1;
        --len;
    }
    return  XXH64_avalanche(hash);
}

#ifdef XXH_OLD_NAMES
#  define PROCESS1_64 XXH_PROCESS1_64
#  define PROCESS4_64 XXH_PROCESS4_64
#  define PROCESS8_64 XXH_PROCESS8_64
#else
#  undef XXH_PROCESS1_64
#  undef XXH_PROCESS4_64
#  undef XXH_PROCESS8_64
#endif

/*!
 * @internal
 * @brief The implementation for @ref XXH64().
 *
 * @param input , len , seed Directly passed from @ref XXH64().
 * @param align Whether @p input is aligned.
 * @return The calculated hash.
 */
XXH_FORCE_INLINE XXH_PUREF xxh_u64
XXH64_endian_align(const xxh_u8* input, size_t len, xxh_u64 seed, XXH_alignment align)
{
    xxh_u64 h64;
    if (input==NULL) XXH_ASSERT(len == 0);

    if (len>=32) {  /* Process a large block of data */
        xxh_u64 acc[4];
        XXH64_initAccs(acc, seed);

        input = XXH64_consumeLong(acc, input, len, align);

        h64 = XXH64_mergeAccs(acc);
    } else {
        h64  = seed + XXH_PRIME64_5;
    }

    h64 += (xxh_u64) len;

    return XXH64_finalize(h64, input, len, align);
}


/*! @ingroup XXH64_family */
XXH_PUBLIC_API XXH64_hash_t XXH64 (XXH_NOESCAPE const void* input, size_t len, XXH64_hash_t seed)
{
#if !defined(XXH_NO_STREAM) && XXH_SIZE_OPT >= 2
    /* Simple version, good for code maintenance, but unfortunately slow for small inputs */
    XXH64_state_t state;
    XXH64_reset(&state, seed);
    XXH64_update(&state, (const xxh_u8*)input, len);
    return XXH64_digest(&state);
#else
    if (XXH_FORCE_ALIGN_CHECK) {
        if ((((size_t)input) & 7)==0) {  /* Input is aligned, let's leverage the speed advantage */
            return XXH64_endian_align((const xxh_u8*)input, len, seed, XXH_aligned);
    }   }

    return XXH64_endian_align((const xxh_u8*)input, len, seed, XXH_unaligned);

#endif
}

/*******   Hash Streaming   *******/
#ifndef XXH_NO_STREAM
/*! @ingroup XXH64_family*/
XXH_PUBLIC_API XXH64_state_t* XXH64_createState(void)
{
    return (XXH64_state_t*)XXH_malloc(sizeof(XXH64_state_t));
}
/*! @ingroup XXH64_family */
XXH_PUBLIC_API XXH_errorcode XXH64_freeState(XXH64_state_t* statePtr)
{
    XXH_free(statePtr);
    return XXH_OK;
}

/*! @ingroup XXH64_family */
XXH_PUBLIC_API void XXH64_copyState(XXH_NOESCAPE XXH64_state_t* dstState, const XXH64_state_t* srcState)
{
    XXH_memcpy(dstState, srcState, sizeof(*dstState));
}

/*! @ingroup XXH64_family */
XXH_PUBLIC_API XXH_errorcode XXH64_reset(XXH_NOESCAPE XXH64_state_t* statePtr, XXH64_hash_t seed)
{
    XXH_ASSERT(statePtr != NULL);
    XXH_memset(statePtr, 0, sizeof(*statePtr));
    XXH64_initAccs(statePtr->acc, seed);
    return XXH_OK;
}

/*! @ingroup XXH64_family */
XXH_PUBLIC_API XXH_errorcode
XXH64_update (XXH_NOESCAPE XXH64_state_t* state, XXH_NOESCAPE const void* input, size_t len)
{
    if (input==NULL) {
        XXH_ASSERT(len == 0);
        return XXH_OK;
    }

    state->total_len += len;

    XXH_ASSERT(state->bufferedSize <= sizeof(state->buffer));
    if (len < sizeof(state->buffer) - state->bufferedSize)  {   /* fill in tmp buffer */
        XXH_memcpy(state->buffer + state->bufferedSize, input, len);
        state->bufferedSize += (XXH32_hash_t)len;
        return XXH_OK;
    }

    {   const xxh_u8* xinput = (const xxh_u8*)input;
        const xxh_u8* const bEnd = xinput + len;

        if (state->bufferedSize) {   /* non-empty buffer => complete first */
            XXH_memcpy(state->buffer + state->bufferedSize, xinput, sizeof(state->buffer) - state->bufferedSize);
            xinput += sizeof(state->buffer) - state->bufferedSize;
            /* and process one round */
            (void)XXH64_consumeLong(state->acc, state->buffer, sizeof(state->buffer), XXH_aligned);
            state->bufferedSize = 0;
        }

        XXH_ASSERT(xinput <= bEnd);
        if ((size_t)(bEnd - xinput) >= sizeof(state->buffer)) {
            /* Process the remaining data */
            xinput = XXH64_consumeLong(state->acc, xinput, (size_t)(bEnd - xinput), XXH_unaligned);
        }

        if (xinput < bEnd) {
            /* Copy the leftover to the tmp buffer */
            XXH_memcpy(state->buffer, xinput, (size_t)(bEnd-xinput));
            state->bufferedSize = (unsigned)(bEnd-xinput);
        }
    }

    return XXH_OK;
}


/*! @ingroup XXH64_family */
XXH_PUBLIC_API XXH64_hash_t XXH64_digest(XXH_NOESCAPE const XXH64_state_t* state)
{
    xxh_u64 h64;

    if (state->total_len >= 32) {
        h64 = XXH64_mergeAccs(state->acc);
    } else {
        h64  = state->acc[2] /*seed*/ + XXH_PRIME64_5;
    }

    h64 += (xxh_u64) state->total_len;

    return XXH64_finalize(h64, state->buffer, (size_t)state->total_len, XXH_aligned);
}
#endif /* !XXH_NO_STREAM */

/******* Canonical representation   *******/

/*! @ingroup XXH64_family */
XXH_PUBLIC_API void XXH64_canonicalFromHash(XXH_NOESCAPE XXH64_canonical_t* dst, XXH64_hash_t hash)
{
    XXH_STATIC_ASSERT(sizeof(XXH64_canonical_t) == sizeof(XXH64_hash_t));
    if (XXH_CPU_LITTLE_ENDIAN) hash = XXH_swap64(hash);
    XXH_memcpy(dst, &hash, sizeof(*dst));
}

/*! @ingroup XXH64_family */
XXH_PUBLIC_API XXH64_hash_t XXH64_hashFromCanonical(XXH_NOESCAPE const XXH64_canonical_t* src)
{
    return XXH_readBE64(src);
}

#include "xxh3_impl.h"

#endif  /* XXH_NO_LONG_LONG */

/*!
 * @}
 */
#endif  /* XXH_IMPLEMENTATION */
