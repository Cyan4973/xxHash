/*
 * xxHash - Extremely Fast Hash algorithm
 * XXH3 and XXH128 implementation
 * Copyright (C) 2012-2023 Yann Collet
 *
 * BSD 2-Clause License (https://www.opensource.org/licenses/bsd-license.php)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You can contact the author at:
 *   - xxHash homepage: https://www.xxhash.com
 *   - xxHash source repository: https://github.com/Cyan4973/xxHash
 *
 * This file is a fragment of xxhash.h, not a standalone header:
 * it is only meaningful at its insertion point in lib/xxhash_root.h.
 * xxhash.h is generated from lib/ by build/amalgamate.sh, see lib/README.md.
 */

#if !defined(XXH_NO_XXH3) && !defined(XXH_NO_XXH32) && !defined(XXH_NO_LONG_LONG)
/* XXH3 depends on XXH32 and XXH64 */

/* *********************************************************************
*  XXH3
*  New generation hash designed for speed on small keys and vectorization
************************************************************************ */
/*!
 * @}
 * @defgroup XXH3_impl XXH3 implementation
 * @ingroup impl
 * @{
 */

/* ===   Compiler specifics   === */

#if XXH_NO_INLINE_HINTS  /* disable inlining hints */
#  define XXH_NO_INLINE static
#elif defined(__GNUC__) || defined(__clang__)
#  define XXH_NO_INLINE static __attribute__((__noinline__))
#elif defined(_MSC_VER)  /* Visual Studio */
#  define XXH_NO_INLINE static __declspec(noinline)
#else
#  define XXH_NO_INLINE static
#endif

/*!
 * @ingroup tuning
 * @def XXH3_INLINE_SECRET
 * @brief Determines whether to inline the XXH3 withSecret code.
 *
 * When the secret size is known, the compiler can improve the performance
 * of XXH3_64bits_withSecret() and XXH3_128bits_withSecret().
 *
 * However, if the secret size is not known, it doesn't have any benefit. This
 * happens when xxHash is compiled into a global symbol. Therefore, if
 * @ref XXH_INLINE_ALL is *not* defined, this will be defined to 0.
 *
 * Additionally, this defaults to 0 on GCC 12+, which has an issue with function pointers
 * that are *sometimes* force inline on -Og, and it is impossible to automatically
 * detect this optimization level.
 */
#ifndef XXH3_INLINE_SECRET
#  if (defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 12) \
     || !defined(XXH_INLINE_ALL)
#    define XXH3_INLINE_SECRET 0
#  else
#    define XXH3_INLINE_SECRET 1
#  endif
#endif

#if XXH3_INLINE_SECRET
#  define XXH3_WITH_SECRET_INLINE XXH_FORCE_INLINE
#else
#  define XXH3_WITH_SECRET_INLINE XXH_NO_INLINE
#endif

/* Specifically for NEON vectors which use the "w" constraint, on
 * Clang. */
#if defined(__clang__) && defined(__ARM_ARCH) && !defined(__wasm__)
#  define XXH_COMPILER_GUARD_CLANG_NEON(var) __asm__("" : "+w" (var))
#else
#  define XXH_COMPILER_GUARD_CLANG_NEON(var) ((void)0)
#endif

#ifndef XXH_memcmp
/*!
 * @internal
 * @brief XXH_memcmp() macro can be redirected at compile time
 * Note: only needed by XXH128.
 */
#  include <string.h>
#  define XXH_memcmp memcmp
#endif


#if (defined(__GNUC__) && (__GNUC__ >= 3))  \
  || (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 800)) \
  || defined(__clang__)
#    define XXH_likely(x) __builtin_expect(x, 1)
#    define XXH_unlikely(x) __builtin_expect(x, 0)
#else
#    define XXH_likely(x) (x)
#    define XXH_unlikely(x) (x)
#endif

#ifndef XXH_HAS_INCLUDE
#  ifdef __has_include
/*
 * Not defined as XXH_HAS_INCLUDE(x) (function-like) because
 * this causes segfaults in Apple Clang 4.2 (on Mac OS X 10.7 Lion)
 */
#    define XXH_HAS_INCLUDE __has_include
#  else
#    define XXH_HAS_INCLUDE(x) 0
#  endif
#endif

#if defined(__GNUC__) || defined(__clang__)
#  if defined(__ARM_FEATURE_SVE)
#    include <arm_sve.h>
#  endif
#  if defined(__ARM_NEON__) || defined(__ARM_NEON) \
   || (defined(_M_ARM) && _M_ARM >= 7) \
   || defined(_M_ARM64) || defined(_M_ARM64EC) \
   || (defined(__wasm_simd128__) && XXH_HAS_INCLUDE(<arm_neon.h>)) /* WASM SIMD128 via SIMDe */
#    define inline __inline__  /* circumvent a clang bug */
#    include <arm_neon.h>
#    undef inline
#  elif defined(__AVX2__)
#    include <immintrin.h>
#  elif defined(__SSE2__)
#    include <emmintrin.h>
#  elif defined(__loongarch_asx)
#    include <lasxintrin.h>
#    include <lsxintrin.h>
#  elif defined(__loongarch_sx)
#    include <lsxintrin.h>
#  elif defined(__riscv_vector)
#    include <riscv_vector.h>
#  endif
#endif

#if defined(_MSC_VER)
#  include <intrin.h>
#endif

/*
 * One goal of XXH3 is to make it fast on both 32-bit and 64-bit, while
 * remaining a true 64-bit/128-bit hash function.
 *
 * This is done by prioritizing a subset of 64-bit operations that can be
 * emulated without too many steps on the average 32-bit machine.
 *
 * For example, these two lines seem similar, and run equally fast on 64-bit:
 *
 *   xxh_u64 x;
 *   x ^= (x >> 47); // good
 *   x ^= (x >> 13); // bad
 *
 * However, to a 32-bit machine, there is a major difference.
 *
 * x ^= (x >> 47) looks like this:
 *
 *   x.lo ^= (x.hi >> (47 - 32));
 *
 * while x ^= (x >> 13) looks like this:
 *
 *   // note: funnel shifts are not usually cheap.
 *   x.lo ^= (x.lo >> 13) | (x.hi << (32 - 13));
 *   x.hi ^= (x.hi >> 13);
 *
 * The first one is significantly faster than the second, simply because the
 * shift is larger than 32. This means:
 *  - All the bits we need are in the upper 32 bits, so we can ignore the lower
 *    32 bits in the shift.
 *  - The shift result will always fit in the lower 32 bits, and therefore,
 *    we can ignore the upper 32 bits in the xor.
 *
 * Thanks to this optimization, XXH3 only requires these features to be efficient:
 *
 *  - Usable unaligned access
 *  - A 32-bit or 64-bit ALU
 *      - If 32-bit, a decent ADC instruction
 *  - A 32 or 64-bit multiply with a 64-bit result
 *  - For the 128-bit variant, a decent byteswap helps short inputs.
 *
 * The first two are already required by XXH32, and almost all 32-bit and 64-bit
 * platforms which can run XXH32 can run XXH3 efficiently.
 *
 * Thumb-1, the classic 16-bit only subset of ARM's instruction set, is one
 * notable exception.
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
 * do a 32->64 multiply with UMULL, and the flexible operand allowing free
 * shifts is helpful, too.
 *
 * Therefore, we do a quick sanity check.
 *
 * If compiling Thumb-1 for a target which supports ARM instructions, we will
 * emit a warning, as it is not a "sane" platform to compile for.
 *
 * Usually, if this happens, it is because of an accident and you probably need
 * to specify -march, as you likely meant to compile for a newer architecture.
 *
 * Credit: large sections of the vectorial and asm source code paths
 *         have been contributed by @easyaspi314
 */
#if defined(__thumb__) && !defined(__thumb2__) && defined(__ARM_ARCH_ISA_ARM)
#   warning "XXH3 is highly inefficient without ARM or Thumb-2."
#endif

/* ==========================================
 * Vectorization detection
 * ========================================== */

#ifdef XXH_DOXYGEN
/*!
 * @ingroup tuning
 * @brief Overrides the vectorization implementation chosen for XXH3.
 *
 * Can be defined to 0 to disable SIMD,
 * or any other authorized value of @ref XXH_VECTOR.
 *
 * If this is not defined, it uses predefined macros to determine the best
 * implementation.
 */
#  define XXH_VECTOR XXH_SCALAR
/*!
 * @ingroup tuning
 * @brief Selects the minimum alignment for XXH3's accumulators.
 *
 * When using SIMD, this should match the alignment required for said vector
 * type, so, for example, 32 for AVX2.
 *
 * Default: Auto detected.
 */
#  define XXH_ACC_ALIGN 8
#endif

/* Actual definition */
#ifndef XXH_DOXYGEN
#endif

#ifndef XXH_VECTOR    /* can be defined on command line */
#  if ( \
        defined(__ARM_NEON__) || defined(__ARM_NEON) /* gcc */ \
     || defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC) /* msvc */ \
     || (defined(__wasm_simd128__) && XXH_HAS_INCLUDE(<arm_neon.h>)) /* wasm simd128 via SIMDe */ \
   ) && ( \
        defined(_WIN32) || defined(__LITTLE_ENDIAN__) /* little endian only */ \
    || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) \
   )
#    define XXH_VECTOR XXH_NEON
#  elif defined(__ARM_FEATURE_SVE)
#    define XXH_VECTOR XXH_SVE
#  elif defined(__AVX512F__)
#    define XXH_VECTOR XXH_AVX512
#  elif defined(__AVX2__)
#    define XXH_VECTOR XXH_AVX2
#  elif defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && (_M_IX86_FP == 2))
#    define XXH_VECTOR XXH_SSE2
#  elif (defined(__PPC64__) && defined(__POWER8_VECTOR__)) \
     || (defined(__s390x__) && defined(__VEC__)) \
     && defined(__GNUC__) /* TODO: IBM XL */
#    define XXH_VECTOR XXH_VSX
#  elif defined(__loongarch_asx)
#    define XXH_VECTOR XXH_LASX
#  elif defined(__loongarch_sx)
#    define XXH_VECTOR XXH_LSX
#  elif defined(__riscv_vector)
#    define XXH_VECTOR XXH_RVV
#  else
#    define XXH_VECTOR XXH_SCALAR
#  endif
#endif

/* __ARM_FEATURE_SVE is only supported by GCC & Clang. */
#if (XXH_VECTOR == XXH_SVE) && !defined(__ARM_FEATURE_SVE)
#  ifdef _MSC_VER
#    pragma warning(once : 4606)
#  else
#    warning "__ARM_FEATURE_SVE isn't supported. Use SCALAR instead."
#  endif
#  undef XXH_VECTOR
#  define XXH_VECTOR XXH_SCALAR
#endif

/*
 * Controls the alignment of the accumulator,
 * for compatibility with aligned vector loads, which are usually faster.
 */
#ifndef XXH_ACC_ALIGN
#  if defined(XXH_X86DISPATCH)
#     define XXH_ACC_ALIGN 64  /* for compatibility with avx512 */
#  elif XXH_VECTOR == XXH_SCALAR  /* scalar */
#     define XXH_ACC_ALIGN 8
#  elif XXH_VECTOR == XXH_SSE2  /* sse2 */
#     define XXH_ACC_ALIGN 16
#  elif XXH_VECTOR == XXH_AVX2  /* avx2 */
#     define XXH_ACC_ALIGN 32
#  elif XXH_VECTOR == XXH_NEON  /* neon */
#     define XXH_ACC_ALIGN 16
#  elif XXH_VECTOR == XXH_VSX   /* vsx */
#     define XXH_ACC_ALIGN 16
#  elif XXH_VECTOR == XXH_AVX512  /* avx512 */
#     define XXH_ACC_ALIGN 64
#  elif XXH_VECTOR == XXH_SVE   /* sve */
#     define XXH_ACC_ALIGN 64
#  elif XXH_VECTOR == XXH_LASX   /* lasx */
#     define XXH_ACC_ALIGN 64
#  elif XXH_VECTOR == XXH_LSX   /* lsx */
#     define XXH_ACC_ALIGN 64
#  elif XXH_VECTOR == XXH_RVV   /* rvv */
#     define XXH_ACC_ALIGN 64   /* could be 8, but 64 may be faster */
#  endif
#endif

#if defined(XXH_X86DISPATCH) || XXH_VECTOR == XXH_SSE2 \
    || XXH_VECTOR == XXH_AVX2 || XXH_VECTOR == XXH_AVX512
#  define XXH_SEC_ALIGN XXH_ACC_ALIGN
#elif XXH_VECTOR == XXH_SVE
#  define XXH_SEC_ALIGN XXH_ACC_ALIGN
#elif XXH_VECTOR == XXH_RVV
#  define XXH_SEC_ALIGN XXH_ACC_ALIGN
#else
#  define XXH_SEC_ALIGN 8
#endif

#if defined(__GNUC__) || defined(__clang__)
#  define XXH_ALIASING __attribute__((__may_alias__))
#else
#  define XXH_ALIASING /* nothing */
#endif

/*
 * UGLY HACK:
 * GCC usually generates the best code with -O3 for xxHash.
 *
 * However, when targeting AVX2, it is overzealous in its unrolling resulting
 * in code roughly 3/4 the speed of Clang.
 *
 * There are other issues, such as GCC splitting _mm256_loadu_si256 into
 * _mm_loadu_si128 + _mm256_inserti128_si256. This is an optimization which
 * only applies to Sandy and Ivy Bridge... which don't even support AVX2.
 *
 * That is why when compiling the AVX2 version, it is recommended to use either
 *   -O2 -mavx2 -march=haswell
 * or
 *   -O2 -mavx2 -mno-avx256-split-unaligned-load
 * for decent performance, or to use Clang instead.
 *
 * Fortunately, we can control the first one with a pragma that forces GCC into
 * -O2, but the other one we can't control without "failed to inline always
 * inline function due to target mismatch" warnings.
 */
#if XXH_VECTOR == XXH_AVX2 /* AVX2 */ \
  && defined(__GNUC__) && !defined(__clang__) /* GCC, not Clang */ \
  && defined(__OPTIMIZE__) && XXH_SIZE_OPT <= 0 /* respect -O0 and -Os */
#  pragma GCC push_options
#  pragma GCC optimize("-O2")
#endif

/* prefetch
 * can be disabled, by declaring XXH_NO_PREFETCH build macro */
#if defined(XXH_NO_PREFETCH)
#  define XXH_PREFETCH(ptr)  (void)(ptr)  /* disabled */
#else
#  if XXH_SIZE_OPT >= 1
#    define XXH_PREFETCH(ptr) (void)(ptr)
#  elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))  /* _mm_prefetch() not defined outside of x86/x64 */
#    include <mmintrin.h>   /* https://msdn.microsoft.com/fr-fr/library/84szxsww(v=vs.90).aspx */
#    define XXH_PREFETCH(ptr)  _mm_prefetch((const char*)(ptr), _MM_HINT_T0)
#  elif defined(__GNUC__) && ( (__GNUC__ >= 4) || ( (__GNUC__ == 3) && (__GNUC_MINOR__ >= 1) ) )
#    define XXH_PREFETCH(ptr)  __builtin_prefetch((ptr), 0 /* rw==read */, 3 /* locality */)
#  else
#    define XXH_PREFETCH(ptr) (void)(ptr)  /* disabled */
#  endif
#endif  /* XXH_NO_PREFETCH */


/* ==========================================
 * XXH3 default settings
 * ========================================== */

#define XXH_SECRET_DEFAULT_SIZE 192   /* minimum XXH3_SECRET_SIZE_MIN */

#if (XXH_SECRET_DEFAULT_SIZE < XXH3_SECRET_SIZE_MIN)
#  error "default keyset is not large enough"
#endif

/*!
 * @internal
 * @def XXH3_kSecret
 * @brief Pseudorandom secret taken directly from FARSH. */
XXH_ALIGN(64) static const xxh_u8 XXH3_kSecret[XXH_SECRET_DEFAULT_SIZE] = {
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

static const xxh_u64 PRIME_MX1 = 0x165667919E3779F9ULL;  /*!< 0b0001011001010110011001111001000110011110001101110111100111111001 */
static const xxh_u64 PRIME_MX2 = 0x9FB21C651E98DF25ULL;  /*!< 0b1001111110110010000111000110010100011110100110001101111100100101 */

#ifdef XXH_OLD_NAMES
#  define kSecret XXH3_kSecret
#endif

#ifdef XXH_DOXYGEN
/*!
 * @brief Calculates a 32-bit to 64-bit long multiply.
 *
 * Implemented as a macro.
 *
 * Wraps `__emulu` on MSVC x86 because it tends to call `__allmul` when it doesn't
 * need to (but it shouldn't need to anyways, it is about 7 instructions to do
 * a 64x64 multiply...). Since we know that this will _always_ emit `MULL`, we
 * use that instead of the normal method.
 *
 * If you are compiling for platforms like Thumb-1 and don't have a better option,
 * you may also want to write your own long multiply routine here.
 *
 * @param x, y Numbers to be multiplied
 * @return 64-bit product of the low 32 bits of @p x and @p y.
 */
XXH_FORCE_INLINE xxh_u64
XXH_mult32to64(xxh_u64 x, xxh_u64 y)
{
   return (x & 0xFFFFFFFF) * (y & 0xFFFFFFFF);
}
#elif defined(_MSC_VER) && defined(_M_IX86)
#    define XXH_mult32to64(x, y) __emulu((unsigned)(x), (unsigned)(y))
#else
/*
 * Downcast + upcast is usually better than masking on older compilers like
 * GCC 4.2 (especially 32-bit ones), all without affecting newer compilers.
 *
 * The other method, (x & 0xFFFFFFFF) * (y & 0xFFFFFFFF), will AND both operands
 * and perform a full 64x64 multiply -- entirely redundant on 32-bit.
 */
#    define XXH_mult32to64(x, y) ((xxh_u64)(xxh_u32)(x) * (xxh_u64)(xxh_u32)(y))
#endif

/*!
 * @brief Calculates a 64->128-bit long multiply.
 *
 * Uses `__uint128_t` and `_umul128` if available, otherwise uses a scalar
 * version.
 *
 * @param lhs , rhs The 64-bit integers to be multiplied
 * @return The 128-bit result represented in an @ref XXH128_hash_t.
 */
static XXH128_hash_t
XXH_mult64to128(xxh_u64 lhs, xxh_u64 rhs)
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
     * Despite being a 32-bit platform, Clang (and emscripten) define this type
     * despite not having the arithmetic for it. This results in a laggy
     * compiler builtin call which calculates a full 128-bit multiply.
     * In that case it is best to use the portable one.
     * https://github.com/Cyan4973/xxHash/issues/211#issuecomment-515575677
     */
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__wasm__) \
    && defined(__SIZEOF_INT128__) \
    || (defined(_INTEGRAL_MAX_BITS) && _INTEGRAL_MAX_BITS >= 128)

    __uint128_t const product = (__uint128_t)lhs * (__uint128_t)rhs;
    XXH128_hash_t r128;
    r128.low64  = (xxh_u64)(product);
    r128.high64 = (xxh_u64)(product >> 64);
    return r128;

    /*
     * MSVC for x64's _umul128 method.
     *
     * xxh_u64 _umul128(xxh_u64 Multiplier, xxh_u64 Multiplicand, xxh_u64 *HighProduct);
     *
     * This compiles to single operand MUL on x64.
     */
#elif (defined(_M_X64) || defined(_M_IA64)) && !defined(_M_ARM64EC)

#ifndef _MSC_VER
#   pragma intrinsic(_umul128)
#endif
    xxh_u64 product_high;
    xxh_u64 const product_low = _umul128(lhs, rhs, &product_high);
    XXH128_hash_t r128;
    r128.low64  = product_low;
    r128.high64 = product_high;
    return r128;

    /*
     * MSVC for ARM64's __umulh method.
     *
     * This compiles to the same MUL + UMULH as GCC/Clang's __uint128_t method.
     */
#elif defined(_M_ARM64) || defined(_M_ARM64EC)

#ifndef _MSC_VER
#   pragma intrinsic(__umulh)
#endif
    XXH128_hash_t r128;
    r128.low64  = lhs * rhs;
    r128.high64 = __umulh(lhs, rhs);
    return r128;

#else
    /*
     * Portable scalar method. Optimized for 32-bit and 64-bit ALUs.
     *
     * This is a fast and simple grade school multiply, which is shown below
     * with base 10 arithmetic instead of base 0x100000000.
     *
     *           9 3 // D2 lhs = 93
     *         x 7 5 // D2 rhs = 75
     *     ----------
     *           1 5 // D2 lo_lo = (93 % 10) * (75 % 10) = 15
     *         4 5 | // D2 hi_lo = (93 / 10) * (75 % 10) = 45
     *         2 1 | // D2 lo_hi = (93 % 10) * (75 / 10) = 21
     *     + 6 3 | | // D2 hi_hi = (93 / 10) * (75 / 10) = 63
     *     ---------
     *         2 7 | // D2 cross = (15 / 10) + (45 % 10) + 21 = 27
     *     + 6 7 | | // D2 upper = (27 / 10) + (45 / 10) + 63 = 67
     *     ---------
     *       6 9 7 5 // D4 res = (27 * 10) + (15 % 10) + (67 * 100) = 6975
     *
     * The reasons for adding the products like this are:
     *  1. It avoids manual carry tracking. Just like how
     *     (9 * 9) + 9 + 9 = 99, the same applies with this for UINT64_MAX.
     *     This avoids a lot of complexity.
     *
     *  2. It hints for, and on Clang, compiles to, the powerful UMAAL
     *     instruction available in ARM's Digital Signal Processing extension
     *     in 32-bit ARMv6 and later, which is shown below:
     *
     *         void UMAAL(xxh_u32 *RdLo, xxh_u32 *RdHi, xxh_u32 Rn, xxh_u32 Rm)
     *         {
     *             xxh_u64 product = (xxh_u64)*RdLo * (xxh_u64)*RdHi + Rn + Rm;
     *             *RdLo = (xxh_u32)(product & 0xFFFFFFFF);
     *             *RdHi = (xxh_u32)(product >> 32);
     *         }
     *
     *     This instruction was designed for efficient long multiplication, and
     *     allows this to be calculated in only 4 instructions at speeds
     *     comparable to some 64-bit ALUs.
     *
     *  3. It isn't terrible on other platforms. Usually this will be a couple
     *     of 32-bit ADD/ADCs.
     */

    /* First calculate all of the cross products. */
    xxh_u64 const lo_lo = XXH_mult32to64(lhs & 0xFFFFFFFF, rhs & 0xFFFFFFFF);
    xxh_u64 const hi_lo = XXH_mult32to64(lhs >> 32,        rhs & 0xFFFFFFFF);
    xxh_u64 const lo_hi = XXH_mult32to64(lhs & 0xFFFFFFFF, rhs >> 32);
    xxh_u64 const hi_hi = XXH_mult32to64(lhs >> 32,        rhs >> 32);

    /* Now add the products together. These will never overflow. */
    xxh_u64 const cross = (lo_lo >> 32) + (hi_lo & 0xFFFFFFFF) + lo_hi;
    xxh_u64 const upper = (hi_lo >> 32) + (cross >> 32)        + hi_hi;
    xxh_u64 const lower = (cross << 32) | (lo_lo & 0xFFFFFFFF);

    XXH128_hash_t r128;
    r128.low64  = lower;
    r128.high64 = upper;
    return r128;
#endif
}

/*!
 * @brief Calculates a 64-bit to 128-bit multiply, then XOR folds it.
 *
 * The reason for the separate function is to prevent passing too many structs
 * around by value. This will hopefully inline the multiply, but we don't force it.
 *
 * @param lhs , rhs The 64-bit integers to multiply
 * @return The low 64 bits of the product XOR'd by the high 64 bits.
 * @see XXH_mult64to128()
 */
static xxh_u64
XXH3_mul128_fold64(xxh_u64 lhs, xxh_u64 rhs)
{
    XXH128_hash_t product = XXH_mult64to128(lhs, rhs);
    return product.low64 ^ product.high64;
}

/*! Seems to produce slightly better code on GCC for some reason. */
XXH_FORCE_INLINE XXH_CONSTF xxh_u64 XXH_xorshift64(xxh_u64 v64, int shift)
{
    XXH_ASSERT(0 <= shift && shift < 64);
    return v64 ^ (v64 >> shift);
}

/*
 * This is a fast avalanche stage,
 * suitable when input bits are already partially mixed
 */
static XXH64_hash_t XXH3_avalanche(xxh_u64 h64)
{
    h64 = XXH_xorshift64(h64, 37);
    h64 *= PRIME_MX1;
    h64 = XXH_xorshift64(h64, 32);
    return h64;
}

/*
 * This is a stronger avalanche,
 * inspired by Pelle Evensen's rrmxmx
 * preferable when input has not been previously mixed
 */
static XXH64_hash_t XXH3_rrmxmx(xxh_u64 h64, xxh_u64 len)
{
    /* this mix is inspired by Pelle Evensen's rrmxmx */
    h64 ^= XXH_rotl64(h64, 49) ^ XXH_rotl64(h64, 24);
    h64 *= PRIME_MX2;
    h64 ^= (h64 >> 35) + len ;
    h64 *= PRIME_MX2;
    return XXH_xorshift64(h64, 28);
}


/* ==========================================
 * Short keys
 * ==========================================
 * One of the shortcomings of XXH32 and XXH64 was that their performance was
 * sub-optimal on short lengths. It used an iterative algorithm which strongly
 * favored lengths that were a multiple of 4 or 8.
 *
 * Instead of iterating over individual inputs, we use a set of single shot
 * functions which piece together a range of lengths and operate in constant time.
 *
 * Additionally, the number of multiplies has been significantly reduced. This
 * reduces latency, especially when emulating 64-bit multiplies on 32-bit.
 *
 * Depending on the platform, this may or may not be faster than XXH32, but it
 * is almost guaranteed to be faster than XXH64.
 */

/*
 * At very short lengths, there isn't enough input to fully hide secrets, or use
 * the entire secret.
 *
 * There is also only a limited amount of mixing we can do before significantly
 * impacting performance.
 *
 * Therefore, we use different sections of the secret and always mix two secret
 * samples with an XOR. This should have no effect on performance on the
 * seedless or withSeed variants because everything _should_ be constant folded
 * by modern compilers.
 *
 * The XOR mixing hides individual parts of the secret and increases entropy.
 *
 * This adds an extra layer of strength for custom secrets.
 */
XXH_FORCE_INLINE XXH_PUREF XXH64_hash_t
XXH3_len_1to3_64b(const xxh_u8* input, size_t len, const xxh_u8* secret, XXH64_hash_t seed)
{
    XXH_ASSERT(input != NULL);
    XXH_ASSERT(1 <= len && len <= 3);
    XXH_ASSERT(secret != NULL);
    /*
     * len = 1: combined = { input[0], 0x01, input[0], input[0] }
     * len = 2: combined = { input[1], 0x02, input[0], input[1] }
     * len = 3: combined = { input[2], 0x03, input[0], input[1] }
     */
    {   xxh_u8  const c1 = input[0];
        xxh_u8  const c2 = input[len >> 1];
        xxh_u8  const c3 = input[len - 1];
        xxh_u32 const combined = ((xxh_u32)c1 << 16) | ((xxh_u32)c2  << 24)
                               | ((xxh_u32)c3 <<  0) | ((xxh_u32)len << 8);
        xxh_u64 const bitflip = (XXH_readLE32(secret) ^ XXH_readLE32(secret+4)) + seed;
        xxh_u64 const keyed = (xxh_u64)combined ^ bitflip;
        return XXH64_avalanche(keyed);
    }
}

XXH_FORCE_INLINE XXH_PUREF XXH64_hash_t
XXH3_len_4to8_64b(const xxh_u8* input, size_t len, const xxh_u8* secret, XXH64_hash_t seed)
{
    XXH_ASSERT(input != NULL);
    XXH_ASSERT(secret != NULL);
    XXH_ASSERT(4 <= len && len <= 8);
    seed ^= (xxh_u64)XXH_swap32((xxh_u32)seed) << 32;
    {   xxh_u32 const input1 = XXH_readLE32(input);
        xxh_u32 const input2 = XXH_readLE32(input + len - 4);
        xxh_u64 const bitflip = (XXH_readLE64(secret+8) ^ XXH_readLE64(secret+16)) - seed;
        xxh_u64 const input64 = input2 + (((xxh_u64)input1) << 32);
        xxh_u64 const keyed = input64 ^ bitflip;
        return XXH3_rrmxmx(keyed, len);
    }
}

XXH_FORCE_INLINE XXH_PUREF XXH64_hash_t
XXH3_len_9to16_64b(const xxh_u8* input, size_t len, const xxh_u8* secret, XXH64_hash_t seed)
{
    XXH_ASSERT(input != NULL);
    XXH_ASSERT(secret != NULL);
    XXH_ASSERT(9 <= len && len <= 16);
    {   xxh_u64 const bitflip1 = (XXH_readLE64(secret+24) ^ XXH_readLE64(secret+32)) + seed;
        xxh_u64 const bitflip2 = (XXH_readLE64(secret+40) ^ XXH_readLE64(secret+48)) - seed;
        xxh_u64 const input_lo = XXH_readLE64(input)           ^ bitflip1;
        xxh_u64 const input_hi = XXH_readLE64(input + len - 8) ^ bitflip2;
        xxh_u64 const acc = len
                          + XXH_swap64(input_lo) + input_hi
                          + XXH3_mul128_fold64(input_lo, input_hi);
        return XXH3_avalanche(acc);
    }
}

XXH_FORCE_INLINE XXH_PUREF XXH64_hash_t
XXH3_len_0to16_64b(const xxh_u8* input, size_t len, const xxh_u8* secret, XXH64_hash_t seed)
{
    XXH_ASSERT(len <= 16);
    {   if (XXH_likely(len >  8)) return XXH3_len_9to16_64b(input, len, secret, seed);
        if (XXH_likely(len >= 4)) return XXH3_len_4to8_64b(input, len, secret, seed);
        if (len) return XXH3_len_1to3_64b(input, len, secret, seed);
        return XXH64_avalanche(seed ^ (XXH_readLE64(secret+56) ^ XXH_readLE64(secret+64)));
    }
}

/*
 * DISCLAIMER: the mid-size code path built on XXH3_mix16B() (lengths 17 to 240)
 * has known collision weaknesses. XXH3 is a non-cryptographic hash: collision
 * resistance is not one of its guarantees, and the seed (or custom secret) only
 * makes intentional collisions *harder*, it is not a security boundary.
 * These weaknesses are nonetheless documented here, so that users relying on a
 * seed for a low per-pair collision probability know what to expect.
 *
 * 1) XXH3_mul128_fold64() does not separate complemented multiplicands.
 *    Modulo 2^128, and with s = a + b + 1 :
 *        (~a) * (~b) == a*b + s - (s + 1) * 2^64
 *    so the low half of the product gains `s` while the high half loses `s`.
 *    Since carry chains are short, both halves happen to flip the same bits
 *    fairly often, leaving their xor (the "fold") unchanged with probability
 *    ~2^-27, instead of the ideal 2^-64. Complementing a single multiplicand
 *    behaves the same way.
 *
 *    Consequence for XXH3_64bits(): for any length in [32, 240], two messages
 *    differing only by the complement of their first 8 (or first 16) bytes
 *    collide with probability ~2^-27 over a uniformly random secret. The
 *    _withSeed variant is also affected, but its collision probability depends
 *    on the chosen pair because the 64-bit seed produces a structured secret.
 *    A fixed pair has been measured to collide with probability ~2^-21 over
 *    random seeds. This is an observed result, not a known upper bound.
 *
 *    Consequence for XXH128(): XXH128_mix32B() mixes each 16-byte chunk a
 *    second time as a raw 64-bit sum, which normally breaks the pattern above.
 *    But that sum is *also* invariant under complementation when
 *    w0 + w1 == 2^64 - 1, and under that additional constraint both output
 *    halves collide simultaneously, again with probability ~2^-27 over a
 *    uniformly random secret. The seeded variant is also affected, and its
 *    exact probability can similarly depend on the chosen pair.
 *    So the 128-bit variant is _not_ immune, contrarily to what was previously
 *    believed, and its effective margin here (2^-27 vs. an ideal 2^-128) is
 *    even thinner than the 64-bit one.
 *
 *    Detailed analysis and measurements in
 *    https://github.com/Cyan4973/xxHash/issues/1127 .
 *
 * 2) *seed-dependent* multicollisions, due to multiplication by zero: when one
 *    of the two multiplicands below is zero, the whole 16-byte chunk stops
 *    contributing to the hash, so all chunks sharing this property collide.
 *    For random inputs, this is a 1 in 2^63 chance of 8 consecutive bytes
 *    cancelling out, in a function invoked at most 16 times per hash, hence
 *    essentially unreachable by accident. But it is trivially reachable by an
 *    attacker who knows the secret (i.e. the unseeded variants). For
 *    comparison, classic UMAC takes a 1 in 2^31 chance of 4 consecutive bytes
 *    cancelling out the secret an arbitrary number of times (addressed for
 *    long inputs in XXH3_accumulate_512).
 *
 * None of the above can be fixed without changing XXH3_mix16B(), which would
 * change every hash value in the [17, 240] range, and XXH3 output values are
 * now frozen. Therefore, if adversarial collision resistance matters for your
 * use case, use a cryptographic hash or a MAC instead.
 */
XXH_FORCE_INLINE xxh_u64 XXH3_mix16B(const xxh_u8* XXH_RESTRICT input,
                                     const xxh_u8* XXH_RESTRICT secret, xxh_u64 seed64)
{
#if defined(__GNUC__) && !defined(__clang__) /* GCC, not Clang */ \
  && defined(__i386__) && defined(__SSE2__)  /* x86 + SSE2 */ \
  && !defined(XXH_ENABLE_AUTOVECTORIZE)      /* Define to disable like XXH32 hack */
    /*
     * UGLY HACK:
     * GCC for x86 tends to autovectorize the 128-bit multiply, resulting in
     * slower code.
     *
     * By forcing seed64 into a register, we disrupt the cost model and
     * cause it to scalarize. See `XXH32_round()`
     *
     * FIXME: Clang's output is still _much_ faster -- On an AMD Ryzen 3600,
     * XXH3_64bits @ len=240 runs at 4.6 GB/s with Clang 9, but 3.3 GB/s on
     * GCC 9.2, despite both emitting scalar code.
     *
     * GCC generates much better scalar code than Clang for the rest of XXH3,
     * which is why finding a more optimal codepath is an interest.
     */
    XXH_COMPILER_GUARD(seed64);
#endif
    {   xxh_u64 const input_lo = XXH_readLE64(input);
        xxh_u64 const input_hi = XXH_readLE64(input+8);
        return XXH3_mul128_fold64(
            input_lo ^ (XXH_readLE64(secret)   + seed64),
            input_hi ^ (XXH_readLE64(secret+8) - seed64)
        );
    }
}

/* For mid range keys, XXH3 uses a Mum-hash variant. */
XXH_FORCE_INLINE XXH_PUREF XXH64_hash_t
XXH3_len_17to128_64b(const xxh_u8* XXH_RESTRICT input, size_t len,
                     const xxh_u8* XXH_RESTRICT secret, size_t secretSize,
                     XXH64_hash_t seed)
{
    XXH_ASSERT(secretSize >= XXH3_SECRET_SIZE_MIN); (void)secretSize;
    XXH_ASSERT(16 < len && len <= 128);

    {   xxh_u64 acc = len * XXH_PRIME64_1;
#if XXH_SIZE_OPT >= 1
        /* Smaller and cleaner, but slightly slower. */
        unsigned int i = (unsigned int)(len - 1) / 32;
        do {
            acc += XXH3_mix16B(input+16 * i, secret+32*i, seed);
            acc += XXH3_mix16B(input+len-16*(i+1), secret+32*i+16, seed);
        } while (i-- != 0);
#else
        if (len > 32) {
            if (len > 64) {
                if (len > 96) {
                    acc += XXH3_mix16B(input+48, secret+96, seed);
                    acc += XXH3_mix16B(input+len-64, secret+112, seed);
                }
                acc += XXH3_mix16B(input+32, secret+64, seed);
                acc += XXH3_mix16B(input+len-48, secret+80, seed);
            }
            acc += XXH3_mix16B(input+16, secret+32, seed);
            acc += XXH3_mix16B(input+len-32, secret+48, seed);
        }
        acc += XXH3_mix16B(input+0, secret+0, seed);
        acc += XXH3_mix16B(input+len-16, secret+16, seed);
#endif
        return XXH3_avalanche(acc);
    }
}

XXH_NO_INLINE XXH_PUREF XXH64_hash_t
XXH3_len_129to240_64b(const xxh_u8* XXH_RESTRICT input, size_t len,
                      const xxh_u8* XXH_RESTRICT secret, size_t secretSize,
                      XXH64_hash_t seed)
{
    XXH_ASSERT(secretSize >= XXH3_SECRET_SIZE_MIN); (void)secretSize;
    XXH_ASSERT(128 < len && len <= XXH3_MIDSIZE_MAX);

    #define XXH3_MIDSIZE_STARTOFFSET 3
    #define XXH3_MIDSIZE_LASTOFFSET  17

    {   xxh_u64 acc = len * XXH_PRIME64_1;
        xxh_u64 acc_end;
        unsigned int const nbRounds = (unsigned int)len / 16;
        unsigned int i;
        XXH_ASSERT(128 < len && len <= XXH3_MIDSIZE_MAX);
        for (i=0; i<8; i++) {
            acc += XXH3_mix16B(input+(16*i), secret+(16*i), seed);
        }
        /* last bytes */
        acc_end = XXH3_mix16B(input + len - 16, secret + XXH3_SECRET_SIZE_MIN - XXH3_MIDSIZE_LASTOFFSET, seed);
        XXH_ASSERT(nbRounds >= 8);
        acc = XXH3_avalanche(acc);
#if defined(__clang__)                                /* Clang */ \
    && (defined(__ARM_NEON) || defined(__ARM_NEON__)) /* NEON */ \
    && !defined(XXH_ENABLE_AUTOVECTORIZE)             /* Define to disable */
        /*
         * UGLY HACK:
         * Clang for ARMv7-A tries to vectorize this loop, similar to GCC x86.
         * In everywhere else, it uses scalar code.
         *
         * For 64->128-bit multiplies, even if the NEON was 100% optimal, it
         * would still be slower than UMAAL (see XXH_mult64to128).
         *
         * Unfortunately, Clang doesn't handle the long multiplies properly and
         * converts them to the nonexistent "vmulq_u64" intrinsic, which is then
         * scalarized into an ugly mess of VMOV.32 instructions.
         *
         * This mess is difficult to avoid without turning autovectorization
         * off completely, but they are usually relatively minor and/or not
         * worth it to fix.
         *
         * This loop is the easiest to fix, as unlike XXH32, this pragma
         * _actually works_ because it is a loop vectorization instead of an
         * SLP vectorization.
         */
        #pragma clang loop vectorize(disable)
#endif
        for (i=8 ; i < nbRounds; i++) {
            /*
             * Prevents clang for unrolling the acc loop and interleaving with this one.
             */
            XXH_COMPILER_GUARD(acc);
            acc_end += XXH3_mix16B(input+(16*i), secret+(16*(i-8)) + XXH3_MIDSIZE_STARTOFFSET, seed);
        }
        return XXH3_avalanche(acc + acc_end);
    }
}


/* =======     Long Keys     ======= */

#define XXH_STRIPE_LEN 64
#define XXH_SECRET_CONSUME_RATE 8   /* nb of secret bytes consumed at each accumulation */
#define XXH_ACC_NB (XXH_STRIPE_LEN / sizeof(xxh_u64))

#ifdef XXH_OLD_NAMES
#  define STRIPE_LEN XXH_STRIPE_LEN
#  define ACC_NB XXH_ACC_NB
#endif

#ifndef XXH_PREFETCH_DIST
#  ifdef __clang__
#    define XXH_PREFETCH_DIST 320
#  else
#    if (XXH_VECTOR == XXH_AVX512)
#      define XXH_PREFETCH_DIST 512
#    else
#      define XXH_PREFETCH_DIST 384
#    endif
#  endif  /* __clang__ */
#endif  /* XXH_PREFETCH_DIST */

/*
 * These macros are to generate an XXH3_accumulate() function.
 * The two arguments select the name suffix and target attribute.
 *
 * The name of this symbol is XXH3_accumulate_<name>() and it calls
 * XXH3_accumulate_512_<name>().
 *
 * It may be useful to hand implement this function if the compiler fails to
 * optimize the inline function.
 */
#define XXH3_ACCUMULATE_TEMPLATE(name)                      \
void                                                        \
XXH3_accumulate_##name(xxh_u64* XXH_RESTRICT acc,           \
                       const xxh_u8* XXH_RESTRICT input,    \
                       const xxh_u8* XXH_RESTRICT secret,   \
                       size_t nbStripes)                    \
{                                                           \
    size_t n;                                               \
    for (n = 0; n < nbStripes; n++ ) {                      \
        const xxh_u8* const in = input + n*XXH_STRIPE_LEN;  \
        XXH_PREFETCH(in + XXH_PREFETCH_DIST);               \
        XXH3_accumulate_512_##name(                         \
                 acc,                                       \
                 in,                                        \
                 secret + n*XXH_SECRET_CONSUME_RATE);       \
    }                                                       \
}


XXH_FORCE_INLINE void XXH_writeLE64(void* dst, xxh_u64 v64)
{
    if (!XXH_CPU_LITTLE_ENDIAN) v64 = XXH_swap64(v64);
    XXH_memcpy(dst, &v64, sizeof(v64));
}

/* Several intrinsic functions below are supposed to accept __int64 as argument,
 * as documented in https://software.intel.com/sites/landingpage/IntrinsicsGuide/ .
 * However, several environments do not define __int64 type,
 * requiring a workaround.
 */
#if !defined (__VMS) \
  && (defined (__cplusplus) \
  || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */) )
    typedef int64_t xxh_i64;
#else
    /* the following type must have a width of 64-bit */
    typedef long long xxh_i64;
#endif


/*
 * XXH3_accumulate_512 is the tightest loop for long inputs, and it is the most optimized.
 *
 * It is a hardened version of UMAC, based off of FARSH's implementation.
 *
 * This was chosen because it adapts quite well to 32-bit, 64-bit, and SIMD
 * implementations, and it is ridiculously fast.
 *
 * We harden it by mixing the original input to the accumulators as well as the product.
 *
 * This means that in the (relatively likely) case of a multiply by zero, the
 * original input is preserved.
 *
 * On 128-bit inputs, we swap 64-bit pairs when we add the input to improve
 * cross-pollination, as otherwise the upper and lower halves would be
 * essentially independent.
 *
 * This doesn't matter on 64-bit hashes since they all get merged together in
 * the end, so we skip the extra step.
 *
 * Both XXH3_64bits and XXH3_128bits use this subroutine.
 */


/* routines for long inputs, for each instruction set (scalar ones are always available) */
#include "xxh3_avx512.h"
#include "xxh3_avx2.h"
#include "xxh3_sse2.h"
#include "xxh3_neon.h"
#include "xxh3_vsx.h"
#include "xxh3_sve.h"
#include "xxh3_lsx.h"
#include "xxh3_lasx.h"
#include "xxh3_rvv.h"
#include "xxh3_scalar.h"


typedef void (*XXH3_f_accumulate)(xxh_u64* XXH_RESTRICT, const xxh_u8* XXH_RESTRICT, const xxh_u8* XXH_RESTRICT, size_t);
typedef void (*XXH3_f_scrambleAcc)(void* XXH_RESTRICT, const void*);
typedef void (*XXH3_f_initCustomSecret)(void* XXH_RESTRICT, xxh_u64);


#if (XXH_VECTOR == XXH_AVX512)

#define XXH3_accumulate_512 XXH3_accumulate_512_avx512
#define XXH3_accumulate     XXH3_accumulate_avx512
#define XXH3_scrambleAcc    XXH3_scrambleAcc_avx512
#define XXH3_initCustomSecret XXH3_initCustomSecret_avx512

#elif (XXH_VECTOR == XXH_AVX2)

#define XXH3_accumulate_512 XXH3_accumulate_512_avx2
#define XXH3_accumulate     XXH3_accumulate_avx2
#define XXH3_scrambleAcc    XXH3_scrambleAcc_avx2
#define XXH3_initCustomSecret XXH3_initCustomSecret_avx2

#elif (XXH_VECTOR == XXH_SSE2)

#define XXH3_accumulate_512 XXH3_accumulate_512_sse2
#define XXH3_accumulate     XXH3_accumulate_sse2
#define XXH3_scrambleAcc    XXH3_scrambleAcc_sse2
#define XXH3_initCustomSecret XXH3_initCustomSecret_sse2

#elif (XXH_VECTOR == XXH_NEON)

#define XXH3_accumulate_512 XXH3_accumulate_512_neon
#define XXH3_accumulate     XXH3_accumulate_neon
#define XXH3_scrambleAcc    XXH3_scrambleAcc_neon
#define XXH3_initCustomSecret XXH3_initCustomSecret_scalar

#elif (XXH_VECTOR == XXH_VSX)

#define XXH3_accumulate_512 XXH3_accumulate_512_vsx
#define XXH3_accumulate     XXH3_accumulate_vsx
#define XXH3_scrambleAcc    XXH3_scrambleAcc_vsx
#define XXH3_initCustomSecret XXH3_initCustomSecret_scalar

#elif (XXH_VECTOR == XXH_SVE)
#define XXH3_accumulate_512 XXH3_accumulate_512_sve
#define XXH3_accumulate     XXH3_accumulate_sve
#define XXH3_scrambleAcc    XXH3_scrambleAcc_scalar
#define XXH3_initCustomSecret XXH3_initCustomSecret_scalar

#elif (XXH_VECTOR == XXH_LASX)
#define XXH3_accumulate_512 XXH3_accumulate_512_lasx
#define XXH3_accumulate     XXH3_accumulate_lasx
#define XXH3_scrambleAcc    XXH3_scrambleAcc_lasx
#define XXH3_initCustomSecret XXH3_initCustomSecret_scalar

#elif (XXH_VECTOR == XXH_LSX)
#define XXH3_accumulate_512 XXH3_accumulate_512_lsx
#define XXH3_accumulate     XXH3_accumulate_lsx
#define XXH3_scrambleAcc    XXH3_scrambleAcc_lsx
#define XXH3_initCustomSecret XXH3_initCustomSecret_scalar

#elif (XXH_VECTOR == XXH_RVV)
#define XXH3_accumulate_512 XXH3_accumulate_512_rvv
#define XXH3_accumulate     XXH3_accumulate_rvv
#define XXH3_scrambleAcc    XXH3_scrambleAcc_rvv
#define XXH3_initCustomSecret XXH3_initCustomSecret_rvv

#else /* scalar */

#define XXH3_accumulate_512 XXH3_accumulate_512_scalar
#define XXH3_accumulate     XXH3_accumulate_scalar
#define XXH3_scrambleAcc    XXH3_scrambleAcc_scalar
#define XXH3_initCustomSecret XXH3_initCustomSecret_scalar

#endif

#if XXH_SIZE_OPT >= 1 /* don't do SIMD for initialization */
#  undef XXH3_initCustomSecret
#  define XXH3_initCustomSecret XXH3_initCustomSecret_scalar
#endif

XXH_FORCE_INLINE void
XXH3_hashLong_internal_loop(xxh_u64* XXH_RESTRICT acc,
                      const xxh_u8* XXH_RESTRICT input, size_t len,
                      const xxh_u8* XXH_RESTRICT secret, size_t secretSize,
                            XXH3_f_accumulate f_acc,
                            XXH3_f_scrambleAcc f_scramble)
{
    size_t const nbStripesPerBlock = (secretSize - XXH_STRIPE_LEN) / XXH_SECRET_CONSUME_RATE;
    size_t const block_len = XXH_STRIPE_LEN * nbStripesPerBlock;
    size_t const nb_blocks = (len - 1) / block_len;

    size_t n;

    XXH_ASSERT(secretSize >= XXH3_SECRET_SIZE_MIN);

    for (n = 0; n < nb_blocks; n++) {
        f_acc(acc, input + n*block_len, secret, nbStripesPerBlock);
        f_scramble(acc, secret + secretSize - XXH_STRIPE_LEN);
    }

    /* last partial block */
    XXH_ASSERT(len > XXH_STRIPE_LEN);
    {   size_t const nbStripes = ((len - 1) - (block_len * nb_blocks)) / XXH_STRIPE_LEN;
        XXH_ASSERT(nbStripes <= (secretSize / XXH_SECRET_CONSUME_RATE));
        f_acc(acc, input + nb_blocks*block_len, secret, nbStripes);

        /* last stripe */
        {   const xxh_u8* const p = input + len - XXH_STRIPE_LEN;
#define XXH_SECRET_LASTACC_START 7  /* not aligned on 8, last secret is different from acc & scrambler */
            XXH3_accumulate_512(acc, p, secret + secretSize - XXH_STRIPE_LEN - XXH_SECRET_LASTACC_START);
    }   }
}

XXH_FORCE_INLINE xxh_u64
XXH3_mix2Accs(const xxh_u64* XXH_RESTRICT acc, const xxh_u8* XXH_RESTRICT secret)
{
    return XXH3_mul128_fold64(
               acc[0] ^ XXH_readLE64(secret),
               acc[1] ^ XXH_readLE64(secret+8) );
}

static XXH_PUREF XXH64_hash_t
XXH3_mergeAccs(const xxh_u64* XXH_RESTRICT acc, const xxh_u8* XXH_RESTRICT secret, xxh_u64 start)
{
    xxh_u64 result64 = start;
    size_t i = 0;

    for (i = 0; i < 4; i++) {
        result64 += XXH3_mix2Accs(acc+2*i, secret + 16*i);
#if defined(__clang__)                                /* Clang */ \
    && (defined(__arm__) || defined(__thumb__))       /* ARMv7 */ \
    && (defined(__ARM_NEON) || defined(__ARM_NEON__)) /* NEON */  \
    && !defined(XXH_ENABLE_AUTOVECTORIZE)             /* Define to disable */
        /*
         * UGLY HACK:
         * Prevent autovectorization on Clang ARMv7-a. Exact same problem as
         * the one in XXH3_len_129to240_64b. Speeds up shorter keys > 240b.
         * XXH3_64bits, len == 256, Snapdragon 835:
         *   without hack: 2063.7 MB/s
         *   with hack:    2560.7 MB/s
         */
        XXH_COMPILER_GUARD(result64);
#endif
    }

    return XXH3_avalanche(result64);
}

/* do not align on 8, so that the secret is different from the accumulator */
#define XXH_SECRET_MERGEACCS_START 11

static XXH_PUREF XXH64_hash_t
XXH3_finalizeLong_64b(const xxh_u64* XXH_RESTRICT acc, const xxh_u8* XXH_RESTRICT secret, xxh_u64 len)
{
    return XXH3_mergeAccs(acc, secret + XXH_SECRET_MERGEACCS_START, len * XXH_PRIME64_1);
}

#define XXH3_INIT_ACC { XXH_PRIME32_3, XXH_PRIME64_1, XXH_PRIME64_2, XXH_PRIME64_3, \
                        XXH_PRIME64_4, XXH_PRIME32_2, XXH_PRIME64_5, XXH_PRIME32_1 }

XXH_FORCE_INLINE XXH64_hash_t
XXH3_hashLong_64b_internal(const void* XXH_RESTRICT input, size_t len,
                           const void* XXH_RESTRICT secret, size_t secretSize,
                           XXH3_f_accumulate f_acc,
                           XXH3_f_scrambleAcc f_scramble)
{
    XXH_ALIGN(XXH_ACC_ALIGN) xxh_u64 acc[XXH_ACC_NB] = XXH3_INIT_ACC;

    XXH3_hashLong_internal_loop(acc, (const xxh_u8*)input, len, (const xxh_u8*)secret, secretSize, f_acc, f_scramble);

    /* converge into final hash */
    XXH_STATIC_ASSERT(sizeof(acc) == 64);
    XXH_ASSERT(secretSize >= sizeof(acc) + XXH_SECRET_MERGEACCS_START);
    return XXH3_finalizeLong_64b(acc, (const xxh_u8*)secret, (xxh_u64)len);
}

/*
 * It's important for performance to transmit secret's size (when it's static)
 * so that the compiler can properly optimize the vectorized loop.
 * This makes a big performance difference for "medium" keys (<1 KB) when using AVX instruction set.
 * When the secret size is unknown, or on GCC 12 where the mix of NO_INLINE and FORCE_INLINE
 * breaks -Og, this is XXH_NO_INLINE.
 */
XXH3_WITH_SECRET_INLINE XXH64_hash_t
XXH3_hashLong_64b_withSecret(const void* XXH_RESTRICT input, size_t len,
                             XXH64_hash_t seed64, const xxh_u8* XXH_RESTRICT secret, size_t secretLen)
{
    (void)seed64;
    return XXH3_hashLong_64b_internal(input, len, secret, secretLen, XXH3_accumulate, XXH3_scrambleAcc);
}

/*
 * It's preferable for performance that XXH3_hashLong is not inlined,
 * as it results in a smaller function for small data, easier to the instruction cache.
 * Note that inside this no_inline function, we do inline the internal loop,
 * and provide a statically defined secret size to allow optimization of vector loop.
 */
XXH_NO_INLINE XXH_PUREF XXH64_hash_t
XXH3_hashLong_64b_default(const void* XXH_RESTRICT input, size_t len,
                          XXH64_hash_t seed64, const xxh_u8* XXH_RESTRICT secret, size_t secretLen)
{
    (void)seed64; (void)secret; (void)secretLen;
    return XXH3_hashLong_64b_internal(input, len, XXH3_kSecret, sizeof(XXH3_kSecret), XXH3_accumulate, XXH3_scrambleAcc);
}

/*
 * XXH3_hashLong_64b_withSeed():
 * Generate a custom key based on alteration of default XXH3_kSecret with the seed,
 * and then use this key for long mode hashing.
 *
 * This operation is decently fast but nonetheless costs a little bit of time.
 * Try to avoid it whenever possible (typically when seed==0).
 *
 * It's important for performance that XXH3_hashLong is not inlined. Not sure
 * why (uop cache maybe?), but the difference is large and easily measurable.
 */
XXH_FORCE_INLINE XXH64_hash_t
XXH3_hashLong_64b_withSeed_internal(const void* input, size_t len,
                                    XXH64_hash_t seed,
                                    XXH3_f_accumulate f_acc,
                                    XXH3_f_scrambleAcc f_scramble,
                                    XXH3_f_initCustomSecret f_initSec)
{
#if XXH_SIZE_OPT <= 0
    if (seed == 0)
        return XXH3_hashLong_64b_internal(input, len,
                                          XXH3_kSecret, sizeof(XXH3_kSecret),
                                          f_acc, f_scramble);
#endif
    {   XXH_ALIGN(XXH_SEC_ALIGN) xxh_u8 secret[XXH_SECRET_DEFAULT_SIZE];
        f_initSec(secret, seed);
        return XXH3_hashLong_64b_internal(input, len, secret, sizeof(secret),
                                          f_acc, f_scramble);
    }
}

/*
 * It's important for performance that XXH3_hashLong is not inlined.
 */
XXH_NO_INLINE XXH64_hash_t
XXH3_hashLong_64b_withSeed(const void* XXH_RESTRICT input, size_t len,
                           XXH64_hash_t seed, const xxh_u8* XXH_RESTRICT secret, size_t secretLen)
{
    (void)secret; (void)secretLen;
    return XXH3_hashLong_64b_withSeed_internal(input, len, seed,
                XXH3_accumulate, XXH3_scrambleAcc, XXH3_initCustomSecret);
}


typedef XXH64_hash_t (*XXH3_hashLong64_f)(const void* XXH_RESTRICT, size_t,
                                          XXH64_hash_t, const xxh_u8* XXH_RESTRICT, size_t);

XXH_FORCE_INLINE XXH64_hash_t
XXH3_64bits_internal(const void* XXH_RESTRICT input, size_t len,
                     XXH64_hash_t seed64, const void* XXH_RESTRICT secret, size_t secretLen,
                     XXH3_hashLong64_f f_hashLong)
{
    XXH_ASSERT(secretLen >= XXH3_SECRET_SIZE_MIN);
    /*
     * If an action is to be taken if `secretLen` condition is not respected,
     * it should be done here.
     * For now, it's a contract pre-condition.
     * Adding a check and a branch here would cost performance at every hash.
     * Also, note that function signature doesn't offer room to return an error.
     */
    if (len <= 16)
        return XXH3_len_0to16_64b((const xxh_u8*)input, len, (const xxh_u8*)secret, seed64);
    if (len <= 128)
        return XXH3_len_17to128_64b((const xxh_u8*)input, len, (const xxh_u8*)secret, secretLen, seed64);
    if (len <= XXH3_MIDSIZE_MAX)
        return XXH3_len_129to240_64b((const xxh_u8*)input, len, (const xxh_u8*)secret, secretLen, seed64);
    return f_hashLong(input, len, seed64, (const xxh_u8*)secret, secretLen);
}


/* ===   Public entry point   === */

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH64_hash_t XXH3_64bits(XXH_NOESCAPE const void* input, size_t length)
{
    return XXH3_64bits_internal(input, length, 0, XXH3_kSecret, sizeof(XXH3_kSecret), XXH3_hashLong_64b_default);
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH64_hash_t
XXH3_64bits_withSecret(XXH_NOESCAPE const void* input, size_t length, XXH_NOESCAPE const void* secret, size_t secretSize)
{
    return XXH3_64bits_internal(input, length, 0, secret, secretSize, XXH3_hashLong_64b_withSecret);
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH64_hash_t
XXH3_64bits_withSeed(XXH_NOESCAPE const void* input, size_t length, XXH64_hash_t seed)
{
    return XXH3_64bits_internal(input, length, seed, XXH3_kSecret, sizeof(XXH3_kSecret), XXH3_hashLong_64b_withSeed);
}

XXH_PUBLIC_API XXH64_hash_t
XXH3_64bits_withSecretandSeed(XXH_NOESCAPE const void* input, size_t length, XXH_NOESCAPE const void* secret, size_t secretSize, XXH64_hash_t seed)
{
    if (length <= XXH3_MIDSIZE_MAX)
        return XXH3_64bits_internal(input, length, seed, XXH3_kSecret, sizeof(XXH3_kSecret), NULL);
    return XXH3_hashLong_64b_withSecret(input, length, seed, (const xxh_u8*)secret, secretSize);
}



/* ==========================================
 * XXH3 128 bits (a.k.a XXH128)
 * ==========================================
 * XXH3's 128-bit variant has better mixing and strength than the 64-bit variant,
 * even without counting the significantly larger output size.
 *
 * For example, extra steps are taken to mitigate the seed-dependent collisions
 * in 17-240 byte inputs (See XXH3_mix16B and XXH128_mix32B). Note that these
 * steps only mitigate, they do not eliminate the issue: see the DISCLAIMER
 * above XXH3_mix16B() for the cases that get through.
 *
 * This strength naturally comes at the cost of some speed, especially on short
 * lengths. Note that longer hashes are about as fast as the 64-bit version
 * due to it using only a slight modification of the 64-bit loop.
 *
 * XXH128 is also more oriented towards 64-bit machines. It is still extremely
 * fast for a _128-bit_ hash on 32-bit (it usually clears XXH64).
 */

XXH_FORCE_INLINE XXH_PUREF XXH128_hash_t
XXH3_len_1to3_128b(const xxh_u8* input, size_t len, const xxh_u8* secret, XXH64_hash_t seed)
{
    /* A doubled version of 1to3_64b with different constants. */
    XXH_ASSERT(input != NULL);
    XXH_ASSERT(1 <= len && len <= 3);
    XXH_ASSERT(secret != NULL);
    /*
     * len = 1: combinedl = { input[0], 0x01, input[0], input[0] }
     * len = 2: combinedl = { input[1], 0x02, input[0], input[1] }
     * len = 3: combinedl = { input[2], 0x03, input[0], input[1] }
     */
    {   xxh_u8 const c1 = input[0];
        xxh_u8 const c2 = input[len >> 1];
        xxh_u8 const c3 = input[len - 1];
        xxh_u32 const combinedl = ((xxh_u32)c1 <<16) | ((xxh_u32)c2 << 24)
                                | ((xxh_u32)c3 << 0) | ((xxh_u32)len << 8);
        xxh_u32 const combinedh = XXH_rotl32(XXH_swap32(combinedl), 13);
        xxh_u64 const bitflipl = (XXH_readLE32(secret) ^ XXH_readLE32(secret+4)) + seed;
        xxh_u64 const bitfliph = (XXH_readLE32(secret+8) ^ XXH_readLE32(secret+12)) - seed;
        xxh_u64 const keyed_lo = (xxh_u64)combinedl ^ bitflipl;
        xxh_u64 const keyed_hi = (xxh_u64)combinedh ^ bitfliph;
        XXH128_hash_t h128;
        h128.low64  = XXH64_avalanche(keyed_lo);
        h128.high64 = XXH64_avalanche(keyed_hi);
        return h128;
    }
}

XXH_FORCE_INLINE XXH_PUREF XXH128_hash_t
XXH3_len_4to8_128b(const xxh_u8* input, size_t len, const xxh_u8* secret, XXH64_hash_t seed)
{
    XXH_ASSERT(input != NULL);
    XXH_ASSERT(secret != NULL);
    XXH_ASSERT(4 <= len && len <= 8);
    seed ^= (xxh_u64)XXH_swap32((xxh_u32)seed) << 32;
    {   xxh_u32 const input_lo = XXH_readLE32(input);
        xxh_u32 const input_hi = XXH_readLE32(input + len - 4);
        xxh_u64 const input_64 = input_lo + ((xxh_u64)input_hi << 32);
        xxh_u64 const bitflip = (XXH_readLE64(secret+16) ^ XXH_readLE64(secret+24)) + seed;
        xxh_u64 const keyed = input_64 ^ bitflip;

        /* Shift len to the left to ensure it is even, this avoids even multiplies. */
        XXH128_hash_t m128 = XXH_mult64to128(keyed, XXH_PRIME64_1 + (len << 2));

        m128.high64 += (m128.low64 << 1);
        m128.low64  ^= (m128.high64 >> 3);

        m128.low64   = XXH_xorshift64(m128.low64, 35);
        m128.low64  *= PRIME_MX2;
        m128.low64   = XXH_xorshift64(m128.low64, 28);
        m128.high64  = XXH3_avalanche(m128.high64);
        return m128;
    }
}

XXH_FORCE_INLINE XXH_PUREF XXH128_hash_t
XXH3_len_9to16_128b(const xxh_u8* input, size_t len, const xxh_u8* secret, XXH64_hash_t seed)
{
    XXH_ASSERT(input != NULL);
    XXH_ASSERT(secret != NULL);
    XXH_ASSERT(9 <= len && len <= 16);
    {   xxh_u64 const bitflipl = (XXH_readLE64(secret+32) ^ XXH_readLE64(secret+40)) - seed;
        xxh_u64 const bitfliph = (XXH_readLE64(secret+48) ^ XXH_readLE64(secret+56)) + seed;
        xxh_u64 const input_lo = XXH_readLE64(input);
        xxh_u64       input_hi = XXH_readLE64(input + len - 8);
        XXH128_hash_t m128 = XXH_mult64to128(input_lo ^ input_hi ^ bitflipl, XXH_PRIME64_1);
        /*
         * Put len in the middle of m128 to ensure that the length gets mixed to
         * both the low and high bits in the 128x64 multiply below.
         */
        m128.low64 += (xxh_u64)(len - 1) << 54;
        input_hi   ^= bitfliph;
        /*
         * Add the high 32 bits of input_hi to the high 32 bits of m128, then
         * add the long product of the low 32 bits of input_hi and XXH_PRIME32_2 to
         * the high 64 bits of m128.
         *
         * The best approach to this operation is different on 32-bit and 64-bit.
         */
        if (sizeof(void *) < sizeof(xxh_u64)) { /* 32-bit */
            /*
             * 32-bit optimized version, which is more readable.
             *
             * On 32-bit, it removes an ADC and delays a dependency between the two
             * halves of m128.high64, but it generates an extra mask on 64-bit.
             */
            m128.high64 += (input_hi & 0xFFFFFFFF00000000ULL) + XXH_mult32to64((xxh_u32)input_hi, XXH_PRIME32_2);
        } else {
            /*
             * 64-bit optimized (albeit more confusing) version.
             *
             * Uses some properties of addition and multiplication to remove the mask:
             *
             * Let:
             *    a = input_hi.lo = (input_hi & 0x00000000FFFFFFFF)
             *    b = input_hi.hi = (input_hi & 0xFFFFFFFF00000000)
             *    c = XXH_PRIME32_2
             *
             *    a + (b * c)
             * Inverse Property: x + y - x == y
             *    a + (b * (1 + c - 1))
             * Distributive Property: x * (y + z) == (x * y) + (x * z)
             *    a + (b * 1) + (b * (c - 1))
             * Identity Property: x * 1 == x
             *    a + b + (b * (c - 1))
             *
             * Substitute a, b, and c:
             *    input_hi.hi + input_hi.lo + ((xxh_u64)input_hi.lo * (XXH_PRIME32_2 - 1))
             *
             * Since input_hi.hi + input_hi.lo == input_hi, we get this:
             *    input_hi + ((xxh_u64)input_hi.lo * (XXH_PRIME32_2 - 1))
             */
            m128.high64 += input_hi + XXH_mult32to64((xxh_u32)input_hi, XXH_PRIME32_2 - 1);
        }
        /* m128 ^= XXH_swap64(m128 >> 64); */
        m128.low64  ^= XXH_swap64(m128.high64);

        {   /* 128x64 multiply: h128 = m128 * XXH_PRIME64_2; */
            XXH128_hash_t h128 = XXH_mult64to128(m128.low64, XXH_PRIME64_2);
            h128.high64 += m128.high64 * XXH_PRIME64_2;

            h128.low64   = XXH3_avalanche(h128.low64);
            h128.high64  = XXH3_avalanche(h128.high64);
            return h128;
    }   }
}

/*
 * Assumption: `secret` size is >= XXH3_SECRET_SIZE_MIN
 */
XXH_FORCE_INLINE XXH_PUREF XXH128_hash_t
XXH3_len_0to16_128b(const xxh_u8* input, size_t len, const xxh_u8* secret, XXH64_hash_t seed)
{
    XXH_ASSERT(len <= 16);
    {   if (len > 8) return XXH3_len_9to16_128b(input, len, secret, seed);
        if (len >= 4) return XXH3_len_4to8_128b(input, len, secret, seed);
        if (len) return XXH3_len_1to3_128b(input, len, secret, seed);
        {   XXH128_hash_t h128;
            xxh_u64 const bitflipl = XXH_readLE64(secret+64) ^ XXH_readLE64(secret+72);
            xxh_u64 const bitfliph = XXH_readLE64(secret+80) ^ XXH_readLE64(secret+88);
            h128.low64 = XXH64_avalanche(seed ^ bitflipl);
            h128.high64 = XXH64_avalanche( seed ^ bitfliph);
            return h128;
    }   }
}

/*
 * A bit slower than XXH3_mix16B, but handles multiply by zero better.
 */
XXH_FORCE_INLINE XXH128_hash_t
XXH128_mix32B(XXH128_hash_t acc, const xxh_u8* input_1, const xxh_u8* input_2,
              const xxh_u8* secret, XXH64_hash_t seed)
{
    acc.low64  += XXH3_mix16B (input_1, secret+0, seed);
    acc.low64  ^= XXH_readLE64(input_2) + XXH_readLE64(input_2 + 8);
    acc.high64 += XXH3_mix16B (input_2, secret+16, seed);
    acc.high64 ^= XXH_readLE64(input_1) + XXH_readLE64(input_1 + 8);
    return acc;
}


XXH_FORCE_INLINE XXH_PUREF XXH128_hash_t
XXH3_len_17to128_128b(const xxh_u8* XXH_RESTRICT input, size_t len,
                      const xxh_u8* XXH_RESTRICT secret, size_t secretSize,
                      XXH64_hash_t seed)
{
    XXH_ASSERT(secretSize >= XXH3_SECRET_SIZE_MIN); (void)secretSize;
    XXH_ASSERT(16 < len && len <= 128);

    {   XXH128_hash_t acc;
        acc.low64 = len * XXH_PRIME64_1;
        acc.high64 = 0;

#if XXH_SIZE_OPT >= 1
        {
            /* Smaller, but slightly slower. */
            unsigned int i = (unsigned int)(len - 1) / 32;
            do {
                acc = XXH128_mix32B(acc, input+16*i, input+len-16*(i+1), secret+32*i, seed);
            } while (i-- != 0);
        }
#else
        if (len > 32) {
            if (len > 64) {
                if (len > 96) {
                    acc = XXH128_mix32B(acc, input+48, input+len-64, secret+96, seed);
                }
                acc = XXH128_mix32B(acc, input+32, input+len-48, secret+64, seed);
            }
            acc = XXH128_mix32B(acc, input+16, input+len-32, secret+32, seed);
        }
        acc = XXH128_mix32B(acc, input, input+len-16, secret, seed);
#endif
        {   XXH128_hash_t h128;
            h128.low64  = acc.low64 + acc.high64;
            h128.high64 = (acc.low64    * XXH_PRIME64_1)
                        + (acc.high64   * XXH_PRIME64_4)
                        + ((len - seed) * XXH_PRIME64_2);
            h128.low64  = XXH3_avalanche(h128.low64);
            h128.high64 = (XXH64_hash_t)0 - XXH3_avalanche(h128.high64);
            return h128;
        }
    }
}

XXH_NO_INLINE XXH_PUREF XXH128_hash_t
XXH3_len_129to240_128b(const xxh_u8* XXH_RESTRICT input, size_t len,
                       const xxh_u8* XXH_RESTRICT secret, size_t secretSize,
                       XXH64_hash_t seed)
{
    XXH_ASSERT(secretSize >= XXH3_SECRET_SIZE_MIN); (void)secretSize;
    XXH_ASSERT(128 < len && len <= XXH3_MIDSIZE_MAX);

    {   XXH128_hash_t acc;
        unsigned i;
        acc.low64 = len * XXH_PRIME64_1;
        acc.high64 = 0;
        /*
         *  We set as `i` as offset + 32. We do this so that unchanged
         * `len` can be used as upper bound. This reaches a sweet spot
         * where both x86 and aarch64 get simple agen and good codegen
         * for the loop.
         */
        for (i = 32; i < 160; i += 32) {
            acc = XXH128_mix32B(acc,
                                input  + i - 32,
                                input  + i - 16,
                                secret + i - 32,
                                seed);
        }
        acc.low64 = XXH3_avalanche(acc.low64);
        acc.high64 = XXH3_avalanche(acc.high64);
        /*
         * NB: `i <= len` will duplicate the last 32-bytes if
         * len % 32 was zero. This is an unfortunate necessity to keep
         * the hash result stable.
         */
        for (i=160; i <= len; i += 32) {
            acc = XXH128_mix32B(acc,
                                input + i - 32,
                                input + i - 16,
                                secret + XXH3_MIDSIZE_STARTOFFSET + i - 160,
                                seed);
        }
        /* last bytes */
        acc = XXH128_mix32B(acc,
                            input + len - 16,
                            input + len - 32,
                            secret + XXH3_SECRET_SIZE_MIN - XXH3_MIDSIZE_LASTOFFSET - 16,
                            (XXH64_hash_t)0 - seed);

        {   XXH128_hash_t h128;
            h128.low64  = acc.low64 + acc.high64;
            h128.high64 = (acc.low64    * XXH_PRIME64_1)
                        + (acc.high64   * XXH_PRIME64_4)
                        + ((len - seed) * XXH_PRIME64_2);
            h128.low64  = XXH3_avalanche(h128.low64);
            h128.high64 = (XXH64_hash_t)0 - XXH3_avalanche(h128.high64);
            return h128;
        }
    }
}

static XXH_PUREF XXH128_hash_t
XXH3_finalizeLong_128b(const xxh_u64* XXH_RESTRICT acc, const xxh_u8* XXH_RESTRICT secret, size_t secretSize, xxh_u64 len)
{
    XXH128_hash_t h128;
    h128.low64 = XXH3_finalizeLong_64b(acc, secret, len);
    h128.high64 = XXH3_mergeAccs(acc, secret + secretSize
                                             - XXH_STRIPE_LEN - XXH_SECRET_MERGEACCS_START,
                                             ~(len * XXH_PRIME64_2));
    return h128;
}

XXH_FORCE_INLINE XXH128_hash_t
XXH3_hashLong_128b_internal(const void* XXH_RESTRICT input, size_t len,
                            const xxh_u8* XXH_RESTRICT secret, size_t secretSize,
                            XXH3_f_accumulate f_acc,
                            XXH3_f_scrambleAcc f_scramble)
{
    XXH_ALIGN(XXH_ACC_ALIGN) xxh_u64 acc[XXH_ACC_NB] = XXH3_INIT_ACC;

    XXH3_hashLong_internal_loop(acc, (const xxh_u8*)input, len, secret, secretSize, f_acc, f_scramble);

    /* converge into final hash */
    XXH_STATIC_ASSERT(sizeof(acc) == 64);
    XXH_ASSERT(secretSize >= sizeof(acc) + XXH_SECRET_MERGEACCS_START);
    return XXH3_finalizeLong_128b(acc, secret, secretSize, (xxh_u64)len);
}

/*
 * It's important for performance that XXH3_hashLong() is not inlined.
 */
XXH_NO_INLINE XXH_PUREF XXH128_hash_t
XXH3_hashLong_128b_default(const void* XXH_RESTRICT input, size_t len,
                           XXH64_hash_t seed64,
                           const void* XXH_RESTRICT secret, size_t secretLen)
{
    (void)seed64; (void)secret; (void)secretLen;
    return XXH3_hashLong_128b_internal(input, len, XXH3_kSecret, sizeof(XXH3_kSecret),
                                       XXH3_accumulate, XXH3_scrambleAcc);
}

/*
 * It's important for performance to pass @p secretLen (when it's static)
 * to the compiler, so that it can properly optimize the vectorized loop.
 *
 * When the secret size is unknown, or on GCC 12 where the mix of NO_INLINE and FORCE_INLINE
 * breaks -Og, this is XXH_NO_INLINE.
 */
XXH3_WITH_SECRET_INLINE XXH128_hash_t
XXH3_hashLong_128b_withSecret(const void* XXH_RESTRICT input, size_t len,
                              XXH64_hash_t seed64,
                              const void* XXH_RESTRICT secret, size_t secretLen)
{
    (void)seed64;
    return XXH3_hashLong_128b_internal(input, len, (const xxh_u8*)secret, secretLen,
                                       XXH3_accumulate, XXH3_scrambleAcc);
}

XXH_FORCE_INLINE XXH128_hash_t
XXH3_hashLong_128b_withSeed_internal(const void* XXH_RESTRICT input, size_t len,
                                XXH64_hash_t seed64,
                                XXH3_f_accumulate f_acc,
                                XXH3_f_scrambleAcc f_scramble,
                                XXH3_f_initCustomSecret f_initSec)
{
    if (seed64 == 0)
        return XXH3_hashLong_128b_internal(input, len,
                                           XXH3_kSecret, sizeof(XXH3_kSecret),
                                           f_acc, f_scramble);
    {   XXH_ALIGN(XXH_SEC_ALIGN) xxh_u8 secret[XXH_SECRET_DEFAULT_SIZE];
        f_initSec(secret, seed64);
        return XXH3_hashLong_128b_internal(input, len, (const xxh_u8*)secret, sizeof(secret),
                                           f_acc, f_scramble);
    }
}

/*
 * It's important for performance that XXH3_hashLong is not inlined.
 */
XXH_NO_INLINE XXH128_hash_t
XXH3_hashLong_128b_withSeed(const void* input, size_t len,
                            XXH64_hash_t seed64, const void* XXH_RESTRICT secret, size_t secretLen)
{
    (void)secret; (void)secretLen;
    return XXH3_hashLong_128b_withSeed_internal(input, len, seed64,
                XXH3_accumulate, XXH3_scrambleAcc, XXH3_initCustomSecret);
}

typedef XXH128_hash_t (*XXH3_hashLong128_f)(const void* XXH_RESTRICT, size_t,
                                            XXH64_hash_t, const void* XXH_RESTRICT, size_t);

XXH_FORCE_INLINE XXH128_hash_t
XXH3_128bits_internal(const void* input, size_t len,
                      XXH64_hash_t seed64, const void* XXH_RESTRICT secret, size_t secretLen,
                      XXH3_hashLong128_f f_hl128)
{
    XXH_ASSERT(secretLen >= XXH3_SECRET_SIZE_MIN);
    /*
     * If an action is to be taken if `secret` conditions are not respected,
     * it should be done here.
     * For now, it's a contract pre-condition.
     * Adding a check and a branch here would cost performance at every hash.
     */
    if (len <= 16)
        return XXH3_len_0to16_128b((const xxh_u8*)input, len, (const xxh_u8*)secret, seed64);
    if (len <= 128)
        return XXH3_len_17to128_128b((const xxh_u8*)input, len, (const xxh_u8*)secret, secretLen, seed64);
    if (len <= XXH3_MIDSIZE_MAX)
        return XXH3_len_129to240_128b((const xxh_u8*)input, len, (const xxh_u8*)secret, secretLen, seed64);
    return f_hl128(input, len, seed64, secret, secretLen);
}


/* ===   Public XXH128 API   === */

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH128_hash_t XXH3_128bits(XXH_NOESCAPE const void* input, size_t len)
{
    return XXH3_128bits_internal(input, len, 0,
                                 XXH3_kSecret, sizeof(XXH3_kSecret),
                                 XXH3_hashLong_128b_default);
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH128_hash_t
XXH3_128bits_withSecret(XXH_NOESCAPE const void* input, size_t len, XXH_NOESCAPE const void* secret, size_t secretSize)
{
    return XXH3_128bits_internal(input, len, 0,
                                 (const xxh_u8*)secret, secretSize,
                                 XXH3_hashLong_128b_withSecret);
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH128_hash_t
XXH3_128bits_withSeed(XXH_NOESCAPE const void* input, size_t len, XXH64_hash_t seed)
{
    return XXH3_128bits_internal(input, len, seed,
                                 XXH3_kSecret, sizeof(XXH3_kSecret),
                                 XXH3_hashLong_128b_withSeed);
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH128_hash_t
XXH3_128bits_withSecretandSeed(XXH_NOESCAPE const void* input, size_t len, XXH_NOESCAPE const void* secret, size_t secretSize, XXH64_hash_t seed)
{
    if (len <= XXH3_MIDSIZE_MAX)
        return XXH3_128bits_internal(input, len, seed, XXH3_kSecret, sizeof(XXH3_kSecret), NULL);
    return XXH3_hashLong_128b_withSecret(input, len, seed, secret, secretSize);
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH128_hash_t
XXH128(XXH_NOESCAPE const void* input, size_t len, XXH64_hash_t seed)
{
    return XXH3_128bits_withSeed(input, len, seed);
}


/* 128-bit utility functions */

/* return : 1 is equal, 0 if different */
/*! @ingroup XXH3_family */
XXH_PUBLIC_API int XXH128_isEqual(XXH128_hash_t h1, XXH128_hash_t h2)
{
    /* note : XXH128_hash_t is compact, it has no padding byte */
    return !(XXH_memcmp(&h1, &h2, sizeof(h1)));
}

/* This prototype is compatible with stdlib's qsort().
 * @return : >0 if *h128_1  > *h128_2
 *           <0 if *h128_1  < *h128_2
 *           =0 if *h128_1 == *h128_2  */
/*! @ingroup XXH3_family */
XXH_PUBLIC_API int XXH128_cmp(XXH_NOESCAPE const void* h128_1, XXH_NOESCAPE const void* h128_2)
{
    XXH128_hash_t const h1 = *(const XXH128_hash_t*)h128_1;
    XXH128_hash_t const h2 = *(const XXH128_hash_t*)h128_2;
    int const hcmp = (h1.high64 > h2.high64) - (h2.high64 > h1.high64);
    /* note : bets that, in most cases, hash values are different */
    if (hcmp) return hcmp;
    return (h1.low64 > h2.low64) - (h2.low64 > h1.low64);
}


/*======   Canonical representation   ======*/
/*! @ingroup XXH3_family */
XXH_PUBLIC_API void
XXH128_canonicalFromHash(XXH_NOESCAPE XXH128_canonical_t* dst, XXH128_hash_t hash)
{
    XXH_STATIC_ASSERT(sizeof(XXH128_canonical_t) == sizeof(XXH128_hash_t));
    if (XXH_CPU_LITTLE_ENDIAN) {
        hash.high64 = XXH_swap64(hash.high64);
        hash.low64  = XXH_swap64(hash.low64);
    }
    XXH_memcpy(dst, &hash.high64, sizeof(hash.high64));
    XXH_memcpy((char*)dst + sizeof(hash.high64), &hash.low64, sizeof(hash.low64));
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH128_hash_t
XXH128_hashFromCanonical(XXH_NOESCAPE const XXH128_canonical_t* src)
{
    XXH128_hash_t h;
    h.high64 = XXH_readBE64(src);
    h.low64  = XXH_readBE64(src->digest + 8);
    return h;
}



/* ==========================================
 * Secret generators
 * ==========================================
 */
#define XXH_MIN(x, y) (((x) > (y)) ? (y) : (x))

XXH_FORCE_INLINE void XXH3_combine16(void* dst, XXH128_hash_t h128)
{
    XXH_writeLE64( dst, XXH_readLE64(dst) ^ h128.low64 );
    XXH_writeLE64( (char*)dst+8, XXH_readLE64((char*)dst+8) ^ h128.high64 );
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH_errorcode
XXH3_generateSecret(XXH_NOESCAPE void* secretBuffer, size_t secretSize, XXH_NOESCAPE const void* customSeed, size_t customSeedSize)
{
#if (XXH_DEBUGLEVEL >= 1)
    XXH_ASSERT(secretBuffer != NULL);
    XXH_ASSERT(secretSize >= XXH3_SECRET_SIZE_MIN);
#else
    /* production mode, assert() are disabled */
    if (secretBuffer == NULL) return XXH_ERROR;
    if (secretSize < XXH3_SECRET_SIZE_MIN) return XXH_ERROR;
#endif

    if (customSeedSize == 0) {
        customSeed = XXH3_kSecret;
        customSeedSize = XXH_SECRET_DEFAULT_SIZE;
    }
#if (XXH_DEBUGLEVEL >= 1)
    XXH_ASSERT(customSeed != NULL);
#else
    if (customSeed == NULL) return XXH_ERROR;
#endif

    /* Fill secretBuffer with a copy of customSeed - repeat as needed */
    {   size_t pos = 0;
        while (pos < secretSize) {
            size_t const toCopy = XXH_MIN((secretSize - pos), customSeedSize);
            XXH_memcpy((char*)secretBuffer + pos, customSeed, toCopy);
            pos += toCopy;
    }   }

    {   size_t const nbSeg16 = secretSize / 16;
        size_t n;
        XXH128_canonical_t scrambler;
        XXH128_canonicalFromHash(&scrambler, XXH128(customSeed, customSeedSize, 0));
        for (n=0; n<nbSeg16; n++) {
            XXH128_hash_t const h128 = XXH128(&scrambler, sizeof(scrambler), n);
            XXH3_combine16((char*)secretBuffer + n*16, h128);
        }
        /* last segment */
        XXH3_combine16((char*)secretBuffer + secretSize - 16, XXH128_hashFromCanonical(&scrambler));
    }
    return XXH_OK;
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API void
XXH3_generateSecret_fromSeed(XXH_NOESCAPE void* secretBuffer, XXH64_hash_t seed)
{
    XXH_ALIGN(XXH_SEC_ALIGN) xxh_u8 secret[XXH_SECRET_DEFAULT_SIZE];
    XXH3_initCustomSecret(secret, seed);
    XXH_ASSERT(secretBuffer != NULL);
    XXH_memcpy(secretBuffer, secret, XXH_SECRET_DEFAULT_SIZE);
}



#include "xxh3_stream_impl.h"


/* Pop our optimization override from above */
#if XXH_VECTOR == XXH_AVX2 /* AVX2 */ \
  && defined(__GNUC__) && !defined(__clang__) /* GCC, not Clang */ \
  && defined(__OPTIMIZE__) && XXH_SIZE_OPT <= 0 /* respect -O0 and -Os */
#  pragma GCC pop_options
#endif

#endif  /* XXH_NO_XXH3 */
