/*
 * xxHash - Extremely Fast Hash algorithm
 * Streaming implementation common to all families: memory functions
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

#ifndef XXH_NO_STREAM

#if defined(XXH_NO_STDLIB)

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

#ifndef XXH_memset
/*!
 * @internal
 * @brief XXH_memset() macro can be redirected at compile time
 */
#  include <string.h>
#  define XXH_memset memset
#endif

#endif  /* !XXH_NO_STREAM */
