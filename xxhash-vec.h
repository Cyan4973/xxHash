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

#elif (XXH_GCC_VERSION >= 407 || defined(__clang__)) \
	&& (defined(__SSE4_1__) || defined(__AVX__) || defined(_M_X64) || defined(_M_IX86_FP))
#undef XXH_VECTORIZE
#define XXH_VECTORIZE 1
/* not NEON */
/* __m128i (SSE) or uint32x4_t (NEON). */
typedef U32 U32x4 __attribute__((__vector_size__(16)));

/* Two U32x4s. */
typedef struct { U32x4 val[2]; } U32x4x2;

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

/* This catches MSVC++ if supplied /TP, and hopefully ICC. */
#elif defined(__cplusplus) && (defined(__SSE4_1__) || defined(__AVX__) || defined(_M_X64) || defined(_M_IX86_FP))
#undef XXH_VECTORIZE
#define XXH_VECTORIZE 1
#include <smmintrin.h>

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

	inline U32x4(const void* pointer)
		: value(_mm_loadu_si128(reinterpret_cast<const __m128i*>(pointer)))
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

/* Defining the operators is slow and tedious. */
#define OP(intrinsic, op1, op2, type) \
    inline U32x4& operator op1(type rhs) \
    { \
	    value = intrinsic(value, rhs); \
	    return *this; \
    } \
    inline friend U32x4& operator op2(U32x4 lhs, type rhs) \
    { \
	    lhs op1 rhs; \
	    return lhs; \
    }

	OP(_mm_add_epi32, +=, +, const U32x4&)
	OP(_mm_mullo_epi32, *=, *, const U32x4&)
	OP(_mm_or_si128, |=, |, const U32x4&)
	OP(_mm_slli_epi32, <<=, <<, int)
	OP(_mm_srli_epi32, >>=, >>, int)

#undef OP
	inline void store(U32 *out) const
	{
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out), value);
	}
};
FORCE_INLINE U32x4 XXH_vec_rotl32(U32x4& lhs, int bits)
{
	return (lhs << bits) | (lhs >> (32 - bits));
}

FORCE_INLINE U32x4 XXH_vec_load_unaligned(const void* data)
{
	return U32x4(data);
}

FORCE_INLINE void XXH_vec_store_unaligned(U32* store, const U32x4 data)
{
	data.store(store);
}

struct U32x4x2
{
	U32x4 val[2];

	U32x4x2() {}
	U32x4x2(U32x4 v1, U32x4 v2) : val { v1, v2 } {}
};

#endif /* C++/SSE4.1 */

#endif /* XXHASH_VEC_H */