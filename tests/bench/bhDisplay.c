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


/* ===  Dependencies  === */

#include <stdlib.h>   /* rand */
#include <stdio.h>    /* printf */
#include <assert.h>

#include "benchHash.h"
#include "bhDisplay.h"


/* ===  benchmark large input  === */

#define MB_UNIT           1000000
#define BENCH_LARGE_ITER_MS   490
#define BENCH_LARGE_TOTAL_MS 1010
static void bench_oneHash_largeInput(Bench_Entry hashDesc, int minlog, int maxlog)
{
    printf("%-7s", hashDesc.name);
    for (int sizelog=minlog; sizelog<=maxlog; sizelog++) {
        size_t const inputSize = (size_t)1 << sizelog;
        double const nbhps = bench_hash(hashDesc.hash, BMK_throughput,
                                        inputSize, BMK_fixedSize,
                                        BENCH_LARGE_TOTAL_MS, BENCH_LARGE_ITER_MS);
        printf(",%9.1f", nbhps * inputSize / MB_UNIT); fflush(NULL);
    }
    printf("\n");
}

void bench_largeInput(Bench_Entry const* hashDescTable, int nbHashes, int minlog, int maxlog)
{
    assert(maxlog <  31);
    assert(minlog >=  0);
    printf("benchmarking large inputs : from %u bytes (log%i) to %u MB (log%i) \n",
        1U << minlog, minlog,
        (1U << maxlog) >> 20, maxlog);
    for (int i=0; i<nbHashes; i++)
        bench_oneHash_largeInput(hashDescTable[i], minlog, maxlog);
}



/* ===  benchmark small input  === */

#define BENCH_SMALL_ITER_MS   170
#define BENCH_SMALL_TOTAL_MS  490
static void bench_throughput_oneHash_smallInputs(Bench_Entry hashDesc, size_t sizeMin, size_t sizeMax)
{
    printf("%-7s", hashDesc.name);
    for (size_t s=sizeMin; s<sizeMax+1; s++) {
        double const nbhps = bench_hash(hashDesc.hash, BMK_throughput,
                                        s, BMK_fixedSize,
                                        BENCH_SMALL_TOTAL_MS, BENCH_SMALL_ITER_MS);
        printf(",%11.1f", nbhps); fflush(NULL);
    }
    printf("\n");
}

void bench_throughput_smallInputs(Bench_Entry const* hashDescTable, int nbHashes, size_t sizeMin, size_t sizeMax)
{
    printf("Throughput small inputs of fixed size : \n");
    for (int i=0; i<nbHashes; i++)
        bench_throughput_oneHash_smallInputs(hashDescTable[i], sizeMin, sizeMax);
}



/* ===   Latency measurements (small keys)   === */

static void bench_latency_oneHash_smallInputs(Bench_Entry hashDesc, size_t size_min, size_t size_max)
{
    printf("%-7s", hashDesc.name);
    for (size_t s=size_min; s<size_max+1; s++) {
        double const nbhps = bench_hash(hashDesc.hash, BMK_latency,
                                        s, BMK_fixedSize,
                                        BENCH_SMALL_TOTAL_MS, BENCH_SMALL_ITER_MS);
        printf(",%11.1f", nbhps); fflush(NULL);
    }
    printf("\n");
}

void bench_latency_smallInputs(Bench_Entry const* hashDescTable, int nbHashes, size_t size_min, size_t size_max)
{
    printf("Latency for small inputs of fixed size : \n");
    for (int i=0; i<nbHashes; i++)
        bench_latency_oneHash_smallInputs(hashDescTable[i], size_min, size_max);
}


/* ===   Random input Length   === */

static void bench_randomInputLength_withOneHash(Bench_Entry hashDesc, size_t size_min, size_t size_max)
{
    printf("%-7s", hashDesc.name);
    for (size_t s=size_min; s<size_max+1; s++) {
        srand((unsigned)s);   /* ensure random sequence of length will be the same for a given s */
        double const nbhps = bench_hash(hashDesc.hash, BMK_throughput,
                                        s, BMK_randomSize,
                                        BENCH_SMALL_TOTAL_MS, BENCH_SMALL_ITER_MS);
        printf(",%11.1f", nbhps); fflush(NULL);
    }
    printf("\n");
}

void bench_throughput_randomInputLength(Bench_Entry const* hashDescTable, int nbHashes, size_t size_min, size_t size_max)
{
    printf("benchmarking random size inputs [1-N] : \n");
    for (int i=0; i<nbHashes; i++)
        bench_randomInputLength_withOneHash(hashDescTable[i], size_min, size_max);
}


/* ===   Latency with Random input Length   === */

static void bench_latency_oneHash_randomInputLength(Bench_Entry hashDesc, size_t size_min, size_t size_max)
{
    printf("%-7s", hashDesc.name);
    for (size_t s=size_min; s<size_max+1; s++) {
        srand((unsigned)s);   /* ensure random sequence of length will be the same for a given s */
        double const nbhps = bench_hash(hashDesc.hash, BMK_latency,
                                        s, BMK_randomSize,
                                        BENCH_SMALL_TOTAL_MS, BENCH_SMALL_ITER_MS);
        printf(",%11.1f", nbhps); fflush(NULL);
    }
    printf("\n");
}

void bench_latency_randomInputLength(Bench_Entry const* hashDescTable, int nbHashes, size_t size_min, size_t size_max)
{
    printf("Latency for small inputs of random size [1-N] : \n");
    for (int i=0; i<nbHashes; i++)
        bench_latency_oneHash_randomInputLength(hashDescTable[i], size_min, size_max);
}
