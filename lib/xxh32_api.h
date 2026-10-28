/*
 * xxHash - Extremely Fast Hash algorithm
 * XXH32 public declarations
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

#ifndef XXH_NO_XXH32

#if defined(XXH_INLINE_ALL) || defined(XXH_PRIVATE_API)
   /* see INLINE mode: undefine names, in case they were defined by a previous inclusion */
#  undef XXH32
#  undef XXH32_canonicalFromHash
#  undef XXH32_hashFromCanonical
#  define XXH32_canonical_t  XXH_IPREF(XXH32_canonical_t)
#  define XXH32_state_s XXH_IPREF(XXH32_state_s)
#  define XXH32_state_t XXH_IPREF(XXH32_state_t)
#endif
#ifdef XXH_NAMESPACE
#  define XXH32 XXH_IPREF(XXH32)
#  define XXH32_canonicalFromHash XXH_IPREF(XXH32_canonicalFromHash)
#  define XXH32_hashFromCanonical XXH_IPREF(XXH32_hashFromCanonical)
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


#endif  /* XXH_NO_XXH32 */
