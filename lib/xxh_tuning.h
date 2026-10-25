/*
 * xxHash - Extremely Fast Hash algorithm
 * Tuning parameters common to all families
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

/* *************************************
*  Tuning parameters
***************************************/

/*!
 * @defgroup tuning Tuning parameters
 * @{
 *
 * Various macros to control xxHash's behavior.
 */
#ifdef XXH_DOXYGEN
/*!
 * @brief Define this to disable 64-bit code.
 *
 * Useful if only using the @ref XXH32_family and you have a strict C90 compiler.
 */
#  define XXH_NO_LONG_LONG
#  undef XXH_NO_LONG_LONG /* don't actually */
/*!
 * @brief Define this to disable the @ref XXH32_family.
 *
 * @ref XXH3_family depends on XXH32, so it is disabled too.
 */
#  define XXH_NO_XXH32
#  undef XXH_NO_XXH32 /* don't actually */
/*!
 * @brief Controls how unaligned memory is accessed.
 *
 * By default, access to unaligned memory is controlled by `memcpy()`, which is
 * safe and portable.
 *
 * Unfortunately, on some target/compiler combinations, the generated assembly
 * is sub-optimal.
 *
 * The below switch allow selection of a different access method
 * in the search for improved performance.
 *
 * @par Possible options:
 *
 *  - `XXH_FORCE_MEMORY_ACCESS=0` (default): `memcpy`
 *   @par
 *     Use `memcpy()`. Safe and portable. Note that most modern compilers will
 *     eliminate the function call and treat it as an unaligned access.
 *
 *  - `XXH_FORCE_MEMORY_ACCESS=1`: `__attribute__((aligned(1)))`
 *   @par
 *     Depends on compiler extensions and is therefore not portable.
 *     This method is safe _if_ your compiler supports it,
 *     and *generally* as fast or faster than `memcpy`.
 *
 *  - `XXH_FORCE_MEMORY_ACCESS=2`: Direct cast
 *  @par
 *     Casts directly and dereferences. This method doesn't depend on the
 *     compiler, but it violates the C standard as it directly dereferences an
 *     unaligned pointer. It can generate buggy code on targets which do not
 *     support unaligned memory accesses, but in some circumstances, it's the
 *     only known way to get the most performance.
 *
 *  - `XXH_FORCE_MEMORY_ACCESS=3`: Byteshift
 *  @par
 *     Also portable. This can generate the best code on old compilers which don't
 *     inline small `memcpy()` calls, and it might also be faster on big-endian
 *     systems which lack a native byteswap instruction. However, some compilers
 *     will emit literal byteshifts even if the target supports unaligned access.
 *
 *
 * @warning
 *   Methods 1 and 2 rely on implementation-defined behavior. Use these with
 *   care, as what works on one compiler/platform/optimization level may cause
 *   another to read garbage data or even crash.
 *
 * See https://fastcompression.blogspot.com/2015/08/accessing-unaligned-memory.html for details.
 *
 * Prefer these methods in priority order (0 > 3 > 1 > 2)
 */
#  define XXH_FORCE_MEMORY_ACCESS 0

/*!
 * @def XXH_SIZE_OPT
 * @brief Controls how much xxHash optimizes for size.
 *
 * xxHash, when compiled, tends to result in a rather large binary size. This
 * is mostly due to heavy usage to forced inlining and constant folding of the
 * @ref XXH3_family to increase performance.
 *
 * However, some developers prefer size over speed. This option can
 * significantly reduce the size of the generated code. When using the `-Os`
 * or `-Oz` options on GCC or Clang, this is defined to 1 by default,
 * otherwise it is defined to 0.
 *
 * Most of these size optimizations can be controlled manually.
 *
 * This is a number from 0-2.
 *  - `XXH_SIZE_OPT` == 0: Default. xxHash makes no size optimizations. Speed
 *    comes first.
 *  - `XXH_SIZE_OPT` == 1: Default for `-Os` and `-Oz`. xxHash is more
 *    conservative and disables hacks that increase code size. It implies the
 *    options @ref XXH_NO_INLINE_HINTS == 1, @ref XXH_FORCE_ALIGN_CHECK == 0,
 *    and @ref XXH3_NEON_LANES == 8 if they are not already defined.
 *  - `XXH_SIZE_OPT` == 2: xxHash tries to make itself as small as possible.
 *    Performance may cry. For example, the single shot functions just use the
 *    streaming API.
 */
#  define XXH_SIZE_OPT 0

/*!
 * @def XXH_FORCE_ALIGN_CHECK
 * @brief If defined to non-zero, adds a special path for aligned inputs (XXH32()
 * and XXH64() only).
 *
 * This is an important performance trick for architectures without decent
 * unaligned memory access performance.
 *
 * It checks for input alignment, and when conditions are met, uses a "fast
 * path" employing direct 32-bit/64-bit reads, resulting in _dramatically
 * faster_ read speed.
 *
 * The check costs one initial branch per hash, which is generally negligible,
 * but not zero.
 *
 * Moreover, it's not useful to generate an additional code path if memory
 * access uses the same instruction for both aligned and unaligned
 * addresses (e.g. x86 and aarch64).
 *
 * In these cases, the alignment check can be removed by setting this macro to 0.
 * Then the code will always use unaligned memory access.
 * Align check is automatically disabled on x86, x64, ARM64, and some ARM chips
 * which are platforms known to offer good unaligned memory accesses performance.
 *
 * It is also disabled by default when @ref XXH_SIZE_OPT >= 1.
 *
 * This option does not affect XXH3 (only XXH32 and XXH64).
 */
#  define XXH_FORCE_ALIGN_CHECK 0

/*!
 * @def XXH_NO_INLINE_HINTS
 * @brief When non-zero, sets all functions to `static`.
 *
 * By default, xxHash tries to force the compiler to inline almost all internal
 * functions.
 *
 * This can usually improve performance due to reduced jumping and improved
 * constant folding, but significantly increases the size of the binary which
 * might not be favorable.
 *
 * Additionally, sometimes the forced inlining can be detrimental to performance,
 * depending on the architecture.
 *
 * XXH_NO_INLINE_HINTS marks all internal functions as static, giving the
 * compiler full control on whether to inline or not.
 *
 * When not optimizing (-O0), using `-fno-inline` with GCC or Clang, or if
 * @ref XXH_SIZE_OPT >= 1, this will automatically be defined.
 */
#  define XXH_NO_INLINE_HINTS 0

/*!
 * @internal
 * @brief Redefines old internal names.
 *
 * For compatibility with code that uses xxHash's internals before the names
 * were changed to improve namespacing. There is no other reason to use this.
 */
#  define XXH_OLD_NAMES
#  undef XXH_OLD_NAMES /* don't actually use, it is ugly. */

/*!
 * @def XXH_NO_STREAM
 * @brief Disables the streaming API.
 *
 * When xxHash is not inlined and the streaming functions are not used, disabling
 * the streaming functions can improve code size significantly, especially with
 * the @ref XXH3_family which tends to make constant folded copies of itself.
 */
#  define XXH_NO_STREAM
#  undef XXH_NO_STREAM /* don't actually */
#endif /* XXH_DOXYGEN */
/*!
 * @}
 */

#ifndef XXH_FORCE_MEMORY_ACCESS   /* can be defined externally, on command line for example */
   /* prefer __packed__ structures (method 1) for GCC
    * < ARMv7 with unaligned access (e.g. Raspbian armhf) still uses byte shifting, so we use memcpy
    * which for some reason does unaligned loads. */
#  if defined(__GNUC__) && !(defined(__ARM_ARCH) && __ARM_ARCH < 7 && defined(__ARM_FEATURE_UNALIGNED))
#    define XXH_FORCE_MEMORY_ACCESS 1
#  endif
#endif

#ifndef XXH_SIZE_OPT
   /* default to 1 for -Os or -Oz */
#  if (defined(__GNUC__) || defined(__clang__)) && defined(__OPTIMIZE_SIZE__)
#    define XXH_SIZE_OPT 1
#  else
#    define XXH_SIZE_OPT 0
#  endif
#endif

#ifndef XXH_FORCE_ALIGN_CHECK  /* can be defined externally */
   /* don't check on sizeopt, x86, aarch64, or arm when unaligned access is available */
#  if XXH_SIZE_OPT >= 1 || \
      defined(__i386)  || defined(__x86_64__) || defined(__aarch64__) || defined(__ARM_FEATURE_UNALIGNED) \
   || defined(_M_IX86) || defined(_M_X64)     || defined(_M_ARM64)    || defined(_M_ARM) /* visual */
#    define XXH_FORCE_ALIGN_CHECK 0
#  else
#    define XXH_FORCE_ALIGN_CHECK 1
#  endif
#endif

#ifndef XXH_NO_INLINE_HINTS
#  if XXH_SIZE_OPT >= 1 || defined(__NO_INLINE__)  /* -O0, -fno-inline */
#    define XXH_NO_INLINE_HINTS 1
#  else
#    define XXH_NO_INLINE_HINTS 0
#  endif
#endif
