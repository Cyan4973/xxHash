/*
 * xxHash - XXH3 Dispatcher for x86-based targets
 * Copyright (C) 2020-2024 Yann Collet
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

#ifndef XXH_X86DISPATCH_H_13563687684
#define XXH_X86DISPATCH_H_13563687684

#include "xxhash.h"  /* XXH64_hash_t, XXH3_state_t */

/*!
 * @def XXH_DISPATCH_API
 * @brief Marks the dispatch symbols.
 *
 * The dispatch functions always live in a separately compiled unit
 * (`xxh_x86dispatch.c`), so they must keep external linkage.
 * They can't employ @ref XXH_PUBLIC_API, which becomes `static`
 * when the including unit requests @ref XXH_INLINE_ALL.
 */
#ifndef XXH_DISPATCH_API
#  if defined(_WIN32) && defined(_MSC_VER) && (defined(XXH_IMPORT) || defined(XXH_EXPORT))
#    ifdef XXH_EXPORT
#      define XXH_DISPATCH_API __declspec(dllexport)
#    else
#      define XXH_DISPATCH_API __declspec(dllimport)
#    endif
#  else
#    define XXH_DISPATCH_API   /* do nothing */
#  endif
#endif

#if defined (__cplusplus)
extern "C" {
#endif

/*!
 * @brief Returns the best XXH3 implementation for x86
 *
 * @return The best @ref XXH_VECTOR implementation.
 * @see XXH_VECTOR_TYPES
 */
XXH_DISPATCH_API int XXH_featureTest(void);

XXH_DISPATCH_API XXH64_hash_t  XXH3_64bits_dispatch(XXH_NOESCAPE const void* input, size_t len);
XXH_DISPATCH_API XXH64_hash_t  XXH3_64bits_withSeed_dispatch(XXH_NOESCAPE const void* input, size_t len, XXH64_hash_t seed);
XXH_DISPATCH_API XXH64_hash_t  XXH3_64bits_withSecret_dispatch(XXH_NOESCAPE const void* input, size_t len, XXH_NOESCAPE const void* secret, size_t secretLen);
XXH_DISPATCH_API XXH_errorcode XXH3_64bits_update_dispatch(XXH_NOESCAPE XXH3_state_t* state, XXH_NOESCAPE const void* input, size_t len);

XXH_DISPATCH_API XXH128_hash_t XXH3_128bits_dispatch(XXH_NOESCAPE const void* input, size_t len);
XXH_DISPATCH_API XXH128_hash_t XXH3_128bits_withSeed_dispatch(XXH_NOESCAPE const void* input, size_t len, XXH64_hash_t seed);
XXH_DISPATCH_API XXH128_hash_t XXH3_128bits_withSecret_dispatch(XXH_NOESCAPE const void* input, size_t len, XXH_NOESCAPE const void* secret, size_t secretLen);
XXH_DISPATCH_API XXH_errorcode XXH3_128bits_update_dispatch(XXH_NOESCAPE XXH3_state_t* state, XXH_NOESCAPE const void* input, size_t len);

#if defined (__cplusplus)
}
#endif


/* automatic replacement of XXH3 functions.
 * can be disabled by setting XXH_DISPATCH_DISABLE_REPLACE */
#ifndef XXH_DISPATCH_DISABLE_REPLACE

# undef  XXH3_64bits
# define XXH3_64bits XXH3_64bits_dispatch
# undef  XXH3_64bits_withSeed
# define XXH3_64bits_withSeed XXH3_64bits_withSeed_dispatch
# undef  XXH3_64bits_withSecret
# define XXH3_64bits_withSecret XXH3_64bits_withSecret_dispatch
# undef  XXH3_64bits_update
# define XXH3_64bits_update XXH3_64bits_update_dispatch

# undef  XXH128
# define XXH128 XXH3_128bits_withSeed_dispatch
# undef  XXH3_128bits
# define XXH3_128bits XXH3_128bits_dispatch
# undef  XXH3_128bits_withSeed
# define XXH3_128bits_withSeed XXH3_128bits_withSeed_dispatch
# undef  XXH3_128bits_withSecret
# define XXH3_128bits_withSecret XXH3_128bits_withSecret_dispatch
# undef  XXH3_128bits_update
# define XXH3_128bits_update XXH3_128bits_update_dispatch

#endif /* XXH_DISPATCH_DISABLE_REPLACE */

#endif /* XXH_X86DISPATCH_H_13563687684 */
