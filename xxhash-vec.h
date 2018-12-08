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

#if defined(__ARM_NEON__) || defined(__ARM_NEON)
#include <arm_neon.h>
#define XXH_NEON
#undef XXH_VECTORIZE
#define XXH_VECTORIZE 1

typedef uint32x4_t U32x4;
typedef uint32x4x2_t U32x4x2;
typedef uint64x2_t U64x2;


/* Neither GCC or Clang can properly optimize the generic version
 * for Arm NEON.
 * Instead of the optimal version, which is this:
 *      vshr.u32        q9, q8, #19
 *      vsli.32         q9, q8, #13
 * GCC and Clang will produce this slower version:
 *      vshr.u32        q9, q8, #19
 *      vshl.i32        q8, q8, #13
 *      vorr            q8, q8, q9
 * This is much faster, and I think a few intrinsics are acceptable. */
#define XXH_vec_rotl32(x, r) vsliq_n_u32(vshrq_n_u32((x), 32 - (r)), (x), (r))
#define XXH_vec_load_unaligned(p) vld1q_u32((const U32*)p)
#define XXH_vec_store_unaligned(p, v) vst1q_u32((U32*)p, v)
#define XXH_vec_load_unaligned(p) vld1q_u32((const U32*)p)
#define XXH_vec_store_unaligned(p, v) vst1q_u32((U32*)p, v)

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
#include <smmintrin.h>
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
};
struct U64x2 {
private:
	__m128i value;
	typedef uint64_t U64;

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
		const U32 high[4] = { rhs >> 32, 0, rhs >> 32, 0 };

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
};
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

struct U32x4x2
{
	U32x4 val[2];

	U32x4x2() {}
	U32x4x2(U32x4 v1, U32x4 v2) : val{ v1, v2 } {}
};
#elif (XXH_GCC_VERSION >= 407 || defined(__clang__)) \
	&& (defined(__SSE4_1__) || defined(__AVX__))
#undef XXH_VECTORIZE
#define XXH_VECTORIZE 1
/* not NEON */
/* __m128i (SSE) or uint32x4_t (NEON). */
typedef U32 U32x4 __attribute__((__vector_size__(16)));

/* Two U32x4s. */
typedef struct { U32x4 val[2]; } U32x4x2;

/* Two U64s. */
typedef U64 U64x2 __attribute__((__vector_size__(16)));

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
#define XXH_vec_store_aligned(p, v) (*(U32x4*)(p) = v)

#elif !defined(XXH_VECTORIZE)
#define XXH_VECTORIZE 0
#endif /* C++/SSE4.1 */

#endif /* XXHASH_VEC_H */