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


/* ===   Compiler specifics   === */

#if defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L   /* >= C99 */
#  define XXH_RESTRICT   restrict
#else
/* note : it might be useful to define __restrict or __restrict__ for some C++ compilers */
#  define XXH_RESTRICT   /* disable */
#endif

#if defined(__GNUC__)
#  if defined(__AVX2__)
#    include <immintrin.h>
#  elif defined(__SSE2__)
#    include <emmintrin.h>
#  elif defined(__ARM_NEON__) || defined(__ARM_NEON)
#    define inline __inline__  /* clang bug */
#    include <arm_neon.h>
#    undef inline
#  endif
#elif defined(_MSC_VER)
#  include <intrin.h>
#endif

/*
 * Sanity check.
 *
 * XXH3 only requires these features to be efficient:
 *
 *  - Usable unaligned access
 *  - A 32-bit or 64-bit ALU
 *      - If 32-bit, a decent ADC instruction
 *  - A 32 or 64-bit multiply with a 64-bit result
 *
 * Almost all 32-bit and 64-bit targets meet this, except for Thumb-1, the
 * classic 16-bit only subset of ARM's instruction set.
 *
 * First of all, Thumb-1 lacks support for the UMULL instruction which
 * performs the important long multiply. This means numerous __aeabi_lmul
 * calls.
 *
 * Second of all, the 8 functional registers are just not enough.
 * Setup for __aeabi_lmul, byteshift loads, pointers, and all arithmetic need
 * Lo registers, and this shuffling results in thousands more MOVs than A32.
 *
 * A32 and T32 don't have this limitation. They can access all 14 registers,
 * do a 32->64 multiply with UMULL, and the flexible operand is helpful too.
 *
 * If compiling Thumb-1 for a target which supports ARM instructions, we
 * will give a warning.
 *
 * Usually, if this happens, it is because of an accident and you probably
 * need to specify -march, as you probably meant to compileh for a newer
 * architecture.
 */
#if defined(__thumb__) && !defined(__thumb2__) && defined(__ARM_ARCH_ISA_ARM)
#   warning "XXH3 is highly inefficient without ARM or Thumb-2."
#endif

/* ==========================================
 * Vectorization detection
 * ========================================== */
#define XXH_SCALAR 0
#define XXH_SSE2   1
#define XXH_AVX2   2
#define XXH_NEON   3
#define XXH_VSX    4

#ifndef XXH_VECTOR    /* can be defined on command line */
#  if defined(__AVX2__)
#    define XXH_VECTOR XXH_AVX2
#  elif defined(__SSE2__) || defined(_M_AMD64) || defined(_M_X64) || (defined(_M_IX86_FP) && (_M_IX86_FP == 2))
#    define XXH_VECTOR XXH_SSE2
#  elif defined(__GNUC__) /* msvc support maybe later */ \
  && (defined(__ARM_NEON__) || defined(__ARM_NEON)) \
  && defined(__LITTLE_ENDIAN__) /* ARM big endian is a thing */
#    define XXH_VECTOR XXH_NEON
#  elif defined(__PPC64__) && defined(__POWER8_VECTOR__) && defined(__GNUC__)
#    define XXH_VECTOR XXH_VSX
#  else
#    define XXH_VECTOR XXH_SCALAR
#  endif
#endif

/* control alignment of accumulator,
 * for compatibility with fast vector loads */
#ifndef XXH_ACC_ALIGN
#  if XXH_VECTOR == 0   /* scalar */
#     define XXH_ACC_ALIGN 8
#  elif XXH_VECTOR == 1  /* sse2 */
#     define XXH_ACC_ALIGN 16
#  elif XXH_VECTOR == 2  /* avx2 */
#     define XXH_ACC_ALIGN 32
#  elif XXH_VECTOR == 3  /* neon */
#     define XXH_ACC_ALIGN 16
#  elif XXH_VECTOR == 4  /* vsx */
#     define XXH_ACC_ALIGN 16
#  endif
#endif

/* U64 XXH_mult32to64(U32 a, U64 b) { return (U64)a * (U64)b; } */
#if defined(_MSC_VER) && defined(_M_IX86)
#    include <intrin.h>
#    define XXH_mult32to64(x, y) __emulu(x, y)
#else
#    define XXH_mult32to64(x, y) ((U64)((x) & 0xFFFFFFFF) * (U64)((y) & 0xFFFFFFFF))
#endif

/* VSX stuff. It's a lot because VSX support is mediocre across compilers and
 * there is a lot of mischief with endianness. */
#if XXH_VECTOR == XXH_VSX
#  include <altivec.h>
#  undef vector
typedef __vector unsigned long long U64x2;
typedef __vector unsigned char U8x16;
typedef __vector unsigned U32x4;

#ifndef XXH_VSX_BE
#  ifdef __BIG_ENDIAN__
#    define XXH_VSX_BE 1
#  elif defined(__VEC_ELEMENT_REG_ORDER__) && __VEC_ELEMENT_REG_ORDER__ == __ORDER_BIG_ENDIAN__
#    warning "-maltivec=be is not recommended. Please use native endianness."
#    define XXH_VSX_BE 1
#  else
#    define XXH_VSX_BE 0
#  endif
#endif

/* We need some helpers for big endian mode. */
#if XXH_VSX_BE
/* A wrapper for POWER9's vec_revb. */
#  ifdef __POWER9_VECTOR__
#    define XXH_vec_revb vec_revb
#  else
XXH_FORCE_INLINE U64x2 XXH_vec_revb(U64x2 val)
{
    U8x16 const vByteSwap = { 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00,
                              0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08 };
    return vec_perm(val, val, vByteSwap);
}
#  endif

/* Power8 Crypto gives us vpermxor which is very handy for
 * PPC64EB.
 *
 * U8x16 vpermxor(U8x16 a, U8x16 b, U8x16 mask)
 * {
 *     U8x16 ret;
 *     for (int i = 0; i < 16; i++) {
 *         ret[i] = a[mask[i] & 0xF] ^ b[mask[i] >> 4];
 *     }
 *     return ret;
 * }
 *
 * Because both of the main loops load the key, swap, and xor it with data,
 * we can combine the key swap into this instruction.
 */
#  ifdef vec_permxor
#    define XXH_vec_permxor vec_permxor
#  else
#    define XXH_vec_permxor __builtin_crypto_vpermxor
#  endif
#endif
/*
 * Because we reinterpret the multiply, there are endian memes: vec_mulo actually becomes
 * vec_mule.
 *
 * Additionally, the intrinsic wasn't added until GCC 8, despite existing for a while.
 * Clang has an easy way to control this, we can just use the builtin which doesn't swap.
 * GCC needs inline assembly. */
#if __has_builtin(__builtin_altivec_vmuleuw)
#  define XXH_vec_mulo __builtin_altivec_vmulouw
#  define XXH_vec_mule __builtin_altivec_vmuleuw
#else
/* Adapted from https://github.com/google/highwayhash/blob/master/highwayhash/hh_vsx.h. */
XXH_FORCE_INLINE U64x2 XXH_vec_mulo(U32x4 a, U32x4 b) {
    U64x2 result;
    __asm__("vmulouw %0, %1, %2" : "=v" (result) : "v" (a), "v" (b));
    return result;
}
XXH_FORCE_INLINE U64x2 XXH_vec_mule(U32x4 a, U32x4 b) {
    U64x2 result;
    __asm__("vmuleuw %0, %1, %2" : "=v" (result) : "v" (a), "v" (b));
    return result;
}
#endif
#endif


/* ==========================================
 * XXH3 default settings
 * ========================================== */

#define XXH_SECRET_DEFAULT_SIZE 192   /* minimum XXH3_SECRET_SIZE_MIN */

#if (XXH_SECRET_DEFAULT_SIZE < XXH3_SECRET_SIZE_MIN)
#  error "default keyset is not large enough"
#endif

XXH_ALIGN(64) static const BYTE kSecret[XXH_SECRET_DEFAULT_SIZE] = {
    0xb8, 0xfe, 0x6c, 0x39, 0x23, 0xa4, 0x4b, 0xbe, 0x7c, 0x01, 0x81, 0x2c, 0xf7, 0x21, 0xad, 0x1c,
    0xde, 0xd4, 0x6d, 0xe9, 0x83, 0x90, 0x97, 0xdb, 0x72, 0x40, 0xa4, 0xa4, 0xb7, 0xb3, 0x67, 0x1f,
    0xcb, 0x79, 0xe6, 0x4e, 0xcc, 0xc0, 0xe5, 0x78, 0x82, 0x5a, 0xd0, 0x7d, 0xcc, 0xff, 0x72, 0x21,
    0xb8, 0x08, 0x46, 0x74, 0xf7, 0x43, 0x24, 0x8e, 0xe0, 0x35, 0x90, 0xe6, 0x81, 0x3a, 0x26, 0x4c,
    0x3c, 0x28, 0x52, 0xbb, 0x91, 0xc3, 0x00, 0xcb, 0x88, 0xd0, 0x65, 0x8b, 0x1b, 0x53, 0x2e, 0xa3,
    0x71, 0x64, 0x48, 0x97, 0xa2, 0x0d, 0xf9, 0x4e, 0x38, 0x19, 0xef, 0x46, 0xa9, 0xde, 0xac, 0xd8,
    0xa8, 0xfa, 0x76, 0x3f, 0xe3, 0x9c, 0x34, 0x3f, 0xf9, 0xdc, 0xbb, 0xc7, 0xc7, 0x0b, 0x4f, 0x1d,
    0x8a, 0x51, 0xe0, 0x4b, 0xcd, 0xb4, 0x59, 0x31, 0xc8, 0x9f, 0x7e, 0xc9, 0xd9, 0x78, 0x73, 0x64,

    0xea, 0xc5, 0xac, 0x83, 0x34, 0xd3, 0xeb, 0xc3, 0xc5, 0x81, 0xa0, 0xff, 0xfa, 0x13, 0x63, 0xeb,
    0x17, 0x0d, 0xdd, 0x51, 0xb7, 0xf0, 0xda, 0x49, 0xd3, 0x16, 0x55, 0x26, 0x29, 0xd4, 0x68, 0x9e,
    0x2b, 0x16, 0xbe, 0x58, 0x7d, 0x47, 0xa1, 0xfc, 0x8f, 0xf8, 0xb8, 0xd1, 0x7a, 0xd0, 0x31, 0xce,
    0x45, 0xcb, 0x3a, 0x8f, 0x95, 0x16, 0x04, 0x28, 0xaf, 0xd7, 0xfb, 0xca, 0xbb, 0x4b, 0x40, 0x7e,
};

/*
 * GCC for x86 has a tendency to use SSE in this loop. While it
 * successfully avoids swapping (as MUL overwrites EAX and EDX), it
 * slows it down because instead of free register swap shifts, it
 * must use pshufd and punpckl/hd.
 *
 * To prevent this, we use this attribute to shut off SSE.
 */
#if defined(__GNUC__) && !defined(__clang__) && defined(__i386__)
__attribute__((__target__("no-sse")))
#endif
static XXH128_hash_t
XXH_mult64to128(U64 lhs, U64 rhs)
{
    /*
     * GCC/Clang __uint128_t method.
     *
     * On most 64-bit targets, GCC and Clang define a __uint128_t type.
     * This is usually the best way as it usually uses a native long 64-bit
     * multiply, such as MULQ on x86_64 or MUL + UMULH on aarch64.
     *
     * Usually.
     *
     * Despite being a 32-bit platform, Clang (and emscripten) define this
     * type despite not having the arithmetic for it. This results in a
     * laggy compiler builtin call which calculates a full 128-bit multiply.
     * In that case it is best to use the portable one.
     * https://github.com/Cyan4973/xxHash/issues/211#issuecomment-515575677
     */
#if defined(__GNUC__) && !defined(__wasm__) \
    && defined(__SIZEOF_INT128__) \
    || (defined(_INTEGRAL_MAX_BITS) && _INTEGRAL_MAX_BITS >= 128)

    __uint128_t product = (__uint128_t)lhs * (__uint128_t)rhs;
    XXH128_hash_t const r128 = { (U64)(product), (U64)(product >> 64) };
    return r128;

    /*
     * MSVC for x64's _umul128 method.
     *
     * U64 _umul128(U64 Multiplier, U64 Multiplicand, U64 *HighProduct);
     *
     * This compiles to single operand MUL on x64.
     */
#elif defined(_M_X64) || defined(_M_IA64)

#ifndef _MSC_VER
#   pragma intrinsic(_umul128)
#endif
    U64 product_high;
    U64 const product_low = _umul128(lhs, rhs, &product_high);
    XXH128_hash_t const r128 = { product_low, product_high };
    return r128;

#else
    /*
     * Portable scalar method. Optimized for 32-bit and 64-bit ALUs.
     *
     * This is a fast and simple grade school multiply, which is shown
     * below with base 10 arithmetic instead of base 0x100000000.
     *
     *           9 3 // D2 lhs = 93
     *         x 7 5 // D2 rhs = 75
     *     ----------
     *           1 5 // D2 lo_lo = (93 % 10) * (75 % 10)
     *         4 5 | // D2 hi_lo = (93 / 10) * (75 % 10)
     *         2 1 | // D2 lo_hi = (93 % 10) * (75 / 10)
     *     + 6 3 | | // D2 hi_hi = (93 / 10) * (75 / 10)
     *     ---------
     *         2 7 | // D2 cross  = (15 / 10) + (45 % 10) + 21
     *     + 6 7 | | // D2 upper  = (27 / 10) + (45 / 10) + 63
     *     ---------
     *       6 9 7 5
     *
     * The reasons for adding the products like this are:
     *  1. It avoids manual carry tracking. Just like how
     *     (9 * 9) + 9 + 9 = 99, the same applies with this for
     *     UINT64_MAX. This avoids a lot of complexity.
     *
     *  2. It hints for, and on Clang, compiles to, the powerful UMAAL
     *     instruction available in ARMv6+ A32/T32, which is shown below:
     *
     *         void UMAAL(U32 *RdLo, U32 *RdHi, U32 Rn, U32 Rm)
     *         {
     *             U64 product = (U64)*RdLo * (U64)*RdHi + Rn + Rm;
     *             *RdLo = (U32)(product & 0xFFFFFFFF);
     *             *RdHi = (U32)(product >> 32);
     *         }
     *
     *     This instruction was designed for efficient long multiplication,
     *     and allows this to be calculated in only 4 instructions which
     *     is comparable to some 64-bit ALUs.
     *
     *  3. It isn't terrible on other platforms. Usually this will be
     *     a couple of 32-bit ADD/ADCs.
     */

    /* First calculate all of the cross products. */
    U64 const lo_lo = XXH_mult32to64(lhs & 0xFFFFFFFF, rhs & 0xFFFFFFFF);
    U64 const hi_lo = XXH_mult32to64(lhs >> 32,        rhs & 0xFFFFFFFF);
    U64 const lo_hi = XXH_mult32to64(lhs & 0xFFFFFFFF, rhs >> 32);
    U64 const hi_hi = XXH_mult32to64(lhs >> 32,        rhs >> 32);

    /* Now add the products together. These will never overflow. */
    U64 const cross = (lo_lo >> 32) + (hi_lo & 0xFFFFFFFF) + lo_hi;
    U64 const upper = (hi_lo >> 32) + (cross >> 32)        + hi_hi;
    U64 const lower = (cross << 32) | (lo_lo & 0xFFFFFFFF);

    XXH128_hash_t r128 = { lower, upper };
    return r128;
#endif
}

/*
 * We want to keep the attribute here because a target switch
 * disables inlining.
 *
 * Does a 64-bit to 128-bit multiply, then XOR folds it.
 * The reason for the separate function is to prevent passing
 * too many structs around by value. This will hopefully inline
 * the multiply, but we don't force it.
 */
#if defined(__GNUC__) && !defined(__clang__) && defined(__i386__)
__attribute__((__target__("no-sse")))
#endif
static U64
XXH3_mul128_fold64(U64 lhs, U64 rhs)
{
    XXH128_hash_t product = XXH_mult64to128(lhs, rhs);
    return product.low64 ^ product.high64;
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
    XXH_ASSERT(data != NULL);
    XXH_ASSERT(1 <= len && len <= 3);
    XXH_ASSERT(keyPtr != NULL);
    {   BYTE const c1 = ((const BYTE*)data)[0];
        BYTE const c2 = ((const BYTE*)data)[len >> 1];
        BYTE const c3 = ((const BYTE*)data)[len - 1];
        U32  const combined = ((U32)c1) + (((U32)c2) << 8) + (((U32)c3) << 16) + (((U32)len) << 24);
        U64  const keyed = (U64)combined ^ (XXH_readLE32(keyPtr) + seed);
        U64  const mixed = keyed * PRIME64_1;
        return XXH3_avalanche(mixed);
    }
}

XXH_FORCE_INLINE XXH64_hash_t
XXH3_len_4to8_64b(const void* data, size_t len, const void* keyPtr, XXH64_hash_t seed)
{
    XXH_ASSERT(data != NULL);
    XXH_ASSERT(keyPtr != NULL);
    XXH_ASSERT(4 <= len && len <= 8);
    {   U32 const in1 = XXH_readLE32(data);
        U32 const in2 = XXH_readLE32((const BYTE*)data + len - 4);
        U64 const in64 = in1 + ((U64)in2 << 32);
        U64 const keyed = in64 ^ (XXH_readLE64(keyPtr) + seed);
        U64 const mix64 = len + ((keyed ^ (keyed >> 51)) * PRIME32_1);
        return XXH3_avalanche((mix64 ^ (mix64 >> 47)) * PRIME64_2);
    }
}

XXH_FORCE_INLINE XXH64_hash_t
XXH3_len_9to16_64b(const void* data, size_t len, const void* keyPtr, XXH64_hash_t seed)
{
    XXH_ASSERT(data != NULL);
    XXH_ASSERT(keyPtr != NULL);
    XXH_ASSERT(9 <= len && len <= 16);
    {   const U64* const key64 = (const U64*) keyPtr;
        U64 const ll1 = XXH_readLE64(data) ^ (XXH_readLE64(key64) + seed);
        U64 const ll2 = XXH_readLE64((const BYTE*)data + len - 8) ^ (XXH_readLE64(key64+1) - seed);
        U64 const acc = len + (ll1 + ll2) + XXH3_mul128_fold64(ll1, ll2);
        return XXH3_avalanche(acc);
    }
}

XXH_FORCE_INLINE XXH64_hash_t
XXH3_len_0to16_64b(const void* data, size_t len, const void* keyPtr, XXH64_hash_t seed)
{
    XXH_ASSERT(len <= 16);
    {   if (len > 8) return XXH3_len_9to16_64b(data, len, keyPtr, seed);
        if (len >= 4) return XXH3_len_4to8_64b(data, len, keyPtr, seed);
        if (len) return XXH3_len_1to3_64b(data, len, keyPtr, seed);
        return 0;
    }
}


/* ===    Long Keys    === */

#define STRIPE_LEN 64
#define XXH_SECRET_CONSUME_RATE 8   /* nb of secret bytes consumed at each accumulation */
#define ACC_NB (STRIPE_LEN / sizeof(U64))

typedef enum { XXH3_acc_64bits, XXH3_acc_128bits } XXH3_accWidth_e;

XXH_FORCE_INLINE void
XXH3_accumulate_512(      void* XXH_RESTRICT acc,
                    const void* XXH_RESTRICT data,
                    const void* XXH_RESTRICT key,
                    XXH3_accWidth_e accWidth)
{
#if (XXH_VECTOR == XXH_AVX2)

    XXH_ASSERT((((size_t)acc) & 31) == 0);
    {   XXH_ALIGN(32) __m256i* const xacc  =       (__m256i *) acc;
        const         __m256i* const xdata = (const __m256i *) data;  /* not really aligned, just for ptr arithmetic, and because _mm256_loadu_si256() requires this type */
        const         __m256i* const xkey  = (const __m256i *) key;   /* not really aligned, just for ptr arithmetic, and because _mm256_loadu_si256() requires this type */

        size_t i;
        for (i=0; i < STRIPE_LEN/sizeof(__m256i); i++) {
            __m256i const d   = _mm256_loadu_si256 (xdata+i);
            __m256i const k   = _mm256_loadu_si256 (xkey+i);
            __m256i const dk  = _mm256_xor_si256 (d,k);                                  /* uint32 dk[8]  = {d0+k0, d1+k1, d2+k2, d3+k3, ...} */
            __m256i const mul = _mm256_mul_epu32 (dk, _mm256_shuffle_epi32 (dk, 0x31));  /* uint64 mul[4] = {dk0*dk1, dk2*dk3, ...} */
            if (accWidth == XXH3_acc_128bits) {
                __m256i const dswap = _mm256_shuffle_epi32(d, _MM_SHUFFLE(1,0,3,2));
                __m256i const add = _mm256_add_epi64(xacc[i], dswap);
                xacc[i]  = _mm256_add_epi64(mul, add);
            } else {  /* XXH3_acc_64bits */
                __m256i const add = _mm256_add_epi64(xacc[i], d);
                xacc[i]  = _mm256_add_epi64(mul, add);
            }
    }   }

#elif (XXH_VECTOR == XXH_SSE2)

    XXH_ASSERT((((size_t)acc) & 15) == 0);
    {   XXH_ALIGN(16) __m128i* const xacc  =       (__m128i *) acc;   /* presumed */
        const         __m128i* const xdata = (const __m128i *) data;  /* not really aligned, just for ptr arithmetic, and because _mm_loadu_si128() requires this type */
        const         __m128i* const xkey  = (const __m128i *) key;   /* not really aligned, just for ptr arithmetic, and because _mm_loadu_si128() requires this type */

        size_t i;
        for (i=0; i < STRIPE_LEN/sizeof(__m128i); i++) {
            __m128i const d   = _mm_loadu_si128 (xdata+i);
            __m128i const k   = _mm_loadu_si128 (xkey+i);
            __m128i const dk  = _mm_xor_si128 (d,k);                                 /* uint32 dk[4]  = {d0+k0, d1+k1, d2+k2, d3+k3} */
            __m128i const mul = _mm_mul_epu32 (dk, _mm_shuffle_epi32 (dk, 0x31));    /* uint64 mul[2] = {dk0*dk1,dk2*dk3} */
            if (accWidth == XXH3_acc_128bits) {
                __m128i const dswap = _mm_shuffle_epi32(d, _MM_SHUFFLE(1,0,3,2));
                __m128i const add = _mm_add_epi64(xacc[i], dswap);
                xacc[i]  = _mm_add_epi64(mul, add);
            } else {  /* XXH3_acc_64bits */
                __m128i const add = _mm_add_epi64(xacc[i], d);
                xacc[i]  = _mm_add_epi64(mul, add);
            }
    }   }

#elif (XXH_VECTOR == XXH_NEON)

    XXH_ASSERT((((size_t)acc) & 15) == 0);
    {
        XXH_ALIGN(16) uint64x2_t* const xacc = (uint64x2_t *) acc;
        /* We don't use a uint32x4_t pointer because it causes bus errors on ARMv7. */
        uint32_t const* const xdata = (const uint32_t *) data;
        uint32_t const* const xkey  = (const uint32_t *) key;

        size_t i;
        for (i=0; i < STRIPE_LEN / sizeof(uint64x2_t); i++) {
#if !defined(__aarch64__) && !defined(__arm64__) && defined(__GNUC__) /* ARM32-specific hack */
            /* vzip on ARMv7 Clang generates a lot of vmovs (technically vorrs) without this.
             * vzip on 32-bit ARM NEON will overwrite the original register, and I think that Clang
             * assumes I don't want to destroy it and tries to make a copy. This slows down the code
             * a lot.
             * aarch64 not only uses an entirely different syntax, but it requires three
             * instructions...
             *    ext    v1.16B, v0.16B, #8    // select high bits because aarch64 can't address them directly
             *    zip1   v3.2s, v0.2s, v1.2s   // first zip
             *    zip2   v2.2s, v0.2s, v1.2s   // second zip
             * ...to do what ARM does in one:
             *    vzip.32 d0, d1               // Interleave high and low bits and overwrite. */

            /* data_vec = xdata[i]; */
            uint32x4_t const data_vec    = vld1q_u32(xdata + (i * 4));
            /* key_vec  = xkey[i];  */
            uint32x4_t const key_vec     = vld1q_u32(xkey  + (i * 4));
            /* data_key = data_vec ^ key_vec; */
            uint32x4_t       data_key;

            if (accWidth == XXH3_acc_64bits) {
                /* Add first to prevent register swaps */
                /* xacc[i] += data_vec; */
                xacc[i] = vaddq_u64 (xacc[i], vreinterpretq_u64_u32(data_vec));
            } else {  /* XXH3_acc_128bits */
                /* xacc[i] += swap(data_vec); */
                /* can probably be optimized better */
                uint64x2_t const data64 = vreinterpretq_u64_u32(data_vec);
                uint64x2_t const swapped= vextq_u64(data64, data64, 1);
                xacc[i] = vaddq_u64 (xacc[i], swapped);
            }

            data_key = veorq_u32(data_vec, key_vec);

            /* Here's the magic. We use the quirkiness of vzip to shuffle data_key in place.
             * shuffle: data_key[0, 1, 2, 3] = data_key[0, 2, 1, 3] */
            __asm__("vzip.32 %e0, %f0" : "+w" (data_key));
            /* xacc[i] += (uint64x2_t) data_key[0, 1] * (uint64x2_t) data_key[2, 3]; */
            xacc[i] = vmlal_u32(xacc[i], vget_low_u32(data_key), vget_high_u32(data_key));

#else
            /* On aarch64, vshrn/vmovn seems to be equivalent to, if not faster than, the vzip method. */

            /* data_vec = xdata[i]; */
            uint32x4_t const data_vec    = vld1q_u32(xdata + (i * 4));
            /* key_vec  = xkey[i];  */
            uint32x4_t const key_vec     = vld1q_u32(xkey  + (i * 4));
            /* data_key = data_vec ^ key_vec; */
            uint32x4_t const data_key    = veorq_u32(data_vec, key_vec);
            /* data_key_lo = (uint32x2_t) (data_key & 0xFFFFFFFF); */
            uint32x2_t const data_key_lo = vmovn_u64  (vreinterpretq_u64_u32(data_key));
            /* data_key_hi = (uint32x2_t) (data_key >> 32); */
            uint32x2_t const data_key_hi = vshrn_n_u64 (vreinterpretq_u64_u32(data_key), 32);
            if (accWidth == XXH3_acc_64bits) {
                /* xacc[i] += data_vec; */
                xacc[i] = vaddq_u64 (xacc[i], vreinterpretq_u64_u32(data_vec));
            } else {  /* XXH3_acc_128bits */
                /* xacc[i] += swap(data_vec); */
                uint64x2_t const data64 = vreinterpretq_u64_u32(data_vec);
                uint64x2_t const swapped= vextq_u64(data64, data64, 1);
                xacc[i] = vaddq_u64 (xacc[i], swapped);
            }
            /* xacc[i] += (uint64x2_t) data_key_lo * (uint64x2_t) data_key_hi; */
            xacc[i] = vmlal_u32 (xacc[i], data_key_lo, data_key_hi);

#endif
        }
    }

#elif (XXH_VECTOR == XXH_VSX)
          U64x2* const xacc =        (U64x2*) acc;    /* presumed aligned */
    U64x2 const* const xdata = (U64x2 const*) data;   /* no alignment restriction */
    U64x2 const* const xkey  = (U64x2 const*) key;    /* no alignment restriction */
    U64x2 const v32 = { 32,  32 };
#if XXH_VSX_BE
    U8x16 const vXorSwap  = { 0x07, 0x16, 0x25, 0x34, 0x43, 0x52, 0x61, 0x70,
                              0x8F, 0x9E, 0xAD, 0xBC, 0xCB, 0xDA, 0xE9, 0xF8 };
#endif
    size_t i;
    for (i = 0; i < STRIPE_LEN / sizeof(U64x2); i++) {
        /* data_vec = xdata[i]; */
        /* key_vec = xkey[i]; */
#if XXH_VSX_BE
        /* byteswap */
        U64x2 const data_vec = XXH_vec_revb(vec_vsx_ld(0, xdata + i));
        U64x2 const key_raw = vec_vsx_ld(0, xkey + i);
        /* See comment above. data_key = data_vec ^ swap(xkey[i]); */
        U64x2 const data_key = (U64x2)XXH_vec_permxor((U8x16)data_vec, (U8x16)key_raw, vXorSwap);
#else
        U64x2 const data_vec = vec_vsx_ld(0, xdata + i);
        U64x2 const key_vec = vec_vsx_ld(0, xkey + i);
        U64x2 const data_key = data_vec ^ key_vec;
#endif
        /* shuffled = (data_key << 32) | (data_key >> 32); */
        U32x4 const shuffled = (U32x4)vec_rl(data_key, v32);
        /* product = ((U64x2)data_key & 0xFFFFFFFF) * ((U64x2)shuffled & 0xFFFFFFFF); */
        U64x2 const product = XXH_vec_mulo((U32x4)data_key, shuffled);
        xacc[i] += product;

        if (accWidth == XXH3_acc_64bits) {
            xacc[i] += data_vec;
        } else {  /* XXH3_acc_128bits */
            /* swap high and low halves */
            U64x2 const data_swapped = vec_xxpermdi(data_vec, data_vec, 2);
            xacc[i] += data_swapped;
        }
    }

#else   /* scalar variant of Accumulator - universal */

    XXH_ALIGN(XXH_ACC_ALIGN) U64* const xacc = (U64*) acc;    /* presumed aligned on 32-bytes boundaries, little hint for the auto-vectorizer */
    const char* const xdata = (const char*) data;  /* no alignment restriction */
    const char* const xkey  = (const char*) key;   /* no alignment restriction */
    size_t i;
    XXH_ASSERT(((size_t)acc & (XXH_ACC_ALIGN-1)) == 0);
    for (i=0; i < ACC_NB; i+=2) {
        U64 const in1 = XXH_readLE64(xdata + 8*i);
        U64 const in2 = XXH_readLE64(xdata + 8*(i+1));
        U64 const key1  = XXH_readLE64(xkey + 8*i);
        U64 const key2  = XXH_readLE64(xkey + 8*(i+1));
        U64 const data_key1 = key1 ^ in1;
        U64 const data_key2 = key2 ^ in2;
        xacc[i]   += XXH_mult32to64(data_key1 & 0xFFFFFFFF, data_key1 >> 32);
        xacc[i+1] += XXH_mult32to64(data_key2 & 0xFFFFFFFF, data_key2 >> 32);
        if (accWidth == XXH3_acc_128bits) {
            xacc[i]   += in2;
            xacc[i+1] += in1;
        } else {  /* XXH3_acc_64bits */
            xacc[i]   += in1;
            xacc[i+1] += in2;
        }
    }
#endif
}

XXH_FORCE_INLINE void
XXH3_scrambleAcc(void* XXH_RESTRICT acc, const void* XXH_RESTRICT key)
{
#if (XXH_VECTOR == XXH_AVX2)

    XXH_ASSERT((((size_t)acc) & 31) == 0);
    {   XXH_ALIGN(32) __m256i* const xacc = (__m256i*) acc;
        const         __m256i* const xkey = (const __m256i *) key;   /* not really aligned, just for ptr arithmetic, and because _mm256_loadu_si256() requires this argument type */
        const __m256i prime32 = _mm256_set1_epi32((int)PRIME32_1);

        size_t i;
        for (i=0; i < STRIPE_LEN/sizeof(__m256i); i++) {
            __m256i data = xacc[i];
            __m256i const shifted = _mm256_srli_epi64(data, 47);
            data = _mm256_xor_si256(data, shifted);

            {   __m256i const k   = _mm256_loadu_si256 (xkey+i);
                __m256i const dk  = _mm256_xor_si256   (data, k);

                __m256i const dk1 = _mm256_mul_epu32 (dk, prime32);

                __m256i const d2  = _mm256_shuffle_epi32 (dk, 0x31);
                __m256i const dk2 = _mm256_mul_epu32 (d2, prime32);
                __m256i const dk2h= _mm256_slli_epi64 (dk2, 32);

                xacc[i] = _mm256_add_epi64(dk1, dk2h);
        }   }
    }

#elif (XXH_VECTOR == XXH_SSE2)

    {   XXH_ALIGN(16) __m128i* const xacc = (__m128i*) acc;
        const         __m128i* const xkey = (const __m128i *) key;   /* not really aligned, just for ptr arithmetic */
        const __m128i prime32 = _mm_set1_epi32((int)PRIME32_1);

        size_t i;
        for (i=0; i < STRIPE_LEN/sizeof(__m128i); i++) {
            __m128i data = xacc[i];
            __m128i const shifted = _mm_srli_epi64(data, 47);
            data = _mm_xor_si128(data, shifted);

            {   __m128i const k   = _mm_loadu_si128 (xkey+i);
                __m128i const dk  = _mm_xor_si128   (data,k);

                __m128i const dk1 = _mm_mul_epu32 (dk, prime32);

                __m128i const d2  = _mm_shuffle_epi32 (dk, 0x31);
                __m128i const dk2 = _mm_mul_epu32 (d2, prime32);
                __m128i const dk2h= _mm_slli_epi64(dk2, 32);

                xacc[i] = _mm_add_epi64(dk1, dk2h);
        }   }
    }

#elif (XXH_VECTOR == XXH_NEON)

    XXH_ASSERT((((size_t)acc) & 15) == 0);

    {   uint64x2_t* const xacc =     (uint64x2_t*) acc;
        uint32_t const* const xkey = (uint32_t const*) key;
        uint32x2_t const prime     = vdup_n_u32 (PRIME32_1);

        size_t i;
        for (i=0; i < STRIPE_LEN/sizeof(uint64x2_t); i++) {
            /* data_vec = xacc[i] ^ (xacc[i] >> 47); */
            uint64x2_t const   acc_vec  = xacc[i];
            uint64x2_t const   shifted  = vshrq_n_u64 (acc_vec, 47);
            uint64x2_t const   data_vec = veorq_u64   (acc_vec, shifted);

            /* key_vec  = xkey[i]; */
            uint32x4_t const   key_vec  = vld1q_u32   (xkey + (i * 4));
            /* data_key = data_vec ^ key_vec; */
            uint32x4_t const   data_key = veorq_u32   (vreinterpretq_u32_u64(data_vec), key_vec);
            /* shuffled = { data_key[0, 2], data_key[1, 3] }; */
            uint32x2x2_t const shuffled = vzip_u32    (vget_low_u32(data_key), vget_high_u32(data_key));

            /* data_key *= PRIME32_1 */

            /* prod_hi = (data_key >> 32) * PRIME32_1; */
            uint64x2_t const   prod_hi = vmull_u32    (shuffled.val[1], prime);
            /* xacc[i] = prod_hi << 32; */
            xacc[i] = vshlq_n_u64(prod_hi, 32);
            /* xacc[i] += (prod_hi & 0xFFFFFFFF) * PRIME32_1; */
            xacc[i] = vmlal_u32(xacc[i], shuffled.val[0], prime);
    }   }

#elif (XXH_VECTOR == XXH_VSX)

          U64x2* const xacc =       (U64x2*) acc;
    const U64x2* const xkey = (const U64x2*) key;
    /* constants */
    U64x2 const v32  = { 32, 32 };
    U64x2 const v47 = { 47, 47 };
    U32x4 const prime = { PRIME32_1, PRIME32_1, PRIME32_1, PRIME32_1 };
    size_t i;
#if XXH_VSX_BE
    /* endian swap */
    U8x16 const vXorSwap  = { 0x07, 0x16, 0x25, 0x34, 0x43, 0x52, 0x61, 0x70,
                              0x8F, 0x9E, 0xAD, 0xBC, 0xCB, 0xDA, 0xE9, 0xF8 };
#endif
    for (i = 0; i < STRIPE_LEN / sizeof(U64x2); i++) {
        U64x2 const acc_vec  = xacc[i];
        U64x2 const data_vec = acc_vec ^ (acc_vec >> v47);
        /* key_vec = xkey[i]; */
#if XXH_VSX_BE
        /* swap bytes words */
        U64x2 const key_raw  = vec_vsx_ld(0, xkey + i);
	U64x2 const data_key = (U64x2)XXH_vec_permxor((U8x16)data_vec, (U8x16)key_raw, vXorSwap);
#else
        U64x2 const key_vec  = vec_vsx_ld(0, xkey + i);
        U64x2 const data_key = data_vec ^ key_vec;
#endif

        /* data_key *= PRIME32_1 */

        /* prod_lo = ((U64x2)data_key & 0xFFFFFFFF) * ((U64x2)prime & 0xFFFFFFFF);  */
        U64x2 const prod_even  = XXH_vec_mule((U32x4)data_key, prime);
        /* prod_hi = ((U64x2)data_key >> 32) * ((U64x2)prime >> 32);  */
        U64x2 const prod_odd  = XXH_vec_mulo((U32x4)data_key, prime);
        xacc[i] = prod_odd + (prod_even << v32);
    }

#else   /* scalar variant of Scrambler - universal */

    XXH_ALIGN(XXH_ACC_ALIGN) U64* const xacc = (U64*) acc;   /* presumed aligned on 32-bytes boundaries, little hint for the auto-vectorizer */
    const char* const xkey = (const char*) key;   /* no alignment restriction */
    int i;
    XXH_ASSERT((((size_t)acc) & (XXH_ACC_ALIGN-1)) == 0);

    for (i=0; i < (int)ACC_NB; i++) {
        U64 const key64 = XXH_readLE64(xkey + 8*i);
        U64 acc64 = xacc[i];
        acc64 ^= acc64 >> 47;
        acc64 ^= key64;
        acc64 *= PRIME32_1;
        xacc[i] = acc64;
    }

#endif
}

/* assumption : nbStripes will not overflow secret size */
XXH_FORCE_INLINE void
XXH3_accumulate(       U64* XXH_RESTRICT acc,
                const void* XXH_RESTRICT data,
                const void* XXH_RESTRICT secret,
                      size_t nbStripes,
                      XXH3_accWidth_e accWidth)
{
    size_t n;
    for (n = 0; n < nbStripes; n++ ) {
        XXH3_accumulate_512(acc,
               (const char*)data   + n*STRIPE_LEN,
               (const char*)secret + n*XXH_SECRET_CONSUME_RATE,
                            accWidth);
    }
}

/* note : clang auto-vectorizes well in SS2 mode _if_ this function is `static`,
 *        and doesn't auto-vectorize it at all if it is `FORCE_INLINE`.
 *        However, it auto-vectorizes better AVX2 if it is `FORCE_INLINE`
 *        Pretty much every other modes and compilers prefer `FORCE_INLINE`.
 */
#if defined(__clang__) && (XXH_VECTOR==0) && !defined(__AVX2__)
static void
#else
XXH_FORCE_INLINE void
#endif
XXH3_hashLong_internal_loop( U64* XXH_RESTRICT acc,
                      const void* XXH_RESTRICT data, size_t len,
                      const void* XXH_RESTRICT secret, size_t secretSize,
                            XXH3_accWidth_e accWidth)
{
    size_t const nb_rounds = (secretSize - STRIPE_LEN) / XXH_SECRET_CONSUME_RATE;
    size_t const block_len = STRIPE_LEN * nb_rounds;
    size_t const nb_blocks = len / block_len;

    size_t n;

    XXH_ASSERT(secretSize >= XXH3_SECRET_SIZE_MIN);

    for (n = 0; n < nb_blocks; n++) {
        XXH3_accumulate(acc, (const char*)data + n*block_len, secret, nb_rounds, accWidth);
        XXH3_scrambleAcc(acc, (const char*)secret + secretSize - STRIPE_LEN);
    }

    /* last partial block */
    XXH_ASSERT(len > STRIPE_LEN);
    {   size_t const nbStripes = (len - (block_len * nb_blocks)) / STRIPE_LEN;
        XXH_ASSERT(nbStripes <= (secretSize / XXH_SECRET_CONSUME_RATE));
        XXH3_accumulate(acc, (const char*)data + nb_blocks*block_len, secret, nbStripes, accWidth);

        /* last stripe */
        if (len & (STRIPE_LEN - 1)) {
            const void* const p = (const char*)data + len - STRIPE_LEN;
#define XXH_SECRET_LASTACC_START 7  /* do not align on 8, so that secret is different from scrambler */
            XXH3_accumulate_512(acc, p, (const char*)secret + secretSize - STRIPE_LEN - XXH_SECRET_LASTACC_START, accWidth);
    }   }
}

XXH_FORCE_INLINE U64
XXH3_mix2Accs(const U64* XXH_RESTRICT acc, const void* XXH_RESTRICT secret)
{
    const U64* const key64 = (const U64*)secret;
    return XXH3_mul128_fold64(
               acc[0] ^ XXH_readLE64(key64),
               acc[1] ^ XXH_readLE64(key64+1) );
}

static XXH64_hash_t
XXH3_mergeAccs(const U64* XXH_RESTRICT acc, const void* XXH_RESTRICT secret, U64 start)
{
    U64 result64 = start;

    result64 += XXH3_mix2Accs(acc+0, (const char*)secret +  0);
    result64 += XXH3_mix2Accs(acc+2, (const char*)secret + 16);
    result64 += XXH3_mix2Accs(acc+4, (const char*)secret + 32);
    result64 += XXH3_mix2Accs(acc+6, (const char*)secret + 48);

    return XXH3_avalanche(result64);
}

#define XXH3_INIT_ACC { PRIME32_3, PRIME64_1, PRIME64_2, PRIME64_3, \
                        PRIME64_4, PRIME32_2, PRIME64_5, PRIME32_1 };

XXH_FORCE_INLINE XXH64_hash_t
XXH3_hashLong_internal(const void* XXH_RESTRICT data, size_t len,
                       const void* XXH_RESTRICT secret, size_t secretSize)
{
    XXH_ALIGN(XXH_ACC_ALIGN) U64 acc[ACC_NB] = XXH3_INIT_ACC;

    XXH3_hashLong_internal_loop(acc, data, len, secret, secretSize, XXH3_acc_64bits);

    /* converge into final hash */
    XXH_STATIC_ASSERT(sizeof(acc) == 64);
#define XXH_SECRET_MERGEACCS_START 11  /* do not align on 8, so that secret is different from accumulator */
    XXH_ASSERT(secretSize >= sizeof(acc) + XXH_SECRET_MERGEACCS_START);
    return XXH3_mergeAccs(acc, (const char*)secret + XXH_SECRET_MERGEACCS_START, (U64)len * PRIME64_1);
}


XXH_NO_INLINE XXH64_hash_t    /* It's important for performance that XXH3_hashLong is not inlined. Not sure why (uop cache maybe ?), but difference is large and easily measurable */
XXH3_hashLong_64b_defaultSecret(const void* XXH_RESTRICT data, size_t len)
{
    return XXH3_hashLong_internal(data, len, kSecret, sizeof(kSecret));
}

XXH_NO_INLINE XXH64_hash_t    /* It's important for performance that XXH3_hashLong is not inlined. Not sure why (uop cache maybe ?), but difference is large and easily measurable */
XXH3_hashLong_64b_withSecret(const void* XXH_RESTRICT data, size_t len,
                             const void* XXH_RESTRICT secret, size_t secretSize)
{
    return XXH3_hashLong_internal(data, len, secret, secretSize);
}


XXH_FORCE_INLINE void XXH_writeLE64(void* dst, U64 v64)
{
    if (!XXH_CPU_LITTLE_ENDIAN) v64 = XXH_swap64(v64);
    memcpy(dst, &v64, sizeof(v64));
}

/* XXH3_initKeySeed() :
 * destination `customSecret` is presumed allocated and same size as `kSecret`.
 */
XXH_FORCE_INLINE void XXH3_initKeySeed(void* customSecret, U64 seed64)
{
          char* const dst = (char*)customSecret;
    const char* const src = (const char*)kSecret;
    int const nbRounds = XXH_SECRET_DEFAULT_SIZE / 16;
    int i;

    XXH_STATIC_ASSERT((XXH_SECRET_DEFAULT_SIZE & 15) == 0);

    for (i=0; i < nbRounds; i++) {
        XXH_writeLE64(dst + 16*i,     XXH_readLE64(src + 16*i)     + seed64);
        XXH_writeLE64(dst + 16*i + 8, XXH_readLE64(src + 16*i + 8) - seed64);
    }
}


/* XXH3_hashLong_64b_withSeed() :
 * Generate a custom key,
 * based on alteration of default kSecret with the seed,
 * and then use this key for long mode hashing.
 * This operation is decently fast but nonetheless costs a little bit of time.
 * Try to avoid it whenever possible (typically when seed==0).
 */
XXH_NO_INLINE XXH64_hash_t    /* It's important for performance that XXH3_hashLong is not inlined. Not sure why (uop cache maybe ?), but difference is large and easily measurable */
XXH3_hashLong_64b_withSeed(const void* data, size_t len, XXH64_hash_t seed)
{
    XXH_ALIGN(8) char secret[XXH_SECRET_DEFAULT_SIZE];
    if (seed==0) return XXH3_hashLong_64b_defaultSecret(data, len);
    XXH3_initKeySeed(secret, seed);
    return XXH3_hashLong_internal(data, len, secret, sizeof(secret));
}


XXH_FORCE_INLINE U64 XXH3_mix16B(const void* XXH_RESTRICT data,
                                 const void* XXH_RESTRICT key, U64 seed64)
{
    const U64* const key64 = (const U64*)key;
    U64 const ll1 = XXH_readLE64(data);
    U64 const ll2 = XXH_readLE64((const BYTE*)data+8);
    return XXH3_mul128_fold64(
               ll1 ^ (XXH_readLE64(key64)   + seed64),
               ll2 ^ (XXH_readLE64(key64+1) - seed64) );
}


XXH_FORCE_INLINE XXH64_hash_t
XXH3_len_17to128_64b(const void* XXH_RESTRICT data, size_t len,
                     const void* XXH_RESTRICT secret, size_t secretSize,
                     XXH64_hash_t seed)
{
    const BYTE* const p = (const BYTE*)data;
    const char* const key = (const char*)secret;

    XXH_ASSERT(secretSize >= XXH3_SECRET_SIZE_MIN); (void)secretSize;
    XXH_ASSERT(16 < len && len <= 128);

    {   U64 acc = len * PRIME64_1;
        if (len > 32) {
            if (len > 64) {
                if (len > 96) {
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

#define XXH3_MIDSIZE_MAX 240

XXH_NO_INLINE XXH64_hash_t
XXH3_len_129to240_64b(const void* XXH_RESTRICT data, size_t len,
                      const void* XXH_RESTRICT secret, size_t secretSize,
                      XXH64_hash_t seed)
{
    const BYTE* const p = (const BYTE*)data;
    const char* const key = (const char*)secret;

    XXH_ASSERT(secretSize >= XXH3_SECRET_SIZE_MIN); (void)secretSize;
    XXH_ASSERT(128 < len && len <= XXH3_MIDSIZE_MAX);

    #define XXH3_MIDSIZE_STARTOFFSET 3
    #define XXH3_MIDSIZE_LASTOFFSET  17

    {   U64 acc = len * PRIME64_1;
        int const nbRounds = (int)len / 16;
        int i;
        for (i=0; i<8; i++) {
            acc += XXH3_mix16B(p+(16*i), key+(16*i), seed);
        }
        acc = XXH3_avalanche(acc);
        XXH_ASSERT(nbRounds >= 8);
        for (i=8 ; i < nbRounds; i++) {
            acc += XXH3_mix16B(p+(16*i), key+(16*(i-8)) + XXH3_MIDSIZE_STARTOFFSET, seed);
        }
        /* last bytes */
        acc += XXH3_mix16B(p + len - 16, key + XXH3_SECRET_SIZE_MIN - XXH3_MIDSIZE_LASTOFFSET, seed);
        return XXH3_avalanche(acc);
    }
}

/* ===   Public entry point   === */

XXH_PUBLIC_API XXH64_hash_t XXH3_64bits(const void* data, size_t len)
{
    if (len <= 16) return XXH3_len_0to16_64b(data, len, kSecret, 0);
    if (len <= 128) return XXH3_len_17to128_64b(data, len, kSecret, sizeof(kSecret), 0);
    if (len <= XXH3_MIDSIZE_MAX) return XXH3_len_129to240_64b(data, len, kSecret, sizeof(kSecret), 0);
    return XXH3_hashLong_64b_defaultSecret(data, len);
}

XXH_PUBLIC_API XXH64_hash_t
XXH3_64bits_withSecret(const void* data, size_t len, const void* secret, size_t secretSize)
{
    XXH_ASSERT(secretSize >= XXH3_SECRET_SIZE_MIN);
    /* if an action must be taken should `secret` conditions not be respected,
     * it should be done here.
     * For now, it's a contract pre-condition.
     * Adding a check and a branch here would cost performance at every hash */
     if (len <= 16) return XXH3_len_0to16_64b(data, len, secret, 0);
     if (len <= 128) return XXH3_len_17to128_64b(data, len, secret, secretSize, 0);
     if (len <= XXH3_MIDSIZE_MAX) return XXH3_len_129to240_64b(data, len, secret, secretSize, 0);
     return XXH3_hashLong_64b_withSecret(data, len, secret, secretSize);
}

XXH_PUBLIC_API XXH64_hash_t
XXH3_64bits_withSeed(const void* data, size_t len, XXH64_hash_t seed)
{
    if (len <= 16) return XXH3_len_0to16_64b(data, len, kSecret, seed);
    if (len <= 128) return XXH3_len_17to128_64b(data, len, kSecret, sizeof(kSecret), seed);
    if (len <= XXH3_MIDSIZE_MAX) return XXH3_len_129to240_64b(data, len, kSecret, sizeof(kSecret), seed);
    return XXH3_hashLong_64b_withSeed(data, len, seed);
}

/* ===   XXH3 streaming   === */

XXH_PUBLIC_API XXH3_state_t* XXH3_createState(void)
{
    return (XXH3_state_t*)XXH_malloc(sizeof(XXH3_state_t));
}

XXH_PUBLIC_API XXH_errorcode XXH3_freeState(XXH3_state_t* statePtr)
{
    XXH_free(statePtr);
    return XXH_OK;
}

XXH_PUBLIC_API void
XXH3_copyState(XXH3_state_t* dst_state, const XXH3_state_t* src_state)
{
    memcpy(dst_state, src_state, sizeof(*dst_state));
}

static void
XXH3_64bits_reset_internal(XXH3_state_t* statePtr,
                           XXH64_hash_t seed,
                           const void* secret, size_t secretSize)
{
    XXH_ASSERT(statePtr != NULL);
    memset(statePtr, 0, sizeof(*statePtr));
    statePtr->acc[0] = PRIME32_3;
    statePtr->acc[1] = PRIME64_1;
    statePtr->acc[2] = PRIME64_2;
    statePtr->acc[3] = PRIME64_3;
    statePtr->acc[4] = PRIME64_4;
    statePtr->acc[5] = PRIME32_2;
    statePtr->acc[6] = PRIME64_5;
    statePtr->acc[7] = PRIME32_1;
    statePtr->seed = seed;
    XXH_ASSERT(secret != NULL);
    statePtr->secret = secret;
    XXH_ASSERT(secretSize >= XXH3_SECRET_SIZE_MIN);
    statePtr->secretLimit = (XXH32_hash_t)(secretSize - STRIPE_LEN);
    statePtr->nbStripesPerBlock = statePtr->secretLimit / XXH_SECRET_CONSUME_RATE;
}

XXH_PUBLIC_API XXH_errorcode
XXH3_64bits_reset(XXH3_state_t* statePtr)
{
    if (statePtr == NULL) return XXH_ERROR;
    XXH3_64bits_reset_internal(statePtr, 0, kSecret, XXH_SECRET_DEFAULT_SIZE);
    return XXH_OK;
}

XXH_PUBLIC_API XXH_errorcode
XXH3_64bits_reset_withSecret(XXH3_state_t* statePtr, const void* secret, size_t secretSize)
{
    if (statePtr == NULL) return XXH_ERROR;
    XXH3_64bits_reset_internal(statePtr, 0, secret, secretSize);
    if (secret == NULL) return XXH_ERROR;
    if (secretSize < XXH3_SECRET_SIZE_MIN) return XXH_ERROR;
    return XXH_OK;
}

XXH_PUBLIC_API XXH_errorcode
XXH3_64bits_reset_withSeed(XXH3_state_t* statePtr, XXH64_hash_t seed)
{
    if (statePtr == NULL) return XXH_ERROR;
    XXH3_64bits_reset_internal(statePtr, seed, kSecret, XXH_SECRET_DEFAULT_SIZE);
    XXH3_initKeySeed(statePtr->customSecret, seed);
    statePtr->secret = statePtr->customSecret;
    return XXH_OK;
}

XXH_FORCE_INLINE void
XXH3_consumeStripes( U64* acc,
                            XXH32_hash_t* nbStripesSoFarPtr, XXH32_hash_t nbStripesPerBlock,
                            const void* data, size_t totalStripes,
                            const void* secret, size_t secretLimit,
                            XXH3_accWidth_e accWidth)
{
    XXH_ASSERT(*nbStripesSoFarPtr < nbStripesPerBlock);
    if (nbStripesPerBlock - *nbStripesSoFarPtr <= totalStripes) {
        /* need a scrambling operation */
        size_t const nbStripes = nbStripesPerBlock - *nbStripesSoFarPtr;
        XXH3_accumulate(acc, data, (const char*)secret + nbStripesSoFarPtr[0] * XXH_SECRET_CONSUME_RATE, nbStripes, accWidth);
        XXH3_scrambleAcc(acc, (const char*)secret + secretLimit);
        XXH3_accumulate(acc, (const char*)data + nbStripes * STRIPE_LEN, secret, totalStripes - nbStripes, accWidth);
        *nbStripesSoFarPtr = (XXH32_hash_t)(totalStripes - nbStripes);
    } else {
        XXH3_accumulate(acc, data, (const char*)secret + nbStripesSoFarPtr[0] * XXH_SECRET_CONSUME_RATE, totalStripes, accWidth);
        *nbStripesSoFarPtr += (XXH32_hash_t)totalStripes;
    }
}

XXH_FORCE_INLINE XXH_errorcode
XXH3_update(XXH3_state_t* state, const void* input, size_t len, XXH3_accWidth_e accWidth)
{
    if (input==NULL)
#if defined(XXH_ACCEPT_NULL_INPUT_POINTER) && (XXH_ACCEPT_NULL_INPUT_POINTER>=1)
        return XXH_OK;
#else
        return XXH_ERROR;
#endif

    {   const BYTE* p = (const BYTE*)input;
        const BYTE* const bEnd = p + len;

        state->totalLen += len;

        if (state->bufferedSize + len <= XXH3_INTERNALBUFFER_SIZE) {  /* fill in tmp buffer */
            XXH_memcpy(state->buffer + state->bufferedSize, input, len);
            state->bufferedSize += (XXH32_hash_t)len;
            return XXH_OK;
        }
        /* input now > XXH3_INTERNALBUFFER_SIZE */

        #define XXH3_INTERNALBUFFER_STRIPES (XXH3_INTERNALBUFFER_SIZE / STRIPE_LEN)
        XXH_STATIC_ASSERT(XXH3_INTERNALBUFFER_SIZE % STRIPE_LEN == 0);   /* clean multiple */

        if (state->bufferedSize) {   /* some data within internal buffer: fill then consume it */
            size_t const loadSize = XXH3_INTERNALBUFFER_SIZE - state->bufferedSize;
            XXH_memcpy(state->buffer + state->bufferedSize, input, loadSize);
            p += loadSize;
            XXH3_consumeStripes(state->acc,
                               &state->nbStripesSoFar, state->nbStripesPerBlock,
                                state->buffer, XXH3_INTERNALBUFFER_STRIPES,
                                state->secret, state->secretLimit,
                                accWidth);
            state->bufferedSize = 0;
        }

        /* consume input by full buffer quantities */
        if (p+XXH3_INTERNALBUFFER_SIZE <= bEnd) {
            const BYTE* const limit = bEnd - XXH3_INTERNALBUFFER_SIZE;
            do {
                XXH3_consumeStripes(state->acc,
                                   &state->nbStripesSoFar, state->nbStripesPerBlock,
                                    p, XXH3_INTERNALBUFFER_STRIPES,
                                    state->secret, state->secretLimit,
                                    accWidth);
                p += XXH3_INTERNALBUFFER_SIZE;
            } while (p<=limit);
        }

        if (p < bEnd) { /* some remaining input data : buffer it */
            XXH_memcpy(state->buffer, p, (size_t)(bEnd-p));
            state->bufferedSize = (XXH32_hash_t)(bEnd-p);
        }
    }

    return XXH_OK;
}

XXH_PUBLIC_API XXH_errorcode
XXH3_64bits_update(XXH3_state_t* state, const void* input, size_t len)
{
    return XXH3_update(state, input, len, XXH3_acc_64bits);
}


XXH_FORCE_INLINE void
XXH3_digest_long (XXH64_hash_t* acc, const XXH3_state_t* state, XXH3_accWidth_e accWidth)
{
    memcpy(acc, state->acc, sizeof(state->acc));  /* digest locally, state remains unaltered, and can continue ingesting more data afterwards */
    if (state->bufferedSize >= STRIPE_LEN) {
        size_t const totalNbStripes = state->bufferedSize / STRIPE_LEN;
        XXH32_hash_t nbStripesSoFar = state->nbStripesSoFar;
        XXH3_consumeStripes(acc,
                           &nbStripesSoFar, state->nbStripesPerBlock,
                            state->buffer, totalNbStripes,
                            state->secret, state->secretLimit,
                            accWidth);
        if (state->bufferedSize % STRIPE_LEN) {  /* one last partial stripe */
            XXH3_accumulate_512(acc,
                                state->buffer + state->bufferedSize - STRIPE_LEN,
                   (const char*)state->secret + state->secretLimit - XXH_SECRET_LASTACC_START,
                                accWidth);
        }
    } else {  /* bufferedSize < STRIPE_LEN */
        if (state->bufferedSize) { /* one last stripe */
            char lastStripe[STRIPE_LEN];
            size_t const catchupSize = STRIPE_LEN - state->bufferedSize;
            memcpy(lastStripe, (const char*)state->buffer + sizeof(state->buffer) - catchupSize, catchupSize);
            memcpy(lastStripe + catchupSize, state->buffer, state->bufferedSize);
            XXH3_accumulate_512(acc,
                                lastStripe,
                   (const char*)state->secret + state->secretLimit - XXH_SECRET_LASTACC_START,
                                accWidth);
    }   }
}

XXH_PUBLIC_API XXH64_hash_t XXH3_64bits_digest (const XXH3_state_t* state)
{
    if (state->totalLen > XXH3_MIDSIZE_MAX) {
        XXH_ALIGN(XXH_ACC_ALIGN) XXH64_hash_t acc[ACC_NB];
        XXH3_digest_long(acc, state, XXH3_acc_64bits);
        return XXH3_mergeAccs(acc, (const char*)state->secret + XXH_SECRET_MERGEACCS_START, (U64)state->totalLen * PRIME64_1);
    }
    /* len <= XXH3_MIDSIZE_MAX : short code */
    if (state->seed)
        return XXH3_64bits_withSeed(state->buffer, (size_t)state->totalLen, state->seed);
    return XXH3_64bits_withSecret(state->buffer, (size_t)(state->totalLen), state->secret, state->secretLimit + STRIPE_LEN);
}

/* ==========================================
 * XXH3 128 bits (=> XXH128)
 * ========================================== */

XXH_FORCE_INLINE XXH128_hash_t
XXH3_len_1to3_128b(const void* data, size_t len, const void* keyPtr, XXH64_hash_t seed)
{
    XXH_ASSERT(data != NULL);
    XXH_ASSERT(1 <= len && len <= 3);
    XXH_ASSERT(keyPtr != NULL);
    {   const U32* const key32 = (const U32*) keyPtr;
        BYTE const c1 = ((const BYTE*)data)[0];
        BYTE const c2 = ((const BYTE*)data)[len >> 1];
        BYTE const c3 = ((const BYTE*)data)[len - 1];
        U32  const combinedl = ((U32)c1) + (((U32)c2) << 8) + (((U32)c3) << 16) + (((U32)len) << 24);
        U32  const combinedh = XXH_swap32(combinedl);
        U64  const keyedl = (U64)combinedl ^ (XXH_readLE32(key32)   + seed);
        U64  const keyedh = (U64)combinedh ^ (XXH_readLE32(key32+1) - seed);
        U64  const mixedl = keyedl * PRIME64_1;
        U64  const mixedh = keyedh * PRIME64_2;
        XXH128_hash_t const h128 = { XXH3_avalanche(mixedl) /*low64*/, XXH3_avalanche(mixedh) /*high64*/ };
        return h128;
    }
}


XXH_FORCE_INLINE XXH128_hash_t
XXH3_len_4to8_128b(const void* data, size_t len, const void* keyPtr, XXH64_hash_t seed)
{
    XXH_ASSERT(data != NULL);
    XXH_ASSERT(keyPtr != NULL);
    XXH_ASSERT(4 <= len && len <= 8);
    {   U32 const in1 = XXH_readLE32(data);
        U32 const in2 = XXH_readLE32((const BYTE*)data + len - 4);
        U64 const in64l = in1 + ((U64)in2 << 32);
        U64 const in64h = XXH_swap64(in64l);
        U64 const keyedl = in64l ^ (XXH_readLE64(keyPtr) + seed);
        U64 const keyedh = in64h ^ (XXH_readLE64((const char*)keyPtr + 8) - seed);
        U64 const mix64l1 = len + ((keyedl ^ (keyedl >> 51)) * PRIME32_1);
        U64 const mix64l2 = (mix64l1 ^ (mix64l1 >> 47)) * PRIME64_2;
        U64 const mix64h1 = ((keyedh ^ (keyedh >> 47)) * PRIME64_1) - len;
        U64 const mix64h2 = (mix64h1 ^ (mix64h1 >> 43)) * PRIME64_4;
        {   XXH128_hash_t const h128 = { XXH3_avalanche(mix64l2) /*low64*/, XXH3_avalanche(mix64h2) /*high64*/ };
            return h128;
    }   }
}

XXH_FORCE_INLINE XXH128_hash_t
XXH3_len_9to16_128b(const void* data, size_t len, const void* keyPtr, XXH64_hash_t seed)
{
    XXH_ASSERT(data != NULL);
    XXH_ASSERT(keyPtr != NULL);
    XXH_ASSERT(9 <= len && len <= 16);
    {   const U64* const key64 = (const U64*) keyPtr;
        U64 const ll1 = XXH_readLE64(data) ^ (XXH_readLE64(key64) + seed);
        U64 const ll2 = XXH_readLE64((const BYTE*)data + len - 8) ^ (XXH_readLE64(key64+1) - seed);
        U64 const inlow = ll1 ^ ll2;
        XXH128_hash_t m128 = XXH_mult64to128(inlow, PRIME64_1);
        m128.high64 += ll2 * PRIME64_1;
        m128.low64  ^= (m128.high64 >> 32);
        {   XXH128_hash_t h128 = XXH_mult64to128(m128.low64, PRIME64_2);
            h128.high64 += m128.high64 * PRIME64_2;
            h128.low64   = XXH3_avalanche(h128.low64);
            h128.high64  = XXH3_avalanche(h128.high64);
            return h128;
    }   }
}

/* Assumption : `secret` size is >= 16
 * Note : it should be >= XXH3_SECRET_SIZE_MIN anyway */
XXH_FORCE_INLINE XXH128_hash_t
XXH3_len_0to16_128b(const void* data, size_t len, const void* secret, XXH64_hash_t seed)
{
    XXH_ASSERT(len <= 16);
    {   if (len > 8) return XXH3_len_9to16_128b(data, len, secret, seed);
        if (len >= 4) return XXH3_len_4to8_128b(data, len, secret, seed);
        if (len) return XXH3_len_1to3_128b(data, len, secret, seed);
        {   XXH128_hash_t const h128 = { 0, 0 };
            return h128;
    }   }
}

XXH_FORCE_INLINE XXH128_hash_t
XXH3_hashLong_128b_internal(const void* XXH_RESTRICT data, size_t len,
                            const void* XXH_RESTRICT secret, size_t secretSize)
{
    XXH_ALIGN(XXH_ACC_ALIGN) U64 acc[ACC_NB] = XXH3_INIT_ACC;

    XXH3_hashLong_internal_loop(acc, data, len, secret, secretSize, XXH3_acc_128bits);

    /* converge into final hash */
    XXH_STATIC_ASSERT(sizeof(acc) == 64);
    XXH_ASSERT(secretSize >= sizeof(acc) + XXH_SECRET_MERGEACCS_START);
    {   U64 const low64 = XXH3_mergeAccs(acc, (const char*)secret + XXH_SECRET_MERGEACCS_START, (U64)len * PRIME64_1);
        U64 const high64 = XXH3_mergeAccs(acc, (const char*)secret + secretSize - sizeof(acc) - XXH_SECRET_MERGEACCS_START, ~((U64)len * PRIME64_2));
        XXH128_hash_t const h128 = { low64, high64 };
        return h128;
    }
}

XXH_NO_INLINE XXH128_hash_t    /* It's important for performance that XXH3_hashLong is not inlined. Not sure why (uop cache maybe ?), but difference is large and easily measurable */
XXH3_hashLong_128b_defaultSecret(const void* data, size_t len)
{
    return XXH3_hashLong_128b_internal(data, len, kSecret, sizeof(kSecret));
}

XXH_NO_INLINE XXH128_hash_t    /* It's important for performance that XXH3_hashLong is not inlined. Not sure why (uop cache maybe ?), but difference is large and easily measurable */
XXH3_hashLong_128b_withSecret(const void* data, size_t len,
                              const void* secret, size_t secretSize)
{
    return XXH3_hashLong_128b_internal(data, len, secret, secretSize);
}

XXH_NO_INLINE XXH128_hash_t    /* It's important for performance that XXH3_hashLong is not inlined. Not sure why (uop cache maybe ?), but difference is large and easily measurable */
XXH3_hashLong_128b_withSeed(const void* data, size_t len, XXH64_hash_t seed)
{
    XXH_ALIGN(8) char secret[XXH_SECRET_DEFAULT_SIZE];
    if (seed == 0) return XXH3_hashLong_128b_defaultSecret(data, len);
    XXH3_initKeySeed(secret, seed);
    return XXH3_hashLong_128b_internal(data, len, secret, sizeof(secret));
}

XXH_NO_INLINE XXH128_hash_t
XXH3_len_129to240_128b(const void* XXH_RESTRICT data, size_t len,
                      const void* XXH_RESTRICT secret, size_t secretSize,
                      XXH64_hash_t seed)
{
    const BYTE* const p = (const BYTE*)data;
    const char* const key = (const char*)secret;

    XXH_ASSERT(secretSize >= XXH3_SECRET_SIZE_MIN); (void)secretSize;
    XXH_ASSERT(128 < len && len <= XXH3_MIDSIZE_MAX);

    {   U64 acc1 = len * PRIME64_1;
        U64 acc2 = 0;
        int const nbRounds = (int)len / 32;
        int i;
        for (i=0; i<4; i++) {
            acc1 += XXH3_mix16B(p+(32*i),    key+(32*i),         seed);
            acc2 += XXH3_mix16B(p+(32*i)+16, key+(32*i)+16, 0ULL-seed);
        }
        acc1 = XXH3_avalanche(acc1);
        acc2 = XXH3_avalanche(acc2);
        XXH_ASSERT(nbRounds >= 4);
        for (i=4 ; i < nbRounds; i++) {
            acc1 += XXH3_mix16B(p+(32*i)   , key+(32*(i-4))    + XXH3_MIDSIZE_STARTOFFSET,      seed);
            acc2 += XXH3_mix16B(p+(32*i)+16, key+(32*(i-4))+16 + XXH3_MIDSIZE_STARTOFFSET, 0ULL-seed);
        }
        /* last bytes */
        acc1 += XXH3_mix16B(p + len - 16, key + XXH3_SECRET_SIZE_MIN - XXH3_MIDSIZE_LASTOFFSET     ,      seed);
        acc2 += XXH3_mix16B(p + len - 32, key + XXH3_SECRET_SIZE_MIN - XXH3_MIDSIZE_LASTOFFSET - 16, 0ULL-seed);

        {   U64 const low64 = acc1 + acc2;
            U64 const high64 = (acc1 * PRIME64_1) + (acc2 * PRIME64_4) + ((len - seed) * PRIME64_2);
            XXH128_hash_t const h128 = { XXH3_avalanche(low64), (XXH64_hash_t)0 - XXH3_avalanche(high64) };
            return h128;
        }
    }
}

XXH_FORCE_INLINE XXH128_hash_t
XXH3_len_17to128_128b(const void* XXH_RESTRICT data, size_t len,
                     const void* XXH_RESTRICT secret, size_t secretSize,
                     XXH64_hash_t seed)
{
    const BYTE* const p = (const BYTE*)data;
    const char* const key = (const char*)secret;

    XXH_ASSERT(secretSize >= XXH3_SECRET_SIZE_MIN); (void)secretSize;
    XXH_ASSERT(16 < len && len <= 128);

    {   U64 acc1 = len * PRIME64_1;
        U64 acc2 = 0;
        if (len > 32) {
            if (len > 64) {
                if (len > 96) {
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

        {   U64 const low64 = acc1 + acc2;
            U64 const high64 = (acc1 * PRIME64_1) + (acc2 * PRIME64_4) + ((len - seed) * PRIME64_2);
            XXH128_hash_t const h128 = { XXH3_avalanche(low64), (XXH64_hash_t)0 - XXH3_avalanche(high64) };
            return h128;
        }
    }
}

XXH_PUBLIC_API XXH128_hash_t XXH3_128bits(const void* data, size_t len)
{
    if (len <= 16) return XXH3_len_0to16_128b(data, len, kSecret, 0);
    if (len <= 128) return XXH3_len_17to128_128b(data, len, kSecret, sizeof(kSecret), 0);
    if (len <= XXH3_MIDSIZE_MAX) return XXH3_len_129to240_128b(data, len, kSecret, sizeof(kSecret), 0);
    return XXH3_hashLong_128b_defaultSecret(data, len);
}

XXH_PUBLIC_API XXH128_hash_t
XXH3_128bits_withSecret(const void* data, size_t len, const void* secret, size_t secretSize)
{
    XXH_ASSERT(secretSize >= XXH3_SECRET_SIZE_MIN);
    /* if an action must be taken should `secret` conditions not be respected,
     * it should be done here.
     * For now, it's a contract pre-condition.
     * Adding a check and a branch here would cost performance at every hash */
     if (len <= 16) return XXH3_len_0to16_128b(data, len, secret, 0);
     if (len <= 128) return XXH3_len_17to128_128b(data, len, secret, secretSize, 0);
     if (len <= XXH3_MIDSIZE_MAX) return XXH3_len_129to240_128b(data, len, secret, secretSize, 0);
     return XXH3_hashLong_128b_withSecret(data, len, secret, secretSize);
}

XXH_PUBLIC_API XXH128_hash_t
XXH3_128bits_withSeed(const void* data, size_t len, XXH64_hash_t seed)
{
    if (len <= 16) return XXH3_len_0to16_128b(data, len, kSecret, seed);
    if (len <= 128) return XXH3_len_17to128_128b(data, len, kSecret, sizeof(kSecret), seed);
    if (len <= XXH3_MIDSIZE_MAX) return XXH3_len_129to240_128b(data, len, kSecret, sizeof(kSecret), seed);
    return XXH3_hashLong_128b_withSeed(data, len, seed);
}

XXH_PUBLIC_API XXH128_hash_t
XXH128(const void* data, size_t len, XXH64_hash_t seed)
{
    return XXH3_128bits_withSeed(data, len, seed);
}


/* ===   XXH3 128-bit streaming   === */

/* all the functions are actually the same as for 64-bit streaming variant,
   just the reset one is different (different initial acc values for 0,5,6,7),
   and near the end of the digest function */

static void
XXH3_128bits_reset_internal(XXH3_state_t* statePtr,
                           XXH64_hash_t seed,
                           const void* secret, size_t secretSize)
{
    XXH3_64bits_reset_internal(statePtr, seed, secret, secretSize);
}

XXH_PUBLIC_API XXH_errorcode
XXH3_128bits_reset(XXH3_state_t* statePtr)
{
    if (statePtr == NULL) return XXH_ERROR;
    XXH3_128bits_reset_internal(statePtr, 0, kSecret, XXH_SECRET_DEFAULT_SIZE);
    return XXH_OK;
}

XXH_PUBLIC_API XXH_errorcode
XXH3_128bits_reset_withSecret(XXH3_state_t* statePtr, const void* secret, size_t secretSize)
{
    if (statePtr == NULL) return XXH_ERROR;
    XXH3_128bits_reset_internal(statePtr, 0, secret, secretSize);
    if (secret == NULL) return XXH_ERROR;
    if (secretSize < XXH3_SECRET_SIZE_MIN) return XXH_ERROR;
    return XXH_OK;
}

XXH_PUBLIC_API XXH_errorcode
XXH3_128bits_reset_withSeed(XXH3_state_t* statePtr, XXH64_hash_t seed)
{
    if (statePtr == NULL) return XXH_ERROR;
    XXH3_128bits_reset_internal(statePtr, seed, kSecret, XXH_SECRET_DEFAULT_SIZE);
    XXH3_initKeySeed(statePtr->customSecret, seed);
    statePtr->secret = statePtr->customSecret;
    return XXH_OK;
}

XXH_PUBLIC_API XXH_errorcode
XXH3_128bits_update(XXH3_state_t* state, const void* input, size_t len)
{
    return XXH3_update(state, input, len, XXH3_acc_128bits);
}

XXH_PUBLIC_API XXH128_hash_t XXH3_128bits_digest (const XXH3_state_t* state)
{
    if (state->totalLen > XXH3_MIDSIZE_MAX) {
        XXH_ALIGN(XXH_ACC_ALIGN) XXH64_hash_t acc[ACC_NB];
        XXH3_digest_long(acc, state, XXH3_acc_128bits);
        XXH_ASSERT(state->secretLimit + STRIPE_LEN >= sizeof(acc) + XXH_SECRET_MERGEACCS_START);
        {   U64 const low64 = XXH3_mergeAccs(acc, (const char*)state->secret + XXH_SECRET_MERGEACCS_START, (U64)state->totalLen * PRIME64_1);
            U64 const high64 = XXH3_mergeAccs(acc, (const char*)state->secret + state->secretLimit + STRIPE_LEN - sizeof(acc) - XXH_SECRET_MERGEACCS_START, ~((U64)state->totalLen * PRIME64_2));
            XXH128_hash_t const h128 = { low64, high64 };
            return h128;
        }
    }
    /* len <= XXH3_MIDSIZE_MAX : short code */
    if (state->seed)
        return XXH3_128bits_withSeed(state->buffer, (size_t)state->totalLen, state->seed);
    return XXH3_128bits_withSecret(state->buffer, (size_t)(state->totalLen), state->secret, state->secretLimit + STRIPE_LEN);
}

/* 128-bit utility functions */

#include <string.h>   /* memcmp */

/* return : 1 is equal, 0 if different */
XXH_PUBLIC_API int XXH128_isEqual(XXH128_hash_t h1, XXH128_hash_t h2)
{
    /* note : XXH128_hash_t is compact, it has no padding byte */
    return !(memcmp(&h1, &h2, sizeof(h1)));
}

/* This prototype is compatible with stdlib's qsort().
 * return : >0 if *h128_1  > *h128_2
 *          <0 if *h128_1  < *h128_2
 *          =0 if *h128_1 == *h128_2  */
XXH_PUBLIC_API int XXH128_cmp(const void* h128_1, const void* h128_2)
{
    XXH128_hash_t const h1 = *(const XXH128_hash_t*)h128_1;
    XXH128_hash_t const h2 = *(const XXH128_hash_t*)h128_2;
    int const hcmp = (h1.high64 > h2.high64) - (h2.high64 > h1.high64);
    /* note : bets that, in most cases, hash values are different */
    if (hcmp) return hcmp;
    return (h1.low64 > h2.low64) - (h2.low64 > h1.low64);
}


/*======   Canonical representation   ======*/
XXH_PUBLIC_API void
XXH128_canonicalFromHash(XXH128_canonical_t* dst, XXH128_hash_t hash)
{
    XXH_STATIC_ASSERT(sizeof(XXH128_canonical_t) == sizeof(XXH128_hash_t));
    if (XXH_CPU_LITTLE_ENDIAN) {
        hash.high64 = XXH_swap64(hash.high64);
        hash.low64  = XXH_swap64(hash.low64);
    }
    memcpy(dst, &hash.high64, sizeof(hash.high64));
    memcpy((char*)dst + sizeof(hash.high64), &hash.low64, sizeof(hash.low64));
}

XXH_PUBLIC_API XXH128_hash_t
XXH128_hashFromCanonical(const XXH128_canonical_t* src)
{
    XXH128_hash_t h;
    h.high64 = XXH_readBE64(src);
    h.low64  = XXH_readBE64(src->digest + 8);
    return h;
}



#endif  /* XXH3_H */
