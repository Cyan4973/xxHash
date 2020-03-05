/*
 * xxhsum - Command line interface for xxhash algorithms
 * Copyright (C) Yann Collet 2013-present
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

/*
 * xxhsum:
 * Provides hash value of a file content, or a list of files, or stdin
 * Display convention is Big Endian, for both 32 and 64 bits algorithms
 */


/* ************************************
 *  Compiler Options
 **************************************/
/* MS Visual */
#if defined(_MSC_VER) || defined(_WIN32)
#  define _CRT_SECURE_NO_WARNINGS   /* removes visual warnings */
#endif

/* Under Linux at least, pull in the *64 commands */
#ifndef _LARGEFILE64_SOURCE
#  define _LARGEFILE64_SOURCE
#endif

/* ************************************
 *  Includes
 **************************************/
#include <stdlib.h>     /* malloc, calloc, free, exit */
#include <stdio.h>      /* fprintf, fopen, ftello64, fread, stdin, stdout, _fileno (when present) */
#include <string.h>     /* strcmp */
#include <sys/types.h>  /* stat, stat64, _stat64 */
#include <sys/stat.h>   /* stat, stat64, _stat64 */
#include <time.h>       /* clock_t, clock, CLOCKS_PER_SEC */
#include <assert.h>     /* assert */
#include <errno.h>      /* errno */

#define XXH_STATIC_LINKING_ONLY   /* *_state_t */
#include "xxhash.h"


/* ************************************
 *  OS-Specific Includes
 **************************************/
#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)) /* UNIX-like OS */ \
   || defined(__midipix__) || defined(__VMS))
#  if (defined(__APPLE__) && defined(__MACH__)) || defined(__SVR4) || defined(_AIX) || defined(__hpux) /* POSIX.1-2001 (SUSv3) conformant */ \
     || defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)  /* BSD distros */
#    define PLATFORM_POSIX_VERSION 200112L
#  else
#    if defined(__linux__) || defined(__linux)
#      ifndef _POSIX_C_SOURCE
#        define _POSIX_C_SOURCE 200112L  /* use feature test macro */
#      endif
#    endif
#    include <unistd.h>  /* declares _POSIX_VERSION */
#    if defined(_POSIX_VERSION)  /* POSIX compliant */
#      define PLATFORM_POSIX_VERSION _POSIX_VERSION
#    else
#      define PLATFORM_POSIX_VERSION 0
#    endif
#  endif
#endif
#if !defined(PLATFORM_POSIX_VERSION)
#  define PLATFORM_POSIX_VERSION -1
#endif

#if (defined(__linux__) && (PLATFORM_POSIX_VERSION >= 1)) \
 || (PLATFORM_POSIX_VERSION >= 200112L) \
 || defined(__DJGPP__) \
 || defined(__MSYS__)
#  include <unistd.h>   /* isatty */
#  define IS_CONSOLE(stdStream) isatty(fileno(stdStream))
#elif defined(MSDOS) || defined(OS2)
#  include <io.h>       /* _isatty */
#  define IS_CONSOLE(stdStream) _isatty(_fileno(stdStream))
#elif defined(WIN32) || defined(_WIN32)
#  include <io.h>      /* _isatty */
#  include <windows.h> /* DeviceIoControl, HANDLE, FSCTL_SET_SPARSE */
#  include <stdio.h>   /* FILE */
static __inline int IS_CONSOLE(FILE* stdStream) {
    DWORD dummy;
    return _isatty(_fileno(stdStream)) && GetConsoleMode((HANDLE)_get_osfhandle(_fileno(stdStream)), &dummy);
}
#else
#  define IS_CONSOLE(stdStream) 0
#endif

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(_WIN32)
#  include <fcntl.h>   /* _O_BINARY */
#  include <io.h>      /* _setmode, _fileno, _get_osfhandle */
#  if !defined(__DJGPP__)
#    include <windows.h> /* DeviceIoControl, HANDLE, FSCTL_SET_SPARSE */
#    include <winioctl.h> /* FSCTL_SET_SPARSE */
#    define SET_BINARY_MODE(file) { int const unused=_setmode(_fileno(file), _O_BINARY); (void)unused; }
#    define SET_SPARSE_FILE_MODE(file) { DWORD dw; DeviceIoControl((HANDLE) _get_osfhandle(_fileno(file)), FSCTL_SET_SPARSE, 0, 0, 0, 0, &dw, 0); }
#  else
#    define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#    define SET_SPARSE_FILE_MODE(file)
#  endif
#else
#  define SET_BINARY_MODE(file)
#  define SET_SPARSE_FILE_MODE(file)
#endif

#if !defined(S_ISREG)
#  define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
#endif

/* Unicode helpers for Windows */
#if defined(_WIN32)
/* Converts a UTF-8 string to UTF-16. Acts like strdup. The string must be freed afterwards. */
static wchar_t *utf8_to_utf16(const char *str)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    if (len == 0) {
        return NULL;
    }
    {   wchar_t *buf = (wchar_t *)malloc((size_t)len * sizeof(wchar_t));
        if (buf != NULL) {
            if (MultiByteToWideChar(CP_UTF8, 0, str, -1, buf, len) == 0) {
                free(buf);
                return NULL;
            }
       }
       return buf;
    }
}
/* Converts a UTF-16 string to UTF-8. Acts like strdup. The string must be freed afterwards. */
static char *utf16_to_utf8(const wchar_t *str)
{
    int len = WideCharToMultiByte(CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL);
    if (len == 0) {
        return NULL;
    }
    {   char *buf = (char *)malloc((size_t)len * sizeof(char));
        if (buf != NULL) {
            if (WideCharToMultiByte(CP_UTF8, 0, str, -1, buf, len, NULL, NULL) == 0) {
                free(buf);
                return NULL;
            }
       }
       return buf;
    }
}

/*
 * fopen wrapper that supports UTF-8
 *
 * fopen will only accept ANSI filenames, which means that we can't open Unicode filenames.
 *
 * In order to open a Unicode filename, we need to convert filenames to UTF-16 and use _wfopen.
 */
static FILE *XXH_fopen_wrapped(const char *filename, const wchar_t *mode)
{
    FILE *f = NULL;
    wchar_t *wide_filename = utf8_to_utf16(filename);
    if (wide_filename != NULL) {
        f = _wfopen(wide_filename, mode);
        free(wide_filename);
    }
    return f;
}

/*
 * fprintf wrapper that supports UTF-8.
 *
 * If we switch stdout's mode to _O_U8TEXT, the console will always print
 * UTF-8, regardless of the console's codepage. However, fprintf will crash
 * with an assertion if it encounters any UTF-8.
 *
 * fwprintf properly prints in _O_U8TEXT mode, but it is not ISO C compatible.
 * Therefore, we can't just replace fprintf with fwprintf.
 *
 * Specifically, '%s' prints UTF-16 strings on Windows instead of UTF-8.
 *
 * Additionally, '%hs' prints strings in ANSI encoding.
 *
 * The workaround to this is to generate a UTF-8 string with snprintf (actually
 * vsnprintf), convert it to UTF-16, and print it with fwprintf.
 *
 * This works reliably even if someone defines __USE_MINGW_ANSI_STDIO.
 *
 * Credit to t-mat: https://github.com/t-mat/xxHash/commit/5691423
 */
static int fprintf_utf8(FILE *stream, const char *format, ...)
{
    int result;
    va_list args;
    va_start(args, format);
    result = _vscprintf(format, args);
    if (result > 0) {
        const size_t nchar = (size_t)result + 1;
        char* u8_str = (char*) malloc(nchar * sizeof(u8_str[0]));
        if (u8_str == NULL) {
            result = -1;
        } else {
            result = vsnprintf(u8_str, nchar, format, args);
            if (result > 0) {
                wchar_t *const u16_buf = utf8_to_utf16(u8_str);
                if (u16_buf == NULL) {
                    result = -1;
                } else {
                    /* %ls: Behaves the same in ISO C and Windows */
                    result = fwprintf(stream, L"%ls", u16_buf);
                    free(u16_buf);
                }
            }
            free(u8_str);
        }
    }
    va_end(args);
    return result;
}
/*
 * Since we always use literals in the "mode" argument, it is just easier to append "L" to
 * the string to make it UTF-16 and avoid the hassle of a second manual conversion.
 */
#  define XXH_fopen(filename, mode) XXH_fopen_wrapped(filename, L##mode)
#else
#  define XXH_fopen(filename, mode) fopen(filename, mode)
#endif

/* ************************************
*  Basic Types
**************************************/
#ifndef MEM_MODULE
# define MEM_MODULE
# if defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L   /* C99 */
#   include <stdint.h>
    typedef uint8_t  U8;
    typedef uint32_t U32;
    typedef uint64_t U64;
#  else
#   include <limits.h>
    typedef unsigned char      U8;
#   if UINT_MAX == 0xFFFFFFFFUL
      typedef unsigned int     U32;
#   else
      typedef unsigned long    U32;
#   endif
    typedef unsigned long long U64;
#  endif
#endif

static unsigned BMK_isLittleEndian(void)
{
    const union { U32 u; U8 c[4]; } one = { 1 };   /* don't use static: performance detrimental  */
    return one.c[0];
}


/* *************************************
 *  Constants
 ***************************************/
#define LIB_VERSION XXH_VERSION_MAJOR.XXH_VERSION_MINOR.XXH_VERSION_RELEASE
#define QUOTE(str) #str
#define EXPAND_AND_QUOTE(str) QUOTE(str)
#define PROGRAM_VERSION EXPAND_AND_QUOTE(LIB_VERSION)

/* Show compiler versions in WELCOME_MESSAGE. VERSION_FMT will return the printf specifiers,
 * and VERSION will contain the comma separated list of arguments to the VERSION_FMT string. */
#if defined(__clang_version__)
/* Clang does its own thing. */
#  ifdef __apple_build_version__
#    define VERSION_FMT ", Apple Clang %s"
#  else
#    define VERSION_FMT ", Clang %s"
#  endif
#  define VERSION  __clang_version__
#elif defined(__VERSION__)
/* GCC and ICC */
#  define VERSION_FMT ", %s"
#  ifdef __INTEL_COMPILER /* icc adds its prefix */
#    define VERSION_STRING __VERSION__
#  else /* assume GCC */
#    define VERSION "GCC " __VERSION__
#  endif
#elif defined(_MSC_FULL_VER) && defined(_MSC_BUILD)
/*
 * MSVC
 *  "For example, if the version number of the Visual C++ compiler is
 *   15.00.20706.01, the _MSC_FULL_VER macro evaluates to 150020706."
 *
 *   https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros?view=vs-2017
 */
#  define VERSION  _MSC_FULL_VER / 10000000 % 100, _MSC_FULL_VER / 100000 % 100, _MSC_FULL_VER % 100000, _MSC_BUILD
#  define VERSION_FMT ", MSVC %02i.%02i.%05i.%02i"
#elif defined(__TINYC__)
/* tcc stores its version in the __TINYC__ macro. */
#  define VERSION_FMT ", tcc %i.%i.%i"
#  define VERSION __TINYC__ / 10000 % 100, __TINYC__ / 100 % 100, __TINYC__ % 100
#else
#  define VERSION_FMT "%s"
#  define VERSION ""
#endif

/* makes the next part easier */
#if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)
#   define ARCH_X64 1
#   define ARCH_X86 "x86_64"
#elif defined(__i386__) || defined(_M_IX86) || defined(_M_IX86_FP)
#   define ARCH_X86 "i386"
#endif

/* Try to detect the architecture. */
#if defined(ARCH_X86)
#  if defined(__AVX2__)
#    define ARCH ARCH_X86 " + AVX2"
#  elif defined(__AVX__)
#    define ARCH ARCH_X86 " + AVX"
#  elif defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__) \
      || defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP == 2)
#     define ARCH ARCH_X86 " + SSE2"
#  else
#     define ARCH ARCH_X86
#  endif
#elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
#  define ARCH "aarch64 + NEON"
#elif defined(__arm__) || defined(__thumb__) || defined(__thumb2__) || defined(_M_ARM)
/* ARM has a lot of different features that can change xxHash significantly. */
#  if defined(__thumb2__) || (defined(__thumb__) && (__thumb__ == 2 || __ARM_ARCH >= 7))
#    define ARCH_THUMB " Thumb-2"
#  elif defined(__thumb__)
#    define ARCH_THUMB " Thumb-1"
#  else
#    define ARCH_THUMB ""
#  endif
/* ARMv7 has unaligned by default */
#  if defined(__ARM_FEATURE_UNALIGNED) || __ARM_ARCH >= 7 || defined(_M_ARMV7VE)
#    define ARCH_UNALIGNED " + unaligned"
#  else
#    define ARCH_UNALIGNED ""
#  endif
#  if defined(__ARM_NEON) || defined(__ARM_NEON__)
#    define ARCH_NEON " + NEON"
#  else
#    define ARCH_NEON ""
#  endif
#  define ARCH "ARMv" EXPAND_AND_QUOTE(__ARM_ARCH) ARCH_THUMB ARCH_NEON ARCH_UNALIGNED
#elif defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__)
#  if defined(__GNUC__) && defined(__POWER9_VECTOR__)
#    define ARCH "ppc64 + POWER9 vector"
#  elif defined(__GNUC__) && defined(__POWER8_VECTOR__)
#    define ARCH "ppc64 + POWER8 vector"
#  else
#    define ARCH "ppc64"
#  endif
#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
#  define ARCH "ppc"
#elif defined(__AVR)
#  define ARCH "AVR"
#elif defined(__mips64)
#  define ARCH "mips64"
#elif defined(__mips)
#  define ARCH "mips"
#elif defined(__s390x__)
#  define ARCH "s390x"
#elif defined(__s390__)
#  define ARCH "s390"
#else
#  define ARCH "unknown"
#endif

static const int g_nbBits = (int)(sizeof(void*)*8);
static const char g_lename[] = "little endian";
static const char g_bename[] = "big endian";
#define ENDIAN_NAME (BMK_isLittleEndian() ? g_lename : g_bename)
static const char author[] = "Yann Collet";
#define WELCOME_MESSAGE(exename) "%s %s (%i-bit %s %s)" VERSION_FMT ", by %s\n", \
                    exename, PROGRAM_VERSION, g_nbBits, ARCH, ENDIAN_NAME, VERSION, author

#define KB *( 1<<10)
#define MB *( 1<<20)
#define GB *(1U<<30)

static size_t XXH_DEFAULT_SAMPLE_SIZE = 100 KB;
#define NBLOOPS    3                              /* Default number of benchmark iterations */
#define TIMELOOP_S 1
#define TIMELOOP  (TIMELOOP_S * CLOCKS_PER_SEC)   /* target timing per iteration */
#define TIMELOOP_MIN (TIMELOOP / 2)               /* minimum timing to validate a result */
#define XXHSUM32_DEFAULT_SEED 0                   /* Default seed for algo_xxh32 */
#define XXHSUM64_DEFAULT_SEED 0                   /* Default seed for algo_xxh64 */

#define MAX_MEM    (2 GB - 64 MB)

static const char stdinName[] = "-";
typedef enum { algo_xxh32, algo_xxh64, algo_xxh128 } algoType;
static const algoType g_defaultAlgo = algo_xxh64;    /* required within main() & usage() */

/* <16 hex char> <SPC> <SPC> <filename> <'\0'>
 * '4096' is typical Linux PATH_MAX configuration. */
#define DEFAULT_LINE_LENGTH (sizeof(XXH64_hash_t) * 2 + 2 + 4096 + 1)

/* Maximum acceptable line length. */
#define MAX_LINE_LENGTH (32 KB)


/* ************************************
 *  Display macros
 **************************************/
#ifdef _WIN32
#define DISPLAY(...)         fprintf_utf8(stderr, __VA_ARGS__)
#define DISPLAYRESULT(...)   fprintf_utf8(stdout, __VA_ARGS__)
#else
#define DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define DISPLAYRESULT(...)   fprintf(stdout, __VA_ARGS__)
#endif

#define DISPLAYLEVEL(l, ...) do { if (g_displayLevel>=l) DISPLAY(__VA_ARGS__); } while (0)
static int g_displayLevel = 2;


/* ************************************
 *  Local variables
 **************************************/
static U32 g_nbIterations = NBLOOPS;


/* ************************************
 *  Benchmark Functions
 **************************************/
static clock_t BMK_clockSpan( clock_t start )
{
    return clock() - start;   /* works even if overflow; Typical max span ~ 30 mn */
}


static size_t BMK_findMaxMem(U64 requiredMem)
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

typedef U32 (*hashFunction)(const void* buffer, size_t bufferSize, U32 seed);

static U32 localXXH32(const void* buffer, size_t bufferSize, U32 seed) { return XXH32(buffer, bufferSize, seed); }

static U32 localXXH64(const void* buffer, size_t bufferSize, U32 seed) { return (U32)XXH64(buffer, bufferSize, seed); }

static U32 localXXH3_64b(const void* buffer, size_t bufferSize, U32 seed) { (void)seed; return (U32)XXH3_64bits(buffer, bufferSize); }
static U32 localXXH3_64b_seeded(const void* buffer, size_t bufferSize, U32 seed) { return (U32)XXH3_64bits_withSeed(buffer, bufferSize, seed); }

static U32 localXXH3_128b(const void* buffer, size_t bufferSize, U32 seed) { (void)seed; return (U32)(XXH3_128bits(buffer, bufferSize).low64); }
static U32 localXXH3_128b_seeded(const void* buffer, size_t bufferSize, U32 seed) { return (U32)(XXH3_128bits_withSeed(buffer, bufferSize, seed).low64); }

static void BMK_benchHash(hashFunction h, const char* hName, const void* buffer, size_t bufferSize)
{
    U32 nbh_perIteration = (U32)((300 MB) / (bufferSize+1)) + 1;  /* first loop conservatively aims for 300 MB/s */
    U32 iterationNb;
    double fastestH = 100000000.;

    DISPLAYLEVEL(2, "\r%70s\r", "");       /* Clean display line */
    if (g_nbIterations<1) g_nbIterations=1;
    for (iterationNb = 1; iterationNb <= g_nbIterations; iterationNb++) {
        U32 r=0;
        clock_t cStart;

        DISPLAYLEVEL(2, "%1u-%-22.22s : %10u ->\r", (unsigned)iterationNb, hName, (unsigned)bufferSize);
        cStart = clock();
        while (clock() == cStart);   /* starts clock() at its exact beginning */
        cStart = clock();

        {   U32 u;
            for (u=0; u<nbh_perIteration; u++)
                r += h(buffer, bufferSize, u);
        }
        if (r==0) DISPLAYLEVEL(3,".\r");  /* do something with r to defeat compiler "optimizing" away hash */

        {   clock_t const nbTicks = BMK_clockSpan(cStart);
            double const ticksPerHash = ((double)nbTicks / TIMELOOP) / nbh_perIteration;
            /*
             * clock() is the only decent portable timer, but it isn't very
             * precise.
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
            if (nbTicks < TIMELOOP_MIN) {
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
                    nbh_perIteration = (U32)nbh_perSecond;
                }
                iterationNb--;   /* try again */
                continue;
            }
            if (ticksPerHash < fastestH) fastestH = ticksPerHash;
            DISPLAYLEVEL(2, "%1u-%-22.22s : %10u -> %8.0f it/s (%7.1f MB/s) \r",
                            (unsigned)iterationNb, hName, (unsigned)bufferSize,
                            (double)1 / fastestH,
                            ((double)bufferSize / (1 MB)) / fastestH );
        }
        {   double nbh_perSecond = (1 / fastestH) + 1;
            if (nbh_perSecond > (double)(4000U<<20)) nbh_perSecond = (double)(4000U<<20);   /* avoid overflow */
            nbh_perIteration = (U32)nbh_perSecond;
        }
    }
    DISPLAYLEVEL(1, "%-24.24s : %10u -> %8.0f it/s (%7.1f MB/s) \n",
                    hName, (unsigned)bufferSize,
                    (double)1 / fastestH,
                    ((double)bufferSize / (1 MB)) / fastestH );
    if (g_displayLevel<1)
        DISPLAYLEVEL(0, "%u, ", (unsigned)((double)1 / fastestH));
}


/*!
 * BMK_benchMem():
 * specificTest: 0 == run all tests, 1+ runs specific test
 * buffer: Must be 8-byte aligned (if malloc'ed, it should be)
 * The real allocated size of buffer is supposed to be >= (bufferSize+3).
 * returns: 0 on success, 1 if error (invalid mode selected)
 */
static int BMK_benchMem(const void* buffer, size_t bufferSize, U32 specificTest)
{
    assert((((size_t)buffer) & 8) == 0);  /* ensure alignment */

    /* XXH32 bench */
    if ((specificTest==0) | (specificTest==1))
        BMK_benchHash(localXXH32, "XXH32", buffer, bufferSize);

    /* Bench XXH32 on Unaligned input */
    if ((specificTest==0) | (specificTest==2))
        BMK_benchHash(localXXH32, "XXH32 unaligned", ((const char*)buffer)+1, bufferSize);

    /* Bench XXH64 */
    if ((specificTest==0) | (specificTest==3))
        BMK_benchHash(localXXH64, "XXH64", buffer, bufferSize);

    /* Bench XXH64 on Unaligned input */
    if ((specificTest==0) | (specificTest==4))
        BMK_benchHash(localXXH64, "XXH64 unaligned", ((const char*)buffer)+3, bufferSize);

    /* Bench XXH3 */
    if ((specificTest==0) | (specificTest==5))
        BMK_benchHash(localXXH3_64b, "XXH3_64b", buffer, bufferSize);

    /* Bench XXH3 on Unaligned input */
    if ((specificTest==0) | (specificTest==6))
        BMK_benchHash(localXXH3_64b, "XXH3_64b unaligned", ((const char*)buffer)+3, bufferSize);

    /* Bench XXH3 */
    if ((specificTest==0) | (specificTest==7))
        BMK_benchHash(localXXH3_64b_seeded, "XXH3_64b seeded", buffer, bufferSize);

    /* Bench XXH3 on Unaligned input */
    if ((specificTest==0) | (specificTest==8))
        BMK_benchHash(localXXH3_64b_seeded, "XXH3_64b seeded unaligned", ((const char*)buffer)+3, bufferSize);

    /* Bench XXH3 */
    if ((specificTest==0) | (specificTest==9))
        BMK_benchHash(localXXH3_128b, "XXH128", buffer, bufferSize);

    /* Bench XXH3 on Unaligned input */
    if ((specificTest==0) | (specificTest==10))
        BMK_benchHash(localXXH3_128b, "XXH128 unaligned", ((const char*)buffer)+3, bufferSize);

    /* Bench XXH3 */
    if ((specificTest==0) | (specificTest==11))
        BMK_benchHash(localXXH3_128b_seeded, "XXH128 seeded", buffer, bufferSize);

    /* Bench XXH3 on Unaligned input */
    if ((specificTest==0) | (specificTest==12))
        BMK_benchHash(localXXH3_128b_seeded, "XXH128 seeded unaligned", ((const char*)buffer)+3, bufferSize);

    if (specificTest > 12) {
        DISPLAY("Benchmark mode invalid.\n");
        return 1;
    }
    return 0;
}


static size_t BMK_selectBenchedSize(const char* fileName)
{   U64 const inFileSize = BMK_GetFileSize(fileName);
    size_t benchedSize = (size_t) BMK_findMaxMem(inFileSize);
    if ((U64)benchedSize > inFileSize) benchedSize = (size_t)inFileSize;
    if (benchedSize < inFileSize) {
        DISPLAY("Not enough memory for '%s' full size; testing %i MB only...\n", fileName, (int)(benchedSize>>20));
    }
    return benchedSize;
}


static int BMK_benchFiles(char** fileNamesTable, int nbFiles, U32 specificTest)
{
    int result = 0;
    int fileIdx;

    for (fileIdx=0; fileIdx<nbFiles; fileIdx++) {
        const char* const inFileName = fileNamesTable[fileIdx];
        assert(inFileName != NULL);

        {   FILE* const inFile = XXH_fopen( inFileName, "rb" );
            size_t const benchedSize = BMK_selectBenchedSize(inFileName);
            char* const buffer = (char*)calloc(benchedSize+16+3, 1);
            void* const alignedBuffer = (buffer+15) - (((size_t)(buffer+15)) & 0xF);  /* align on next 16 bytes */

            /* Checks */
            if (inFile==NULL){
                DISPLAY("Error: Could not open '%s': %s.\n", inFileName, strerror(errno));
                free(buffer);
                return 11;
            }
            if(!buffer) {
                DISPLAY("\nError: Out of memory.\n");
                fclose(inFile);
                return 12;
            }

            /* Fill input buffer */
            DISPLAYLEVEL(2, "\rLoading %s...        \n", inFileName);
            {   size_t const readSize = fread(alignedBuffer, 1, benchedSize, inFile);
                fclose(inFile);
                if(readSize != benchedSize) {
                    DISPLAY("\nError: Could not read '%s': %s.\n", inFileName, strerror(errno));
                    free(buffer);
                    return 13;
            }   }

            /* bench */
            result |= BMK_benchMem(alignedBuffer, benchedSize, specificTest);

            free(buffer);
    }   }

    return result;
}


static int BMK_benchInternal(size_t keySize, U32 specificTest)
{
    void* const buffer = calloc(keySize+16+3, 1);
    if (!buffer) {
        DISPLAY("\nError: Out of memory.\n");
        return 12;
    }

    {   const void* const alignedBuffer = ((char*)buffer+15) - (((size_t)((char*)buffer+15)) & 0xF);  /* align on next 16 bytes */

        /* bench */
        DISPLAYLEVEL(1, "Sample of ");
        if (keySize > 10 KB) {
            DISPLAYLEVEL(1, "%u KB", (unsigned)(keySize >> 10));
        } else {
            DISPLAYLEVEL(1, "%u bytes", (unsigned)keySize);
        }
        DISPLAYLEVEL(1, "...        \n");

        {   int const result = BMK_benchMem(alignedBuffer, keySize, specificTest);
            free(buffer);
            return result;
    }   }
}


/* ************************************************
 * Self-test:
 * ensure results consistency accross platforms
 *********************************************** */

static void BMK_checkResult32(XXH32_hash_t r1, XXH32_hash_t r2)
{
    static int nbTests = 1;
    if (r1!=r2) {
        DISPLAY("\rError: 32-bit hash test %i: Internal sanity check failed!\n", nbTests);
        DISPLAY("\rGot 0x%08X, expected 0x%08X.\n", (unsigned)r1, (unsigned)r2);
        DISPLAY("\rNote: If you modified the hash functions, make sure to either update the values\n"
                  "or temporarily comment out the tests in BMK_sanityCheck.\n");
        exit(1);
    }
    nbTests++;
}

static void BMK_checkResult64(XXH64_hash_t r1, XXH64_hash_t r2)
{
    static int nbTests = 1;
    if (r1!=r2) {
        DISPLAY("\rError: 64-bit hash test %i: Internal sanity check failed!\n", nbTests);
        DISPLAY("\rGot 0x%08X%08XULL, expected 0x%08X%08XULL.\n",
                (unsigned)(r1>>32), (unsigned)r1, (unsigned)(r2>>32), (unsigned)r2);
        DISPLAY("\rNote: If you modified the hash functions, make sure to either update the values\n"
                  "or temporarily comment out the tests in BMK_sanityCheck.\n");
        exit(1);
    }
    nbTests++;
}

static void BMK_checkResult128(XXH128_hash_t r1, XXH128_hash_t r2)
{
    static int nbTests = 1;
    if ((r1.low64 != r2.low64) || (r1.high64 != r2.high64)) {
        DISPLAY("\rError: 128-bit hash test %i: Internal sanity check failed.\n", nbTests);
        DISPLAY("\rGot { 0x%08X%08XULL, 0x%08X%08XULL }, expected { 0x%08X%08XULL, 0x%08X%08XULL } \n",
                (unsigned)(r1.low64>>32), (unsigned)r1.low64, (unsigned)(r1.high64>>32), (unsigned)r1.high64,
                (unsigned)(r2.low64>>32), (unsigned)r2.low64, (unsigned)(r2.high64>>32), (unsigned)r2.high64 );
        DISPLAY("\rNote: If you modified the hash functions, make sure to either update the values\n"
                  "or temporarily comment out the tests in BMK_sanityCheck.\n");
        exit(1);
    }
    nbTests++;
}


static void BMK_testXXH32(const void* data, size_t len, U32 seed, U32 Nresult)
{
    XXH32_state_t state;
    size_t pos;

    if (len>0) assert(data != NULL);

    BMK_checkResult32(XXH32(data, len, seed), Nresult);

    (void)XXH32_reset(&state, seed);
    (void)XXH32_update(&state, data, len);
    BMK_checkResult32(XXH32_digest(&state), Nresult);

    (void)XXH32_reset(&state, seed);
    for (pos=0; pos<len; pos++)
        (void)XXH32_update(&state, ((const char*)data)+pos, 1);
    BMK_checkResult32(XXH32_digest(&state), Nresult);
}

static void BMK_testXXH64(const void* data, size_t len, U64 seed, U64 Nresult)
{
    XXH64_state_t state;
    size_t pos;

    if (len>0) assert(data != NULL);

    BMK_checkResult64(XXH64(data, len, seed), Nresult);

    (void)XXH64_reset(&state, seed);
    (void)XXH64_update(&state, data, len);
    BMK_checkResult64(XXH64_digest(&state), Nresult);

    (void)XXH64_reset(&state, seed);
    for (pos=0; pos<len; pos++)
        (void)XXH64_update(&state, ((const char*)data)+pos, 1);
    BMK_checkResult64(XXH64_digest(&state), Nresult);
}

void BMK_testXXH3(const void* data, size_t len, U64 seed, U64 Nresult)
{
    if (len>0) assert(data != NULL);

    {   U64 const Dresult = XXH3_64bits_withSeed(data, len, seed);
        BMK_checkResult64(Dresult, Nresult);
    }

    /* check that the no-seed variant produces same result as seed==0 */
    if (seed == 0) {
        U64 const Dresult = XXH3_64bits(data, len);
        BMK_checkResult64(Dresult, Nresult);
    }

    /* streaming API test */
    {   XXH3_state_t state;

        /* single ingestion */
        (void)XXH3_64bits_reset_withSeed(&state, seed);
        (void)XXH3_64bits_update(&state, data, len);
        BMK_checkResult64(XXH3_64bits_digest(&state), Nresult);

        if (len > 3) {
            /* 2 ingestions */
            (void)XXH3_64bits_reset_withSeed(&state, seed);
            (void)XXH3_64bits_update(&state, data, 3);
            (void)XXH3_64bits_update(&state, (const char*)data+3, len-3);
            BMK_checkResult64(XXH3_64bits_digest(&state), Nresult);
        }

        /* byte by byte ingestion */
        {   size_t pos;
            (void)XXH3_64bits_reset_withSeed(&state, seed);
            for (pos=0; pos<len; pos++)
                (void)XXH3_64bits_update(&state, ((const char*)data)+pos, 1);
            BMK_checkResult64(XXH3_64bits_digest(&state), Nresult);
    }   }
}

void BMK_testXXH3_withSecret(const void* data, size_t len, const void* secret, size_t secretSize, U64 Nresult)
{
    if (len>0) assert(data != NULL);

    {   U64 const Dresult = XXH3_64bits_withSecret(data, len, secret, secretSize);
        BMK_checkResult64(Dresult, Nresult);
    }

    /* streaming API test */
    {   XXH3_state_t state;
        (void)XXH3_64bits_reset_withSecret(&state, secret, secretSize);
        (void)XXH3_64bits_update(&state, data, len);
        BMK_checkResult64(XXH3_64bits_digest(&state), Nresult);

        /* byte by byte ingestion */
        {   size_t pos;
            (void)XXH3_64bits_reset_withSecret(&state, secret, secretSize);
            for (pos=0; pos<len; pos++)
                (void)XXH3_64bits_update(&state, ((const char*)data)+pos, 1);
            BMK_checkResult64(XXH3_64bits_digest(&state), Nresult);
    }   }
}

void BMK_testXXH128(const void* data, size_t len, U64 seed, XXH128_hash_t Nresult)
{
    {   XXH128_hash_t const Dresult = XXH3_128bits_withSeed(data, len, seed);
        BMK_checkResult128(Dresult, Nresult);
    }

    /* check that XXH128() is identical to XXH3_128bits_withSeed() */
    {   XXH128_hash_t const Dresult2 = XXH128(data, len, seed);
        BMK_checkResult128(Dresult2, Nresult);
    }

    /* check that the no-seed variant produces same result as seed==0 */
    if (seed == 0) {
        XXH128_hash_t const Dresult = XXH3_128bits(data, len);
        BMK_checkResult128(Dresult, Nresult);
    }

    /* streaming API test */
    {   XXH3_state_t state;

        /* single ingestion */
        (void)XXH3_128bits_reset_withSeed(&state, seed);
        (void)XXH3_128bits_update(&state, data, len);
        BMK_checkResult128(XXH3_128bits_digest(&state), Nresult);

        if (len > 3) {
            /* 2 ingestions */
            (void)XXH3_128bits_reset_withSeed(&state, seed);
            (void)XXH3_128bits_update(&state, data, 3);
            (void)XXH3_128bits_update(&state, (const char*)data+3, len-3);
            BMK_checkResult128(XXH3_128bits_digest(&state), Nresult);
        }

        /* byte by byte ingestion */
        {   size_t pos;
            (void)XXH3_128bits_reset_withSeed(&state, seed);
            for (pos=0; pos<len; pos++)
                (void)XXH3_128bits_update(&state, ((const char*)data)+pos, 1);
            BMK_checkResult128(XXH3_128bits_digest(&state), Nresult);
    }   }

}

#define SANITY_BUFFER_SIZE 2243

/*!
 * BMK_sanityCheck():
 * Runs a sanity check before the benchmark.
 *
 * Exits on an incorrect output.
 */
static void BMK_sanityCheck(void)
{
    const U32 prime = 2654435761U;
    const U64 prime64 = 11400714785074694797ULL;
    U8 sanityBuffer[SANITY_BUFFER_SIZE];
    U64 byteGen = prime;

    int i;
    for (i=0; i<SANITY_BUFFER_SIZE; i++) {
        sanityBuffer[i] = (U8)(byteGen>>56);
        byteGen *= prime64;
    }


    BMK_testXXH32(NULL,          0, 0,     0x02CC5D05);
    BMK_testXXH32(NULL,          0, prime, 0x36B78AE7);
    BMK_testXXH32(sanityBuffer,  1, 0,     0xCF65B03E);
    BMK_testXXH32(sanityBuffer,  1, prime, 0xB4545AA4);
    BMK_testXXH32(sanityBuffer, 14, 0,     0x1208E7E2);
    BMK_testXXH32(sanityBuffer, 14, prime, 0x6AF1D1FE);
    BMK_testXXH32(sanityBuffer,222, 0,     0x5BD11DBD);
    BMK_testXXH32(sanityBuffer,222, prime, 0x58803C5F);

    BMK_testXXH64(NULL        ,  0, 0,     0xEF46DB3751D8E999ULL);
    BMK_testXXH64(NULL        ,  0, prime, 0xAC75FDA2929B17EFULL);
    BMK_testXXH64(sanityBuffer,  1, 0,     0xE934A84ADB052768ULL);
    BMK_testXXH64(sanityBuffer,  1, prime, 0x5014607643A9B4C3ULL);
    BMK_testXXH64(sanityBuffer,  4, 0,     0x9136A0DCA57457EEULL);
    BMK_testXXH64(sanityBuffer, 14, 0,     0x8282DCC4994E35C8ULL);
    BMK_testXXH64(sanityBuffer, 14, prime, 0xC3BD6BF63DEB6DF0ULL);
    BMK_testXXH64(sanityBuffer,222, 0,     0xB641AE8CB691C174ULL);
    BMK_testXXH64(sanityBuffer,222, prime, 0x20CB8AB7AE10C14AULL);

    BMK_testXXH3(NULL,           0, 0,       0x776EDDFB6BFD9195ULL);  /* empty string */
    BMK_testXXH3(NULL,           0, prime64, 0x6AFCE90814C488CBULL);
    BMK_testXXH3(sanityBuffer,   1, 0,       0xB936EBAE24CB01C5ULL);  /*  1 -  3 */
    BMK_testXXH3(sanityBuffer,   1, prime64, 0xF541B1905037FC39ULL);  /*  1 -  3 */
    BMK_testXXH3(sanityBuffer,   6, 0,       0x27B56A84CD2D7325ULL);  /*  4 -  8 */
    BMK_testXXH3(sanityBuffer,   6, prime64, 0x84589C116AB59AB9ULL);  /*  4 -  8 */
    BMK_testXXH3(sanityBuffer,  12, 0,       0xA713DAF0DFBB77E7ULL);  /*  9 - 16 */
    BMK_testXXH3(sanityBuffer,  12, prime64, 0xE7303E1B2336DE0EULL);  /*  9 - 16 */
    BMK_testXXH3(sanityBuffer,  24, 0,       0xA3FE70BF9D3510EBULL);  /* 17 - 32 */
    BMK_testXXH3(sanityBuffer,  24, prime64, 0x850E80FC35BDD690ULL);  /* 17 - 32 */
    BMK_testXXH3(sanityBuffer,  48, 0,       0x397DA259ECBA1F11ULL);  /* 33 - 64 */
    BMK_testXXH3(sanityBuffer,  48, prime64, 0xADC2CBAA44ACC616ULL);  /* 33 - 64 */
    BMK_testXXH3(sanityBuffer,  80, 0,       0xBCDEFBBB2C47C90AULL);  /* 65 - 96 */
    BMK_testXXH3(sanityBuffer,  80, prime64, 0xC6DD0CB699532E73ULL);  /* 65 - 96 */
    BMK_testXXH3(sanityBuffer, 195, 0,       0xCD94217EE362EC3AULL);  /* 129-240 */
    BMK_testXXH3(sanityBuffer, 195, prime64, 0xBA68003D370CB3D9ULL);  /* 129-240 */

    BMK_testXXH3(sanityBuffer, 403, 0,       0x1B2AFF3B46C74648ULL);  /* one block, last stripe is overlapping */
    BMK_testXXH3(sanityBuffer, 403, prime64, 0xB654F6FFF42AD787ULL);  /* one block, last stripe is overlapping */
    BMK_testXXH3(sanityBuffer, 512, 0,       0x43E368661808A9E8ULL);  /* one block, finishing at stripe boundary */
    BMK_testXXH3(sanityBuffer, 512, prime64, 0x3A865148E584E5B9ULL);  /* one block, finishing at stripe boundary */
    BMK_testXXH3(sanityBuffer,2048, 0,       0xC7169244BBDA8BD4ULL);  /* 2 blocks, finishing at block boundary */
    BMK_testXXH3(sanityBuffer,2048, prime64, 0x74BF9A802BBDFBAEULL);  /* 2 blocks, finishing at block boundary */
    BMK_testXXH3(sanityBuffer,2240, 0,       0x30FEB637E114C0C7ULL);  /* 3 blocks, finishing at stripe boundary */
    BMK_testXXH3(sanityBuffer,2240, prime64, 0xEEF78A36185EB61FULL);  /* 3 blocks, finishing at stripe boundary */
    BMK_testXXH3(sanityBuffer,2243, 0,       0x62C631454648A193ULL);  /* 3 blocks, last stripe is overlapping */
    BMK_testXXH3(sanityBuffer,2243, prime64, 0x6CF80A4BADEA4428ULL);  /* 3 blocks, last stripe is overlapping */

    {   const void* const secret = sanityBuffer + 7;
        const size_t secretSize = XXH3_SECRET_SIZE_MIN + 11;
        assert(sizeof(sanityBuffer) >= XXH3_SECRET_SIZE_MIN + 7 + 11);
        BMK_testXXH3_withSecret(NULL,           0, secret, secretSize, 0x6775FD10343C92C3ULL);  /* empty string */
        BMK_testXXH3_withSecret(sanityBuffer,   1, secret, secretSize, 0xC3382C326E24E3CDULL);  /*  1 -  3 */
        BMK_testXXH3_withSecret(sanityBuffer,   6, secret, secretSize, 0x82C90AB0519369ADULL);  /*  4 -  8 */
        BMK_testXXH3_withSecret(sanityBuffer,  12, secret, secretSize, 0x14631E773B78EC57ULL);  /*  9 - 16 */
        BMK_testXXH3_withSecret(sanityBuffer,  24, secret, secretSize, 0xCDD5542E4A9D9FE8ULL);  /* 17 - 32 */
        BMK_testXXH3_withSecret(sanityBuffer,  48, secret, secretSize, 0x33ABD54D094B2534ULL);  /* 33 - 64 */
        BMK_testXXH3_withSecret(sanityBuffer,  80, secret, secretSize, 0xE687BA1684965297ULL);  /* 65 - 96 */
        BMK_testXXH3_withSecret(sanityBuffer, 195, secret, secretSize, 0xA057273F5EECFB20ULL);  /* 129-240 */

        BMK_testXXH3_withSecret(sanityBuffer, 403, secret, secretSize, 0xF9C0BA5BA3AF70B8ULL);  /* one block, last stripe is overlapping */
        BMK_testXXH3_withSecret(sanityBuffer, 512, secret, secretSize, 0x7896E65DCFA09071ULL);  /* one block, finishing at stripe boundary */
        BMK_testXXH3_withSecret(sanityBuffer,2048, secret, secretSize, 0xD6545DB87ECFD98CULL);  /* >= 2 blocks, at least one scrambling */
        BMK_testXXH3_withSecret(sanityBuffer,2243, secret, secretSize, 0x887810081C32460AULL);  /* >= 2 blocks, at least one scrambling, last stripe unaligned */
    }

    {   XXH128_hash_t const expected = { 0x1F17545BCE1061F1ULL, 0x07FD4E968E916AE1ULL };
        BMK_testXXH128(NULL,           0, 0,     expected);         /* empty string */
    }
    {   XXH128_hash_t const expected = { 0x7282E631387D51ACULL, 0x8743B0A8131AB9E6ULL };
        BMK_testXXH128(NULL,           0, prime, expected);
    }
    {   XXH128_hash_t const expected = { 0xB936EBAE24CB01C5ULL, 0x2554B05763A71A05ULL };
        BMK_testXXH128(sanityBuffer,   1, 0,     expected);         /* 1-3 */
    }
    {   XXH128_hash_t const expected = { 0xCA57C628C04B45B8ULL, 0x916831F4DCD21CF9ULL };
        BMK_testXXH128(sanityBuffer,   1, prime, expected);         /* 1-3 */
    }
    {   XXH128_hash_t const expected = { 0x3E7039BDDA43CFC6ULL, 0x082AFE0B8162D12AULL };
        BMK_testXXH128(sanityBuffer,   6, 0,     expected);         /* 4-8 */
    }
    {   XXH128_hash_t const expected = { 0x269D8F70BE98856EULL, 0x5A865B5389ABD2B1ULL };
        BMK_testXXH128(sanityBuffer,   6, prime, expected);         /* 4-8 */
    }
    {   XXH128_hash_t const expected = { 0x061A192713F69AD9ULL, 0x6E3EFD8FC7802B18ULL };
        BMK_testXXH128(sanityBuffer,  12, 0,     expected);         /* 9-16 */
    }
    {   XXH128_hash_t const expected = { 0x9BE9F9A67F3C7DFBULL, 0xD7E09D518A3405D3ULL };
        BMK_testXXH128(sanityBuffer,  12, prime, expected);         /* 9-16 */
    }
    {   XXH128_hash_t const expected = { 0x1E7044D28B1B901DULL, 0x0CE966E4678D3761ULL };
        BMK_testXXH128(sanityBuffer,  24, 0,     expected);         /* 17-32 */
    }
    {   XXH128_hash_t const expected = { 0xD7304C54EBAD40A9ULL, 0x3162026714A6A243ULL };
        BMK_testXXH128(sanityBuffer,  24, prime, expected);         /* 17-32 */
    }
    {   XXH128_hash_t const expected = { 0xF942219AED80F67BULL, 0xA002AC4E5478227EULL };
        BMK_testXXH128(sanityBuffer,  48, 0,     expected);         /* 33-64 */
    }
    {   XXH128_hash_t const expected = { 0x7BA3C3E453A1934EULL, 0x163ADDE36C072295ULL };
        BMK_testXXH128(sanityBuffer,  48, prime, expected);         /* 33-64 */
    }
    {   XXH128_hash_t const expected = { 0x5E8BAFB9F95FB803ULL, 0x4952F58181AB0042ULL };
        BMK_testXXH128(sanityBuffer,  81, 0,     expected);         /* 65-96 */
    }
    {   XXH128_hash_t const expected = { 0x703FBB3D7A5F755CULL, 0x2724EC7ADC750FB6ULL };
        BMK_testXXH128(sanityBuffer,  81, prime, expected);         /* 65-96 */
    }
    {   XXH128_hash_t const expected = { 0xF1AEBD597CEC6B3AULL, 0x337E09641B948717ULL };
        BMK_testXXH128(sanityBuffer, 222, 0,     expected);         /* 129-240 */
    }
    {   XXH128_hash_t const expected = { 0xAE995BB8AF917A8DULL, 0x91820016621E97F1ULL };
        BMK_testXXH128(sanityBuffer, 222, prime, expected);         /* 129-240 */
    }
    {   XXH128_hash_t const expected = { 0xCDEB804D65C6DEA4ULL, 0x1B6DE21E332DD73DULL };
        BMK_testXXH128(sanityBuffer, 403, 0,     expected);         /* one block, last stripe is overlapping */
    }
    {   XXH128_hash_t const expected = { 0x6259F6ECFD6443FDULL, 0xBED311971E0BE8F2ULL };
        BMK_testXXH128(sanityBuffer, 403, prime64, expected);       /* one block, last stripe is overlapping */
    }
    {   XXH128_hash_t const expected = { 0x1443B8153EBEE367ULL, 0x98EC7E48CD872997ULL };
        BMK_testXXH128(sanityBuffer, 512, 0,     expected);         /* one block, finishing at stripe boundary */
    }
    {   XXH128_hash_t const expected = { 0x43FDC6823A52F1F2ULL, 0x2F748A4F194E1EF0ULL };
        BMK_testXXH128(sanityBuffer, 512, prime64, expected);       /* one block, finishing at stripe boundary */
    }
    {   XXH128_hash_t const expected = { 0xF4258501BE8E0623ULL, 0x6930A2267A755B20ULL };
        BMK_testXXH128(sanityBuffer,2048, 0,     expected);         /* two blocks, finishing at block boundary */
    }
    {   XXH128_hash_t const expected = { 0x10CC56C2FA0AD9ACULL, 0xD0D7A3C2EEF2D892ULL };
        BMK_testXXH128(sanityBuffer,2048, prime, expected);         /* two blocks, finishing at block boundary */
    }
    {   XXH128_hash_t const expected = { 0x5890AE7ACBB84A7EULL, 0x85C327B377AA7E62ULL };
        BMK_testXXH128(sanityBuffer,2240, 0,     expected);         /* two blocks, ends at stripe boundary */
    }
    {   XXH128_hash_t const expected = { 0x205E6D72DCCBD2AAULL, 0x62B70214DB075235ULL };
        BMK_testXXH128(sanityBuffer,2240, prime, expected);         /* two blocks, ends at stripe boundary */
    }
    {   XXH128_hash_t const expected = { 0xF403CEA1763CD9CCULL, 0x0CDABF3F3C98B371ULL };
        BMK_testXXH128(sanityBuffer,2237, 0,     expected);         /* two blocks, last stripe is overlapping */
    }
    {   XXH128_hash_t const expected = { 0xF3824EE446018851ULL, 0xC81B751764BD53C5ULL };
        BMK_testXXH128(sanityBuffer,2237, prime, expected);         /* two blocks, last stripe is overlapping */
    }

    DISPLAYLEVEL(3, "\r%70s\r", "");       /* Clean display line */
    DISPLAYLEVEL(3, "Sanity check -- all tests ok\n");
}


/* ********************************************************
*  File Hashing
**********************************************************/

static void BMK_display_LittleEndian(const void* ptr, size_t length)
{
    const U8* p = (const U8*)ptr;
    size_t idx;
    for (idx=length-1; idx<length; idx--)    /* intentional underflow to negative to detect end */
        DISPLAYRESULT("%02x", p[idx]);
}

static void BMK_display_BigEndian(const void* ptr, size_t length)
{
    const U8* p = (const U8*)ptr;
    size_t idx;
    for (idx=0; idx<length; idx++)
        DISPLAYRESULT("%02x", p[idx]);
}

typedef union {
    XXH32_hash_t   xxh32;
    XXH64_hash_t   xxh64;
    XXH128_hash_t xxh128;
} Multihash;

/*
 * BMK_hashStream:
 * Reads data from `inFile`, generating an incremental hash of type hashType,
 * using `buffer` of size `blockSize` for temporary storage.
 */
static Multihash
BMK_hashStream(FILE* inFile,
               algoType hashType,
               void* buffer, size_t blockSize)
{
    XXH32_state_t state32;
    XXH64_state_t state64;
    XXH3_state_t state128;

    /* Init */
    (void)XXH32_reset(&state32, XXHSUM32_DEFAULT_SEED);
    (void)XXH64_reset(&state64, XXHSUM64_DEFAULT_SEED);
    (void)XXH3_128bits_reset(&state128);

    /* Load file & update hash */
    {   size_t readSize = 1;
        while (readSize) {
            readSize = fread(buffer, 1, blockSize, inFile);
            switch(hashType)
            {
            case algo_xxh32:
                (void)XXH32_update(&state32, buffer, readSize);
                break;
            case algo_xxh64:
                (void)XXH64_update(&state64, buffer, readSize);
                break;
            case algo_xxh128:
                (void)XXH3_128bits_update(&state128, buffer, readSize);
                break;
            default:
                assert(0);
            }
    }   }

    {   Multihash finalHash;
        switch(hashType)
        {
        case algo_xxh32:
            finalHash.xxh32 = XXH32_digest(&state32);
            break;
        case algo_xxh64:
            finalHash.xxh64 = XXH64_digest(&state64);
            break;
        case algo_xxh128:
            finalHash.xxh128 = XXH3_128bits_digest(&state128);
            break;
        default:
            assert(0);
        }
        return finalHash;
    }
}


typedef enum { big_endian, little_endian} endianess;

static int BMK_hash(const char* fileName,
                    const algoType hashType,
                    const endianess displayEndianess)
{
    FILE*  inFile;
    size_t const blockSize = 64 KB;
    void*  buffer;
    Multihash hashValue;

    /* Check file existence */
    if (fileName == stdinName) {
        inFile = stdin;
        fileName = "stdin";
        SET_BINARY_MODE(stdin);
    } else {
        inFile = XXH_fopen( fileName, "rb" );
    }
    if (inFile==NULL) {
        DISPLAY("Error: Could not open '%s': %s. \n", fileName, strerror(errno));
        return 1;
    }

    /* Memory allocation & restrictions */
    buffer = malloc(blockSize);
    if(!buffer) {
        DISPLAY("\nError: Out of memory.\n");
        fclose(inFile);
        return 1;
    }

    /* loading notification */
    {   const size_t fileNameSize = strlen(fileName);
        const char* const fileNameEnd = fileName + fileNameSize;
        const int maxInfoFilenameSize = (int)(fileNameSize > 30 ? 30 : fileNameSize);
        int infoFilenameSize = 1;
        while ((infoFilenameSize < maxInfoFilenameSize)
            && (fileNameEnd[-1-infoFilenameSize] != '/')
            && (fileNameEnd[-1-infoFilenameSize] != '\\') )
              infoFilenameSize++;
        DISPLAYLEVEL(2, "\rLoading %s...  \r", fileNameEnd - infoFilenameSize);

        /* Load file & update hash */
        hashValue = BMK_hashStream(inFile, hashType, buffer, blockSize);

        fclose(inFile);
        free(buffer);
        DISPLAYLEVEL(2, "%*s             \r", infoFilenameSize, "");  /* erase line */
    }

    /* display Hash value followed by file name */
    switch(hashType)
    {
    case algo_xxh32:
        {   XXH32_canonical_t hcbe32;
            (void)XXH32_canonicalFromHash(&hcbe32, hashValue.xxh32);
            displayEndianess==big_endian ?
                BMK_display_BigEndian(&hcbe32, sizeof(hcbe32)) : BMK_display_LittleEndian(&hcbe32, sizeof(hcbe32));
            break;
        }
    case algo_xxh64:
        {   XXH64_canonical_t hcbe64;
            (void)XXH64_canonicalFromHash(&hcbe64, hashValue.xxh64);
            displayEndianess==big_endian ?
                BMK_display_BigEndian(&hcbe64, sizeof(hcbe64)) : BMK_display_LittleEndian(&hcbe64, sizeof(hcbe64));
            break;
        }
    case algo_xxh128:
        {   XXH128_canonical_t hcbe128;
            (void)XXH128_canonicalFromHash(&hcbe128, hashValue.xxh128);
            displayEndianess==big_endian ?
                BMK_display_BigEndian(&hcbe128, sizeof(hcbe128)) : BMK_display_LittleEndian(&hcbe128, sizeof(hcbe128));
            break;
        }
    default:
        assert(0);
    }
    DISPLAYRESULT("  %s\n", fileName);

    return 0;
}


/*
 * BMK_hashFiles:
 * If fnTotal==0, read from stdin instead.
 */
static int BMK_hashFiles(char** fnList, int fnTotal,
                         algoType hashType, endianess displayEndianess)
{
    int fnNb;
    int result = 0;

    if (fnTotal==0)
        return BMK_hash(stdinName, hashType, displayEndianess);

    for (fnNb=0; fnNb<fnTotal; fnNb++)
        result += BMK_hash(fnList[fnNb], hashType, displayEndianess);
    DISPLAYLEVEL(2, "\r%70s\r", "");
    return result;
}


typedef enum {
    GetLine_ok,
    GetLine_eof,
    GetLine_exceedMaxLineLength,
    GetLine_outOfMemory,
} GetLineResult;

typedef enum {
    CanonicalFromString_ok,
    CanonicalFromString_invalidFormat,
} CanonicalFromStringResult;

typedef enum {
    ParseLine_ok,
    ParseLine_invalidFormat,
} ParseLineResult;

typedef enum {
    LineStatus_hashOk,
    LineStatus_hashFailed,
    LineStatus_failedToOpen,
} LineStatus;

typedef union {
    XXH32_canonical_t xxh32;
    XXH64_canonical_t xxh64;
    XXH128_canonical_t xxh128;
} Canonical;

typedef struct {
    Canonical   canonical;
    const char* filename;
    int         xxhBits;    /* canonical type: 32:xxh32, 64:xxh64, 128:xxh128 */
} ParsedLine;

typedef struct {
    unsigned long   nProperlyFormattedLines;
    unsigned long   nImproperlyFormattedLines;
    unsigned long   nMismatchedChecksums;
    unsigned long   nOpenOrReadFailures;
    unsigned long   nMixedFormatLines;
    int             xxhBits;
    int             quit;
} ParseFileReport;

typedef struct {
    const char*     inFileName;
    FILE*           inFile;
    int             lineMax;
    char*           lineBuf;
    size_t          blockSize;
    char*           blockBuf;
    U32             strictMode;
    U32             statusOnly;
    U32             warn;
    U32             quiet;
    ParseFileReport report;
} ParseFileArg;


/*
 * Reads a line from stream `inFile`.
 * Returns GetLine_ok, if it reads line successfully.
 * Returns GetLine_eof, if stream reaches EOF.
 * Returns GetLine_exceedMaxLineLength, if line length is longer than MAX_LINE_LENGTH.
 * Returns GetLine_outOfMemory, if line buffer memory allocation failed.
 */
static GetLineResult getLine(char** lineBuf, int* lineMax, FILE* inFile)
{
    GetLineResult result = GetLine_ok;
    size_t len = 0;

    if ((*lineBuf == NULL) || (*lineMax<1)) {
        free(*lineBuf);  /* in case it's != NULL */
        *lineMax = 0;
        *lineBuf = (char*)malloc(DEFAULT_LINE_LENGTH);
        if(*lineBuf == NULL) return GetLine_outOfMemory;
        *lineMax = DEFAULT_LINE_LENGTH;
    }

    for (;;) {
        const int c = fgetc(inFile);
        if (c == EOF) {
            /*
             * If we meet EOF before first character, returns GetLine_eof,
             * otherwise GetLine_ok.
             */
            if (len == 0) result = GetLine_eof;
            break;
        }

        /* Make enough space for len+1 (for final NUL) bytes. */
        if (len+1 >= (size_t)*lineMax) {
            char* newLineBuf = NULL;
            size_t newBufSize = (size_t)*lineMax;

            newBufSize += (newBufSize/2) + 1; /* x 1.5 */
            if (newBufSize > MAX_LINE_LENGTH) newBufSize = MAX_LINE_LENGTH;
            if (len+1 >= newBufSize) return GetLine_exceedMaxLineLength;

            newLineBuf = (char*) realloc(*lineBuf, newBufSize);
            if (newLineBuf == NULL) return GetLine_outOfMemory;

            *lineBuf = newLineBuf;
            *lineMax = (int)newBufSize;
        }

        if (c == '\n') break;
        (*lineBuf)[len++] = (char) c;
    }

    (*lineBuf)[len] = '\0';
    return result;
}


/*
 * Converts one hexadecimal character to integer.
 * Returns -1 if the given character is not hexadecimal.
 */
static int charToHex(char c)
{
    int result = -1;
    if (c >= '0' && c <= '9') {
        result = (int) (c - '0');
    } else if (c >= 'A' && c <= 'F') {
        result = (int) (c - 'A') + 0x0a;
    } else if (c >= 'a' && c <= 'f') {
        result = (int) (c - 'a') + 0x0a;
    }
    return result;
}


/*
 * Converts XXH32 canonical hexadecimal string `hashStr` to the big endian unsigned
 * char array `dst`.
 *
 * Returns CANONICAL_FROM_STRING_INVALID_FORMAT if hashStr is not well formatted.
 * Returns CANONICAL_FROM_STRING_OK if hashStr is parsed successfully.
 */
static CanonicalFromStringResult canonicalFromString(unsigned char* dst,
                                                     size_t dstSize,
                                                     const char* hashStr)
{
    size_t i;
    for (i = 0; i < dstSize; ++i) {
        int h0, h1;

        h0 = charToHex(hashStr[i*2 + 0]);
        if (h0 < 0) return CanonicalFromString_invalidFormat;

        h1 = charToHex(hashStr[i*2 + 1]);
        if (h1 < 0) return CanonicalFromString_invalidFormat;

        dst[i] = (unsigned char) ((h0 << 4) | h1);
    }
    return CanonicalFromString_ok;
}


/*
 * Parse single line of xxHash checksum file.
 * Returns PARSE_LINE_ERROR_INVALID_FORMAT if the line is not well formatted.
 * Returns PARSE_LINE_OK if the line is parsed successfully.
 * And members of parseLine will be filled by parsed values.
 *
 *  - line must be terminated with '\0'.
 *  - Since parsedLine.filename will point within given argument `line`,
 *    users must keep `line`s content when they are using parsedLine.
 *
 * xxHash checksum lines should have the following format:
 *
 *      <8, 16, or 32 hexadecimal char> <space> <space> <filename...> <'\0'>
 */
static ParseLineResult parseLine(ParsedLine* parsedLine, const char* line)
{
    const char* const firstSpace = strchr(line, ' ');
    if (firstSpace == NULL) return ParseLine_invalidFormat;

    {   const char* const secondSpace = firstSpace + 1;
        if (*secondSpace != ' ') return ParseLine_invalidFormat;

        parsedLine->filename = NULL;
        parsedLine->xxhBits = 0;

        switch (firstSpace - line)
        {
        case 8:
            {   XXH32_canonical_t* xxh32c = &parsedLine->canonical.xxh32;
                if (canonicalFromString(xxh32c->digest, sizeof(xxh32c->digest), line)
                    != CanonicalFromString_ok) {
                    return ParseLine_invalidFormat;
                }
                parsedLine->xxhBits = 32;
                break;
            }

        case 16:
            {   XXH64_canonical_t* xxh64c = &parsedLine->canonical.xxh64;
                if (canonicalFromString(xxh64c->digest, sizeof(xxh64c->digest), line)
                    != CanonicalFromString_ok) {
                    return ParseLine_invalidFormat;
                }
                parsedLine->xxhBits = 64;
                break;
            }

        case 32:
            {   XXH128_canonical_t* xxh128c = &parsedLine->canonical.xxh128;
                if (canonicalFromString(xxh128c->digest, sizeof(xxh128c->digest), line)
                    != CanonicalFromString_ok) {
                    return ParseLine_invalidFormat;
                }
                parsedLine->xxhBits = 128;
                break;
            }

        default:
                return ParseLine_invalidFormat;
                break;
        }

        parsedLine->filename = secondSpace + 1;
    }
    return ParseLine_ok;
}


/*!
 * Parse xxHash checksum file.
 */
static void parseFile1(ParseFileArg* parseFileArg)
{
    const char* const inFileName = parseFileArg->inFileName;
    ParseFileReport* const report = &parseFileArg->report;

    unsigned long lineNumber = 0;
    memset(report, 0, sizeof(*report));

    while (!report->quit) {
        LineStatus lineStatus = LineStatus_hashFailed;
        ParsedLine parsedLine;
        memset(&parsedLine, 0, sizeof(parsedLine));

        lineNumber++;
        if (lineNumber == 0) {
            /* This is unlikely happen, but md5sum.c has this error check. */
            DISPLAY("%s: Error: Too many checksum lines\n", inFileName);
            report->quit = 1;
            break;
        }

        {   GetLineResult const getLineResult = getLine(&parseFileArg->lineBuf,
                                                        &parseFileArg->lineMax,
                                                         parseFileArg->inFile);
            if (getLineResult != GetLine_ok) {
                if (getLineResult == GetLine_eof) break;

                switch (getLineResult)
                {
                case GetLine_ok:
                case GetLine_eof:
                    /* These cases never happen.  See above getLineResult related "if"s.
                       They exist just for make gcc's -Wswitch-enum happy. */
                    assert(0);
                    break;

                default:
                    DISPLAY("%s:%lu: Error: Unknown error.\n", inFileName, lineNumber);
                    break;

                case GetLine_exceedMaxLineLength:
                    DISPLAY("%s:%lu: Error: Line too long.\n", inFileName, lineNumber);
                    break;

                case GetLine_outOfMemory:
                    DISPLAY("%s:%lu: Error: Out of memory.\n", inFileName, lineNumber);
                    break;
                }
                report->quit = 1;
                break;
        }   }

        if (parseLine(&parsedLine, parseFileArg->lineBuf) != ParseLine_ok) {
            report->nImproperlyFormattedLines++;
            if (parseFileArg->warn) {
                DISPLAY("%s:%lu: Error: Improperly formatted checksum line.\n",
                        inFileName, lineNumber);
            }
            continue;
        }

        if (report->xxhBits != 0 && report->xxhBits != parsedLine.xxhBits) {
            /* Don't accept xxh32/xxh64 mixed file */
            report->nImproperlyFormattedLines++;
            report->nMixedFormatLines++;
            if (parseFileArg->warn) {
                DISPLAY("%s: %lu: Error: Multiple hash types in one file.\n",
                        inFileName, lineNumber);
            }
            continue;
        }

        report->nProperlyFormattedLines++;
        if (report->xxhBits == 0) {
            report->xxhBits = parsedLine.xxhBits;
        }

        do {
            FILE* const fp = XXH_fopen(parsedLine.filename, "rb");
            if (fp == NULL) {
                lineStatus = LineStatus_failedToOpen;
                break;
            }
            lineStatus = LineStatus_hashFailed;
            switch (parsedLine.xxhBits)
            {
            case 32:
                {   Multihash const xxh = BMK_hashStream(fp, algo_xxh32, parseFileArg->blockBuf, parseFileArg->blockSize);
                    if (xxh.xxh32 == XXH32_hashFromCanonical(&parsedLine.canonical.xxh32)) {
                        lineStatus = LineStatus_hashOk;
                }   }
                break;

            case 64:
                {   Multihash const xxh = BMK_hashStream(fp, algo_xxh64, parseFileArg->blockBuf, parseFileArg->blockSize);
                    if (xxh.xxh64 == XXH64_hashFromCanonical(&parsedLine.canonical.xxh64)) {
                        lineStatus = LineStatus_hashOk;
                }   }
                break;

            case 128:
                {   Multihash const xxh = BMK_hashStream(fp, algo_xxh128, parseFileArg->blockBuf, parseFileArg->blockSize);
                    if (XXH128_isEqual(xxh.xxh128, XXH128_hashFromCanonical(&parsedLine.canonical.xxh128))) {
                        lineStatus = LineStatus_hashOk;
                }   }
                break;

            default:
                break;
            }
            fclose(fp);
        } while (0);

        switch (lineStatus)
        {
        default:
            DISPLAY("%s: Error: Unknown error.\n", inFileName);
            report->quit = 1;
            break;

        case LineStatus_failedToOpen:
            report->nOpenOrReadFailures++;
            if (!parseFileArg->statusOnly) {
                DISPLAYRESULT("%s:%lu: Could not open or read '%s': %s.\n",
                    inFileName, lineNumber, parsedLine.filename, strerror(errno));
            }
            break;

        case LineStatus_hashOk:
        case LineStatus_hashFailed:
            {   int b = 1;
                if (lineStatus == LineStatus_hashOk) {
                    /* If --quiet is specified, don't display "OK" */
                    if (parseFileArg->quiet) b = 0;
                } else {
                    report->nMismatchedChecksums++;
                }

                if (b && !parseFileArg->statusOnly) {
                    DISPLAYRESULT("%s: %s\n", parsedLine.filename
                        , lineStatus == LineStatus_hashOk ? "OK" : "FAILED");
            }   }
            break;
        }
    }   /* while (!report->quit) */
}


/*  Parse xxHash checksum file.
 *  Returns 1, if all procedures were succeeded.
 *  Returns 0, if any procedures was failed.
 *
 *  If strictMode != 0, return error code if any line is invalid.
 *  If statusOnly != 0, don't generate any output.
 *  If warn != 0, print a warning message to stderr.
 *  If quiet != 0, suppress "OK" line.
 *
 *  "All procedures are succeeded" means:
 *    - Checksum file contains at least one line and less than SIZE_T_MAX lines.
 *    - All files are properly opened and read.
 *    - All hash values match with its content.
 *    - (strict mode) All lines in checksum file are consistent and well formatted.
 */
static int checkFile(const char* inFileName,
                     const endianess displayEndianess,
                     U32 strictMode,
                     U32 statusOnly,
                     U32 warn,
                     U32 quiet)
{
    int result = 0;
    FILE* inFile = NULL;
    ParseFileArg parseFileArgBody;
    ParseFileArg* const parseFileArg = &parseFileArgBody;
    ParseFileReport* const report = &parseFileArg->report;

    if (displayEndianess != big_endian) {
        /* Don't accept little endian */
        DISPLAY( "Check file mode doesn't support little endian\n" );
        return 0;
    }

    /* note: stdinName is special constant pointer.  It is not a string. */
    if (inFileName == stdinName) {
        /*
         * Note: Since we expect text input for xxhash -c mode,
         * we don't set binary mode for stdin.
         */
        inFileName = "stdin";
        inFile = stdin;
    } else {
        inFile = XXH_fopen( inFileName, "rt" );
    }

    if (inFile == NULL) {
        DISPLAY("Error: Could not open '%s': %s\n", inFileName, strerror(errno));
        return 0;
    }

    parseFileArg->inFileName    = inFileName;
    parseFileArg->inFile        = inFile;
    parseFileArg->lineMax       = DEFAULT_LINE_LENGTH;
    parseFileArg->lineBuf       = (char*) malloc((size_t) parseFileArg->lineMax);
    parseFileArg->blockSize     = 64 * 1024;
    parseFileArg->blockBuf      = (char*) malloc(parseFileArg->blockSize);
    parseFileArg->strictMode    = strictMode;
    parseFileArg->statusOnly    = statusOnly;
    parseFileArg->warn          = warn;
    parseFileArg->quiet         = quiet;

    parseFile1(parseFileArg);

    free(parseFileArg->blockBuf);
    free(parseFileArg->lineBuf);

    if (inFile != stdin) fclose(inFile);

    /* Show error/warning messages.  All messages are copied from md5sum.c
     */
    if (report->nProperlyFormattedLines == 0) {
        DISPLAY("%s: no properly formatted xxHash checksum lines found\n", inFileName);
    } else if (!statusOnly) {
        if (report->nImproperlyFormattedLines) {
            DISPLAYRESULT("%lu %s are improperly formatted\n"
                , report->nImproperlyFormattedLines
                , report->nImproperlyFormattedLines == 1 ? "line" : "lines");
        }
        if (report->nOpenOrReadFailures) {
            DISPLAYRESULT("%lu listed %s could not be read\n"
                , report->nOpenOrReadFailures
                , report->nOpenOrReadFailures == 1 ? "file" : "files");
        }
        if (report->nMismatchedChecksums) {
            DISPLAYRESULT("%lu computed %s did NOT match\n"
                , report->nMismatchedChecksums
                , report->nMismatchedChecksums == 1 ? "checksum" : "checksums");
    }   }

    /* Result (exit) code logic is copied from
     * gnu coreutils/src/md5sum.c digest_check() */
    result =   report->nProperlyFormattedLines != 0
            && report->nMismatchedChecksums == 0
            && report->nOpenOrReadFailures == 0
            && (!strictMode || report->nImproperlyFormattedLines == 0)
            && report->quit == 0;
    return result;
}


static int checkFiles(char** fnList, int fnTotal,
                      const endianess displayEndianess,
                      U32 strictMode,
                      U32 statusOnly,
                      U32 warn,
                      U32 quiet)
{
    int ok = 1;

    /* Special case for stdinName "-",
     * note: stdinName is not a string.  It's special pointer. */
    if (fnTotal==0) {
        ok &= checkFile(stdinName, displayEndianess, strictMode, statusOnly, warn, quiet);
    } else {
        int fnNb;
        for (fnNb=0; fnNb<fnTotal; fnNb++)
            ok &= checkFile(fnList[fnNb], displayEndianess, strictMode, statusOnly, warn, quiet);
    }
    return ok ? 0 : 1;
}


/* ********************************************************
*  Main
**********************************************************/

static int usage(const char* exename)
{
    DISPLAY( WELCOME_MESSAGE(exename) );
    DISPLAY( "Usage: %s [OPTION] [FILES]...\n", exename);
    DISPLAY( "Print or check xxHash checksums.\n\n" );
    DISPLAY( "When no filename provided or when '-' is provided, uses stdin as input.\n");
    DISPLAY( "Arguments: \n");
    DISPLAY( "  -H#                  Select hash algorithm. 0=32bits, 1=64bits, 2=128bits (default: %i)\n", (int)g_defaultAlgo);
    DISPLAY( "  -c                   Read xxHash sums from the [filenames] and check them\n");
    DISPLAY( "  -h                   Display long help and exit\n");
    return 0;
}


static int usage_advanced(const char* exename)
{
    usage(exename);
    DISPLAY( "Advanced :\n");
    DISPLAY( "  -V, --version        Display version information\n");
    DISPLAY( "  -q, --quiet          Do not display 'Loading' messages\n");
    DISPLAY( "      --little-endian  Display hashes in little endian convention (default: big endian) \n");
    DISPLAY( "  -h, --help           Display long help and exit\n");
    DISPLAY( "  -b [N]               Run a benchmark (runs all by default, or Nth benchmark)\n");
    DISPLAY( "  -i ITERATIONS        Number of times to run the benchmark (default: %u)\n", (unsigned)g_nbIterations);
    DISPLAY( "\n");
    DISPLAY( "The following four options are useful only when verifying checksums (-c):\n");
    DISPLAY( "  -q, --quiet          Don't print OK for each successfully verified file\n");
    DISPLAY( "      --status         Don't output anything, status code shows success\n");
    DISPLAY( "      --strict         Exit non-zero for improperly formatted checksum lines\n");
    DISPLAY( "      --warn           Warn about improperly formatted checksum lines\n");
    return 0;
}

static int badusage(const char* exename)
{
    DISPLAY("Wrong parameters\n");
    usage(exename);
    return 1;
}

static void errorOut(const char* msg)
{
    DISPLAY("%s \n", msg); exit(1);
}

/*!
 * readU32FromCharChecked():
 * @return 0 if success, and store the result in *value.
 * Allows and interprets K, KB, KiB, M, MB and MiB suffix.
 * Will also modify `*stringPtr`, advancing it to position where it stopped reading.
 * @return 1 if an overflow error occurs
 */
static int readU32FromCharChecked(const char** stringPtr, unsigned* value)
{
    static unsigned const max = (((unsigned)(-1)) / 10) - 1;
    unsigned result = 0;
    while ((**stringPtr >='0') && (**stringPtr <='9')) {
        if (result > max) return 1; /* overflow error */
        result *= 10;
        result += (unsigned)(**stringPtr - '0');
        (*stringPtr)++ ;
    }
    if ((**stringPtr=='K') || (**stringPtr=='M')) {
        unsigned const maxK = ((unsigned)(-1)) >> 10;
        if (result > maxK) return 1; /* overflow error */
        result <<= 10;
        if (**stringPtr=='M') {
            if (result > maxK) return 1; /* overflow error */
            result <<= 10;
        }
        (*stringPtr)++;  /* skip `K` or `M` */
        if (**stringPtr=='i') (*stringPtr)++;
        if (**stringPtr=='B') (*stringPtr)++;
    }
    *value = result;
    return 0;
}

/*!
 * readU32FromChar():
 * @return: unsigned integer value read from input in `char` format.
 *  allows and interprets K, KB, KiB, M, MB and MiB suffix.
 *  Will also modify `*stringPtr`, advancing it to position where it stopped reading.
 *  Note: function will exit() program if digit sequence overflows
 */
static unsigned readU32FromChar(const char** stringPtr) {
    unsigned result;
    if (readU32FromCharChecked(stringPtr, &result)) {
        static const char errorMsg[] = "Error: numeric value too large";
        errorOut(errorMsg);
    }
    return result;
}

static int XXH_main(int argc, char** argv)
{
    int i, filenamesStart = 0;
    const char* const exename = argv[0];
    U32 benchmarkMode = 0;
    U32 fileCheckMode = 0;
    U32 strictMode    = 0;
    U32 statusOnly    = 0;
    U32 warn          = 0;
    U32 specificTest  = 0;
    size_t keySize    = XXH_DEFAULT_SAMPLE_SIZE;
    algoType algo     = g_defaultAlgo;
    endianess displayEndianess = big_endian;

    /* special case: xxhNNsum default to NN bits checksum */
    if (strstr(exename,  "xxh32sum") != NULL) algo = algo_xxh32;
    if (strstr(exename,  "xxh64sum") != NULL) algo = algo_xxh64;
    if (strstr(exename, "xxh128sum") != NULL) algo = algo_xxh128;

    for(i=1; i<argc; i++) {
        const char* argument = argv[i];

        if(!argument) continue;   /* Protection if arguments are empty */

        if (!strcmp(argument, "--little-endian")) { displayEndianess = little_endian; continue; }
        if (!strcmp(argument, "--check")) { fileCheckMode = 1; continue; }
        if (!strcmp(argument, "--strict")) { strictMode = 1; continue; }
        if (!strcmp(argument, "--status")) { statusOnly = 1; continue; }
        if (!strcmp(argument, "--quiet")) { g_displayLevel--; continue; }
        if (!strcmp(argument, "--warn")) { warn = 1; continue; }
        if (!strcmp(argument, "--help")) { return usage_advanced(exename); }
        if (!strcmp(argument, "--version")) { DISPLAY(WELCOME_MESSAGE(exename)); return 0; }

        if (*argument!='-') {
            if (filenamesStart==0) filenamesStart=i;   /* only supports a continuous list of filenames */
            continue;
        }

        /* command selection */
        argument++;   /* note: *argument=='-' */

        while (*argument!=0) {
            switch(*argument)
            {
            /* Display version */
            case 'V':
                DISPLAY(WELCOME_MESSAGE(exename)); return 0;

            /* Display help on usage */
            case 'h':
                return usage_advanced(exename);

            /* select hash algorithm */
            case 'H':
                algo = (algoType)(argument[1] - '0');
                argument+=2;
                if (!((algo >= algo_xxh32) && (algo <= algo_xxh128)))
                    return badusage(exename);
                break;

            /* File check mode */
            case 'c':
                fileCheckMode=1;
                argument++;
                break;

            /* Warning mode (file check mode only, alias of "--warning") */
            case 'w':
                warn=1;
                argument++;
                break;

            /* Trigger benchmark mode */
            case 'b':
                argument++;
                benchmarkMode = 1;
                specificTest = readU32FromChar(&argument); /* select one specific test */
                break;

            /* Modify Nb Iterations (benchmark only) */
            case 'i':
                argument++;
                g_nbIterations = readU32FromChar(&argument);
                break;

            /* Modify Block size (benchmark only) */
            case 'B':
                argument++;
                keySize = readU32FromChar(&argument);
                break;

            /* Modify verbosity of benchmark output (hidden option) */
            case 'q':
                argument++;
                g_displayLevel--;
                break;

            default:
                return badusage(exename);
            }
        }
    }   /* for(i=1; i<argc; i++) */

    /* Check benchmark mode */
    if (benchmarkMode) {
        DISPLAYLEVEL(2, WELCOME_MESSAGE(exename) );
        BMK_sanityCheck();
        if (filenamesStart==0) return BMK_benchInternal(keySize, specificTest);
        return BMK_benchFiles(argv+filenamesStart, argc-filenamesStart, specificTest);
    }

    /* Check if input is defined as console; trigger an error in this case */
    if ( (filenamesStart==0) && IS_CONSOLE(stdin) ) return badusage(exename);

    if (filenamesStart==0) filenamesStart = argc;
    if (fileCheckMode) {
        return checkFiles(argv+filenamesStart, argc-filenamesStart,
                          displayEndianess, strictMode, statusOnly, warn, (g_displayLevel < 2) /*quiet*/);
    } else {
        return BMK_hashFiles(argv+filenamesStart, argc-filenamesStart, algo, displayEndianess);
    }
}

#if defined(_WIN32)
/* Converts a UTF-16 argv to UTF-8. */
static char **convert_argv(int argc, wchar_t **argv)
{
    char **buf = (char **)malloc((size_t)(argc + 1) * sizeof(char *));
    if (buf != NULL) {
        int i;
        for (i = 0; i < argc; i++) {
            buf[i] = utf16_to_utf8(argv[i]);
        }
        buf[argc] = NULL;
    }
    return buf;
}
/* Frees arguments returned by convert_argv */
static void free_argv(int argc, char **argv)
{
    int i;
    if (argv == NULL) {
        return;
    }
    for (i = 0; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);
}

/*
 * The original MinGW doesn't define _O_U8TEXT unless __MSVCRT_VERSION__ is
 * defined to 0x0800 or higher, a.k.a. MSVC 2005.
 *
 * It is defined to 0x40000 on all Windows versions that support it, so we
 * just define it manually.
 *
 * Even if you are linking to a really old MSVC runtime, the worst thing that
 * can happen is that it silently errors and Unicode text doesn't appear in the
 * console. ASCII text would work as expected, and that is its primary usage.
 *
 * However, at least on Windows 10, this seems to work with msvcrt.dll.
 */
#ifndef _O_U8TEXT
#  define _O_U8TEXT 0x40000
#endif

/*
 * On Windows, main's argv parameter is useless. Instead of UTF-8, you get ANSI
 * encoding, and any unknown characters will show up as mojibake.
 *
 * While this doesn't affect most programs, what does happen is that we can't
 * open any files with Unicode filenames.
 *
 * On MSVC or when -municode is used in MSYS2, we can just use wmain to get
 * UTF-16 command line arguments and convert them to UTF-8.
 *
 * However, without the -municode flag (which isn't even available on the
 * original MinGW), we will get a linker error.
 *
 * To fix this, we can combine main with GetCommandLineW and CommandLineToArgvW
 * to get the real UTF-16 arguments.
 */
#if defined(_MSC_VER) || defined(_UNICODE) || defined(UNICODE)

#if defined(__cplusplus)
extern "C"
#endif
int wmain(int argc, wchar_t **utf16_argv)
{
    char **argv;
#else
int main(int argc, char **argv)
{
    wchar_t **utf16_argv = CommandLineToArgvW(GetCommandLineW(), &argc);
#endif
    int ret;
    /* Attempt to set stdin and stdout to UTF-8 mode. */
    const int oldStdoutMode = _setmode(_fileno(stdout), _O_U8TEXT);
    const int oldStderrMode = _setmode(_fileno(stderr), _O_U8TEXT);

    /* Convert the UTF-16 arguments to UTF-8. */
    argv = convert_argv(argc, utf16_argv);

    if (argv == NULL) {
        fprintf(stderr, "Error converting command line arguments!\n");
        /* return 1; */
        ret = 1;
    } else {
        /* While we're here, we will set stderr to unbuffered mode to make text
         * display instantly on MinGW. */
        setvbuf(stderr, NULL, _IONBF, 0);

        /* Call our real main function */
        ret = XXH_main(argc, argv);

        free_argv(argc, argv);
    }
#if !(defined(_MSC_VER) || defined(_UNICODE) || defined(UNICODE))
    /* CommandLineToArgvW needs to be freed with LocalFree. */
    LocalFree(utf16_argv);
#endif
    fflush(stdout); _setmode(_fileno(stdout), oldStdoutMode);
    fflush(stderr); _setmode(_fileno(stderr), oldStderrMode);
    return ret;
}

#else
int main(int argc, char **argv)
{
    return XXH_main(argc, argv);
}
#endif
