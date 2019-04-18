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
   When XXH_MULTI_TARGET is defined, this will be compiled multiple times.
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


/* ==========================================
 * Vectorization detection
 * ========================================== */

#ifndef XXH_VECTOR    /* can be defined on command line */
#  if defined(__AVX2__)
#    define XXH_VECTOR XXH_AVX2
#  elif defined(__SSE2__) \
    || (defined(_MSC_VER) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))) /* MSVC doesn't define __SSE2__. */
#    define XXH_VECTOR XXH_SSE2
/* msvc support maybe later */
#  elif defined(__GNUC__) \
  && (defined(__ARM_NEON__) || defined(__ARM_NEON)) \
  && defined(__LITTLE_ENDIAN__) /* ARM big endian is a thing */
#    define XXH_VECTOR XXH_NEON
#  else
#    define XXH_VECTOR XXH_SCALAR
#  endif
#endif


#if XXH_VECTOR == XXH_SSE2 || XXH_VECTOR == XXH_AVX2
#  include <immintrin.h>
#elif XXH_VECTOR == XXH_NEON
#  define inline __inline__ /* clang bug */
#    include <arm_neon.h>
#    undef inline
#endif

#undef hashLong
#ifdef XXH_MULTI_TARGET
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
/* The use of reserved identifiers is intentional; these are not to be used directly. */
#  if XXH_VECTOR == XXH_AVX2
#    define hashLong _XXH3_hashLong_AVX2
#  elif XXH_VECTOR == XXH_SSE2
#    define hashLong _XXH3_hashLong_SSE2
#  elif XXH_VECTOR == XXH_NEON
#    define hashLong _XXH3_hashLong_NEON
#  else
#    define hashLong _XXH3_hashLong_Scalar
#  endif
#else
#  define XXH_HIDDEN_API static
#  define hashLong XXH3_hashLong
#endif

/* ===    Long Keys    === */


XXH_FORCE_INLINE void
XXH3_accumulate_512_target(void* acc, const void *restrict data, const void *restrict key)
{
#if (XXH_VECTOR == XXH_AVX2)

    assert(((size_t)acc) & 31 == 0);
    {   ALIGN(32) __m256i* const xacc  =       (__m256i *) acc;
        const     __m256i* const xdata = (const __m256i *) data;
        const     __m256i* const xkey  = (const __m256i *) key;

        size_t i;
        for (i=0; i < STRIPE_LEN/sizeof(__m256i); i++) {
            __m256i const d   = _mm256_loadu_si256 (xdata+i);
            __m256i const k   = _mm256_loadu_si256 (xkey+i);
            __m256i const dk  = _mm256_xor_si256 (d,k);                                  /* uint32 dk[8]  = {d0+k0, d1+k1, d2+k2, d3+k3, ...} */
            __m256i const res = _mm256_mul_epu32 (dk, _mm256_shuffle_epi32 (dk, 0x31));  /* uint64 res[4] = {dk0*dk1, dk2*dk3, ...} */
            __m256i const add = _mm256_add_epi64(d, xacc[i]);
            xacc[i]  = _mm256_add_epi64(res, add);
        }
    }

#elif (XXH_VECTOR == XXH_SSE2)

    assert(((size_t)acc) & 15 == 0);
    {   ALIGN(16) __m128i* const xacc  =       (__m128i *) acc;
        const     __m128i* const xdata = (const __m128i *) data;
        const     __m128i* const xkey  = (const __m128i *) key;

        size_t i;
        for (i=0; i < STRIPE_LEN/sizeof(__m128i); i++) {
            __m128i const d   = _mm_loadu_si128 (xdata+i);
            __m128i const k   = _mm_loadu_si128 (xkey+i);
            __m128i const dk  = _mm_xor_si128 (d,k);                                 /* uint32 dk[4]  = {d0+k0, d1+k1, d2+k2, d3+k3} */
            __m128i const res = _mm_mul_epu32 (dk, _mm_shuffle_epi32 (dk, 0x31));    /* uint64 res[2] = {dk0*dk1,dk2*dk3} */
            __m128i const add = _mm_add_epi64(d, xacc[i]);
            xacc[i]  = _mm_add_epi64(res, add);
        }
    }

#elif (XXH_VECTOR == XXH_NEON)   /* to be updated, no longer with latest sse/avx updates */

    assert(((size_t)acc) & 15 == 0);
    {       uint64x2_t* const xacc  =     (uint64x2_t *)acc;
        const uint32_t* const xdata = (const uint32_t *)data;
        const uint32_t* const xkey  = (const uint32_t *)key;

        size_t i;
        for (i=0; i < STRIPE_LEN / sizeof(uint64x2_t); i++) {
            uint32x4_t const d = vld1q_u32(xdata+i*4);                           /* U32 d[4] = xdata[i]; */
            uint32x4_t const k = vld1q_u32(xkey+i*4);                            /* U32 k[4] = xkey[i]; */
            uint32x4_t dk = veorq_u32(d, k);                                     /* U32 dk[4] = {d0^k0, d1^k1, d2^k2, d3^k3} */
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
            __asm__("vzip.32 %e0, %f0" : "+w" (dk));                             /* dk = { dk0, dk2, dk1, dk3 }; */
            xacc[i] = vaddq_u64(xacc[i], vreinterpretq_u64_u32(d));              /* xacc[i] += (U64x2)d; */
            xacc[i] = vmlal_u32(xacc[i], vget_low_u32(dk), vget_high_u32(dk));   /* xacc[i] += { (U64)dk0*dk1, (U64)dk2*dk3 }; */
#else
            /* On aarch64, vshrn/vmovn seems to be equivalent to, if not faster than, the vzip method. */
            uint32x2_t dkL = vmovn_u64(vreinterpretq_u64_u32(dk));               /* U32 dkL[2] = dk & 0xFFFFFFFF; */
            uint32x2_t dkH = vshrn_n_u64(vreinterpretq_u64_u32(dk), 32);         /* U32 dkH[2] = dk >> 32; */
            xacc[i] = vaddq_u64(xacc[i], vreinterpretq_u64_u32(d));              /* xacc[i] += (U64x2)d; */
            xacc[i] = vmlal_u32(xacc[i], dkL, dkH);                              /* xacc[i] += (U64x2)dkL*(U64x2)dkH; */
#endif
        }
    }

#else   /* scalar variant of Accumulator - universal */

          U64* const xacc  =       (U64*) acc;   /* presumed aligned */
    const U32* const xdata = (const U32*) data;
    const U32* const xkey  = (const U32*) key;

    int i;
    for (i=0; i < (int)ACC_NB; i++) {
        int const left = 2*i;
        int const right= 2*i + 1;
        U32 const dataLeft  = XXH_readLE32(xdata + left);
        U32 const dataRight = XXH_readLE32(xdata + right);
        xacc[i] += XXH_mult32to64(dataLeft ^ xkey[left], dataRight ^ xkey[right]);
        xacc[i] += dataLeft + ((U64)dataRight << 32);
    }

#endif
}

static void XXH3_scrambleAcc_target(void* acc, const void* key)
{
#if (XXH_VECTOR == XXH_AVX2)

    assert(((size_t)acc) & 31 == 0);
    {   ALIGN(32) __m256i* const xacc = (__m256i*) acc;
        const     __m256i* const xkey  = (const __m256i *) key;
        const __m256i k1 = _mm256_set1_epi32((int)PRIME32_1);

        size_t i;
        for (i=0; i < STRIPE_LEN/sizeof(__m256i); i++) {
            __m256i data = xacc[i];
            __m256i const shifted = _mm256_srli_epi64(data, 47);
            data = _mm256_xor_si256(data, shifted);

            {   __m256i const k   = _mm256_loadu_si256 (xkey+i);
                __m256i const dk  = _mm256_xor_si256   (data, k);          /* U32 dk[4]  = {d0+k0, d1+k1, d2+k2, d3+k3} */

                __m256i const dk1 = _mm256_mul_epu32 (dk, k1);

                __m256i const d2  = _mm256_shuffle_epi32 (dk, 0x31);
                __m256i const dk2 = _mm256_mul_epu32 (d2, k1);
                __m256i const dk2h= _mm256_slli_epi64 (dk2, 32);

                xacc[i] = _mm256_add_epi64(dk1, dk2h);
        }   }
    }

#elif (XXH_VECTOR == XXH_SSE2)

    {   ALIGN(16) __m128i* const xacc = (__m128i*) acc;
        const     __m128i* const xkey  = (const __m128i *) key;
        const __m128i k1 = _mm_set1_epi32((int)PRIME32_1);

        size_t i;
        for (i=0; i < STRIPE_LEN/sizeof(__m128i); i++) {
            __m128i data = xacc[i];
            __m128i const shifted = _mm_srli_epi64(data, 47);
            data = _mm_xor_si128(data, shifted);

            {   __m128i const k   = _mm_loadu_si128 (xkey+i);
                __m128i const dk  = _mm_xor_si128   (data,k);

                __m128i const dk1 = _mm_mul_epu32 (dk,k1);

                __m128i const d2  = _mm_shuffle_epi32 (dk, 0x31);
                __m128i const dk2 = _mm_mul_epu32 (d2,k1);
                __m128i const dk2h= _mm_slli_epi64(dk2, 32);

                xacc[i] = _mm_add_epi64(dk1, dk2h);
        }   }
    }

#elif (XXH_VECTOR == XXH_NEON)   /*  <============================================ Needs update !!!!!!!!!!! */

    assert(((size_t)acc) & 15 == 0);
    {       uint64x2_t* const xacc =     (uint64x2_t*) acc;
        const uint32_t* const xkey = (const uint32_t*) key;
        size_t i;
        uint32x2_t const k1 = vdup_n_u32(PRIME32_1);
        uint32x2_t const k2 = vdup_n_u32(PRIME32_2);

        for (i=0; i < STRIPE_LEN/sizeof(uint64x2_t); i++) {
            uint64x2_t data = xacc[i];
            uint64x2_t const shifted = vshrq_n_u64(data, 47);          /* uint64 shifted[2] = data >> 47; */
            data = veorq_u64(data, shifted);                           /* data ^= shifted; */
            {
                uint32x4_t const k = vld1q_u32(xkey+i*4);               /* load */
                uint32x4_t const dk = veorq_u32(vreinterpretq_u32_u64(data), k); /* dk = data ^ key */
                /* shuffle: 0, 1, 2, 3 -> 0, 2, 1, 3 */
                uint32x2x2_t const split = vzip_u32(vget_low_u32(dk), vget_high_u32(dk));
                uint64x2_t const dk1 = vmull_u32(split.val[0],k1);     /* U64 dk[2]  = {(U64)d0*k0, (U64)d2*k2} */
                uint64x2_t const dk2 = vmull_u32(split.val[1],k2);     /* U64 dk2[2] = {(U64)d1*k1, (U64)d3*k3} */
                xacc[i] = veorq_u64(dk1, dk2);                         /* xacc[i] = dk^dk2;             */
        }   }
    }

#else   /* scalar variant of Scrambler - universal */

          U64* const xacc =       (U64*) acc;
    const U32* const xkey = (const U32*) key;

    int i;
    assert(((size_t)acc) & 7 == 0);
    for (i=0; i < (int)ACC_NB; i++) {
        U64 const key64 = XXH3_readKey64(xkey + 2*i);
        U64 acc64 = xacc[i];
        acc64 ^= acc64 >> 47;
        acc64 ^= key64;
        acc64 *= PRIME32_1;
        xacc[i] = acc64;
    }

#endif
}

static void XXH3_accumulate_target(U64* acc, const void* restrict data, const U32* restrict key, size_t nbStripes)
{
    size_t n;
    /* Clang doesn't unroll this loop without the pragma. Unrolling can be up to 1.4x faster. */
#if defined(__clang__) && !defined(__OPTIMIZE_SIZE__)
#  pragma clang loop unroll(enable)
#endif
    for (n = 0; n < nbStripes; n++ ) {
        XXH3_accumulate_512_target(acc, (const BYTE*)data + n*STRIPE_LEN, key);
        key += 2;
    }
}

XXH_HIDDEN_API void
hashLong(U64* restrict acc, const void* restrict data, size_t len, const U32* restrict key)
{
    #define NB_KEYS ((KEYSET_DEFAULT_SIZE - STRIPE_ELTS) / 2)

    size_t const block_len = STRIPE_LEN * NB_KEYS;
    size_t const nb_blocks = len / block_len;

    size_t n;
    for (n = 0; n < nb_blocks; n++) {
        XXH3_accumulate_target(acc, (const BYTE*)data + n*block_len, key, NB_KEYS);
        XXH3_scrambleAcc_target(acc, key + (KEYSET_DEFAULT_SIZE - STRIPE_ELTS));
    }

    /* last partial block */
    assert(len > STRIPE_LEN);
    {   size_t const nbStripes = (len % block_len) / STRIPE_LEN;
        assert(nbStripes < NB_KEYS);
        XXH3_accumulate_target(acc, (const BYTE*)data + nb_blocks*block_len, key, nbStripes);

        /* last stripe */
        if (len & (STRIPE_LEN - 1)) {
            const BYTE* const p = (const BYTE*) data + len - STRIPE_LEN;
            XXH3_accumulate_512_target(acc, p, key + nbStripes*2);
    }   }
}

#undef hashLong
#endif /* XXH3_TARGET_C */
