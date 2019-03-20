/*
*  Main program to benchmark hash functions
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


/* ===  dependencies  === */

#include <stdio.h>       /* printf */
#include <limits.h>      /* INT_MAX */
#include "bhDisplay.h"   /* bench_x */


/* ===  defines list of hashes `hashCandidates` and NB_HASHES  *** */

#include "hashes.h"


/* ===  parse command line  === */

#undef NDEBUG
#include <assert.h>


/*! readIntFromChar() :
 *  allows and interprets K, KB, KiB, M, MB and MiB suffix.
 *  Will also modify `*stringPtr`, advancing it to position where it stopped reading.
 */
static int readIntFromChar(const char** stringPtr)
{
    static int const max = (INT_MAX / 10) - 1;
    int result = 0;
    while ((**stringPtr >='0') && (**stringPtr <='9')) {
        assert(result < max);
        result *= 10;
        result += (unsigned)(**stringPtr - '0');
        (*stringPtr)++ ;
    }
    if ((**stringPtr=='K') || (**stringPtr=='M')) {
        int const maxK = INT_MAX >> 10;
        assert(result < maxK);
        result <<= 10;
        if (**stringPtr=='M') {
            assert(result < maxK);
            result <<= 10;
        }
        (*stringPtr)++;  /* skip `K` or `M` */
        if (**stringPtr=='i') (*stringPtr)++;
        if (**stringPtr=='B') (*stringPtr)++;
    }
    return result;
}


/** longCommandWArg() :
 *  check if *stringPtr is the same as longCommand.
 *  If yes, @return 1 and advances *stringPtr to the position which immediately follows longCommand.
 * @return 0 and doesn't modify *stringPtr otherwise.
 */
static int longCommandWArg(const char** stringPtr, const char* longCommand)
{
    size_t const comSize = strlen(longCommand);
    int const result = !strncmp(*stringPtr, longCommand, comSize);
    if (result) *stringPtr += comSize;
    return result;
}


/* ===   default values - can be redefined at compilation time   === */

#ifndef SMALL_SIZE_MIN_DEFAULT
#  define SMALL_SIZE_MIN_DEFAULT   1
#endif
#ifndef SMALL_SIZE_MAX_DEFAULT
#  define SMALL_SIZE_MAX_DEFAULT 128
#endif
#ifndef LARGE_SIZELOG_MIN_DEFAULT
#  define LARGE_SIZELOG_MIN_DEFAULT   9
#endif
#ifndef LARGE_SIZELOG_MAX_DEFAULT
#  define LARGE_SIZELOG_MAX_DEFAULT  28
#endif



int main(int argc, const char** argv)
{
    int nb_h_test = NB_HASHES;
    int largeTest_log_min = LARGE_SIZELOG_MIN_DEFAULT;
    int largeTest_log_max = LARGE_SIZELOG_MAX_DEFAULT;
    size_t smallTest_size_min = SMALL_SIZE_MIN_DEFAULT;
    size_t smallTest_size_max = SMALL_SIZE_MAX_DEFAULT;

    int arg_nb;
    for (arg_nb = 1; arg_nb < argc; arg_nb++) {
        const char** arg = argv + arg_nb;
        if (longCommandWArg(arg, "--n=")) { nb_h_test = readIntFromChar(arg); continue; }
        if (longCommandWArg(arg, "--minl=")) { largeTest_log_min = readIntFromChar(arg); continue; }
        if (longCommandWArg(arg, "--maxl=")) { largeTest_log_max = readIntFromChar(arg); continue; }
        if (longCommandWArg(arg, "--mins=")) { smallTest_size_min = (size_t)readIntFromChar(arg); continue; }
        if (longCommandWArg(arg, "--maxs=")) { smallTest_size_max = (size_t)readIntFromChar(arg); continue; }
    }

    printf(" ===  benchmarking %i hash functions  === \n", nb_h_test);
    if (largeTest_log_max >= largeTest_log_min) {
        bench_largeInput(hashCandidates, nb_h_test, largeTest_log_min, largeTest_log_max);
    }
    if (smallTest_size_max >= smallTest_size_min) {
        bench_throughput_smallInputs(hashCandidates, nb_h_test, smallTest_size_min, smallTest_size_max);
        bench_throughput_randomInputLength(hashCandidates, nb_h_test, smallTest_size_min, smallTest_size_max);
        bench_latency_smallInputs(hashCandidates, nb_h_test, smallTest_size_min, smallTest_size_max);
        bench_latency_randomInputLength(hashCandidates, nb_h_test, smallTest_size_min, smallTest_size_max);
    }

    return 0;
}
