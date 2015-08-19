/*
xxhsum - Command line interface for xxhash algorithms
Copyright (C) Yann Collet 2012-2015

GPL v2 License

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
- xxHash source repository : https://github.com/Cyan4973/xxHash
*/

/*************************************
*  Compiler Options
*************************************/
/* MS Visual */
#if defined(_MSC_VER) || defined(_WIN32)
#  define _CRT_SECURE_NO_WARNINGS   /* removes visual warnings */
#  define BMK_LEGACY_TIMER 1        /* gettimeofday() not supported by MSVC */
#endif

/* Under Linux at least, pull in the *64 commands */
#define _LARGEFILE64_SOURCE


/*************************************
*  Includes
*************************************/
#include <stdlib.h>     /* malloc */
#include <stdio.h>      /* fprintf, fopen, ftello64, fread, stdin, stdout; when present : _fileno */
#include <string.h>     /* strcmp */
#include <sys/types.h>  /* stat64 */
#include <sys/stat.h>   /* stat64 */

#include "xxhash.h"


/*************************************
*  OS-Specific Includes
*************************************/
/* Use ftime() if gettimeofday() is not available on your target */
#if defined(BMK_LEGACY_TIMER)
#  include <sys/timeb.h>   /* timeb, ftime */
#else
#  include <sys/time.h>    /* gettimeofday */
#endif

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(_WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>    /* _O_BINARY */
#  include <io.h>       /* _setmode, _isatty */
#  ifdef __MINGW32__
   int _fileno(FILE *stream);   /* MINGW somehow forgets to include this windows declaration into <stdio.h> */
#  endif
#  define SET_BINARY_MODE(file) _setmode(_fileno(file), _O_BINARY)
#  define IS_CONSOLE(stdStream) _isatty(_fileno(stdStream))
#else
#  include <unistd.h>   /* isatty, STDIN_FILENO */
#  define SET_BINARY_MODE(file)
#  define IS_CONSOLE(stdStream) isatty(STDIN_FILENO)
#endif

#if !defined(S_ISREG)
#  define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
#endif


/*************************************
*  Basic Types
*************************************/
#if defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L   /* C99 */
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

static unsigned BMK_isLittleEndian(void)
{
    const union { U32 i; BYTE c[4]; } one = { 1 };   /* don't use static : performance detrimental  */
    return one.c[0];
}


/**************************************
*  Constants
**************************************/
#define PROGRAM_NAME exename
#define PROGRAM_VERSION ""
static const int g_nbBits = (int)(sizeof(void*)*8);
static const char g_lename[] = "little endian";
static const char g_bename[] = "big endian";
#define ENDIAN_NAME (BMK_isLittleEndian() ? g_lename : g_bename)
#define COMPILED __DATE__
static const char author[] = "Yann Collet";
#define WELCOME_MESSAGE "%s %s (%i-bits %s), by %s (%s) \n", PROGRAM_NAME, PROGRAM_VERSION,  g_nbBits, ENDIAN_NAME, author, COMPILED

#define NBLOOPS    3           /* Default number of benchmark iterations */
#define TIMELOOP   2500        /* Minimum timing per iteration */

#define KB *( 1<<10)
#define MB *( 1<<20)
#define GB *(1U<<30)

#define MAX_MEM    (2 GB - 64 MB)

static const char stdinName[] = "-";


/*************************************
*  Display macros
*************************************/
#define DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define DISPLAYRESULT(...)   fprintf(stdout, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...) if (g_displayLevel>=l) DISPLAY(__VA_ARGS__);
static unsigned g_displayLevel = 1;


/*************************************
*  Local variables
*************************************/
static int g_nbIterations = NBLOOPS;
static int g_fn_selection = 1;    /* required within main() & usage() */
static size_t g_sampleSize = 100 KB;


/*************************************
*  Benchmark Functions
*************************************/
#if defined(BMK_LEGACY_TIMER)

static int BMK_GetMilliStart(void)
{
  /* Based on Legacy ftime()
   * Rolls over every ~ 12.1 days (0x100000/24/60/60)
   * Use GetMilliSpan to correct for rollover */
  struct timeb tb;
  int nCount;
  ftime( &tb );
  nCount = (int) (tb.millitm + (tb.time & 0xfffff) * 1000);
  return nCount;
}

#else

static int BMK_GetMilliStart(void)
{
  /* Based on newer gettimeofday()
   * Use GetMilliSpan to correct for rollover */
  struct timeval tv;
  int nCount;
  gettimeofday(&tv, NULL);
  nCount = (int) (tv.tv_usec/1000 + (tv.tv_sec & 0xfffff) * 1000);
  return nCount;
}

#endif

static int BMK_GetMilliSpan( int nTimeStart )
{
    int nSpan = BMK_GetMilliStart() - nTimeStart;
    if ( nSpan < 0 )
        nSpan += 0x100000 * 1000;
    return nSpan;
}


static size_t BMK_findMaxMem(U64 requiredMem)
{
    size_t step = 64 MB;
    BYTE* testmem=NULL;

    requiredMem = (((requiredMem >> 26) + 1) << 26);
    requiredMem += 2*step;
    if (requiredMem > MAX_MEM) requiredMem = MAX_MEM;

    while (!testmem)
    {
        if (requiredMem > step) requiredMem -= step;
        else requiredMem >>= 1;
        testmem = (BYTE*) malloc ((size_t)requiredMem);
    }
    free (testmem);

    /* keep some space available */
    if (requiredMem > step) requiredMem -= step;
    else requiredMem >>= 1;

    return (size_t)requiredMem;
}


static U64 BMK_GetFileSize(const char* infilename)
{
    int r;
#if defined(_MSC_VER)
    struct _stat64 statbuf;
    r = _stat64(infilename, &statbuf);
#else
    struct stat statbuf;
    r = stat(infilename, &statbuf);
#endif
    if (r || !S_ISREG(statbuf.st_mode)) return 0;   /* No good... */
    return (U64)statbuf.st_size;
}

typedef void (*hashFunction)(const void* buffer, size_t bufferSize);

static void localXXH32(const void* buffer, size_t bufferSize) { XXH32(buffer, bufferSize, 0); }

static void localXXH64(const void* buffer, size_t bufferSize) { XXH64(buffer, bufferSize, 0); }

static void BMK_benchHash(hashFunction h, const char* hName, const void* buffer, size_t bufferSize)
{
    static const int nbh_perloop = 100;
    int iterationNb;
    double fastestH = 100000000.;

    DISPLAY("\r%79s\r", "");       /* Clean display line */
    if (g_nbIterations<1) g_nbIterations=1;
    for (iterationNb = 1; iterationNb <= g_nbIterations; iterationNb++)
    {
        int nbHashes = 0;
        int milliTime;

        DISPLAY("%1i-%-17.17s : %10i ->\r", iterationNb, hName, (int)bufferSize);

        /* Timing loop */
        milliTime = BMK_GetMilliStart();
        while(BMK_GetMilliStart() == milliTime);
        milliTime = BMK_GetMilliStart();
        while(BMK_GetMilliSpan(milliTime) < TIMELOOP)
        {
            int i;
            for (i=0; i<nbh_perloop; i++)
            {
                h(buffer, bufferSize);
            }
            nbHashes += nbh_perloop;
        }
        milliTime = BMK_GetMilliSpan(milliTime);
        if ((double)milliTime < fastestH*nbHashes) fastestH = (double)milliTime/nbHashes;
        DISPLAY("%1i-%-17.17s : %10i -> %7.1f MB/s\r", iterationNb, hName, (int)bufferSize, (double)bufferSize / fastestH / 1000.);
    }
    DISPLAY("%-19.19s : %10i -> %7.1f MB/s  \n", hName, (int)bufferSize, (double)bufferSize / fastestH / 1000.);
}


/* Note : buffer is supposed malloc'ed, hence aligned */
static void BMK_benchMem(const void* buffer, size_t bufferSize)
{
    /* XXH32 bench */
    BMK_benchHash(localXXH32, "XXH32", buffer, bufferSize);

    /* Bench XXH32 on Unaligned input */
    if (bufferSize>1)
        BMK_benchHash(localXXH32, "XXH32 unaligned", ((const char*)buffer)+1, bufferSize-1);

    /* Bench XXH64 */
    BMK_benchHash(localXXH64, "XXH64", buffer, bufferSize);

    /* Bench XXH64 on Unaligned input */
    if (bufferSize>1)
        BMK_benchHash(localXXH64, "XXH64 unaligned", ((const char*)buffer)+1, bufferSize-1);
}


static int BMK_benchFiles(const char** fileNamesTable, int nbFiles)
{
    int fileIdx=0;

    while (fileIdx<nbFiles)
    {
        FILE*  inFile;
        const char* inFileName;
        U64    inFileSize;
        size_t benchedSize;
        size_t readSize;
        char*  buffer;
        char*  alignedBuffer;

        /* Check file existence */
        inFileName = fileNamesTable[fileIdx++];
        inFile = fopen( inFileName, "rb" );
        if ((inFile==NULL) || (inFileName==NULL))
        {
            DISPLAY( "Pb opening %s\n", inFileName);
            return 11;
        }

        /* Memory allocation & restrictions */
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
        alignedBuffer = (buffer+15) - (((size_t)(buffer+15)) & 0xF);   /* align on next 16 bytes boundaries */

        /* Fill input buffer */
        DISPLAY("\rLoading %s...        \n", inFileName);
        readSize = fread(alignedBuffer, 1, benchedSize, inFile);
        fclose(inFile);

        if(readSize != benchedSize)
        {
            DISPLAY("\nError: problem reading file '%s' !!    \n", inFileName);
            free(buffer);
            return 13;
        }

        /* bench */
        BMK_benchMem(alignedBuffer, benchedSize);

        free(buffer);
    }

    return 0;
}



static int BMK_benchInternal(void)
{
    const size_t benchedSize = g_sampleSize;
    void*  buffer;

    buffer = malloc(benchedSize);
    if(!buffer)
    {
        DISPLAY("\nError: not enough memory!\n");
        return 12;
    }

    /* bench */
    DISPLAY("\rSample of %u KB...        \n", (U32)(benchedSize >> 10));
    BMK_benchMem(buffer, benchedSize);

    free(buffer);

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
        DISPLAY("\r %08X%08X != %08X%08X \n", (U32)(r1>>32), (U32)r1, (U32)(r2>>32), (U32)r2);
        exit(1);
    }
    nbTests++;
}


static void BMK_testSequence64(void* sentence, int len, U64 seed, U64 Nresult)
{
    U64 Dresult;
    XXH64_state_t state;
    int index;

    Dresult = XXH64(sentence, len, seed);
    BMK_checkResult64(Dresult, Nresult);

    XXH64_reset(&state, seed);
    XXH64_update(&state, sentence, len);
    Dresult = XXH64_digest(&state);
    BMK_checkResult64(Dresult, Nresult);

    XXH64_reset(&state, seed);
    for (index=0; index<len; index++) XXH64_update(&state, ((char*)sentence)+index, 1);
    Dresult = XXH64_digest(&state);
    BMK_checkResult64(Dresult, Nresult);
}


static void BMK_testSequence(const void* sequence, size_t len, U32 seed, U32 Nresult)
{
    U32 Dresult;
    XXH32_state_t state;
    size_t index;

    Dresult = XXH32(sequence, len, seed);
    BMK_checkResult(Dresult, Nresult);

    XXH32_reset(&state, seed);
    XXH32_update(&state, sequence, len);
    Dresult = XXH32_digest(&state);
    BMK_checkResult(Dresult, Nresult);

    XXH32_reset(&state, seed);
    for (index=0; index<len; index++) XXH32_update(&state, ((const char*)sequence)+index, 1);
    Dresult = XXH32_digest(&state);
    BMK_checkResult(Dresult, Nresult);
}


#define SANITY_BUFFER_SIZE 101
static void BMK_sanityCheck(void)
{
    BYTE sanityBuffer[SANITY_BUFFER_SIZE];
    int i;
    static const U32 prime = 2654435761U;
    U32 byteGen = prime;


    for (i=0; i<SANITY_BUFFER_SIZE; i++)
    {
        sanityBuffer[i] = (BYTE)(byteGen>>24);
        byteGen *= byteGen;
    }

    BMK_testSequence(NULL,          0, 0,     0x02CC5D05);
    BMK_testSequence(NULL,          0, prime, 0x36B78AE7);
    BMK_testSequence(sanityBuffer,  1, 0,     0xB85CBEE5);
    BMK_testSequence(sanityBuffer,  1, prime, 0xD5845D64);
    BMK_testSequence(sanityBuffer, 14, 0,     0xE5AA0AB4);
    BMK_testSequence(sanityBuffer, 14, prime, 0x4481951D);
    BMK_testSequence(sanityBuffer, SANITY_BUFFER_SIZE, 0,     0x1F1AA412);
    BMK_testSequence(sanityBuffer, SANITY_BUFFER_SIZE, prime, 0x498EC8E2);

    BMK_testSequence64(NULL        ,  0, 0,     0xEF46DB3751D8E999ULL);
    BMK_testSequence64(NULL        ,  0, prime, 0xAC75FDA2929B17EFULL);
    BMK_testSequence64(sanityBuffer,  1, 0,     0x4FCE394CC88952D8ULL);
    BMK_testSequence64(sanityBuffer,  1, prime, 0x739840CB819FA723ULL);
    BMK_testSequence64(sanityBuffer, 14, 0,     0xCFFA8DB881BC3A3DULL);
    BMK_testSequence64(sanityBuffer, 14, prime, 0x5B9611585EFCC9CBULL);
    BMK_testSequence64(sanityBuffer, SANITY_BUFFER_SIZE, 0,     0x0EAB543384F878ADULL);
    BMK_testSequence64(sanityBuffer, SANITY_BUFFER_SIZE, prime, 0xCAA65939306F1E21ULL);

    DISPLAY("\r%79s\r", "");       /* Clean display line */
    DISPLAYLEVEL(2, "Sanity check -- all tests ok\n");
}


static void BMK_display_BigEndian(const void* ptr, size_t length)
{
    const BYTE* p = (const BYTE*)ptr;
    size_t index = BMK_isLittleEndian() ? length-1 : 0;
    int incr = BMK_isLittleEndian() ? -1 : 1;
    while (index<length) { DISPLAYRESULT("%02x", p[index]); index += incr; }   /* intentional underflow to negative to detect end */
}


static int BMK_hash(const char* fileName, U32 hashNb)
{
    FILE*  inFile;
    size_t const blockSize = 64 KB;
    size_t readSize;
    void*  buffer;
    XXH64_state_t state;   /* sizeof >= XXH32_state_t */

    /* Check file existence */
    if (fileName == stdinName)
    {
        inFile = stdin;
        SET_BINARY_MODE(stdin);
    }
    else
        inFile = fopen( fileName, "rb" );
    if (inFile==NULL)
    {
        DISPLAY( "Pb opening %s\n", fileName);
        return 11;
    }

    /* Memory allocation & restrictions */
    buffer = malloc(blockSize);
    if(!buffer)
    {
        DISPLAY("\nError: not enough memory!\n");
        fclose(inFile);
        return 12;
    }

    /* Init */
    switch(hashNb)
    {
    case 0:
        XXH32_reset((XXH32_state_t*)&state, 0);
        break;
    case 1:
        XXH64_reset(&state, 0);
        break;
    default:
        DISPLAY("Error : bad hash algorithm ID\n");
        fclose(inFile);
        free(buffer);
        return -1;
    }


    /* Load file & update hash */
    DISPLAY("\rLoading %s...        \r", fileName);
    readSize = 1;
    while (readSize)
    {
        readSize = fread(buffer, 1, blockSize, inFile);
        switch(hashNb)
        {
        case 0:
            XXH32_update((XXH32_state_t*)&state, buffer, readSize);
            break;
        case 1:
            XXH64_update(&state, buffer, readSize);
            break;
        default:
            break;
        }
    }
    fclose(inFile);
    free(buffer);

    /* display Hash */
    switch(hashNb)
    {
    case 0:
        {
            U32 h32 = XXH32_digest((XXH32_state_t*)&state);
            BMK_display_BigEndian(&h32, 4);
            DISPLAYRESULT("  %s        \n", fileName);
            break;
        }
    case 1:
        {
            U64 h64 = XXH64_digest(&state);
            BMK_display_BigEndian(&h64, 8);
            DISPLAYRESULT("  %s    \n", fileName);
            break;
        }
    default:
            break;
    }

    return 0;
}


static int BMK_hashFiles(const char** fnList, int fnTotal, U32 hashNb)
{
    int fnNb;
    int result = 0;
    if (fnTotal==0)
    {
        result = BMK_hash(stdinName, hashNb);
    }
    else
    {
        for (fnNb=0; fnNb<fnTotal; fnNb++)
            result |= BMK_hash(fnList[fnNb], hashNb);
    }
    return result;
}


/*********************************************************
*  Main
*********************************************************/

static int usage(const char* exename)
{
    DISPLAY( WELCOME_MESSAGE );
    DISPLAY( "Usage :\n");
    DISPLAY( "      %s [arg] [filename]\n", exename);
    DISPLAY( "When no filename provided, or - provided : use stdin as input\n");
    DISPLAY( "Arguments :\n");
    DISPLAY( " -H# : hash selection : 0=32bits, 1=64bits (default %i)\n", g_fn_selection);
    DISPLAY( " -b  : benchmark mode \n");
    DISPLAY( " -i# : number of iterations (benchmark mode; default %i)\n", g_nbIterations);
    DISPLAY( " -h  : help (this text)\n");
    return 0;
}


static int badusage(const char* exename)
{
    DISPLAY("Wrong parameters\n");
    usage(exename);
    return 1;
}


int main(int argc, const char** argv)
{
    int i, filenamesStart=0;
    const char* exename = argv[0];
    U32 benchmarkMode = 0;

    /* special case : xxh32sum default to 32 bits checksum */
    if (strstr(exename, "xxh32sum")!=NULL) g_fn_selection=0;

    for(i=1; i<argc; i++)
    {
        const char* argument = argv[i];

        if(!argument) continue;   /* Protection, if argument empty */

        if (*argument!='-')
        {
            if (filenamesStart==0) filenamesStart=i;   /* only supports a continuous list of filenames */
            continue;
        }

        /* command selection */
        argument++;   /* note : *argument=='-' */

        while (*argument!=0)
        {
            switch(*argument)
            {
            /* Display version */
            case 'V':
                DISPLAY(WELCOME_MESSAGE); return 0;

            /* Display help on usage */
            case 'h':
                return usage(exename);

            /* select hash algorithm */
            case 'H':
                g_fn_selection = argument[1] - '0';
                argument+=2;
                break;

            /* Trigger benchmark mode */
            case 'b':
                argument++;
                benchmarkMode=1;
                break;

            /* Modify Nb Iterations (benchmark only) */
            case 'i':
                g_nbIterations = argument[1] - '0';
                argument+=2;
                break;

            /* Modify Block size (benchmark only) */
            case 'B':
                argument++;
                g_sampleSize = 0;
                while (argument[0]>='0' && argument[0]<='9')
                    g_sampleSize *= 10, g_sampleSize += argument[0]-'0', argument++;
                break;

            default:
                return badusage(exename);
            }
        }
    }

    /* Check benchmark mode */
    if (benchmarkMode)
    {
        DISPLAY( WELCOME_MESSAGE );
        BMK_sanityCheck();
        if (filenamesStart==0) return BMK_benchInternal();
        return BMK_benchFiles(argv+filenamesStart, argc-filenamesStart);
    }

    /* Check if input is defined as console; trigger an error in this case */
    if ( (filenamesStart==0) && IS_CONSOLE(stdin) ) return badusage(exename);

    if(g_fn_selection < 0 || g_fn_selection > 1) return badusage(exename);

    if (filenamesStart==0) filenamesStart = argc;
    return BMK_hashFiles(argv+filenamesStart, argc-filenamesStart, g_fn_selection);
}
