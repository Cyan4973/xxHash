/*
 * Inlined-xxHash + dispatch test program
 * Validates that "xxh_x86dispatch.h" can be included
 * by a unit which also requests XXH_INLINE_ALL,
 * and that the resulting hashes are still correct.
 * https://github.com/Cyan4973/xxHash/issues/1071
 *
 * Copyright (C) 2025 Yann Collet
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

#include <stdio.h>   /* printf */
#include <stdlib.h>  /* exit */

/* The unit inlines xxHash, and nonetheless employs the dispatcher,
 * which is necessarily provided by a separate unit (xxh_x86dispatch.o).
 * XXH3 symbols are then transparently redirected towards the dispatcher. */
#define XXH_INLINE_ALL
#include "../xxh_x86dispatch.h"

#define SAMPLE_SIZE 2048
#define SECRET_SIZE 192   /* >= XXH3_SECRET_SIZE_MIN */
#define CHUNK_SIZE  111   /* not a multiple of the internal buffer size */
#define PRIME64 0x9E3779B185EBCA8DULL

/* Reference values, generated with the regular (non-inlined, non-dispatched)
 * implementation. The dispatcher must produce the same hashes. */
#define REF_H64            0xb4f8525c05de5f3dULL
#define REF_H64_SEED       0x5c87faa7dbd27308ULL
#define REF_H64_SECRET     0x09562d78c9a60209ULL
#define REF_H128_LOW       0xb4f8525c05de5f3dULL
#define REF_H128_HIGH      0x9d6084b19c6acc09ULL
#define REF_H128_SEED_LOW  0x5c87faa7dbd27308ULL
#define REF_H128_SEED_HIGH 0xc357a8a574a3b552ULL
#define REF_H128_SECR_LOW  0x09562d78c9a60209ULL
#define REF_H128_SECR_HIGH 0x1068d313042d0e1bULL

static void fillSample(unsigned char* buffer, size_t size)
{
    size_t n;
    unsigned rng = 0x9E3779B1U;
    for (n = 0; n < size; n++) {
        rng = (rng * 2654435761U) + 1;
        buffer[n] = (unsigned char)(rng >> 24);
    }
}

static void check64(const char* testName, XXH64_hash_t result, XXH64_hash_t expected)
{
    printf("%-34s : 0x%016llx ", testName, (unsigned long long)result);
    if (result != expected) {
        printf(": FAIL (expected 0x%016llx) \n", (unsigned long long)expected);
        exit(1);
    }
    printf(": OK \n");
}

static void check128(const char* testName, XXH128_hash_t result,
                     XXH64_hash_t expectedLow, XXH64_hash_t expectedHigh)
{
    printf("%-34s : 0x%016llx%016llx ", testName,
           (unsigned long long)result.high64, (unsigned long long)result.low64);
    if ((result.low64 != expectedLow) || (result.high64 != expectedHigh)) {
        printf(": FAIL (expected 0x%016llx%016llx) \n",
               (unsigned long long)expectedHigh, (unsigned long long)expectedLow);
        exit(1);
    }
    printf(": OK \n");
}

int main(void)
{
    unsigned char sample[SAMPLE_SIZE];
    XXH3_state_t* state;
    size_t pos;

    fillSample(sample, SAMPLE_SIZE);
    printf("dispatched from an XXH_INLINE_ALL unit, best implementation = %i \n",
           XXH_featureTest());

    check64("XXH3_64bits", XXH3_64bits(sample, SAMPLE_SIZE), REF_H64);
    check64("XXH3_64bits_withSeed",
            XXH3_64bits_withSeed(sample, SAMPLE_SIZE, PRIME64), REF_H64_SEED);
    check64("XXH3_64bits_withSecret",
            XXH3_64bits_withSecret(sample, SAMPLE_SIZE, sample, SECRET_SIZE), REF_H64_SECRET);

    check128("XXH3_128bits", XXH3_128bits(sample, SAMPLE_SIZE),
             REF_H128_LOW, REF_H128_HIGH);
    check128("XXH3_128bits_withSeed",
             XXH3_128bits_withSeed(sample, SAMPLE_SIZE, PRIME64),
             REF_H128_SEED_LOW, REF_H128_SEED_HIGH);
    check128("XXH3_128bits_withSecret",
             XXH3_128bits_withSecret(sample, SAMPLE_SIZE, sample, SECRET_SIZE),
             REF_H128_SECR_LOW, REF_H128_SECR_HIGH);

    /* streaming: reset() and digest() are inlined,
     * while update() is provided by the dispatcher,
     * hence both must agree on the state's layout */
    state = XXH3_createState();
    if (state == NULL) { printf("not enough memory \n"); return 1; }

    XXH3_64bits_reset(state);
    for (pos = 0; pos < SAMPLE_SIZE; pos += CHUNK_SIZE) {
        size_t const rem = SAMPLE_SIZE - pos;
        XXH3_64bits_update(state, sample + pos, (rem < CHUNK_SIZE) ? rem : CHUNK_SIZE);
    }
    check64("XXH3_64bits streaming", XXH3_64bits_digest(state), REF_H64);

    XXH3_128bits_reset(state);
    for (pos = 0; pos < SAMPLE_SIZE; pos += CHUNK_SIZE) {
        size_t const rem = SAMPLE_SIZE - pos;
        XXH3_128bits_update(state, sample + pos, (rem < CHUNK_SIZE) ? rem : CHUNK_SIZE);
    }
    check128("XXH3_128bits streaming", XXH3_128bits_digest(state),
             REF_H128_LOW, REF_H128_HIGH);

    XXH3_freeState(state);
    printf("all inlined dispatch tests completed successfully \n");
    return 0;
}
