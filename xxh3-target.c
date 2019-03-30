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
   This contains the target-dependent code for XXH3, namely
   for the hashLong routine.
   When XXH_MULTI_TARGET is defined, this will be compiled three times
   for x86-based targets: One for scalar-only code, one for SSE2 code,
   and one for AVX2 code. It will emit a different symbol each time.
   When it is not defined, this file is included directly from xxh3.h.
*/


/* ===   Dependencies   === */

#ifndef XXH3_TARGET_C
#define XXH3_TARGET_C

#undef XXH_INLINE_ALL   /* in case it's already defined */
#define XXH_INLINE_ALL
#include "xxhash.h"

#undef NDEBUG   /* avoid redefinition */
#define NDEBUG
#include <assert.h>

#if defined(__SSE2__)
#  include <immintrin.h>
#endif

#if defined(__GNUC__)
#  if defined(__SSE2__)
#    include <x86intrin.h>
#  elif defined(__ARM_NEON__) || defined(__ARM_NEON)
#    define inline __inline__ /* clang bug */
#    include <arm_neon.h>
#    undef inline
#  endif
#elif defined(_MSC_VER)
#  include <intrin.h>
#  ifndef ALIGN
#  define ALIGN(n)      __declspec(align(n))
#endif
#else
#ifndef ALIGN
#  define ALIGN(n)   /* disabled */
#endif
#endif

#ifndef XXH_MULTI_TARGET
#  define XXH_HIDDEN_API static
#else
#  if !defined(_WIN32) && defined(__GNUC__) && __GNUC__ >= 4
   /* Avoid leaking a symbol */
#    define XXH_VISIBILITY_HIDDEN  __attribute__ ((visibility ("hidden")))
#  else
#    define XXH_VISIBILITY_HIDDEN
#  endif
#  ifdef __cplusplus
#    define XXH_HIDDEN_API extern "C" XXH_VISIBILITY_HIDDEN
#  else
#    define XXH_HIDDEN_API XXH_VISIBILITY_HIDDEN
#  endif
#endif

#ifndef KEYSET_DEFAULT_SIZE
#  define KEYSET_DEFAULT_SIZE 48   /* minimum 32 */
#endif
/* ===    Long Keys    === */
#ifndef STRIPE_LEN
#  define STRIPE_LEN 64
#endif
#ifndef STRIPE_ELTS
#  define STRIPE_ELTS (STRIPE_LEN / sizeof(U32))
#endif


/* ==========================================
 * Vectorization detection
 * ========================================== */
#define XXH_SCALAR 0
#define XXH_SSE2   1
#define XXH_AVX2   2
#define XXH_NEON   3
#define XXH_VSX    4

#ifndef XXH_VECTOR
#  if defined(__AVX2__)
#    define XXH_VECTOR XXH_AVX2
#  elif defined(__SSE2__)
#    define XXH_VECTOR XXH_SSE2
/* msvc support maybe later */
#  elif defined(__GNUC__) \
    && (defined(__ARM_NEON__) || defined(__ARM_NEON)) \
    && defined(__LITTLE_ENDIAN__) /* ARM big endian is a thing */
#    define XXH_VECTOR XXH_NEON
/* Right now, we are only supporting ppc64le, don't have a ppc64be right now to test.
 * code is very fast with recent clang versions (30 GB/s vs 15 GB/s XXH64 on POWER9),
 * but hasn't been verified for correctness yet. */
#  elif defined(__LITTLE_ENDIAN__) && defined(__PPC64__) && defined(__VSX__) && defined(__GNUC__)
#    define XXH_VECTOR XXH_VSX
#  else
#    define XXH_VECTOR XXH_SCALAR
#  endif
#endif

#ifdef XXH_MULTI_TARGET
/* The use of reserved identifiers is intentional; these are not to be used directly. */
#  if XXH_VECTOR == XXH_AVX2
#    define hashLong _XXH3_hashLong_AVX2
#  elif XXH_VECTOR == XXH_SSE2
#    define hashLong _XXH3_hashLong_SSE2
#  else
#    define hashLong _XXH3_hashLong_Scalar
#  endif
#else
#  define hashLong XXH3_hashLong
#endif


#define XXH_CONCAT_2(x, y) x##y
#define XXH_CONCAT(x, y) XXH_CONCAT_2(x, y)

/* So we don't need to write the SSE2 and AVX2 code twice.
 * XXH_vec = either __m128i or __m256i
 * XXH_MM(n) = either _mm_n or _mm256_n
 * XXH_MM_SI(n) = either _mm_n_si128 or _mm256_n_si256 */
#if XXH_VECTOR == XXH_AVX2
typedef __m256i XXH_vec;
#  define XXH_MM(x) XXH_CONCAT(_mm256_,x)
#  define XXH_MM_SI(x) XXH_MM(XXH_CONCAT(x,_si256))
#  define VEC_SIZE 32
#elif XXH_VECTOR == XXH_SSE2
typedef __m128i XXH_vec;
#  define XXH_MM(x) XXH_CONCAT(_mm_,x)
#  define XXH_MM_SI(x) XXH_MM(XXH_CONCAT(x,_si128))
#  define VEC_SIZE 16
#endif

/* VSX stuff */
#if XXH_VECTOR == XXH_VSX
#  include <altivec.h>
typedef __vector unsigned long long U64x2;
typedef __vector unsigned U32x4;
/* Adapted from https://github.com/google/highwayhash/blob/master/highwayhash/hh_vsx.h.
 * Leaving the BE code here just to be safe. */
XXH_FORCE_INLINE U64x2 XXH_vsxMultEven(U32x4 a, U32x4 b) {
    U64x2 result;
#ifdef __LITTLE_ENDIAN__
    __asm__("vmulouw %0, %1, %2" : "=v" (result) : "v" (a), "v" (b));
#else
    __asm__("vmuleuw %0, %1, %2" : "=v" (result) : "v" (a), "v" (b));
#endif
    return result;
}
XXH_FORCE_INLINE U64x2 XXH_vsxMultOdd(U32x4 a, U32x4 b) {
    U64x2 result;
#ifdef __LITTLE_ENDIAN__
    __asm__("vmuleuw %0, %1, %2" : "=v" (result) : "v" (a), "v" (b));
#else
    __asm__("vmuloww %0, %1, %2" : "=v" (result) : "v" (a), "v" (b));
#endif
    return result;
}
#endif

#ifndef ACC_NB
#  define ACC_NB (STRIPE_LEN / sizeof(U64))
#endif
XXH_FORCE_INLINE void
XXH3_accumulate_512(void *restrict acc, const void *restrict data, const void *restrict key)
{

#if (XXH_VECTOR == XXH_AVX2) || (XXH_VECTOR == XXH_SSE2)
    assert(((size_t)acc) & (VEC_SIZE - 1) == 0);
    {   ALIGN(VEC_SIZE) XXH_vec* const xacc  =       (XXH_vec *) acc;
        const           XXH_vec* const xdata = (const XXH_vec *) data;
        const           XXH_vec* const xkey  = (const XXH_vec *) key;

        size_t i;
        for (i=0; i < STRIPE_LEN / VEC_SIZE; i++) {
            /* data_vec = xdata[i]; */
            XXH_vec const data_vec = XXH_MM_SI(loadu)      (xdata + i);
            /* key_vec  = xkey[i];  */
            XXH_vec const key_vec  = XXH_MM_SI(loadu)      (xkey + i);
            /* data_key = data_vec ^ key_vec; */
            XXH_vec const data_key = XXH_MM_SI(xor)        (data_vec, key_vec);
            /* shuffled = data_key[1, undef, 3, undef]; // essentially data_key >> 32; */
            XXH_vec const shuffled = XXH_MM(shuffle_epi32) (data_key, 0x31);
            /* product  = (shuffled & 0xFFFFFFFF) * (data_key & 0xFFFFFFFF); */
            XXH_vec const product  = XXH_MM(mul_epu32)     (shuffled, data_key);
            /* xacc[i] += data_vec; */
            xacc[i] = XXH_MM(add_epi64) (xacc[i], data_vec);
            /* xacc[i] += product; */
            xacc[i] = XXH_MM(add_epi64) (xacc[i], product);
        }
    }

#elif (XXH_VECTOR == XXH_NEON)   /* to be updated, no longer with latest sse/avx updates */

    assert(((size_t)acc) & 15 == 0);
    {
        uint64x2_t* const xacc  =     (uint64x2_t *) acc;
        /* We don't use a uint32x4_t pointer because it causes bus errors on ARMv7. */
        uint32_t const* const xdata = (const uint32_t *) data;
        uint32_t const* const xkey  = (const uint32_t *) key;

        size_t i;
        for (i=0; i < STRIPE_LEN / sizeof(uint64x2_t); i++) {
#if !defined(__aarch64__) && !defined(__arm64__) /* ARM32-specific hack */
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
            uint32x4_t       data_key    = veorq_u32(data_vec, key_vec);

            /* Here's the magic. We use the quirkiness of vzip to shuffle data_key in place.
             * shuffle: data_key[0, 1, 2, 3] = data_key[0, 2, 1, 3] */
            __asm__("vzip.32 %e0, %f0" : "+w" (data_key));
            /* xacc[i] += data_vec; */
            xacc[i] = vaddq_u64(xacc[i], vreinterpretq_u64_u32(data_vec));
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
            /* xacc[i] += data_vec; */
            xacc[i] = vaddq_u64 (xacc[i], vreinterpretq_u64_u32(data_vec));
            /* xacc[i] += (uint64x2_t) data_key_lo * (uint64x2_t) data_key_hi; */
            xacc[i] = vmlal_u32 (xacc[i], data_key_lo, data_key_hi);
#endif
        }
    }
#elif XXH_VECTOR == XXH_VSX
          U64x2* const xacc =        (U64x2*) acc;
    U64x2 const* const xdata = (U64x2 const*) data;
    U64x2 const* const xkey  = (U64x2 const*) key;
    U64x2 const v32 = { 32,  32 };

    size_t i;
    for (i = 0; i < STRIPE_LEN / sizeof(U64x2); i++) {
        /* data_vec = xdata[i]; */
        U64x2 const data_vec = vec_vsx_ld(0, xdata + i);
        /* key_vec = xkey[i]; */
        U64x2 const key_vec = vec_vsx_ld(0, xkey + i);
        U64x2 data_key = data_vec ^ key_vec;
        /* shuffled = (data_key << 32) | (data_key >> 32); */
        U32x4 shuffled = (U32x4)vec_rl(data_key, v32);
        /* product = ((U64x2)data_key & 0xFFFFFFFF) * ((U64x2)shuffled & 0xFFFFFFFF); */
        U64x2 product = XXH_vsxMultEven((U32x4)data_key, shuffled);

        xacc[i] += product;
        xacc[i] += data_vec;
    }
#else   /* scalar variant - universal */
          U64* const xacc  =       (U64*) acc;   /* presumed aligned */
    const U32* const xdata = (const U32*) data;
    const U32* const xkey  = (const U32*) key;
    size_t i;

    for (i=0; i < ACC_NB; i++) {
        U64 const data_val = XXH_readLE64(xdata + 2 * i);
        U64 const key_val = XXH3_readKey64(xkey + 2 * i);
        U64 const data_key  = key_val ^ data_val;
        xacc[i] += XXH_mult32to64(data_key & 0xFFFFFFFF, data_key >> 32);
        xacc[i] += data_val;
    }
#endif
}

XXH_FORCE_INLINE void
XXH3_scrambleAcc(void* restrict acc, const void* restrict key)
{
#if (XXH_VECTOR == XXH_AVX2) || (XXH_VECTOR == XXH_SSE2)
    assert(((size_t)acc) & (VEC_SIZE - 1) == 0);
    {
        ALIGN(VEC_SIZE)
        XXH_vec      * const xacc =       (XXH_vec*) acc;
        XXH_vec const* const xkey = (const XXH_vec*) key;
        XXH_vec const prime = XXH_MM(set1_epi32) ((int) PRIME32_1);

        size_t i;
        for (i=0; i < STRIPE_LEN / VEC_SIZE; i++) {
            /* data_vec = xacc[i] ^ (xacc[i] >> 47); */
            XXH_vec const acc_vec  = xacc[i];
            XXH_vec const shifted  = XXH_MM(srli_epi64)    (acc_vec, 47);
            XXH_vec const data_vec = XXH_MM_SI(xor)        (acc_vec, shifted);

            /* key_vec  = xkey[i]; */
            XXH_vec const key_vec  = XXH_MM_SI(loadu)      (xkey + i);
            /* data_key = data_vec ^ key_vec; */
            XXH_vec const data_key = XXH_MM_SI(xor)        (data_vec, key_vec);
            /* shuffled = data_key[1, undef, 3, undef]; // essentially data_key >> 32; */
            XXH_vec const shuffled = XXH_MM(shuffle_epi32) (data_key, 0x31);
            /* data_key *= PRIME32_1; // 32-bit * 64-bit */
            /* prod_hi = data_key >> 32 * PRIME32_1; */
            XXH_vec const prod_hi = XXH_MM(mul_epu32)      (shuffled, prime);
            /* prod_hi_top = prod_hi << 32; */
            XXH_vec const prod_hi_top = XXH_MM(slli_epi64) (prod_hi, 32);
            /* prod_lo = (data_key & 0xFFFFFFFF) * PRIME32_1; */
            XXH_vec const prod_lo = XXH_MM(mul_epu32)      (data_key, prime);
            /* xacc[i] = prod_hi_top + prod_lo; */
            xacc[i] = XXH_MM(add_epi64) (prod_hi_top, prod_lo);
        }
    }
#elif (XXH_VECTOR == XXH_NEON)

    assert(((size_t)acc) & 15 == 0);
    {
            uint64x2_t* const xacc =     (uint64x2_t*) acc;
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
        }
    }
#elif (XXH_VECTOR == XXH_VSX)
          U64x2* const xacc =       (U64x2*) acc;
    const U64x2* const xkey = (const U64x2*) key;
    /* constants */
    U64x2 const v32  = { 32, 32 };
    U64x2 const v47 = { 47, 47 };
    U32x4 const prime = { PRIME32_1, PRIME32_1, PRIME32_1, PRIME32_1 };
    size_t i;

    for (i = 0; i < STRIPE_LEN / sizeof(U64x2); i++) {
        U64x2 const acc_vec  = xacc[i];
        U64x2 const data_vec = acc_vec ^ (acc_vec >> v47);
        /* key_vec = xkey[i]; */
        U64x2 const key_vec  = vec_vsx_ld(0, xkey + i);
        U64x2 const data_key = data_vec ^ key_vec;

        /* data_key *= PRIME32_1 */

        /* prod_lo = ((U64x2)data_key & 0xFFFFFFFF) * ((U64x2)prime & 0xFFFFFFFF);  */
        U64x2 const prod_lo  = XXH_vsxMultEven((U32x4)data_key, prime);
        /* prod_hi = ((U64x2)data_key >> 32) * ((U64x2)prime >> 32);  */
        U64x2 const prod_hi  = XXH_vsxMultOdd((U32x4)data_key, prime);
        xacc[i] = prod_lo + (prod_hi << v32);
    }
#else   /* scalar variant - universal */

          U64* const xacc =       (U64*) acc;
    const U32* const xkey = (const U32*) key;

    size_t  i;
    assert(((size_t)acc) & 7 == 0);

    for (i=0; i < ACC_NB; i++) {
        U64 const key_val = XXH3_readKey64(xkey + (2 * i));
        U64 const acc_val = xacc[i];
        U64 const data_val = acc_val ^ (acc_val >> 47);
        U64 const data_key = data_val ^ key_val;
        /* U64 * U32
         * On 64-bit machines, this is a normal multiply.
         * On a 32-bit machine, the multiply would have a 32-bit add,
         * a 32-bit multiply, and a 32-bit to 64-bit multiply.
         *
         * In ARM assembly, it would be this:
         *
         * r0: before: data_key & 0xFFFFFFFF after: product & 0xFFFFFFFF
         * r1: before: data_key >> 32        after: product >> 32
         * r2: before: PRIME32_1             after: unchanged
         *
         *    umull   r0, r3, r0, r2     @ r0 = ((U64)r0 * (U64)r2) & 0xFFFFFFFF;
         *                               @ r3 = ((U64)r0 * (U64)r2) >> 32;
         *    mla     r1, r1, r2, r3     @ r1 = (r1 * r2) + r3;
         */
        U64 const productLo = (data_key & 0xFFFFFFFF) * PRIME32_1;
        U64 const productHi = (data_key >> 32) * PRIME32_1;
        xacc[i] = productLo + (productHi << 32);
    }

#endif
}

static void XXH3_accumulate(U64* acc, const void* restrict data, const U32* restrict key, size_t nbStripes)
{
    size_t n;

    for (n = 0; n < nbStripes; n++ ) {
        XXH3_accumulate_512(acc, (const BYTE*)data + n * STRIPE_LEN, key);
        key += 2;
    }
}

#ifdef __GNUC__ /* inlining often slows this down */
__attribute__((__noinline__))
#endif
XXH_HIDDEN_API void
hashLong(U64* restrict acc, const void* restrict data, size_t len, const U32* restrict key)
{
    #define NB_KEYS ((KEYSET_DEFAULT_SIZE - STRIPE_ELTS) / 2)

    size_t const block_len = STRIPE_LEN * NB_KEYS;
    size_t const nb_blocks = len / block_len;
    size_t n;

    for (n = 0; n < nb_blocks; n++) {
        size_t i;

        /* Clang doesn't unroll this loop without the pragma. Unrolling can make things much faster.
         * For some reason, Clang has trouble unrolling if this is in XXH3_accumulate.
         * ARM does not benefit from unrolling more than 2 times, but PPC and x86 do. */
#       if defined(__clang__) && !defined(__OPTIMIZE_SIZE__) \
               && (defined(__PPC__) || defined(__x86_64__) || defined(__i386__))
#           pragma clang loop unroll(enable)
#       endif
        for (i = 0; i < NB_KEYS; i += 2) {
            XXH3_accumulate_512(acc, (const BYTE*) data + n * STRIPE_LEN, key + (2 * i));
            XXH3_accumulate_512(acc, (const BYTE*) data + n * STRIPE_LEN, key + (2 * (i + 1)));
        }

        XXH3_scrambleAcc(acc, key + (KEYSET_DEFAULT_SIZE - STRIPE_ELTS));
    }

    /* last partial block */
    assert(len > STRIPE_LEN);
    {   size_t const nbStripes = (len % block_len) / STRIPE_LEN;
        assert(nbStripes < NB_KEYS);
        XXH3_accumulate(acc, (const BYTE*)data + nb_blocks*block_len, key, nbStripes);

        /* last stripe */
        if (len & (STRIPE_LEN - 1)) {
            const BYTE* const p = (const BYTE*) data + len - STRIPE_LEN;
            XXH3_accumulate_512(acc, p, key + nbStripes*2);
    }   }
}

#undef hashLong

#endif /* XXH3_TARGET_C */
