/*
   xxHash - Extremely Fast Hash algorithm
   Development source file for `xxh3`
   Copyright (C) 2019-present, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
   - xxHash source repository : https://github.com/Cyan4973/xxHash
*/

/* This is a private header which implements CPU dispatch code. */
#ifndef XXH3_DISPATCH_H
#define XXH3_DISPATCH_H

#define XXH_TARGET_MODE_NONE 0
#define XXH_TARGET_MODE_X86 1
#define XXH_TARGET_MODE_ARM 2

/* Figure out the best way to get a cpuid. */
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_AMD64) || defined(_M_X64))
 /* MSVC has intrinsics for this. */
#   include <intrin.h>
#   define XXH_CPUID __cpuid
#   define XXH_CPUIDEX __cpuidex
#   define XXH_TARGET_MODE XXH_TARGET_MODE_X86
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))

/* The GCC family can easily use inline assembly. */
static void XXH_CPUIDEX(int* cpuInfo, int function_id, int function_ext)
{
    int eax, ecx;
    eax = function_id;
    ecx = function_ext;
    __asm__ __volatile__(
#ifdef (__x86_64__) /* EBX is required for PIC so save it */
# define EBX "q %%rbx\n"
#else
# define EBX "l %%ebx\n"
#endif
        "push" EBX
        "cpuid\n"
        "movl %%ebx, %1\n"
        "pop" EBX
      : "+a" (eax), "=r" (cpuInfo[1]), "+c" (ecx), "=d" (cpuInfo[3]));
#undef EBX
    cpuInfo[0] = eax;
    cpuInfo[2] = ecx;
}
static void XXH_CPUID(int *cpuInfo, int function_id)
{
    XXH_CPUIDEX(cpuInfo, function_id, 0);
}
#  define XXH_TARGET_MODE XXH_TARGET_MODE_X86
/* ARM Linux */
#elif defined(__linux__) && (defined(__arm__) || defined(__thumb__) || defined(__thumb2__))
#  include <stdio.h> /* ARM needs fopen, fclose, and fgets */
#  define XXH_TARGET_MODE XXH_TARGET_MODE_ARM
#else
#  define XXH_TARGET_MODE XXH_TARGET_MODE_NONE
#  undef XXH_MULTI_TARGET
#endif

#if defined(XXH_MULTI_TARGET)
/* Prototypes for our code. Because the code is in separate files, we need a symbol.
 * The reserved identifiers are intentional. */
#ifdef __cplusplus
extern "C" {
#endif
void _XXH3_hashLong_AVX2(U64* restrict acc, const void* restrict data, size_t len, const U32* restrict key);
void _XXH3_hashLong_SSE2(U64* restrict acc, const void* restrict data, size_t len, const U32* restrict key);
void _XXH3_hashLong_Scalar(U64* restrict acc, const void* restrict data, size_t len, const U32* restrict key);
void _XXH3_hashLong_NEON(U64* restrict acc, const void* restrict data, size_t len, const U32* restrict key);
#ifdef __cplusplus
}
#endif

/* What hashLong version we decided on. cpuid is a SLOW instruction -- calling it takes anywhere
 * from 30-40 to THOUSANDS of cycles), so we really don't want to call it more than once. */
static XXH_cpu_mode_t cpu_mode = XXH_CPU_MODE_AUTO;

/* What the best version supported is. This is used for verification on XXH_setCpuMode to prevent
 * a SIGILL. It can be turned off with -DXXH_NO_VERIFY_MULTI_TARGET, in which the selected hash will
 * be used unconditionally. */
static XXH_cpu_mode_t supported_cpu_mode = XXH_CPU_MODE_AUTO;

typedef void (*XXH3_hashLong_t)(U64* restrict acc, const void* restrict data, size_t len, const U32* restrict key);


/* We need to add a prototype for XXH3_dispatcher so we can refer to it later. */
static void
XXH3_dispatcher(U64* restrict acc, const void* restrict data, size_t len, const U32* restrict key);

/* We also store this as a function pointer, so we can just jump to it at runtime. */
static XXH3_hashLong_t XXH3_hashLong = &XXH3_dispatcher;


/* Tests features for x86 targets and sets the cpu_mode and the XXH3_hashLong function pointer
 * to the correct value.
 *
 * On GCC compatible compilers, this will be run at program startup.
 *
 * xxh3-target.c will include this file. If we don't do this, the constructor will be called
 * multiple times. We don't want that. */
#if defined(__GNUC__) && !defined(XXH3_TARGET_C)
__attribute__((__constructor__))
#endif
static XXH3_hashLong_t XXH3_featureTest(void)
{
    if (supported_cpu_mode != XXH_CPU_MODE_AUTO) {
#if XXH_TARGET_MODE == XXH_TARGET_MODE_X86
        if (supported_cpu_mode == XXH_CPU_MODE_AVX2) {
            cpu_mode = XXH_CPU_MODE_AVX2;
            XXH3_hashLong = &_XXH3_hashLong_AVX2;
        } else if (supported_cpu_mode == XXH_CPU_MODE_SSE2 {
            cpu_mode = XXH_CPU_MODE_SSE2;
            XXH3_hashLong = &_XXH3_hashLong_SSE2;
        } else
#elif XXH_TARGET_MODE == XXH_TARGET_MODE_ARM
        if (supported_cpu_mode == XXH_CPU_MODE_NEON) {
            cpu_mode = XXH_CPU_MODE_NEON;
            XXH3_hashLong = &_XXH3_hashLong_NEON;
        } else
#endif
        {
            cpu_mode = XXH_CPU_MODE_SCALAR;
            XXH3_hashLong = &_XXH3_hashLong_Scalar;
        }
        return XXH3_hashLong;
    }

#if XXH_TARGET_MODE == XXH_TARGET_MODE_X86
    {
        int max, data[4];

        /* First, get how many CPUID function parameters there are by calling CPUID with eax = 0. */
        XXH_CPUID(data, /* eax */ 0);
        max = data[0];
        /* AVX2 is on the Extended Features page (eax = 7, ecx = 0), on bit 5 of ebx. */
        if (max >= 7) {
            XXH_CPUIDEX(data, /* eax */ 7, /* ecx */ 0);
            if (data[1] & (1 << 5)) {
                cpu_mode = supported_cpu_mode = XXH_CPU_MODE_AVX2;
                XXH3_hashLong = &_XXH3_hashLong_AVX2;
                return XXH3_hashLong;
            }
        }
        /* SSE2 is on the Processor Info and Feature Bits page (eax = 1), on bit 26 of edx. */
        if (max >= 1) {
            XXH_CPUID(data, /* eax */ 1);
            if (data[3] & (1 << 26)) {
                cpu_mode = supported_cpu_mode = XXH_CPU_MODE_SSE2;
                XXH3_hashLong = &_XXH3_hashLong_SSE2;
                return XXH3_hashLong;
            }
        }
    }
#elif XXH_TARGET_MODE == XXH_TARGET_MODE_ARM
    {
        /* Parse /proc/cpuinfo. This is the best version on Linux because the test
         * instruction needs privledged mode for no apparent reason. The cpufeatures
         * library from the NDK also uses this method. */
        char buf[1024];
        FILE *f = fopen("/proc/cpuinfo", "r");

        if (f != NULL) {
            while (fgets(buf, 1024, f) != NULL) {
                if (strstr(buf, "neon") != NULL || strstr(buf, "asimd") != NULL) {
                    fclose(f);

                    cpu_mode = supported_cpu_mode = XXH_CPU_MODE_NEON;
                    XXH3_hashLong = &_XXH3_hashLong_NEON;
                    return XXH3_hashLong;
                }
            }
            fclose(f);
        }
    }
#endif
    /* Must be scalar. */
    cpu_mode = supported_cpu_mode = XXH_CPU_MODE_SCALAR;
    XXH3_hashLong = &_XXH3_hashLong_Scalar;
    return XXH3_hashLong;
}

/* Sets up the dispatcher and then calls the actual hash function. */
static void
XXH3_dispatcher(U64* restrict acc, const void* restrict data, size_t len, const U32* restrict key)
{
    /* We haven't checked CPUID yet, so we check it now. On GCC, we try to get this to run
     * at program startup to hide our very dirty secret from the benchmarks. */
    return (XXH3_featureTest())(acc, data, len, key);
}

/* Sets the XXH3_hashLong variant. When XXH_MULTI_TARGET is not defined, this
 * does nothing.
 *
 * Unless XXH_NO_VERIFY_MULTI_TARGET is defined, this will automatically fall back
 * to the next best XXH3 mode, so, for example, even if you set it to AVX2, the code
 * will not crash even if it is run on, for example, a Core 2 Duo which doesn't support
 * AVX2. */
XXH_PUBLIC_API XXH_cpu_mode_t XXH3_setCpuMode(XXH_cpu_mode_t mode)
{
/* Defining XXH_NO_VERIFY_MULTI_TARGET will allow you to set the CPU mode to
 * an unsupported mode. */
#ifndef XXH_NO_VERIFY_MULTI_TARGET
    /* Call the featureTest if it hasn't been called already */
    if (supported_cpu_mode == XXH_CPU_MODE_AUTO)
        XXH3_featureTest();
/* Even thougu XXH_CPU_MODE_NEON is greater than that, it will never
 * be defined */
#   define TRY_SET_MODE(mode_num, funcptr) \
    do { \
        if (mode == (mode_num) && supported_cpu_mode >= (mode_num)) { \
            cpu_mode = (mode_num); \
            XXH3_hashLong = &(funcptr); \
            return (mode_num); \
        } \
   } while (0)
#else
#   define TRY_SET_MODE(mode_num, funcptr) \
   do { \
       if (mode == (mode_num)) { \
            cpu_mode = (mode_num); \
            XXH3_hashLong = &(funcptr); \
            return (mode_num); \
       }
    } while (0)
#endif

#if XXH_TARGET_MODE == XXH_TARGET_MODE_X86
    TRY_SET_MODE(XXH_CPU_MODE_AVX2, _XXH3_hashLong_AVX2);
    TRY_SET_MODE(XXH_CPU_MODE_SSE2, _XXH3_hashLong_SSE2);
#elif XXH_TARGET_MODE == XXH_TARGET_MODE_ARM
    TRY_SET_MODE(XXH_CPU_MODE_NEON, _XXH3_hashLong_NEON);
#endif
    if (mode == XXH_CPU_MODE_SCALAR) {
        cpu_mode = XXH_CPU_MODE_SCALAR;
        XXH3_hashLong = &_XXH3_hashLong_Scalar;
        return XXH_CPU_MODE_SCALAR;
    }
    cpu_mode = XXH_CPU_MODE_AUTO;
    XXH3_hashLong = &XXH3_dispatcher;
    return XXH_CPU_MODE_AUTO;
#undef TRY_SET_MODE
}

/* Should we keep this? */
XXH_PUBLIC_API XXH_cpu_mode_t XXH3_getCpuMode(void)
{
    return cpu_mode;
}
#endif /* XXH_MULTI_TARGET && !XXH3_TARGET_C */
#endif /* XXH3_DISPATCH_H */