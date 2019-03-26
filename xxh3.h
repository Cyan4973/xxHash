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

/* Note :
   This file is separated for development purposes.
   It will be integrated into `xxhash.c` when development phase is complete.
*/

#ifndef XXH3_H
#define XXH3_H


/* ===   Dependencies   === */

#undef XXH_INLINE_ALL   /* in case it's already defined */
#define XXH_INLINE_ALL
#include "xxhash.h"

#undef NDEBUG   /* avoid redefinition */
#define NDEBUG
#include <assert.h>


/* ===   Compiler versions   === */

#if !(defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)   /* C99+ */
#  define restrict   /* disable */
#endif

#if defined(__GNUC__)
#  if defined(__SSE2__)
#    include <x86intrin.h>
#  elif defined(__ARM_NEON__) || defined(__ARM_NEON)
#    define inline __inline__ /* clang bug */
#    include <arm_neon.h>
#    undef inline
#  endif
#  define ALIGN(n)      __attribute__ ((aligned(n)))
#elif defined(_MSC_VER)
#  include <intrin.h>
#  define ALIGN(n)      __declspec(align(n))
#else
#  define ALIGN(n)   /* disabled */
#endif

/* U64 XXH_mult32to64(U32 a, U64 b) { return (U64)a * (U64)b; } */
#ifdef _MSC_VER
#   include <intrin.h>
    /* MSVC doesn't do a good job with the mull detection. */
#   define XXH_mult32to64 __emulu
#else
#   define XXH_mult32to64(x, y) ((U64)((x) & 0xFFFFFFFF) * (U64)((y) & 0xFFFFFFFF))
#endif

/* ==========================================
 * XXH3 default settings
 * ========================================== */

#define KEYSET_DEFAULT_SIZE 48   /* minimum 32 */


ALIGN(64) static const U32 kKey[KEYSET_DEFAULT_SIZE] = {
    0xb8fe6c39,0x23a44bbe,0x7c01812c,0xf721ad1c,
    0xded46de9,0x839097db,0x7240a4a4,0xb7b3671f,
    0xcb79e64e,0xccc0e578,0x825ad07d,0xccff7221,
    0xb8084674,0xf743248e,0xe03590e6,0x813a264c,
    0x3c2852bb,0x91c300cb,0x88d0658b,0x1b532ea3,
    0x71644897,0xa20df94e,0x3819ef46,0xa9deacd8,
    0xa8fa763f,0xe39c343f,0xf9dcbbc7,0xc70b4f1d,
    0x8a51e04b,0xcdb45931,0xc89f7ec9,0xd9787364,

    0xeac5ac83,0x34d3ebc3,0xc581a0ff,0xfa1363eb,
    0x170ddd51,0xb7f0da49,0xd3165526,0x29d4689e,
    0x2b16be58,0x7d47a1fc,0x8ff8b8d1,0x7ad031ce,
    0x45cb3a8f,0x95160428,0xafd7fbca,0xbb4b407e,
};


#if defined(__GNUC__) && defined(__i386__)
/* GCC is stupid and tries to vectorize this.
 * This tells GCC that it is wrong. */
__attribute__((__target__("no-sse")))
#endif
static U64
XXH3_mul128_fold64(U64 ll1, U64 ll2)
{
#if defined(__SIZEOF_INT128__) || (defined(_INTEGRAL_MAX_BITS) && _INTEGRAL_MAX_BITS >= 128)

    __uint128_t lll = (__uint128_t)ll1 * ll2;
    return (U64)lll ^ (U64)(lll >> 64);

#elif defined(_M_X64) || defined(_M_IA64)

#ifndef _MSC_VER
#   pragma intrinsic(_umul128)
#endif
    U64 llhigh;
    U64 const lllow = _umul128(ll1, ll2, &llhigh);
    return lllow ^ llhigh;

    /* Do it out manually on 32-bit.
     * This is a modified, unrolled, widened, and optimized version of the
     * mulqdu routine from Hacker's Delight.
     *
     *   https://www.hackersdelight.org/hdcodetxt/mulqdu.c.txt
     *
     * This was modified to use U32->U64 multiplication instead
     * of U16->U32, to add the high and low values in the end,
     * be endian-independent, and I added a partial assembly
     * implementation for ARM. */

    /* An easy 128-bit folding multiply on ARMv6T2 and ARMv7-A/R can be done with
     * the mighty umaal (Unsigned Multiply Accumulate Accumulate Long) which takes 4 cycles
     * or less, doing a long multiply and adding two 32-bit integers:
     *
     *     void umaal(U32 *RdLo, U32 *RdHi, U32 Rn, U32 Rm)
     *     {
     *         U64 prodAcc = (U64)Rn * (U64)Rm;
     *         prodAcc += *RdLo;
     *         prodAcc += *RdHi;
     *         *RdLo = prodAcc & 0xFFFFFFFF;
     *         *RdHi = prodAcc >> 32;
     *     }
     *
     * This is compared to umlal which adds to a single 64-bit integer:
     *
     *     void umlal(U32 *RdLo, U32 *RdHi, U32 Rn, U32 Rm)
     *     {
     *         U64 prodAcc = (U64)Rn * (U64)Rm;
     *         prodAcc += (*RdLo | ((U64)*RdHi << 32);
     *         *RdLo = prodAcc & 0xFFFFFFFF;
     *         *RdHi = prodAcc >> 32;
     *     }
     *
     * Getting the compiler to emit them is like pulling teeth, and checking
     * for it is annoying because ARMv7-M lacks this instruction. However, it
     * is worth it, because this is an otherwise expensive operation. */

     /* GCC-compatible, ARMv6t2 or ARMv7+, non-M variant, and 32-bit */
#elif defined(__GNUC__) /* GCC-compatible */ \
    && defined(__ARM_ARCH) && !defined(__aarch64__) && !defined(__arm64__) /* 32-bit ARM */\
    && !defined(__ARM_ARCH_7M__) /* <- Not ARMv7-M  vv*/ \
        && !(defined(__TARGET_ARCH_ARM) && __TARGET_ARCH_ARM == 0 && __TARGET_ARCH_THUMB == 4) \
    && (defined(__ARM_ARCH_6T2__) || __ARM_ARCH > 6) /* ARMv6T2 or later */

    U32 w[4] = { 0 };
    U32 u[2] = { (U32)(ll1 >> 32), (U32)ll1 };
    U32 v[2] = { (U32)(ll2 >> 32), (U32)ll2 };
    U32 k;

    /* U64 t = (U64)u[1] * (U64)v[1];
     * w[3] = t & 0xFFFFFFFF;
     * k = t >> 32; */
    __asm__("umull %0, %1, %2, %3"
            : "=r" (w[3]), "=r" (k)
            : "r" (u[1]), "r" (v[1]));

    /* t = (U64)u[0] * (U64)v[1] + w[2] + k;
     * w[2] = t & 0xFFFFFFFF;
     * k = t >> 32; */
    __asm__("umaal %0, %1, %2, %3"
            : "+r" (w[2]), "+r" (k)
            : "r" (u[0]), "r" (v[1]));
    w[1] = k;
    k = 0;

    /* t = (U64)u[1] * (U64)v[0] + w[2] + k;
     * w[2] = t & 0xFFFFFFFF;
     * k = t >> 32; */
    __asm__("umaal %0, %1, %2, %3"
            : "+r" (w[2]), "+r" (k)
            : "r" (u[1]), "r" (v[0]));

    /* t = (U64)u[0] * (U64)v[0] + w[1] + k;
     * w[1] = t & 0xFFFFFFFF;
     * k = t >> 32; */
    __asm__("umaal %0, %1, %2, %3"
            : "+r" (w[1]), "+r" (k)
            : "r" (u[0]), "r" (v[0]));
    w[0] = k;

    return (w[1] | ((U64)w[0] << 32)) ^ (w[3] | ((U64)w[2] << 32));

#else /* Portable scalar version */

    /* emulate 64x64->128b multiplication, using four 32x32->64 */
    U32 const h1 = (U32)(ll1 >> 32);
    U32 const h2 = (U32)(ll2 >> 32);
    U32 const l1 = (U32)ll1;
    U32 const l2 = (U32)ll2;

    U64 const llh  = XXH_mult32to64(h1, h2);
    U64 const llm1 = XXH_mult32to64(l1, h2);
    U64 const llm2 = XXH_mult32to64(h1, l2);
    U64 const lll  = XXH_mult32to64(l1, l2);

    U64 const t = lll + (llm1 << 32);
    U64 const carry1 = t < lll;

    U64 const lllow = t + (llm2 << 32);
    U64 const carry2 = lllow < t;
    U64 const llhigh = llh + (llm1 >> 32) + (llm2 >> 32) + carry1 + carry2;

    return llhigh ^ lllow;

#endif
}


static XXH64_hash_t XXH3_avalanche(U64 h64)
{
    h64 ^= h64 >> 37;
    h64 *= PRIME64_3;
    h64 ^= h64 >> 32;
    return h64;
}


/* ==========================================
 * Short keys
 * ========================================== */

XXH_FORCE_INLINE XXH64_hash_t
XXH3_len_1to3_64b(const void* data, size_t len, const void* keyPtr, XXH64_hash_t seed)
{
    assert(data != NULL);
    assert(len > 0 && len <= 3);
    assert(keyPtr != NULL);
    {   const U32* const key32 = (const U32*) keyPtr;
        BYTE const c1 = ((const BYTE*)data)[0];
        BYTE const c2 = ((const BYTE*)data)[len >> 1];
        BYTE const c3 = ((const BYTE*)data)[len - 1];
        U32  const l1 = (U32)(c1) + ((U32)(c2) << 8);
        U32  const l2 = (U32)(len) + ((U32)(c3) << 2);
        U64  const ll11 = XXH_mult32to64(l1 + (U32)seed + key32[0],
                                         l2 + (U32)(seed >> 32) + key32[1]);
        return XXH3_avalanche(ll11);
    }
}

XXH_FORCE_INLINE XXH64_hash_t
XXH3_len_4to8_64b(const void* data, size_t len, const void* keyPtr, XXH64_hash_t seed)
{
    assert(data != NULL);
    assert(key != NULL);
    assert(len >= 4 && len <= 8);
    {   const U32* const key32 = (const U32*) keyPtr;
        U32 const l1 = XXH_readLE32(data) + key32[0];
        U32 const l2 = XXH_readLE32((const BYTE*)data + len - 4) ^ key32[1];
        U64 const ll1 = (l1 ^ (l2>>3)) + ((U64)l2 << 32) + seed;
        U64 const ll11 = len + (ll1 * PRIME64_1);
        return XXH3_avalanche(ll11);
    }
}

XXH_FORCE_INLINE U64
XXH3_readKey64(const void* ptr)
{
    assert(((size_t)ptr & 7) == 0);   /* aligned on 8-bytes boundaries */
    if (XXH_CPU_LITTLE_ENDIAN) {
        return *(const U64*)ptr;
    } else {
        const U32* const ptr32 = (const U32*)ptr;
        return (U64)ptr32[0] + (((U64)ptr32[1]) << 32);
    }
}

XXH_FORCE_INLINE XXH64_hash_t
XXH3_len_9to16_64b(const void* data, size_t len, const void* keyPtr, XXH64_hash_t seed)
{
    assert(data != NULL);
    assert(key != NULL);
    assert(len >= 9 && len <= 16);
    {   const U64* const key64 = (const U64*) keyPtr;
        U64 const ll1 = XXH_readLE64(data) ^ (XXH3_readKey64(key64) + seed);
        U64 const ll2 = XXH_readLE64((const BYTE*)data + len - 8) ^ (XXH3_readKey64(key64+1) - seed);
        U64 const acc = len + (ll1 + ll2) + XXH3_mul128_fold64(ll1, ll2);
        return XXH3_avalanche(acc);
    }
}

XXH_FORCE_INLINE XXH64_hash_t
XXH3_len_0to16_64b(const void* data, size_t len, XXH64_hash_t seed)
{
    assert(data != NULL);
    assert(len <= 16);
    {   if (len > 8) return XXH3_len_9to16_64b(data, len, kKey, seed);
        if (len >= 4) return XXH3_len_4to8_64b(data, len, kKey, seed);
        if (len) return XXH3_len_1to3_64b(data, len, kKey, seed);
        return seed;
    }
}

#ifdef XXH_MULTI_TARGET
/* Figure out the best way to get a cpuid. */
#  if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_AMD64) || defined(_M_X64))
   /* MSVC has intrinsics for this. */
#     include <intrin.h>
#     define XXH_CPUID __cpuid
#     define XXH_CPUIDEX __cpuidext
#  elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
   /* The GCC family can easily use inline assembly. */
   static void XXH_CPUIDEX(int* cpuInfo, int function_id, int function_ext)
   {
       int eax, ecx;
       eax = function_id;
       ecx = function_ext;
       __asm__ __volatile__("cpuid"
         : "+a" (eax), "=b" (cpuInfo[1]), "+c" (ecx), "=d" (cpuInfo[3]));
       cpuInfo[0] = eax;
       cpuInfo[2] = ecx;
   }
   static void XXH_CPUID(int *cpuInfo, int function_id)
   {
       XXH_CPUIDEX(cpuInfo, function_id, 0);
   }
#  else
   /* We don't know how to do this, just let them use the default. */
#  undef XXH_MULTI_TARGET
   static void XXH_CPUIDEX(int* cpuInfo, int function_id, int function_ext)
   {
   }
   static void XXH_CPUID(int* cpuInfo, int function_id)
   {
   }
#  endif /* else */
#endif /* XXH_MULTI_TARGET */

/* ===    Long Keys    === */

#define STRIPE_LEN 64
#define STRIPE_ELTS (STRIPE_LEN / sizeof(U32))
#define ACC_NB (STRIPE_LEN / sizeof(U64))

#ifdef XXH_MULTI_TARGET

/* Prototypes for our code */
#ifdef __cplusplus
extern "C" {
#endif
void _XXH3_hashLong_AVX2(U64* acc, const void* data, size_t len, const U32* key);
void _XXH3_hashLong_SSE2(U64* acc, const void* data, size_t len, const U32* key);
void _XXH3_hashLong_Scalar(U64* acc, const void* data, size_t len, const U32* key);
#ifdef __cplusplus
}
#endif

/* What hashLong version we decided on. cpuid is a SLOW instruction -- calling it takes anywhere
 * from 30-40 to THOUSANDS of cycles), so we really don't want to call it more than once. */
static XXH_cpu_mode_t cpu_mode = XXH_CPU_MODE_AUTO;

/* xxh3-target.c will include this file. If we don't do this, the constructor will be called
 * multiple times. We don't want that. */
#if !defined(XXH3_TARGET_C) && defined(__GNUC__)
__attribute__((__constructor__))
#endif
static void
XXH3_featureTest(void)
{
    int max, data[4];
    /* First, get how many CPUID function parameters there are by calling CPUID with eax = 0. */
    XXH_CPUID(data, /* eax */ 0);
    max = data[0];
    /* AVX2 is on the Extended Features page (eax = 7, ecx = 0), on bit 5 of ebx. */
    if (max >= 7) {
        XXH_CPUIDEX(data, /* eax */ 7, /* ecx */ 0);
        if (data[1] & (1 << 5)) {
            cpu_mode = XXH_CPU_MODE_AVX2;
            return;
        }
    }
    /* SSE2 is on the Processor Info and Feature Bits page (eax = 1), on bit 26 of edx. */
    if (max >= 1) {
        XXH_CPUID(data, /* eax */ 1);
        if (data[3] & (1 << 26)) {
            cpu_mode = XXH_CPU_MODE_SSE2;
            return;
        }
    }
    /* Must be scalar. */
    cpu_mode = XXH_CPU_MODE_SCALAR;
}

static void
XXH3_hashLong(U64* restrict acc, const void* restrict data, size_t len, const U32* restrict key)
{
    /* We haven't checked CPUID yet, so we check it now. On GCC, we try to get this to run
     * at program startup to hide our very dirty secret from the benchmarks. */
    if (cpu_mode == XXH_CPU_MODE_AUTO) {
        XXH3_featureTest();
    }
    switch (cpu_mode) {
    case XXH_CPU_MODE_AVX2:
        _XXH3_hashLong_AVX2(acc, data, len, key);
        return;
    case XXH_CPU_MODE_SSE2:
         _XXH3_hashLong_SSE2(acc, data, len, key);
         return;
    default:
         _XXH3_hashLong_Scalar(acc, data, len, key);
         return;
    }
}
#else /* !XXH_MULTI_TARGET */
   /* Include the C file directly and let the compiler decide which implementation to use. */
#  include "xxh3-target.c"
#endif /* XXH_MULTI_TARGET */

/* Should we keep this? */
XXH_PUBLIC_API void XXH3_forceCpuMode(XXH_cpu_mode_t mode)
{
#ifdef XXH_MULTI_TARGET
    cpu_mode = mode;
#endif
}


XXH_FORCE_INLINE U64 XXH3_mix2Accs(const U64* acc, const void* key)
{
    const U64* const key64 = (const U64*)key;
    return XXH3_mul128_fold64(
               acc[0] ^ XXH3_readKey64(key64),
               acc[1] ^ XXH3_readKey64(key64+1) );
}

static XXH64_hash_t XXH3_mergeAccs(const U64* acc, const U32* key, U64 start)
{
    U64 result64 = start;

    result64 += XXH3_mix2Accs(acc+0, key+0);
    result64 += XXH3_mix2Accs(acc+2, key+4);
    result64 += XXH3_mix2Accs(acc+4, key+8);
    result64 += XXH3_mix2Accs(acc+6, key+12);

    return XXH3_avalanche(result64);
}


XXH_FORCE_INLINE void XXH3_initKeySeed(U32* key, U64 seed64)
{
    U32 const seed1 = (U32)seed64;
    U32 const seed2 = (U32)(seed64 >> 32);
    int i;
    assert(KEYSET_DEFAULT_SIZE & 3 == 0);
    for (i=0; i < KEYSET_DEFAULT_SIZE; i+=4) {
        key[i+0] = kKey[i+0] + seed1;
        key[i+1] = kKey[i+1] - seed2;
        key[i+2] = kKey[i+2] + seed2;
        key[i+3] = kKey[i+3] - seed1;
    }
}

XXH_NO_INLINE XXH64_hash_t    /* It's important for performance that XXH3_hashLong is not inlined. Not sure why (uop cache maybe ?), but difference is large and easily measurable */
XXH3_hashLong_64b(const void* data, size_t len, XXH64_hash_t seed)
{
    ALIGN(64) U64 acc[ACC_NB] = { seed, PRIME64_1, PRIME64_2, PRIME64_3, PRIME64_4, PRIME64_5, (U64)0 - seed, 0 };
    ALIGN(64) U32 key[KEYSET_DEFAULT_SIZE];

    XXH3_initKeySeed(key, seed);

    XXH3_hashLong(acc, data, len, kKey);

    /* converge into final hash */
    assert(sizeof(acc) == 64);
    return XXH3_mergeAccs(acc, key, (U64)len * PRIME64_1);
}


XXH_FORCE_INLINE U64 XXH3_mix16B(const void* data, const void* key, U64 seed64)
{
    const U64* const key64 = (const U64*)key;
    U64 const ll1 = XXH_readLE64(data);
    U64 const ll2 = XXH_readLE64((const BYTE*)data+8);
    return XXH3_mul128_fold64(
               ll1 ^ (XXH3_readKey64(key64)   + seed64),
               ll2 ^ (XXH3_readKey64(key64+1) - seed64) ) ;
}
#undef XXH_PUBLIC_API
#define XXH_PUBLIC_API

/* ===   Public entry point   === */

XXH_PUBLIC_API XXH64_hash_t
XXH3_64bits_withSeed(const void* data, size_t len, XXH64_hash_t seed)
{
    const BYTE* const p = (const BYTE*)data;
    const char* const key = (const char*)kKey;

    if (len <= 16) return XXH3_len_0to16_64b(data, len, seed);

    {   U64 acc = len * PRIME64_1;
        if (len > 32) {
            if (len > 64) {
                if (len > 96) {
                    if (len > 128) return XXH3_hashLong_64b(data, len, seed);

                    acc += XXH3_mix16B(p+48, key+96, seed);
                    acc += XXH3_mix16B(p+len-64, key+112, seed);
                }

                acc += XXH3_mix16B(p+32, key+64, seed);
                acc += XXH3_mix16B(p+len-48, key+80, seed);
            }

            acc += XXH3_mix16B(p+16, key+32, seed);
            acc += XXH3_mix16B(p+len-32, key+48, seed);
        }

        acc += XXH3_mix16B(p+0, key+0, seed);
        acc += XXH3_mix16B(p+len-16, key+16, seed);

        return XXH3_avalanche(acc);
    }
}


XXH_PUBLIC_API XXH64_hash_t XXH3_64bits(const void* data, size_t len)
{
    return XXH3_64bits_withSeed(data, len, 0);
}



/* ==========================================
 * XXH3 128 bits (=> XXH128)
 * ========================================== */

XXH_FORCE_INLINE XXH128_hash_t
XXH3_len_1to3_128b(const void* data, size_t len, const void* keyPtr, XXH64_hash_t seed)
{
    assert(data != NULL);
    assert(len > 0 && len <= 3);
    assert(keyPtr != NULL);
    {   const U32* const key32 = (const U32*) keyPtr;
        BYTE const c1 = ((const BYTE*)data)[0];
        BYTE const c2 = ((const BYTE*)data)[len >> 1];
        BYTE const c3 = ((const BYTE*)data)[len - 1];
        U32  const l1 = (U32)(c1) + ((U32)(c2) << 8);
        U32  const l2 = (U32)(len) + ((U32)(c3) << 2);
        U64  const ll11 = XXH_mult32to64(l1 + seed + key32[0], l2 + key32[1]);
        U64  const ll12 = XXH_mult32to64(l1 + key32[2], l2 - seed + key32[3]);
        XXH128_hash_t const h128 = { XXH3_avalanche(ll11), XXH3_avalanche(ll12) };
        return h128;
    }
}


XXH_FORCE_INLINE XXH128_hash_t
XXH3_len_4to8_128b(const void* data, size_t len, const void* keyPtr, XXH64_hash_t seed)
{
    assert(data != NULL);
    assert(len >= 4 && len <= 8);
    {   const U32* const key32 = (const U32*) keyPtr;
        U32 const l1 = XXH_readLE32(data) + (U32)seed + key32[0];
        U32 const l2 = XXH_readLE32((const BYTE*)data + len - 4) + (U32)(seed >> 32) + key32[1];
        U64 const acc1 = len + l1 + ((U64)l2 << 32) + XXH_mult32to64(l1, l2);
        U64 const acc2 = len*PRIME64_1 + l1*PRIME64_2 + l2*PRIME64_3;
        {   XXH128_hash_t const h128 = { XXH3_avalanche(acc1), XXH3_avalanche(acc2) };
            return h128;
        }
    }
}

XXH_FORCE_INLINE XXH128_hash_t
XXH3_len_9to16_128b(const void* data, size_t len, const void* keyPtr, XXH64_hash_t seed)
{
    assert(data != NULL);
    assert(key != NULL);
    assert(len >= 9 && len <= 16);
    {   const U64* const key64 = (const U64*) keyPtr;
        U64 acc1 = PRIME64_1 * ((U64)len + seed);
        U64 acc2 = PRIME64_2 * ((U64)len - seed);
        U64 const ll1 = XXH_readLE64(data);
        U64 const ll2 = XXH_readLE64((const BYTE*)data + len - 8);
        acc1 += XXH3_mul128_fold64(ll1 + XXH3_readKey64(key64+0), ll2 + XXH3_readKey64(key64+1));
        acc2 += XXH3_mul128_fold64(ll1 + XXH3_readKey64(key64+2), ll2 + XXH3_readKey64(key64+3));
        {   XXH128_hash_t const h128 = { XXH3_avalanche(acc1), XXH3_avalanche(acc2) };
            return h128;
        }
    }
}

XXH_FORCE_INLINE XXH128_hash_t
XXH3_len_0to16_128b(const void* data, size_t len, XXH64_hash_t seed)
{
    assert(data != NULL);
    assert(len <= 16);
    {   if (len > 8) return XXH3_len_9to16_128b(data, len, kKey, seed);
        if (len >= 4) return XXH3_len_4to8_128b(data, len, kKey, seed);
        if (len) return XXH3_len_1to3_128b(data, len, kKey, seed);
        {   XXH128_hash_t const h128 = { seed, (XXH64_hash_t)0 - seed };
            return h128;
        }
    }
}

XXH_NO_INLINE XXH128_hash_t    /* It's important for performance that XXH3_hashLong is not inlined. Not sure why (uop cache maybe ?), but difference is large and easily measurable */
XXH3_hashLong_128b(const void* data, size_t len, XXH64_hash_t seed)
{
    ALIGN(64) U64 acc[ACC_NB] = { seed, PRIME64_1, PRIME64_2, PRIME64_3, PRIME64_4, PRIME64_5, (U64)0 - seed, 0 };
    assert(len > 128);

    XXH3_hashLong(acc, data, len, kKey);

    /* converge into final hash */
    assert(sizeof(acc) == 64);
    {   U64 const low64 = XXH3_mergeAccs(acc, kKey, (U64)len * PRIME64_1);
        U64 const high64 = XXH3_mergeAccs(acc, kKey+16, ((U64)len+1) * PRIME64_2);
        XXH128_hash_t const h128 = { low64, high64 };
        return h128;
    }
}

XXH_PUBLIC_API XXH128_hash_t
XXH3_128bits_withSeed(const void* data, size_t len, XXH64_hash_t seed)
{
    if (len <= 16) return XXH3_len_0to16_128b(data, len, seed);

    {   U64 acc1 = PRIME64_1 * (len + seed);
        U64 acc2 = 0;
        const BYTE* const p = (const BYTE*)data;
        const char* const key = (const char*)kKey;
        if (len > 32) {
            if (len > 64) {
                if (len > 96) {
                    if (len > 128) return XXH3_hashLong_128b(data, len, seed);

                    acc1 += XXH3_mix16B(p+48, key+96, seed);
                    acc2 += XXH3_mix16B(p+len-64, key+112, seed);
                }

                acc1 += XXH3_mix16B(p+32, key+64, seed);
                acc2 += XXH3_mix16B(p+len-48, key+80, seed);
            }

            acc1 += XXH3_mix16B(p+16, key+32, seed);
            acc2 += XXH3_mix16B(p+len-32, key+48, seed);

        }

        acc1 += XXH3_mix16B(p+0, key+0, seed);
        acc2 += XXH3_mix16B(p+len-16, key+16, seed);

        {   U64 const part1 = acc1 + acc2;
            U64 const part2 = (acc1 * PRIME64_3) + (acc2 * PRIME64_4) + ((len - seed) * PRIME64_2);
            XXH128_hash_t const h128 = { XXH3_avalanche(part1), (XXH64_hash_t)0 - XXH3_avalanche(part2) };
            return h128;
        }
    }
}


XXH_PUBLIC_API XXH128_hash_t XXH3_128bits(const void* data, size_t len)
{
    return XXH3_128bits_withSeed(data, len, 0);
}


XXH_PUBLIC_API XXH128_hash_t XXH128(const void* data, size_t len, XXH64_hash_t seed)
{
    return XXH3_128bits_withSeed(data, len, seed);
}

#endif  /* XXH3_H */
