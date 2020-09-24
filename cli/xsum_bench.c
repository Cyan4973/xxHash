/*
 * xxhsum - Command line interface for xxhash algorithms
 * Copyright (C) 2013-2020 Yann Collet
 *
 * GPL v2 License
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * You can contact the author at:
 *   - xxHash homepage: https://www.xxhash.com
 *   - xxHash source repository: https://github.com/Cyan4973/xxHash
 */

#include "xsum_config.h"
#include "xsum_bench.h"        /* prototypes */
#include "xsum_output.h"       /* XSUM_log, XSUM_logVerbose */
#include "xsum_os_specific.h"  /* XSUM_fopen */
#include "xsum_sanity_check.h" /* XSUM_fillTestBuffer */
#include <stdlib.h>            /* malloc, calloc, free, strtoul, exit */
#include <stdio.h>             /* fread, fclose */
#include <assert.h>            /* assert */
#include <string.h>            /* strlen, memcpy, strerror */
#include <errno.h>             /* errno */
#include <limits.h>            /* ULONG_MAX */
#include "timefn.h"            /* UTIL_time_t, etc */
#ifndef XXH_STATIC_LINKING_ONLY
#  define XXH_STATIC_LINKING_ONLY
#endif
#include "../xxhash.h"

/* ************************************
 *  Benchmark Functions
 **************************************/
#if XSUM_NO_BENCH
static int XSUM_noBench(void)
{
    XSUM_log("This version of xxhsum was compiled without benchmarks.\n");
    return 1;
}

XSUM_API int XSUM_benchFiles(char*const* fileNamesTable, int nbFiles)
{
    (void)fileNamesTable;
    (void)nbFiles;
    return XSUM_noBench();
}
XSUM_API int XSUM_benchInternal(size_t keySize)
{
    (void)keySize;
    return XSUM_noBench();
}

XSUM_API void XSUM_setBenchID(XSUM_U32 id, int fill)
{
    (void)id;
    (void)fill;
    /* nop */
}
XSUM_API void XSUM_setBenchIter(XSUM_U32 iter)
{
    (void)iter;
    /* nop */
}

#else
#define KB *( 1<<10)
#define MB *( 1<<20)
#define GB *(1U<<30)

#define MAX_MEM    (2 GB - 64 MB)

#define XSUM_TIMELOOP_S 1
#define XSUM_SECOND (1*1000000000ULL)                  /* 1 second in nanoseconds */
#define XSUM_TIMELOOP  (XSUM_TIMELOOP_S * XSUM_SECOND) /* target timing per iteration */
#define XSUM_TIMELOOP_MIN (XSUM_TIMELOOP / 2)          /* minimum timing to validate a result */

static XSUM_U32 XSUM_nbIterations = XSUM_BENCH_NB_ITER;

static size_t XSUM_findMaxMem(XSUM_U64 requiredMem)
{
    size_t const step = 64 MB;
    void* testmem = NULL;

    requiredMem = (((requiredMem >> 26) + 1) << 26);
    requiredMem += 2*step;
    if (requiredMem > MAX_MEM) requiredMem = MAX_MEM;

    while (!testmem) {
        if (requiredMem > step) requiredMem -= step;
        else requiredMem >>= 1;
        testmem = malloc ((size_t)requiredMem);
    }
    free (testmem);

    /* keep some space available */
    if (requiredMem > step) requiredMem -= step;
    else requiredMem >>= 1;

    return (size_t)requiredMem;
}

/*
 * Allocates a string containing s1 and s2 concatenated. Acts like strdup.
 * The result must be freed.
 */
static char* XSUM_strcatDup(const char* s1, const char* s2)
{
    assert(s1 != NULL);
    assert(s2 != NULL);
    {   size_t len1 = strlen(s1);
        size_t len2 = strlen(s2);
        char* buf = (char*)malloc(len1 + len2 + 1);
        if (buf != NULL) {
            /* strcpy(buf, s1) */
            memcpy(buf, s1, len1);
            /* strcat(buf, s2) */
            memcpy(buf + len1, s2, len2 + 1);
        }
        return buf;
    }
}


/*
 * A secret buffer used for benchmarking XXH3's withSecret variants.
 *
 * In order for the bench to be realistic, the secret buffer would need to be
 * pre-generated.
 *
 * Adding a pointer to the parameter list would be messy.
 */
static XSUM_U8 XSUM_benchSecretBuf[XXH3_SECRET_SIZE_MIN];

/*
 * Wrappers for the benchmark.
 *
 * If you would like to add other hashes to the bench, create a wrapper and add
 * it to the XSUM_hashesToBench table. It will automatically be added.
 */
typedef XSUM_U32 (*XSUM_hashFunction)(const void* buffer, size_t bufferSize, XSUM_U32 seed);

static XSUM_U32 XSUM_wrapXXH32(const void* buffer, size_t bufferSize, XSUM_U32 seed)
{
    return XXH32(buffer, bufferSize, seed);
}
static XSUM_U32 XSUM_wrapXXH64(const void* buffer, size_t bufferSize, XSUM_U32 seed)
{
    return (XSUM_U32)XXH64(buffer, bufferSize, seed);
}
static XSUM_U32 XSUM_wrapXXH3_64b(const void* buffer, size_t bufferSize, XSUM_U32 seed)
{
    (void)seed;
    return (XSUM_U32)XXH3_64bits(buffer, bufferSize);
}
static XSUM_U32 XSUM_wrapXXH3_64b_seeded(const void* buffer, size_t bufferSize, XSUM_U32 seed)
{
    return (XSUM_U32)XXH3_64bits_withSeed(buffer, bufferSize, seed);
}
static XSUM_U32 XSUM_wrapXXH3_64b_secret(const void* buffer, size_t bufferSize, XSUM_U32 seed)
{
    (void)seed;
    return (XSUM_U32)XXH3_64bits_withSecret(buffer, bufferSize, XSUM_benchSecretBuf, sizeof(XSUM_benchSecretBuf));
}
static XSUM_U32 XSUM_wrapXXH3_128b(const void* buffer, size_t bufferSize, XSUM_U32 seed)
{
    (void)seed;
    return (XSUM_U32)(XXH3_128bits(buffer, bufferSize).low64);
}
static XSUM_U32 XSUM_wrapXXH3_128b_seeded(const void* buffer, size_t bufferSize, XSUM_U32 seed)
{
    return (XSUM_U32)(XXH3_128bits_withSeed(buffer, bufferSize, seed).low64);
}
static XSUM_U32 XSUM_wrapXXH3_128b_secret(const void* buffer, size_t bufferSize, XSUM_U32 seed)
{
    (void)seed;
    return (XSUM_U32)(XXH3_128bits_withSecret(buffer, bufferSize, XSUM_benchSecretBuf, sizeof(XSUM_benchSecretBuf)).low64);
}
static XSUM_U32 XSUM_wrapXXH3_stream(const void* buffer, size_t bufferSize, XSUM_U32 seed)
{
    XXH3_state_t state;
    (void)seed;
    XXH3_64bits_reset(&state);
    XXH3_64bits_update(&state, buffer, bufferSize);
    return (XSUM_U32)XXH3_64bits_digest(&state);
}
static XSUM_U32 XSUM_wrapXXH3_stream_seeded(const void* buffer, size_t bufferSize, XSUM_U32 seed)
{
    XXH3_state_t state;
    XXH3_INITSTATE(&state);
    XXH3_64bits_reset_withSeed(&state, (XXH64_hash_t)seed);
    XXH3_64bits_update(&state, buffer, bufferSize);
    return (XSUM_U32)XXH3_64bits_digest(&state);
}
static XSUM_U32 XSUM_wrapXXH128_stream(const void* buffer, size_t bufferSize, XSUM_U32 seed)
{
    XXH3_state_t state;
    (void)seed;
    XXH3_128bits_reset(&state);
    XXH3_128bits_update(&state, buffer, bufferSize);
    return (XSUM_U32)(XXH3_128bits_digest(&state).low64);
}
static XSUM_U32 XSUM_wrapXXH128_stream_seeded(const void* buffer, size_t bufferSize, XSUM_U32 seed)
{
    XXH3_state_t state;
    XXH3_INITSTATE(&state);
    XXH3_128bits_reset_withSeed(&state, (XXH64_hash_t)seed);
    XXH3_128bits_update(&state, buffer, bufferSize);
    return (XSUM_U32)(XXH3_128bits_digest(&state).low64);
}

typedef struct {
    const char* name;
    XSUM_hashFunction func;
} XSUM_hashInfo;

static const XSUM_hashInfo XSUM_hashesToBench[] = {
    { "XXH32",                &XSUM_wrapXXH32 },
    { "XXH64",                &XSUM_wrapXXH64 },
    { "XXH3_64b",             &XSUM_wrapXXH3_64b },
    { "XXH3_64b w/seed",      &XSUM_wrapXXH3_64b_seeded },
    { "XXH3_64b w/secret",    &XSUM_wrapXXH3_64b_secret },
    { "XXH128",               &XSUM_wrapXXH3_128b },
    { "XXH128 w/seed",        &XSUM_wrapXXH3_128b_seeded },
    { "XXH128 w/secret",      &XSUM_wrapXXH3_128b_secret },
    { "XXH3_stream",          &XSUM_wrapXXH3_stream },
    { "XXH3_stream w/seed",   &XSUM_wrapXXH3_stream_seeded },
    { "XXH128_stream",        &XSUM_wrapXXH128_stream },
    { "XXH128_stream w/seed", &XSUM_wrapXXH128_stream_seeded },
    /* Add any extra hashes you want to bench here */
};

#define XSUM_NB_HASHFUNC ((int)(sizeof(XSUM_hashesToBench)/sizeof(XSUM_hashesToBench[0])))

#define XSUM_NB_TESTFUNC (1 + 2 * XSUM_NB_HASHFUNC)
static char XSUM_testIDs[XSUM_NB_TESTFUNC] = { 0 };
static const char XSUM_kDefaultTestIDs[XSUM_NB_TESTFUNC] = { 0,
        1 /*XXH32*/, 0,
        1 /*XXH64*/, 0,
        1 /*XXH3*/, 0, 0, 0, 0, 0,
        1 /*XXH128*/
        /* zero fill the rest */
};

#define XSUM_HASHNAME_MAX 29
static void XSUM_benchHash(XSUM_hashFunction h, const char* hName, int testID,
                          const void* buffer, size_t bufferSize)
{
    XSUM_U32 nbh_perIteration = (XSUM_U32)((300 MB) / (bufferSize+1)) + 1;  /* first iteration conservatively aims for 300 MB/s */
    unsigned iterationNb, nbIterations = XSUM_nbIterations + !XSUM_nbIterations /* min 1 */;
    double fastestH = 100000000.;
    assert(XSUM_HASHNAME_MAX > 2);
    XSUM_logVerbose(2, "\r%80s\r", "");       /* Clean display line */

    for (iterationNb = 1; iterationNb <= nbIterations; iterationNb++) {
        XSUM_U32 r=0;
        UTIL_time_t cStart;

        XSUM_logVerbose(2, "%2u-%-*.*s : %10u ->\r",
                        iterationNb,
                        XSUM_HASHNAME_MAX, XSUM_HASHNAME_MAX, hName,
                        (unsigned)bufferSize);
        UTIL_waitForNextTick(); /* Wait for start of timer tick */
        cStart = UTIL_getTime();

        {   XSUM_U32 u;
            for (u=0; u<nbh_perIteration; u++)
                r += h(buffer, bufferSize, u);
        }
        if (r==0) XSUM_logVerbose(3,".\r");  /* do something with r to defeat compiler "optimizing" hash away */

        {   PTime const nbTicks = UTIL_clockSpanNano(cStart);
            double const ticksPerHash = ((double)nbTicks / XSUM_TIMELOOP) / nbh_perIteration;
            /*
             * clock() is the only decent portable timer, but it isn't very
             * precise. UTIL_getTime and UTIL_clockSpanNano may use these.
             *
             * Sometimes, this lack of precision is enough that the benchmark
             * finishes before there are enough ticks to get a meaningful result.
             *
             * For example, on a Core 2 Duo (without any sort of Turbo Boost),
             * the imprecise timer caused peculiar results like so:
             *
             *    XXH3_64b                   4800.0 MB/s // conveniently even
             *    XXH3_64b unaligned         4800.0 MB/s
             *    XXH3_64b seeded            9600.0 MB/s // magical 2x speedup?!
             *    XXH3_64b seeded unaligned  4800.0 MB/s
             *
             * If we sense a suspiciously low number of ticks, we increase the
             * iterations until we can get something meaningful.
             */
            if (nbTicks < XSUM_TIMELOOP_MIN) {
                /* Not enough time spent in benchmarking, risk of rounding bias */
                if (nbTicks == 0) { /* faster than resolution timer */
                    nbh_perIteration *= 100;
                } else {
                    /*
                     * update nbh_perIteration so that the next round lasts
                     * approximately 1 second.
                     */
                    double nbh_perSecond = (1 / ticksPerHash) + 1;
                    if (nbh_perSecond > (double)(4000U<<20)) nbh_perSecond = (double)(4000U<<20);   /* avoid overflow */
                    nbh_perIteration = (XSUM_U32)nbh_perSecond;
                }
                /* XSUM_nbIterations==0 => quick evaluation, no claim of accuracy */
                if (XSUM_nbIterations>0) {
                    iterationNb--;   /* new round for a more accurate speed evaluation */
                    continue;
                }
            }
            if (ticksPerHash < fastestH) fastestH = ticksPerHash;
            if (fastestH>0.) { /* avoid div by zero */
                XSUM_logVerbose(2, "%2u-%-*.*s : %10u -> %8.0f it/s (%7.1f MB/s) \r",
                            iterationNb,
                            XSUM_HASHNAME_MAX, XSUM_HASHNAME_MAX, hName,
                            (unsigned)bufferSize,
                            (double)1 / fastestH,
                            ((double)bufferSize / (1 MB)) / fastestH);
        }   }
        {   double nbh_perSecond = (1 / fastestH) + 1;
            if (nbh_perSecond > (double)(4000U<<20)) nbh_perSecond = (double)(4000U<<20);   /* avoid overflow */
            nbh_perIteration = (XSUM_U32)nbh_perSecond;
        }
    }
    XSUM_logVerbose(1, "%2i#%-*.*s : %10u -> %8.0f it/s (%7.1f MB/s) \n",
                    testID,
                    XSUM_HASHNAME_MAX, XSUM_HASHNAME_MAX, hName,
                    (unsigned)bufferSize,
                    (double)1 / fastestH,
                    ((double)bufferSize / (1 MB)) / fastestH);
    if (XSUM_logLevel<1)
        XSUM_logVerbose(0, "%u, ", (unsigned)((double)1 / fastestH));
}


/*!
 * XSUM_benchMem():
 * buffer: Must be 16-byte aligned.
 * The real allocated size of buffer is supposed to be >= (bufferSize+3).
 * returns: 0 on success, 1 if error (invalid mode selected)
 */
static void XSUM_benchMem(const void* buffer, size_t bufferSize)
{
    assert((((size_t)buffer) & 15) == 0);  /* ensure alignment */
    XSUM_fillTestBuffer(XSUM_benchSecretBuf, sizeof(XSUM_benchSecretBuf));
    {   int i;
        for (i = 1; i < XSUM_NB_TESTFUNC; i++) {
            int const hashFuncID = (i-1) / 2;
            assert(XSUM_hashesToBench[hashFuncID].name != NULL);
            if (XSUM_testIDs[i] == 0) continue;
            /* aligned */
            if ((i % 2) == 1) {
                XSUM_benchHash(XSUM_hashesToBench[hashFuncID].func, XSUM_hashesToBench[hashFuncID].name, i, buffer, bufferSize);
            }
            /* unaligned */
            if ((i % 2) == 0) {
                /* Append "unaligned". */
                char* const hashNameBuf = XSUM_strcatDup(XSUM_hashesToBench[hashFuncID].name, " unaligned");
                assert(hashNameBuf != NULL);
                XSUM_benchHash(XSUM_hashesToBench[hashFuncID].func, hashNameBuf, i, ((const char*)buffer)+3, bufferSize);
                free(hashNameBuf);
            }
    }   }
}

static size_t XSUM_selectBenchedSize(const char* fileName)
{
    XSUM_U64 const inFileSize = XSUM_getFileSize(fileName);
    size_t benchedSize = (size_t) XSUM_findMaxMem(inFileSize);
    if ((XSUM_U64)benchedSize > inFileSize) benchedSize = (size_t)inFileSize;
    if (benchedSize < inFileSize) {
        XSUM_log("Not enough memory for '%s' full size; testing %i MB only...\n", fileName, (int)(benchedSize>>20));
    }
    return benchedSize;
}


XSUM_API int XSUM_benchFiles(char*const* fileNamesTable, int nbFiles)
{
    int fileIdx;
    for (fileIdx=0; fileIdx<nbFiles; fileIdx++) {
        const char* const inFileName = fileNamesTable[fileIdx];
        assert(inFileName != NULL);

        {   FILE* const inFile = XSUM_fopen( inFileName, "rb" );
            size_t const benchedSize = XSUM_selectBenchedSize(inFileName);
            char* const buffer = (char*)calloc(benchedSize+16+3, 1);
            void* const alignedBuffer = (buffer+15) - (((size_t)(buffer+15)) & 0xF);  /* align on next 16 bytes */

            /* Checks */
            if (inFile==NULL){
                XSUM_log("Error: Could not open '%s': %s.\n", inFileName, strerror(errno));
                free(buffer);
                exit(11);
            }
            if(!buffer) {
                XSUM_log("\nError: Out of memory.\n");
                fclose(inFile);
                exit(12);
            }

            /* Fill input buffer */
            {   size_t const readSize = fread(alignedBuffer, 1, benchedSize, inFile);
                fclose(inFile);
                if(readSize != benchedSize) {
                    XSUM_log("\nError: Could not read '%s': %s.\n", inFileName, strerror(errno));
                    free(buffer);
                    exit(13);
            }   }

            /* bench */
            XSUM_benchMem(alignedBuffer, benchedSize);

            free(buffer);
    }   }
    return 0;
}


XSUM_API int XSUM_benchInternal(size_t keySize)
{
    void* const buffer = calloc(keySize+16+3, 1);
    if (buffer == NULL) {
        XSUM_log("\nError: Out of memory.\n");
        exit(12);
    }

    {   const void* const alignedBuffer = ((char*)buffer+15) - (((size_t)((char*)buffer+15)) & 0xF);  /* align on next 16 bytes */

        /* bench */
        XSUM_logVerbose(1, "Sample of ");
        if (keySize > 10 KB) {
            XSUM_logVerbose(1, "%u KB", (unsigned)(keySize >> 10));
        } else {
            XSUM_logVerbose(1, "%u bytes", (unsigned)keySize);
        }
        XSUM_logVerbose(1, "...        \n");

        XSUM_benchMem(alignedBuffer, keySize);
        free(buffer);
    }
    return 0;
}

XSUM_API void XSUM_setBenchID(XSUM_U32 id, int fill)
{
    if (fill) {
        if (id == 0) {
            memcpy(XSUM_testIDs, XSUM_kDefaultTestIDs, sizeof(XSUM_testIDs));
        } else if (id == 99 || XSUM_testIDs[0] == 99) {
            memset(XSUM_testIDs, 1, sizeof(XSUM_testIDs));
        }
    } else {
        if (id < XSUM_NB_TESTFUNC) {
            XSUM_testIDs[id] |= 1; /* will not overwrite 99 */
        } else {
            XSUM_testIDs[0] = 99;
        }
    }
}

XSUM_API void XSUM_setBenchIter(XSUM_U32 iter)
{
    XSUM_nbIterations = iter;
}

#undef KB
#undef MB
#undef GB

#endif /* !XSUM_NO_BENCH */
