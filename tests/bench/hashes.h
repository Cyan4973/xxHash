/*
*  List hash algorithms to benchmark
*  Part of xxHash project
*  Copyright (C) 2019-present, Yann Collet
*
*  BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions are
*  met:
*
*  * Redistributions of source code must retain the above copyright
*  notice, this list of conditions and the following disclaimer.
*  * Redistributions in binary form must reproduce the above
*  copyright notice, this list of conditions and the following disclaimer
*  in the documentation and/or other materials provided with the
*  distribution.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
*  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
*  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
*  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
*  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
*  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
*  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
*  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*  You can contact the author at :
*  - xxHash homepage: http://www.xxhash.com
*  - xxHash source repository : https://github.com/Cyan4973/xxHash
*/


/* ===   Dependencies   === */

#include <stddef.h>   /* size_t */


/* ==================================================
 *   Non-portable hash algorithms
 * =============================================== */


#ifdef HARDWARE_SUPPORT

/* list here hash algorithms depending on specific hardware support,
 * including for example :
 * - Hardware crc32c
 * - Hardware AES support
 * - Carryless Multipliers (clmul)
 * - AVX2
 */

#endif



/* ==================================================
 * List of hashes
 * =============================================== */

/* Each hash must be wrapped in a thin redirector conformant with the BMK_benchfn_t.
 * BMK_benchfn_t is generic, not specifically designed for hashes.
 * For hashes, the following parameters are expected to be useless :
 * dst, dstCapacity, customPayload.
 *
 * The result of each hash is assumed to be provided as function return value.
 * This condition is important for latency measurements.
 */

 /* ===  xxHash  === */

#include "xxh3.h"

size_t XXH32_wrapper(const void* src, size_t srcSize, void* dst, size_t dstCapacity, void* customPayload)
{
    (void)dst; (void)dstCapacity; (void)customPayload;
    return (size_t) XXH32(src, srcSize, 0);
}


size_t XXH64_wrapper(const void* src, size_t srcSize, void* dst, size_t dstCapacity, void* customPayload)
{
    (void)dst; (void)dstCapacity; (void)customPayload;
    return (size_t) XXH64(src, srcSize, 0);
}


size_t xxh3_wrapper(const void* src, size_t srcSize, void* dst, size_t dstCapacity, void* customPayload)
{
    (void)dst; (void)dstCapacity; (void)customPayload;
    return (size_t) XXH3_64bits(src, srcSize);
}


size_t XXH128_wrapper(const void* src, size_t srcSize, void* dst, size_t dstCapacity, void* customPayload)
{
    (void)dst; (void)dstCapacity; (void)customPayload;
    return (size_t) XXH3_128bits(src, srcSize).low64;
}



/* ==================================================
 * Table of hashes
 * =============================================== */

#include "bhDisplay.h"   /* Bench_Entry */

#ifndef HARDWARE_SUPPORT
#  define NB_HASHES 4
#else
#  define NB_HASHES 4
#endif

Bench_Entry const hashCandidates[NB_HASHES] = {
    { "xxh3"  , xxh3_wrapper },
    { "XXH32" , XXH32_wrapper },
    { "XXH64" , XXH64_wrapper },
    { "XXH128", XXH128_wrapper },
#ifdef HARDWARE_SUPPORT
    /* list here codecs which require specific hardware support, such SSE4.1, PCLMUL, AVX2, etc. */
#endif
};
