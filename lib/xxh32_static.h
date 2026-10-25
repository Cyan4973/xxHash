/*
 * xxHash - Extremely Fast Hash algorithm
 * XXH32 declarations for static linking only
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

#endif  /* XXH_NO_XXH32 */
