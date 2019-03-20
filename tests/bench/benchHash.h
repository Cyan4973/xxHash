/*
*  Hash benchmark module
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


#ifndef BENCH_HASH_H_983426678
#define BENCH_HASH_H_983426678

#if defined (__cplusplus)
extern "C" {
#endif


/* ===  Dependencies  === */

#include "benchfn.h"   /* BMK_benchFn_t */


/* ===  Declarations  === */

typedef enum { BMK_throughput, BMK_latency } BMK_benchMode;

typedef enum { BMK_fixedSize,   /* hash always `size` bytes */
               BMK_randomSize,  /* hash a random nb of bytes, between 1 and `size` (inclusive) */
} BMK_sizeMode;

/* bench_hash() :
 * returns speed expressed as nb hashes per second.
 * total_time_ms : time spent benchmarking the hash function with given parameters
 * iter_time_ms : time spent for one round. If multiple rounds are run, bench_hash() will report the speed of best round.
 */
double bench_hash(BMK_benchFn_t hashfn,
                  BMK_benchMode benchMode,
                  size_t size, BMK_sizeMode sizeMode,
                  unsigned total_time_ms, unsigned iter_time_ms);



#if defined (__cplusplus)
}
#endif

#endif /* BENCH_HASH_H_983426678 */
