/*
 * xxHash - Extremely Fast Hash algorithm
 * XXH3 and XXH128 streaming declarations
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

#if !defined(XXH_NO_XXH3) && !defined(XXH_NO_XXH32) && !defined(XXH_NO_LONG_LONG) && !defined(XXH_NO_STREAM)
/* XXH3 depends on XXH32 and XXH64 */

#if defined(XXH_INLINE_ALL) || defined(XXH_PRIVATE_API)
   /* see INLINE mode: undefine names, in case they were defined by a previous inclusion */
#  undef XXH3_createState
#  undef XXH3_freeState
#  undef XXH3_copyState
#  undef XXH3_64bits_reset
#  undef XXH3_64bits_reset_withSeed
#  undef XXH3_64bits_reset_withSecret
#  undef XXH3_64bits_reset_withSecretandSeed
#  undef XXH3_64bits_update
#  undef XXH3_64bits_digest
#  undef XXH3_128bits_reset
#  undef XXH3_128bits_reset_withSeed
#  undef XXH3_128bits_reset_withSecret
#  undef XXH3_128bits_reset_withSecretandSeed
#  undef XXH3_128bits_update
#  undef XXH3_128bits_digest
#endif
#ifdef XXH_NAMESPACE
#  define XXH3_createState XXH_IPREF(XXH3_createState)
#  define XXH3_freeState XXH_IPREF(XXH3_freeState)
#  define XXH3_copyState XXH_IPREF(XXH3_copyState)
#  define XXH3_64bits_reset XXH_IPREF(XXH3_64bits_reset)
#  define XXH3_64bits_reset_withSeed XXH_IPREF(XXH3_64bits_reset_withSeed)
#  define XXH3_64bits_reset_withSecret XXH_IPREF(XXH3_64bits_reset_withSecret)
#  define XXH3_64bits_reset_withSecretandSeed XXH_IPREF(XXH3_64bits_reset_withSecretandSeed)
#  define XXH3_64bits_update XXH_IPREF(XXH3_64bits_update)
#  define XXH3_64bits_digest XXH_IPREF(XXH3_64bits_digest)
#  define XXH3_128bits_reset XXH_IPREF(XXH3_128bits_reset)
#  define XXH3_128bits_reset_withSeed XXH_IPREF(XXH3_128bits_reset_withSeed)
#  define XXH3_128bits_reset_withSecret XXH_IPREF(XXH3_128bits_reset_withSecret)
#  define XXH3_128bits_reset_withSecretandSeed XXH_IPREF(XXH3_128bits_reset_withSecretandSeed)
#  define XXH3_128bits_update XXH_IPREF(XXH3_128bits_update)
#  define XXH3_128bits_digest XXH_IPREF(XXH3_128bits_digest)
#endif

/*******   XXH3 64-bit streaming   *******/
/*
 * Streaming requires state maintenance.
 * This operation costs memory and CPU.
 * As a consequence, streaming is slower than one-shot hashing.
 * For better performance, prefer one-shot functions whenever applicable.
 */

/*!
 * @brief The opaque state struct for the XXH3 streaming API.
 *
 * @see XXH3_state_s for details.
 * @see @ref streaming_example "Streaming Example"
 */
typedef struct XXH3_state_s XXH3_state_t;
XXH_PUBLIC_API XXH_MALLOCF XXH3_state_t* XXH3_createState(void);
XXH_PUBLIC_API XXH_errorcode XXH3_freeState(XXH3_state_t* statePtr);

/*!
 * @brief Copies one @ref XXH3_state_t to another.
 *
 * @param dst_state The state to copy to.
 * @param src_state The state to copy from.
 * @pre
 *   @p dst_state and @p src_state must not be `NULL` and must not overlap.
 */
XXH_PUBLIC_API void XXH3_copyState(XXH_NOESCAPE XXH3_state_t* dst_state, XXH_NOESCAPE const XXH3_state_t* src_state);

/*!
 * @brief Resets an @ref XXH3_state_t to begin a new hash.
 *
 * @param statePtr The state struct to reset.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @note
 *   - This function resets `statePtr` and generate a secret with default parameters.
 *   - Call this function before @ref XXH3_64bits_update().
 *   - Digest will be equivalent to `XXH3_64bits()`.
 *
 * @see @ref streaming_example "Streaming Example"
 *
 */
XXH_PUBLIC_API XXH_errorcode XXH3_64bits_reset(XXH_NOESCAPE XXH3_state_t* statePtr);

/*!
 * @brief Resets an @ref XXH3_state_t with 64-bit seed to begin a new hash.
 *
 * @param statePtr The state struct to reset.
 * @param seed     The 64-bit seed to alter the hash result predictably.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @note
 *   - This function resets `statePtr` and generate a secret from `seed`.
 *   - Call this function before @ref XXH3_64bits_update().
 *   - Digest will be equivalent to `XXH3_64bits_withSeed()`.
 *
 * @see @ref streaming_example "Streaming Example"
 *
 */
XXH_PUBLIC_API XXH_errorcode XXH3_64bits_reset_withSeed(XXH_NOESCAPE XXH3_state_t* statePtr, XXH64_hash_t seed);

/*!
 * @brief Resets an @ref XXH3_state_t with secret data to begin a new hash.
 *
 * @param statePtr The state struct to reset.
 * @param secret     The secret data.
 * @param secretSize The length of @p secret, in bytes.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @note
 *   `secret` is referenced, it _must outlive_ the hash streaming session.
 *
 * Similar to one-shot API, `secretSize` must be >= @ref XXH3_SECRET_SIZE_MIN,
 * and the quality of produced hash values depends on secret's entropy
 * (secret's content should look like a bunch of random bytes).
 * When in doubt about the randomness of a candidate `secret`,
 * consider employing `XXH3_generateSecret()` instead (see below).
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_errorcode XXH3_64bits_reset_withSecret(XXH_NOESCAPE XXH3_state_t* statePtr, XXH_NOESCAPE const void* secret, size_t secretSize);

/*!
 * @brief Consumes a block of @p input to an @ref XXH3_state_t.
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
XXH_PUBLIC_API XXH_errorcode XXH3_64bits_update (XXH_NOESCAPE XXH3_state_t* statePtr, XXH_NOESCAPE const void* input, size_t length);

/*!
 * @brief Returns the calculated XXH3 64-bit hash value from an @ref XXH3_state_t.
 *
 * @param statePtr The state struct to calculate the hash from.
 *
 * @pre
 *  @p statePtr must not be `NULL`.
 *
 * @return The calculated XXH3 64-bit hash value from that state.
 *
 * @note
 *   Calling XXH3_64bits_digest() will not affect @p statePtr, so you can update,
 *   digest, and update again.
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_PUREF XXH64_hash_t XXH3_64bits_digest (XXH_NOESCAPE const XXH3_state_t* statePtr);


/*******   XXH3 128-bit streaming   *******/
/*
 * Streaming requires state maintenance.
 * This operation costs memory and CPU.
 * As a consequence, streaming is slower than one-shot hashing.
 * For better performance, prefer one-shot functions whenever applicable.
 *
 * XXH3_128bits uses the same XXH3_state_t as XXH3_64bits().
 * Use already declared XXH3_createState() and XXH3_freeState().
 *
 * All reset and streaming functions have same meaning as their 64-bit counterpart.
 */

/*!
 * @brief Resets an @ref XXH3_state_t to begin a new hash.
 *
 * @param statePtr The state struct to reset.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @note
 *   - This function resets `statePtr` and generate a secret with default parameters.
 *   - Call it before @ref XXH3_128bits_update().
 *   - Digest will be equivalent to `XXH3_128bits()`.
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_errorcode XXH3_128bits_reset(XXH_NOESCAPE XXH3_state_t* statePtr);

/*!
 * @brief Resets an @ref XXH3_state_t with 64-bit seed to begin a new hash.
 *
 * @param statePtr The state struct to reset.
 * @param seed     The 64-bit seed to alter the hash result predictably.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @note
 *   - This function resets `statePtr` and generate a secret from `seed`.
 *   - Call it before @ref XXH3_128bits_update().
 *   - Digest will be equivalent to `XXH3_128bits_withSeed()`.
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_errorcode XXH3_128bits_reset_withSeed(XXH_NOESCAPE XXH3_state_t* statePtr, XXH64_hash_t seed);
/*!
 * @brief Resets an @ref XXH3_state_t with secret data to begin a new hash.
 *
 * @param statePtr   The state struct to reset.
 * @param secret     The secret data.
 * @param secretSize The length of @p secret, in bytes.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * `secret` is referenced, it _must outlive_ the hash streaming session.
 * Similar to one-shot API, `secretSize` must be >= @ref XXH3_SECRET_SIZE_MIN,
 * and the quality of produced hash values depends on secret's entropy
 * (secret's content should look like a bunch of random bytes).
 * When in doubt about the randomness of a candidate `secret`,
 * consider employing `XXH3_generateSecret()` instead (see below).
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_errorcode XXH3_128bits_reset_withSecret(XXH_NOESCAPE XXH3_state_t* statePtr, XXH_NOESCAPE const void* secret, size_t secretSize);

/*!
 * @brief Consumes a block of @p input to an @ref XXH3_state_t.
 *
 * Call this to incrementally consume blocks of data.
 *
 * @param statePtr The state struct to update.
 * @param input The block of data to be hashed, at least @p length bytes in size.
 * @param length The length of @p input, in bytes.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @note
 *   The memory between @p input and @p input + @p length must be valid,
 *   readable, contiguous memory. However, if @p length is `0`, @p input may be
 *   `NULL`. In C++, this also must be *TriviallyCopyable*.
 *
 */
XXH_PUBLIC_API XXH_errorcode XXH3_128bits_update (XXH_NOESCAPE XXH3_state_t* statePtr, XXH_NOESCAPE const void* input, size_t length);

/*!
 * @brief Returns the calculated XXH3 128-bit hash value from an @ref XXH3_state_t.
 *
 * @param statePtr The state struct to calculate the hash from.
 *
 * @pre
 *  @p statePtr must not be `NULL`.
 *
 * @return The calculated XXH3 128-bit hash value from that state.
 *
 * @note
 *   Calling XXH3_128bits_digest() will not affect @p statePtr, so you can update,
 *   digest, and update again.
 *
 */
XXH_PUBLIC_API XXH_PUREF XXH128_hash_t XXH3_128bits_digest (XXH_NOESCAPE const XXH3_state_t* statePtr);


#endif  /* !XXH_NO_XXH3 && !XXH_NO_STREAM */
