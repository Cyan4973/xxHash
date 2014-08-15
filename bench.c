/*
bench.c - Demo program to benchmark open-source algorithm
Copyright (C) Yann Collet 2012-2014

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

You can contact the author at :
- Blog homepage : http://fastcompression.blogspot.com/
- Discussion group : https://groups.google.com/forum/?fromgroups#!forum/lz4c
*/

//**************************************
// Compiler Options
//**************************************
// Visual warning messages (must be first line)
#define _CRT_SECURE_NO_WARNINGS

// Under Linux at least, pull in the *64 commands
#define _LARGEFILE64_SOURCE


//**************************************
// Includes
//**************************************
#include <stdlib.h>     // malloc
#include <stdio.h>      // fprintf, fopen, ftello64
#include <sys/timeb.h>  // timeb
#include <sys/types.h>  // stat64
#include <sys/stat.h>   // stat64


//**************************************
// Compiler specifics
//**************************************
#if !defined(S_ISREG)
#  define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
#endif

//**************************************
// Hash Functions to test
//**************************************
#include "xxhash.h"
#define DEFAULTHASH XXH32
#define HASH0 XXH32

// Making a wrapper to fit into the 32 bit api
unsigned int XXH64_32(const void* key, unsigned int len, unsigned int seed)
{
	unsigned long long hash = XXH64(key, len, seed);
	return (unsigned int)(hash & 0xFFFFFFFF);
}
#define HASH1 XXH64_32


//**************************************
// Basic Types
//**************************************
#if defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L   // C99
# include <stdint.h>
  typedef  uint8_t BYTE;
  typedef uint16_t U16;
  typedef uint32_t U32;
  typedef  int32_t S32;
  typedef uint64_t U64;
#else
  typedef unsigned char       BYTE;
  typedef unsigned short      U16;
  typedef unsigned int        U32;
  typedef   signed int        S32;
  typedef unsigned long long  U64;
#endif


//**************************************
// Constants
//**************************************
#define PROGRAM_NAME "xxHash tester"
#define PROGRAM_VERSION ""
#define COMPILED __DATE__
#define AUTHOR "Yann Collet"
#define WELCOME_MESSAGE "*** %s %i-bits %s, by %s (%s) ***\n", PROGRAM_NAME, (int)(sizeof(void*)*8), PROGRAM_VERSION, AUTHOR, COMPILED

#define NBLOOPS    3           // Default number of benchmark iterations
#define TIMELOOP   2500        // Minimum timing per iteration

#define KB *(1U<<10)
#define MB *(1U<<20)
#define GB *(1U<<30)

#define MAX_MEM    (2 GB - 64 MB)

#define PRIME 2654435761U

//**************************************
// Local structures
//**************************************
struct hashFunctionPrototype
{
    unsigned int (*hashFunction)(const void*, unsigned int, unsigned int);
};


//**************************************
// MACRO
//**************************************
#define DISPLAY(...) fprintf(stderr, __VA_ARGS__)



//**************************************
// Benchmark Parameters
//**************************************
static int nbIterations = NBLOOPS;

void BMK_SetNbIterations(int nbLoops)
{
    nbIterations = nbLoops;
    DISPLAY("- %i iterations-", nbIterations);
}



//*********************************************************
// Benchmark Functions
//*********************************************************

static int BMK_GetMilliStart(void)
{
    // Supposed to be portable
    // Rolls over every ~ 12.1 days (0x100000/24/60/60)
    // Use GetMilliSpan to correct for rollover
    struct timeb tb;
    int nCount;
    ftime( &tb );
    nCount = tb.millitm + (tb.time & 0xfffff) * 1000;
    return nCount;
}


static int BMK_GetMilliSpan( int nTimeStart )
{
    int nSpan = BMK_GetMilliStart() - nTimeStart;
    if ( nSpan < 0 )
        nSpan += 0x100000 * 1000;
    return nSpan;
}


static size_t BMK_findMaxMem(U64 requestedMem)
{
    size_t step = (64 MB);
    size_t allocatedMemory;
    BYTE* testmem=NULL;

    requestedMem += 3*step;
    requestedMem -= (size_t)requestedMem & (step-1);
    if (requestedMem > MAX_MEM) requestedMem = MAX_MEM;
    allocatedMemory = (size_t)requestedMem;

    while (!testmem)
    {
        allocatedMemory -= step;
        testmem = (BYTE*) malloc((size_t)allocatedMemory);
    }
    free (testmem);

    return (size_t) (allocatedMemory - step);
}


static U64 BMK_GetFileSize(char* infilename)
{
    int r;
#if defined(_MSC_VER)
    struct _stat64 statbuf;
    r = _stat64(infilename, &statbuf);
#else
    struct stat statbuf;
    r = stat(infilename, &statbuf);
#endif
    if (r || !S_ISREG(statbuf.st_mode)) return 0;   // No good...
    return (U64)statbuf.st_size;
}


int BMK_benchFile(char** fileNamesTable, int nbFiles, int selection)
{
    int fileIdx=0;
    struct hashFunctionPrototype hashP;
    U32 hashResult=0;

    U64 totals = 0;
    double totalc = 0.;


    // Init
    switch (selection)
    {
#ifdef HASH0
    case 0 : hashP.hashFunction = HASH0; break;
#endif
#ifdef HASH1
    case 1 : hashP.hashFunction = HASH1; break;
#endif
#ifdef HASH2
    case 2 : hashP.hashFunction = HASH2; break;
#endif
    default: hashP.hashFunction = DEFAULTHASH;
    }

    DISPLAY("Selected fn %d", selection);

    // Loop for each file
    while (fileIdx<nbFiles)
    {
        FILE*  inFile;
        char*  inFileName;
        U64    inFileSize;
        size_t benchedSize;
        size_t readSize;
        char*  buffer;
        char*  alignedBuffer;

        // Check file existence
        inFileName = fileNamesTable[fileIdx++];
        inFile = fopen( inFileName, "rb" );
        if (inFile==NULL)
        {
            DISPLAY( "Pb opening %s\n", inFileName);
            return 11;
        }

        // Memory allocation & restrictions
        inFileSize = BMK_GetFileSize(inFileName);
        benchedSize = (size_t) BMK_findMaxMem(inFileSize);
        if ((U64)benchedSize > inFileSize) benchedSize = (size_t)inFileSize;
        if (benchedSize < inFileSize)
        {
            DISPLAY("Not enough memory for '%s' full size; testing %i MB only...\n", inFileName, (int)(benchedSize>>20));
        }

        buffer = (char*)malloc((size_t )benchedSize+16);
        if(!buffer)
        {
            DISPLAY("\nError: not enough memory!\n");
            fclose(inFile);
            return 12;
        }
        alignedBuffer = (buffer+15) - (((size_t)(buffer+15)) & 0xF);   // align on next 16 bytes boundaries

        // Fill input buffer
        DISPLAY("\rLoading %s...        \n", inFileName);
        readSize = fread(alignedBuffer, 1, benchedSize, inFile);
        fclose(inFile);

        if(readSize != benchedSize)
        {
            DISPLAY("\nError: problem reading file '%s' !!    \n", inFileName);
            free(buffer);
            return 13;
        }


        // Bench XXH32
        {
            int interationNb;
            double fastestC = 100000000.;

            DISPLAY("\r%79s\r", "");       // Clean display line
            for (interationNb = 1; interationNb <= nbIterations; interationNb++)
            {
                int nbHashes = 0;
                int milliTime;

                DISPLAY("%1i-%-14.14s : %10i ->\r", interationNb, "XXH32", (int)benchedSize);

                // Hash loop
                milliTime = BMK_GetMilliStart();
                while(BMK_GetMilliStart() == milliTime);
                milliTime = BMK_GetMilliStart();
                while(BMK_GetMilliSpan(milliTime) < TIMELOOP)
                {
                    int i;
                    for (i=0; i<100; i++)
                    {
                        hashResult = hashP.hashFunction(alignedBuffer, (int)benchedSize, 0);
                        nbHashes++;
                    }
                }
                milliTime = BMK_GetMilliSpan(milliTime);
                if ((double)milliTime < fastestC*nbHashes) fastestC = (double)milliTime/nbHashes;
                DISPLAY("%1i-%-14.14s : %10i -> %7.1f MB/s\r", interationNb, "XXH32", (int)benchedSize, (double)benchedSize / fastestC / 1000.);
            }
            DISPLAY("%-16.16s : %10i -> %7.1f MB/s   0x%08X\n", "XXH32", (int)benchedSize, (double)benchedSize / fastestC / 1000., hashResult);

            totals += benchedSize;
            totalc += fastestC;
        }

        // Bench Unaligned XXH32
        {
            int interationNb;
            double fastestC = 100000000.;

            DISPLAY("\r%79s\r", "");       // Clean display line
            for (interationNb = 1; (interationNb <= nbIterations) && ((benchedSize>1)); interationNb++)
            {
                int nbHashes = 0;
                int milliTime;

                DISPLAY("%1i-%-14.14s : %10i ->\r", interationNb, "(unaligned)", (int)benchedSize);
                // Hash loop
                milliTime = BMK_GetMilliStart();
                while(BMK_GetMilliStart() == milliTime);
                milliTime = BMK_GetMilliStart();
                while(BMK_GetMilliSpan(milliTime) < TIMELOOP)
                {
                    int i;
                    for (i=0; i<100; i++)
                    {
                        hashResult = hashP.hashFunction(alignedBuffer+1, (int)benchedSize-1, 0);
                        nbHashes++;
                    }
                }
                milliTime = BMK_GetMilliSpan(milliTime);
                if ((double)milliTime < fastestC*nbHashes) fastestC = (double)milliTime/nbHashes;
                DISPLAY("%1i-%-14.14s : %10i -> %7.1f MB/s\r", interationNb, "XXH32 (unaligned)", (int)(benchedSize-1), (double)(benchedSize-1) / fastestC / 1000.);
            }
            DISPLAY("%-16.16s : %10i -> %7.1f MB/s \n", "XXH32 (unaligned)", (int)benchedSize-1, (double)(benchedSize-1) / fastestC / 1000.);
        }

        // Bench XXH64
        {
            int interationNb;
            double fastestC = 100000000.;
            unsigned long long h64 = 0;

            DISPLAY("\r%79s\r", "");       // Clean display line
            for (interationNb = 1; interationNb <= nbIterations; interationNb++)
            {
                int nbHashes = 0;
                int milliTime;

                DISPLAY("%1i-%-14.14s : %10i ->\r", interationNb, "XXH64", (int)benchedSize);

                // Hash loop
                milliTime = BMK_GetMilliStart();
                while(BMK_GetMilliStart() == milliTime);
                milliTime = BMK_GetMilliStart();
                while(BMK_GetMilliSpan(milliTime) < TIMELOOP)
                {
                    int i;
                    for (i=0; i<100; i++)
                    {
                        h64 = XXH64(alignedBuffer, (int)benchedSize, 0);
                        nbHashes++;
                    }
                }
                milliTime = BMK_GetMilliSpan(milliTime);
                if ((double)milliTime < fastestC*nbHashes) fastestC = (double)milliTime/nbHashes;
                DISPLAY("%1i-%-14.14s : %10i -> %7.1f MB/s\r", interationNb, "XXH64", (int)benchedSize, (double)benchedSize / fastestC / 1000.);
            }
            DISPLAY("%-16.16s : %10i -> %7.1f MB/s   0x%08X%08X\n", "XXH64", (int)benchedSize, (double)benchedSize / fastestC / 1000., (U32)(h64>>32), (U32)(h64));

            totals += benchedSize;
            totalc += fastestC;
        }

        free(buffer);
    }

    if (nbFiles > 1)
        printf("%-16.16s :%11llu -> %7.1f MB/s\n", "  TOTAL", (long long unsigned int)totals, (double)totals/totalc/1000.);

    return 0;
}



static void BMK_checkResult(U32 r1, U32 r2)
{
    static int nbTests = 1;

    if (r1==r2) DISPLAY("\rTest%3i : %08X == %08X   ok   ", nbTests, r1, r2);
    else
    {
        DISPLAY("\rERROR : Test%3i : %08X <> %08X   !!!!!   \n", nbTests, r1, r2);
        exit(1);
    }
    nbTests++;
}


static void BMK_checkResult64(U64 r1, U64 r2)
{
    static int nbTests = 1;

    if (r1!=r2)
    {
        DISPLAY("\rERROR : Test%3i : 64-bits values non equals   !!!!!   \n", nbTests);
        DISPLAY("\r %08X%08X != %08X%08X \n", (U32)(r1>>32), (U32)r1, (U32)(r2<<32), (U32)r2);
        exit(1);
    }
    nbTests++;
}


static void BMK_testSequence64(void* sentence, int len, U64 seed, U64 Nresult)
{
    U64 Dresult;
    void* state;
    int index;

    Dresult = XXH64(sentence, len, seed);
    BMK_checkResult64(Dresult, Nresult);

    state = XXH64_init(seed);
    XXH64_update(state, sentence, len);
    Dresult = XXH64_digest(state);
    BMK_checkResult64(Dresult, Nresult);

    state = XXH64_init(seed);
    for (index=0; index<len; index++) XXH64_update(state, ((char*)sentence)+index, 1);
    Dresult = XXH64_digest(state);
    BMK_checkResult64(Dresult, Nresult);
}


static void BMK_testSequence(void* sentence, int len, U32 seed, U32 Nresult)
{
    U32 Dresult;
    void* state;
    int index;

    Dresult = XXH32(sentence, len, seed);
    BMK_checkResult(Dresult, Nresult);

    state = XXH32_init(seed);
    XXH32_update(state, sentence, len);
    Dresult = XXH32_digest(state);
    BMK_checkResult(Dresult, Nresult);

    state = XXH32_init(seed);
    for (index=0; index<len; index++) XXH32_update(state, ((char*)sentence)+index, 1);
    Dresult = XXH32_digest(state);
    BMK_checkResult(Dresult, Nresult);
}


#define SANITY_BUFFER_SIZE 101
static void BMK_sanityCheck(void)
{
    BYTE sanityBuffer[SANITY_BUFFER_SIZE];
    int i;
    U32 prime = PRIME;

    for (i=0; i<SANITY_BUFFER_SIZE; i++)
    {
        sanityBuffer[i] = (BYTE)(prime>>24);
        prime *= prime;
    }

    BMK_testSequence(NULL,          0, 0,     0x02CC5D05);
    BMK_testSequence(NULL,          0, PRIME, 0x36B78AE7);
    BMK_testSequence(sanityBuffer,  1, 0,     0xB85CBEE5);
    BMK_testSequence(sanityBuffer,  1, PRIME, 0xD5845D64);
    BMK_testSequence(sanityBuffer, 14, 0,     0xE5AA0AB4);
    BMK_testSequence(sanityBuffer, 14, PRIME, 0x4481951D);
    BMK_testSequence(sanityBuffer, SANITY_BUFFER_SIZE, 0,     0x1F1AA412);
    BMK_testSequence(sanityBuffer, SANITY_BUFFER_SIZE, PRIME, 0x498EC8E2);

    BMK_testSequence64(NULL        ,  0, 0,     0xEF46DB3751D8E999ULL);
    BMK_testSequence64(NULL        ,  0, PRIME, 0xAC75FDA2929B17EFULL);
    BMK_testSequence64(sanityBuffer,  1, 0,     0x4FCE394CC88952D8ULL);
    BMK_testSequence64(sanityBuffer,  1, PRIME, 0x739840CB819FA723ULL);
    BMK_testSequence64(sanityBuffer, 14, 0,     0xCFFA8DB881BC3A3DULL);
    BMK_testSequence64(sanityBuffer, 14, PRIME, 0x5B9611585EFCC9CBULL);
    BMK_testSequence64(sanityBuffer, SANITY_BUFFER_SIZE, 0,     0x0EAB543384F878ADULL);
    BMK_testSequence64(sanityBuffer, SANITY_BUFFER_SIZE, PRIME, 0xCAA65939306F1E21ULL);

    DISPLAY("\r%79s\r", "");       // Clean display line
    DISPLAY("Sanity check -- all tests ok\n");
}


//*********************************************************
//  Main
//*********************************************************

int usage(char* exename)
{
    DISPLAY( "Usage :\n");
    DISPLAY( "      %s [arg] filename\n", exename);
    DISPLAY( "Arguments :\n");
    DISPLAY( " -i# : number of iterations \n");
    DISPLAY( " -s# : Function selection [0,1]. Default is 0 \n");
    DISPLAY( " -h  : help (this text)\n");
    return 0;
}


int badusage(char* exename)
{
    DISPLAY("Wrong parameters\n");
    usage(exename);
    return 0;
}


int main(int argc, char** argv)
{
    int i,
        filenamesStart=2;
    char* input_filename=0;
    int fn_selection = 0;

    // Welcome message
    DISPLAY( WELCOME_MESSAGE );

    // Check results are good
    BMK_sanityCheck();

    if (argc<2) { badusage(argv[0]); return 1; }

    for(i=1; i<argc; i++)
    {
        char* argument = argv[i];

        if(!argument) continue;   // Protection if argument empty

        // Select command
        if (argument[0]=='-')
        {
            argument ++;

            // Display help on usage
            if ( argument[0] =='h' ) { usage(argv[0]); return 0; }

            // Modify Nb Iterations (benchmark only)
            if ( argument[0] =='i' ) { int iters = argument[1] - '0'; BMK_SetNbIterations(iters); continue; }

            // select function
            if ( argument[0] =='s' ) { fn_selection = argument[1] - '0'; continue; }
        }

        // first provided filename is input
        if (!input_filename) { input_filename=argument; filenamesStart=i; continue; }

    }

    // No input filename ==> Error
    if(!input_filename) { badusage(argv[0]); return 1; }

    if(fn_selection < 0 || fn_selection > 1) { badusage(argv[0]); return 1; }

    return BMK_benchFile(argv+filenamesStart, argc-filenamesStart, fn_selection);
}
