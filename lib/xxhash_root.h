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

#include "xxh_doc.h"        /* main page of documentation */

#if defined(__cplusplus) && !defined(XXH_NO_EXTERNC_GUARD)
extern "C" {
#endif

/*!
 * @defgroup public Public API
 * Contains details on the public xxHash functions.
 * @{
 */
#include "xxh_modes.h"      /* XXH_INLINE_ALL, XXH_PRIVATE_API, XXH_NAMESPACE */

/* ****************************************************************
 *  Stable API
 *****************************************************************/
#ifndef XXHASH_H_5627135585666179
#define XXHASH_H_5627135585666179 1

#include "xxh_api.h"        /* declarations common to all families */

#include "xxh32_api.h"
#include "xxh32_stream_api.h"

#include "xxh64_api.h"
#include "xxh64_stream_api.h"

#include "xxh3_api.h"
#include "xxh3_stream_api.h"

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

#include "xxh32_static.h"

#include "xxh64_static.h"

#include "xxh3_static.h"
#include "xxh3_stream_static.h"

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

#include "xxh_tuning.h"     /* tuning parameters common to all families */

/*!
 * @defgroup impl Implementation
 * @{
 */

#include "xxh_stream_impl.h" /* streaming memory functions common to all families */
#include "xxh_compiler.h"   /* compiler specifics common to all families */
#include "xxh_memory.h"     /* basic types and memory primitives common to all families */

/* *************************************
*  Misc
***************************************/
/*! @ingroup public */
XXH_PUBLIC_API unsigned XXH_versionNumber (void) { return XXH_VERSION_NUMBER; }

#include "xxh32_impl.h"
#include "xxh32_stream_impl.h"

#include "xxh64_impl.h"
#include "xxh64_stream_impl.h"

#include "xxh3_impl.h"      /* includes xxh3_stream_impl.h */

/*!
 * @}
 */
#endif  /* XXH_IMPLEMENTATION */
