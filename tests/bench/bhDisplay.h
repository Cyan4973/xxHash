/*
*  CSV Display module for the hash benchmark program
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

#ifndef BH_DISPLAY_H_192088098
#define BH_DISPLAY_H_192088098

#if defined (__cplusplus)
extern "C" {
#endif


/* ===  Dependencies  === */

#include "benchfn.h"   /* BMK_benchFn_t */


/* ===  Declarations  === */

typedef struct {
    const char* name;
    BMK_benchFn_t hash;
} Bench_Entry;

void bench_largeInput(Bench_Entry const* hashDescTable, int nbHashes, int sizeLogMin, int sizeLogMax);

void bench_throughput_smallInputs(Bench_Entry const* hashDescTable, int nbHashes, size_t sizeMin, size_t sizeMax);
void bench_throughput_randomInputLength(Bench_Entry const* hashDescTable, int nbHashes, size_t sizeMin, size_t sizeMax);

void bench_latency_smallInputs(Bench_Entry const* hashDescTable, int nbHashes, size_t sizeMin, size_t sizeMax);
void bench_latency_randomInputLength(Bench_Entry const* hashDescTable, int nbHashes, size_t sizeMin, size_t sizeMax);



#if defined (__cplusplus)
}
#endif

#endif   /* BH_DISPLAY_H_192088098 */
