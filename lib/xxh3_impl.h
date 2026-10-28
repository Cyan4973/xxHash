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
   || (defined(__wasm_simd128__) && XXH_HAS_INCLUDE(<arm_neon.h>) /* WASM SIMD128 via SIMDe */ \
       && (defined(__EMSCRIPTEN__) || (defined(XXH_VECTOR) && XXH_VECTOR == XXH_NEON)))
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
     || (defined(__wasm_simd128__) && XXH_HAS_INCLUDE(<arm_neon.h>) /* wasm simd128 via SIMDe */ \
         && defined(__EMSCRIPTEN__)) \
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


/*
 * A limited variant of xxhash.h may only contain the routines of some instruction sets:
 * those selected by XXH_VECTOR, or by the x86 dispatcher, must be available.
 */
#if ((XXH_VECTOR == XXH_SSE2 || defined(XXH_X86DISPATCH)) && !defined(XXH3_ROUTINES_SSE2)) \
 || ((XXH_VECTOR == XXH_AVX2 || (defined(XXH_DISPATCH_AVX2) && XXH_DISPATCH_AVX2 != 0)) && !defined(XXH3_ROUTINES_AVX2)) \
 || ((XXH_VECTOR == XXH_AVX512 || (defined(XXH_DISPATCH_AVX512) && XXH_DISPATCH_AVX512 != 0)) && !defined(XXH3_ROUTINES_AVX512)) \
 || (XXH_VECTOR == XXH_NEON && !defined(XXH3_ROUTINES_NEON)) \
 || (XXH_VECTOR == XXH_SVE && !defined(XXH3_ROUTINES_SVE)) \
 || (XXH_VECTOR == XXH_VSX && !defined(XXH3_ROUTINES_VSX)) \
 || (XXH_VECTOR == XXH_LSX && !defined(XXH3_ROUTINES_LSX)) \
 || (XXH_VECTOR == XXH_LASX && !defined(XXH3_ROUTINES_LASX)) \
 || (XXH_VECTOR == XXH_RVV && !defined(XXH3_ROUTINES_RVV))
#  error "XXH_VECTOR or the x86 dispatcher selects an instruction set whose routines are not in this variant of xxhash.h"
#endif

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


#include "xxh3_64bits_impl.h"
#include "xxh3_128bits_impl.h"


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
