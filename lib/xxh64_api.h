/*
 * xxHash - Extremely Fast Hash algorithm
 * XXH64 public declarations
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

#ifndef XXH_NO_LONG_LONG

#if defined(XXH_INLINE_ALL) || defined(XXH_PRIVATE_API)
   /* see INLINE mode: undefine names, in case they were defined by a previous inclusion */
#  undef XXH64
#  undef XXH64_canonicalFromHash
#  undef XXH64_hashFromCanonical
#  define XXH64_canonical_t  XXH_IPREF(XXH64_canonical_t)
#  define XXH64_state_s XXH_IPREF(XXH64_state_s)
#  define XXH64_state_t XXH_IPREF(XXH64_state_t)
#endif
#ifdef XXH_NAMESPACE
#  define XXH64 XXH_IPREF(XXH64)
#  define XXH64_canonicalFromHash XXH_IPREF(XXH64_canonicalFromHash)
#  define XXH64_hashFromCanonical XXH_IPREF(XXH64_hashFromCanonical)
#endif

/*!
 * @}
 * @ingroup public
 * @{
 */

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

#endif  /* XXH_NO_LONG_LONG */
