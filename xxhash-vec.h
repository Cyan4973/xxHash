/*
*  xxHash - Fast Hash algorithm
*  Copyright (C) 2012-2016, Yann Collet
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

#ifndef XXHASH_VEC_H
#define XXHASH_VEC_H

/* GCC and Clang warn on -Wcast-align when casting to __m128i. We don't really have many options
 * but to silence the warning. */
#ifdef __clang__
#define XXH_DISABLE_W_CAST_ALIGN(x) __extension__ ({ \
    _Pragma("clang diagnostic push") \
    _Pragma("clang diagnostic ignored \"-Wcast-align\"") \
    (x); \
    _Pragma("clang diagnostic pop") \
})
#elif defined(__GNUC__)
#define XXH_DISABLE_W_CAST_ALIGN(x) __extension__ ({ \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wcast-align\"") \
    (x); \
    _Pragma("GCC diagnostic pop") \
})
#else
#define XXH_DISABLE_W_CAST_ALIGN(x) (x)
#endif

#if defined(__GNUC__) && (defined(__ARM_NEON__) || defined(__ARM_NEON))

/* Dumb, dumb, dumb. Clang uses the inline keyword in arm_neon.h,
 * which causes errors on C89 mode. */
#if defined(__clang_) && !defined(inline)
#define inline __inline__
#endif
#include <arm_neon.h>
#undef inline


#define XXH_NEON
#undef XXH_VECTORIZE
#define XXH_VECTORIZE 1

typedef uint32x4_t U32x4;

#ifndef XXH_NO_LONG_LONG
typedef uint64x2_t U64x2;
#endif

/* Neither GCC or Clang can properly optimize the generic version
 * for Arm NEON.
 * Instead of the optimal version, which is this:
 *      vshl.i32        q9, q8, #13 @ q9 = q8 << 13
 *      vsra.32         q9, q8, #19 @ q9 += q8 >> 19;
 * GCC and Clang will produce this slower version:
 *      vshr.u32        q9, q8, #19
 *      vshl.i32        q8, q8, #13
 *      vorr            q8, q8, q9
 * This is much faster. However, Clang is stupid, and will happily optimize
 * the top code in intrinsics to the bottom code!
 *
 * So, for the best performance, we sadly need inline assembly. */
/* Clang will convert to Apple syntax for us. */
#define XXH_vec_rotl32(_x, _r) __extension__ ({ \
    uint32x4_t _tmp = vshlq_n_u32(_x, _r); \
    XXH_FORCE_VECTOR_REG(_tmp); \
    vsraq_n_u32(_tmp, _x, 32 - _r); \
})
#define XXH_vec_rotl64(_x, _r) __extension__ ({ \
    uint32x4_t _tmp = vshlq_n_u64(_x, _r); \
    XXH_FORCE_VECTOR_REG(_tmp); \
    vsraq_n_u64(_tmp, _x, 64 - _r); \
})

#define XXH_vec_load_unaligned(p) vreinterpretq_u32_u8(vld1q_u8((const BYTE*)(p)))
#define XXH_vec_store_unaligned(p, v) vst1q_u8((BYTE*)(p), vreinterpretq_u8_u32((v)))
#define XXH_vec_load_aligned(p) vreinterpretq_u32_u8(vld1q_u8((const BYTE*)(p)))
#define XXH_vec_store_aligned(p, v) vst1q_u8((BYTE*)(p), vreinterpretq_u8_u32((v)))

/**************************************************
 * 64-bit multiplication in NEON is missing from the instruction set.
 * There is a way to do it that works like so:
 *    Truncated grid method
 *      ___________________________
 *     |   |    b      |    a      |
 *     |---|-----------|-----------|
 *     | d | b*d << 64 | a*d << 32 |
 *     |---|-----------|-----------|
 *     | c | b*c << 32 |    a*c    |
 *     '---'-----------'-----------'
 *     Add all of them together.
 *    We can eliminate (b*d << 64) because we truncate to 64 bits.
 *    Technically only need to do a 32-64-bit multiply on (a*c). However,
 *    some versions of Clang will scalarize it if we use vmul because of an
 *    InstCombine bug.
 *
 * We can do this two ways, which we use both.
 *
 * XXH_U64x2_ssemul uses the same method used by Clang and GCC when multiplying
 * with vector extensions. We use this when we can interleave the inputs beforehand,
 * as that puts the elements in a way we can easily perform two multiplies on.
 * We do this with a vld2_u32 which will put the low bits first, and the high bits last.
 * This makes it easier to cross multiply.
 *
 * XXH_U64x2_twomul uses a different method, which does a U32x4 multiply and a vpaddlq_u32.
 * We do this when we can only interleave a single multiple. We interleave it with a vrev64q_u32
 * and a vmovn beforehand, and we don't have to interleave the input that way.
 */

FORCE_INLINE uint64x2_t XXH_U64x2_ssemul(const uint32x2x2_t top, const uint32x2x2_t bot)
{
    /*
     * product = (U64x2)a * (U64x2)d;
     * product += (U64x2)b * (U64x2)c;
     * product <<= 32;
     * product += (U64x2)a * (U64x2)c;
     */
    uint64x2_t product = vmull_u32(top.val[0], bot.val[1]);
    product = vmlal_u32(product, top.val[1], bot.val[0]);
    product = vshlq_n_u64(product, 32);
    product = vmlal_u32(product, top.val[0], bot.val[0]);
    return product;
}

FORCE_INLINE uint64x2_t XXH_U64x2_ssemul_add(uint64x2_t acc, const uint32x2x2_t top, const uint32x2x2_t bot)
{
    /*
     * product = (U64x2)a * (U64x2)d;
     * product += (U64x2)b * (U64x2)c;
     * product <<= 32;
     * product += (U64x2)a * (U64x2)c;
     */
    uint64x2_t product = vmull_u32(top.val[0], bot.val[1]);
    acc = vmlal_u32(acc, top.val[0], bot.val[0]);
    product = vmlal_u32(product, top.val[1], bot.val[0]);
    product = vshlq_n_u64(product, 32);
    acc = vaddq_u64(acc, product);
    return acc;
}
/* Expects botRev to be a value preswapped with a vrev64q_u32, and botLo is the low 32 bits. */
FORCE_INLINE uint64x2_t XXH_U64x2_twomul(const uint64x2_t top,
                                         const uint32x4_t botRev, const uint32x2_t botLo)
{
    /*  U32x2 product[2] = {
     *      topLo * botHi,
     *      topHi * botLo
     *  };
     *  U64x2 ret = (U64x2)product[0] + (U64x2)product[1];
     *  ret <<= 32;
     *  ret += (U64x2)topLo * (U64x2)botLo;
     */
    uint32x2_t topLo = vmovn_u64(top);
    uint32x4_t product = vmulq_u32(vreinterpretq_u32_u64(top), botRev);
    uint64x2_t ret = vpaddlq_u32(product);
    ret = vshlq_n_u64(ret, 32);
    ret = vmlal_u32(ret, topLo, botLo);
    return ret;
}

FORCE_INLINE const BYTE* XXH64_NEON32(const BYTE* p, const BYTE* bEnd,
                                      const U64 seed, U64 *h64)
{
    const U64 PRIME1[2] = { PRIME64_1, PRIME64_1 };
    const U64 PRIME2[2] = { PRIME64_2, PRIME64_2 };

    const U64 STATE1[2] = { PRIME64_1 + PRIME64_2, PRIME64_2 };
    const U64 STATE2[2] = { 0, -PRIME64_1 };

    /* Interleave our constants in two ways. */
    const uint64x2_t prime1_base = vld1q_u64(PRIME1);
    /* prime1_base & 0xFFFFFFFF; */
    const uint32x2_t prime1_low = vmovn_u64(prime1_base);
    /* (prime1_base << 32) | (prime1_base >> 32); */
    const uint32x4_t prime1_swapped = vrev64q_u32(vreinterpretq_u32_u64(prime1_base));

    /* { PRIME64_1 & 0xFFFFFFFF, PRIME64_1 & 0xFFFFFFFF, PRIME64_1 >> 32, PRIME64_1 >> 32 } */
    const uint32x2x2_t prime2 = vld2_u32((const U32*)PRIME2);

    const uint64x2_t seed_vec = vdupq_n_u64(seed);

    uint64x2_t v[2];

    v[0] = vld1q_u64(STATE1);
    v[1] = vld1q_u64(STATE2);
    v[0] = vaddq_u64(v[0], seed_vec);
    v[1] = vaddq_u64(v[1], seed_vec);

    do {
        uint32x2x2_t val = vld2_u32((const U32*)p);

        v[0] = XXH_U64x2_ssemul_add(v[0], val, prime2);
        v[0] = XXH_vec_rotl64(v[0], 31); /* rotl */
        v[0] = XXH_U64x2_twomul(v[0], prime1_swapped, prime1_low);
        p += 16;

        val = vld2_u32((const U32*)p);
        v[1] = XXH_U64x2_ssemul_add(v[1], val, prime2);
        v[1] = XXH_vec_rotl64(v[1], 31); /* rotl */
        v[1] = XXH_U64x2_twomul(v[1], prime1_swapped, prime1_low);
        p += 16;
    } while (p < bEnd);
    {
        /* We need a copy for the rotl. */
        uint64x2_t v0_cpy = v[0];
        uint64x2_t v1_cpy = v[1];

        const uint64x2_t prime2_base = vld1q_u64((const U64*)PRIME2);
        const uint32x4_t prime2_swapped = vrev64q_u32(vreinterpretq_u32_u64(prime2_base));

        /* We perform rounds in XXH64_mergeLane. It is faster to do this now.
         * If we do this later, we might have to reload the primes. */
        v[0] = XXH_U64x2_twomul(v[0], prime2_swapped, prime2.val[0]);
        v[0] = XXH_vec_rotl64(v[0], 31);
        v[0] = XXH_U64x2_twomul(v[0], prime1_swapped, prime1_low);

        v[1] = XXH_U64x2_twomul(v[1], prime2_swapped, prime2.val[0]);
        v[1] = XXH_vec_rotl64(v[1], 31);
        v[1] = XXH_U64x2_twomul(v[1], prime1_swapped, prime1_low);

        /* rotl(v1, 1) + rotl(v2, 7) + rotl(v3, 12) + rotl(v4, 18) */
        {
            /* NEON uses negative shifts for right shifts when not shifting by
             * an immediate value. */
            const int64_t rotlVals[4][2] = {
                /* left[0] */ { 1, 7 },
                /* left[1] */ { 12, 18 },
                /* right[0] */ { -(64 - 1), -(64 - 7) },
                /* right[1] */ { -(64 - 12), -(64 - 18) }
            };
            int64x2_t left0 = vld1q_s64(rotlVals[0]);
            int64x2_t left1 = vld1q_s64(rotlVals[1]);
            int64x2_t right0 = vld1q_s64(rotlVals[2]);
            int64x2_t right1 = vld1q_s64(rotlVals[3]);
            uint64x1_t merged; /* for later */
            v0_cpy = vorrq_u64(vshlq_u64(v0_cpy, left0), vshlq_u64(v0_cpy, right0));
            v1_cpy = vorrq_u64(vshlq_u64(v1_cpy, left1), vshlq_u64(v1_cpy, right1));

            /* Do some cheap addition while we are here. */
            v0_cpy = vaddq_u64(v0_cpy, v1_cpy);
            merged = vadd_u64(vget_low_u64(v0_cpy), vget_high_u64(v0_cpy));

            *h64 = merged[0];
        }
        *h64 ^= v[0][0];
        *h64 = *h64 * PRIME64_1 + PRIME64_4;
        *h64 ^= v[0][1];
        *h64 = *h64 * PRIME64_1 + PRIME64_4;
        *h64 ^= v[1][0];
        *h64 = *h64 * PRIME64_1 + PRIME64_4;
        *h64 ^= v[1][1];
        *h64 = *h64 * PRIME64_1 + PRIME64_4;
    }
    return p;
}
 /* Like XXH_vec_rotl32, but takes a vector as r. No NEON-optimized
  * version for this one. */
FORCE_INLINE U32x4 XXH_rotlvec_vec32(U32x4 x, const U32x4 r)
{
    const U32x4 v32 = { 32, 32, 32, 32 };
    return (x << r) | (x >> (v32 - r));
}

/* This catches MSVC++ if supplied /TP, and hopefully ICC.
 * g++ benefits here on 32-bit because it does not vectorize XXH64 properly. */
#elif defined(__cplusplus) && !defined(__clang__) \
  && (defined(__SSE4_1__) || defined(__AVX__))
#undef XXH_VECTORIZE
#define XXH_VECTORIZE 1
#include <immintrin.h>
#include <stdio.h>
#include <initializer_list>
/* A very simple wrapper around __m128i */
struct U32x4 {
private:
    __m128i value;
public:
    inline U32x4() {}
    inline U32x4(U32 v1, U32 v2, U32 v3, U32 v4)
        : value(_mm_set_epi32(v4, v3, v2, v1))
    {
    }
    inline U32x4(__m128i v) : value(v) {}
    explicit inline U32x4(const __m128i* pointer)
        : value(_mm_loadu_si128(pointer))
    {
    }
    inline U32x4(const U32 v)
        : value(_mm_set1_epi32(v))
    {
    }
    inline operator __m128i() const
    {
        return value;
    }
    inline U32 operator[](size_t pos)
    {
        switch (pos & 3) {
        case 0: return _mm_extract_epi32(value, 0);
        case 1: return _mm_extract_epi32(value, 1);
        case 2: return _mm_extract_epi32(value, 2);
        default: return _mm_extract_epi32(value, 3);
        }
    }
    /* Defining the operators is slow and tedious. */
#define OP(intrinsic, op1, op2, type) \
    inline U32x4& operator op1(type rhs) \
    { \
        this->value = intrinsic(this->value, rhs); \
        return *this; \
    } \
    inline friend U32x4 operator op2(U32x4 lhs, type rhs) \
    { \
        U32x4 tmp = intrinsic(lhs.value, rhs); \
        return tmp; \
    }

    OP(_mm_add_epi32, +=, +, const U32x4)
    OP(_mm_mullo_epi32, *=, *, const U32x4)
    OP(_mm_or_si128, |=, | , const U32x4)

    OP(_mm_slli_epi32, <<=, << , int)
    OP(_mm_srli_epi32, >>=, >> , int)
#undef OP
    inline void store(U32 *out) const
    {
        _mm_storeu_si128(reinterpret_cast<__m128i*>(out), value);
    }
    inline void store_aligned(__m128i *out) const
    {
        _mm_store_si128(reinterpret_cast<__m128i*>(XXH_assume_aligned(out, 16)), value);
    }
};
#ifndef XXH_NO_LONG_LONG
struct U64x2 {
private:
    __m128i value;

public:
    inline U64x2() {}
    inline U64x2(U64 v1, U64 v2)
        : value(_mm_set_epi64x(v2, v1))
    {
    }
    inline U64x2(__m128i v) : value(v) {}
    explicit inline U64x2(const __m128i* pointer)
        : value(_mm_loadu_si128(pointer))
    {
    }
    static inline U64x2 LoadAligned(const __m128i* pointer)
    {
        U64x2 ret(_mm_load_si128(pointer));
        return ret;
    }
    inline U64x2(const U64 v)
        : value(_mm_set1_epi64x(v))
    {
    }
    inline operator __m128i() const
    {
        return value;
    }

    /* Defining the operators is slow and tedious. */
#define OP(intrinsic, op1, op2, type) \
    inline U64x2& operator op1(type rhs) \
    { \
        value = intrinsic(value, rhs); \
        return *this; \
    } \
    inline friend U64x2 operator op2(U64x2 lhs, type rhs) \
    { \
        U64x2 tmp = intrinsic(lhs.value, rhs); \
        return tmp; \
    }

    OP(_mm_add_epi64, +=, +, const U64x2)
    OP(_mm_or_si128, |=, | , const U64x2)

    OP(_mm_slli_epi64, <<=, << , int)
    OP(_mm_srli_epi64, >>=, >> , int)

#undef OP
    /* *this *= val & 0xFFFFFFFF */
    inline U64x2& MultLow(const U64x2 val)
    {
        value = _mm_mul_epu32(value, val);
        return *this;
    }
    inline U64x2 MultLow(U64x2 lhs, const U64x2 rhs)
    {
        lhs.MultLow(rhs);
        return lhs;
    }
    /* Multiplication is more complex. There isn't a simple U64x2 * U64x2 instruction,
     * we have to do this manually.
     * Thankfully, this is doable with the pmuludq instruction (MultLow), which multiplies
     * a 64-bit vector by a 32-bit value.
     *
     * This is based on Clang's output.
     *
     * Unfortunately, there is no such thing for NEON, and this is slower than 64-bit math
     * on x86_64. */
    inline U64x2& operator *=(const U64 rhs)
    {
        const U64 dup[2] = { rhs, rhs };
        /* we could do (U64[2]) { rhs >> 32, rhs >> 32 }, but it slows down. */
        const U32 high[4] = { static_cast<const U32>(rhs >> 32), 0, static_cast<const U32>(rhs >> 32), 0 };

        U64x2 xmm1(reinterpret_cast<const __m128i*>(dup));
        U64x2 xmm2 = value;
        U64x2 xmm3 = value;
        U64x2 xmm0 = MultLow(*this, U64x2(reinterpret_cast<const __m128i*>(high)));
        xmm3 >>= 32;
        xmm3.MultLow(xmm1);
        xmm2.MultLow(xmm1);
        xmm0 += xmm3;
        xmm0 <<= 32;
        value = xmm0 + xmm2;
        return *this;
    }
    inline friend U64x2 operator *(U64x2 lhs, const U64 rhs)
    {
        lhs *= rhs;
        return lhs;
    }

    inline void store(U64 *out) const
    {
        _mm_storeu_si128(reinterpret_cast<__m128i*>(out), value);
    }

    inline void store_aligned(__m128i *out) const
    {
        _mm_store_si128(reinterpret_cast<__m128i*>(XXH_assume_aligned(out, 16)), value);
    }
};
#endif /* !XXH_NO_LONG_LONG */
FORCE_INLINE U32x4 XXH_vec_rotl32(U32x4 lhs, int bits)
{
    return (lhs << bits) | (lhs >> (32 - bits));
}

FORCE_INLINE U32x4 XXH_vec_load_unaligned(const BYTE* data)
{
    U32x4 val(_mm_loadu_si128(reinterpret_cast<const __m128i*>(data)));
    return val;
}

FORCE_INLINE U32x4 XXH_vec_load_unaligned(const U32* data)
{
    return U32x4(_mm_loadu_si128(reinterpret_cast<const __m128i*>(data)));
}

FORCE_INLINE void XXH_vec_store_unaligned(U32* store, const U32x4 data)
{
    data.store(store);
}

FORCE_INLINE U32x4 XXH_vec_load_aligned(const U32x4* data)
{
    return U32x4(_mm_load_si128(reinterpret_cast<const __m128i*>(XXH_assume_aligned(data, 16))));
}

FORCE_INLINE void XXH_vec_store_aligned(U32x4* store, const U32x4 data)
{
    data.store_aligned(reinterpret_cast<__m128i*>(store));
}

#elif (XXH_GCC_VERSION >= 407 || defined(__clang__)) \
    && (defined(__SSE4_1__) || defined(__AVX__))
#undef XXH_VECTORIZE
#define XXH_VECTORIZE 1
/* not NEON */
/* __m128i (SSE) or uint32x4_t (NEON). */
typedef U32 U32x4 __attribute__((__vector_size__(16)));

#ifndef XXH_NO_LONG_LONG
/* Two U64s. */
typedef U64 U64x2 __attribute__((__vector_size__(16)));
#endif

/* Clang < 5.0 doesn't support int -> vector conversions.
 * Yuck. */
FORCE_INLINE U32x4 XXH_vec_rotl32(U32x4 x, U32 r)
{
    const U32x4 left = { r, r, r, r };
    const U32x4 right = {
        32 - r,
        32 - r,
        32 - r,
        32 - r
    };
    return (x << left) | (x >> right);
}
#if defined(__SSE4_1__) || defined(__AVX__)
#include <emmintrin.h>
#define XXH_vec_load_unaligned(p) XXH_DISABLE_W_CAST_ALIGN((U32x4)_mm_loadu_si128((const __m128i*)(p)))
#define XXH_vec_store_unaligned(p,v) XXH_DISABLE_W_CAST_ALIGN(_mm_storeu_si128((__m128i*)(p), (__m128i)(v)))

#define XXH_vec_load_aligned(p) XXH_DISABLE_W_CAST_ALIGN ( (U32x4)_mm_load_si128((const __m128i*)(p)) )
#define XXH_vec_store_aligned(p,v) XXH_DISABLE_W_CAST_ALIGN ( _mm_store_si128((__m128i*)(p), (__m128i)(v)) )
#else
/* emmintrin.h's _mm_loadu_si128 code. */
FORCE_INLINE U32x4 XXH_vec_load_unaligned(const void* p)
{
    struct loader {
        U32x4 v;
    } __attribute__((__packed__, __may_alias__));
    return ((const struct loader*)p)->v;
}

/* _mm_storeu_si128 */
FORCE_INLINE void XXH_vec_store_unaligned(void* p, const U32x4 v)
{
    struct loader {
        U32x4 v;
    } __attribute__((__packed__, __may_alias__));
    ((struct loader*)p)->v = v;
}

#define XXH_vec_load_aligned(p) *(U32x4*)(p)
#define XXH_vec_store_aligned(p, v) (*(U32x4*)(p) = (v))
#endif

#elif !defined(XXH_VECTORIZE)
#define XXH_VECTORIZE 0
#if defined(__SSE2__) && (defined(__i386__) || defined(_M_X86))
#define XXH_VECTORIZE_XXH64 1
#endif
#endif /* C++/SSE4.1 */

/* 32-bit SSE2 always gets this. */
#if defined(__SSE2__) && (defined(XXH_VECTORIZE_XXH64) || defined(__i386__) || defined(_M_IX86))
#include <emmintrin.h>
#define XXH_load_and_swap XXH_load_unaligned
#define XXH64x2_mult_interleaved XXH_U64x2_mult
typedef U64 U64x2 __attribute__((vector_size(16)));

FORCE_INLINE __m128i XXH_U64x2_mult(__m128i top, const __m128i botHi, const __m128i botLo)
{
    __m128i topHiBotLo, returnval, topLoBotLo, topLoBotHi;
    /* Prevent a bug in Clang 7.0 which will cause an unwanted sign bit mask. */
    XXH_FORCE_VECTOR_REG(top);

    topLoBotLo = _mm_mul_epu32(top, botLo);
    topLoBotHi = _mm_mul_epu32(top, botHi);
    top = _mm_srli_epi64(top, 32);
    topHiBotLo = _mm_mul_epu32(top, botLo);
    returnval = _mm_add_epi64(topHiBotLo, topLoBotHi);
    returnval = _mm_slli_epi64(returnval, 32);
    returnval = _mm_add_epi64(returnval, topLoBotLo);
    return returnval;
}

#define XXH_vec_rotl64(val,amt) (_mm_or_si128(_mm_slli_epi64(val, amt), _mm_srli_epi64(val, 64 - amt)))
XXH_ALIGN_16 static const U64 STATE1[2] = { PRIME64_1 + PRIME64_2, PRIME64_2 };
XXH_ALIGN_16 static const U64 STATE2[2] = { 0, -PRIME64_1 };


/* Only intrinsics here! */
FORCE_INLINE const BYTE* XXH64_SSE2(const BYTE* p, const BYTE* const limit,
                                    const U64 seed, U64 *h64)
{
    /* Interleave our constants. */
    const __m128i prime1_low = _mm_set1_epi64x(PRIME64_1);
    const __m128i prime1_high = _mm_set1_epi64x(PRIME64_1 >> 32);
    const __m128i prime2_low = _mm_set1_epi64x(PRIME64_2);
    const __m128i prime2_high = _mm_set1_epi64x(PRIME64_2 >> 32);
    const __m128i seed_vec = _mm_set1_epi64x(seed);
    XXH_ALIGN_16 U64 state[2][2];
    XXH_ALIGN_16 __m128i v[2];

    v[0] = _mm_load_si128((const __m128i*)STATE1);
    v[1] = _mm_load_si128((const __m128i*)STATE2);
    v[0] = _mm_add_epi64(v[0], seed_vec);
    v[1] = _mm_add_epi64(v[1], seed_vec);

    if (XXH_FORCE_ALIGN_CHECK && (((size_t)p & 15) == 0)) {
        do {
            __m128i val = _mm_load_si128((const __m128i*)XXH_assume_aligned(p, 16));
            val = XXH_U64x2_mult(val, prime2_high, prime2_low);
            v[0] = _mm_add_epi64(v[0], val);
            v[0] = XXH_vec_rotl64(v[0], 31); /* rotl */
            v[0] = XXH_U64x2_mult(v[0], prime1_high, prime1_low);
            p += 16;

            val = _mm_load_si128((const __m128i*)XXH_assume_aligned(p, 16));
            val = XXH_U64x2_mult(val, prime2_high, prime2_low);
            v[1] = _mm_add_epi64(v[1], val);
            v[1] = XXH_vec_rotl64(v[1], 31); /* rotl */
            v[1] = XXH_U64x2_mult(v[1], prime1_high, prime1_low);
            p += 16;
        } while (p < limit);
    } else {
        do {
            __m128i val = _mm_loadu_si128((const __m128i*)p);
            val = XXH_U64x2_mult(val, prime2_high, prime2_low);
            v[0] = _mm_add_epi64(v[0], val);
            v[0] = XXH_vec_rotl64(v[0], 31);
            v[0] = XXH_U64x2_mult(v[0], prime1_high, prime1_low);
            p += 16;

            val = _mm_loadu_si128((const __m128i*)p);
            val = XXH_U64x2_mult(val, prime2_high, prime2_low);
            v[1] = _mm_add_epi64(v[1], val);
            v[1] = XXH_vec_rotl64(v[1], 31);
            v[1] = XXH_U64x2_mult(v[1], prime1_high, prime1_low);
            p += 16;
        } while (p < limit);
    }
    _mm_store_si128((__m128i*)state[0], v[0]);
    _mm_store_si128((__m128i*)state[1], v[1]);

    /* SSE2's _mm_sll_epi64 is stupid.
     * In order to have two shift values, you either need to do a lot of shuffling,
     * or you must use AVX2 for _mm_sllv_epi64. */
    *h64 = XXH_rotl64(state[0][0],  1) + XXH_rotl64(state[0][1],  7)
         + XXH_rotl64(state[1][0], 12) + XXH_rotl64(state[1][1], 18);

    /* XXH64_mergeLane */
    /* We perform rounds in XXH64_mergeLane. It is faster to do this now.
     * If we do this later, we might have to reload the primes. */
    v[0] = XXH_U64x2_mult(v[0], prime2_high, prime2_low);
    v[1] = XXH_U64x2_mult(v[1], prime2_high, prime2_low);

    v[0]  = XXH_vec_rotl64(v[0], 31);
    v[1]  = XXH_vec_rotl64(v[1], 31);

    v[0] = XXH_U64x2_mult(v[0], prime1_high, prime1_low);
    v[1] = XXH_U64x2_mult(v[1], prime1_high, prime1_low);

    _mm_store_si128((__m128i*)state[0], v[0]);
    *h64 ^= state[0][0];
    *h64 = *h64 * PRIME64_1 + PRIME64_4;
    *h64 ^= state[0][1];
    *h64 = *h64 * PRIME64_1 + PRIME64_4;

    _mm_store_si128((__m128i*)state[1], v[1]);
    *h64 ^= state[1][0];
    *h64 = *h64 * PRIME64_1 + PRIME64_4;
    *h64 ^= state[1][1];
    *h64 = *h64 * PRIME64_1 + PRIME64_4;
    return p;
}
#endif

#endif /* XXHASH_VEC_H */
