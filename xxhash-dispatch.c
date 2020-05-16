/*
 * xxHash - Extremely Fast Hash algorithm
 * Copyright (C) 2020 Yann Collet
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
 */


/*
 * Dispatcher code for XXH3 on x86-based targets.
 *
 * The design is as follows.
 *
 * 1. In the XXH3 implementation, instead of jumping directly to the main loop,
 *    we jump to the global variable XXH3_dispatch_table, which has pointers
 *    to the different variants of the main loop, similar to how C++ vtables
 *    work. These functions are marked with XXH3_DISPATCH_FUNC.
 *   - This method avoids dispatching with short inputs on the single shot
 *     variant; most of the time, the hashLong code will never run, and the
 *     overhead is not worth it.
 *   - Since the state management overhead make the dispatch overhead less
 *     significant, and because the dispatcher logic is less trivial to break
 *     off from, we always dispatch on XXH3_update. We don't dispatch on
 *     XXH3_digest, since the accumulate loop is only called a few times.
 * 2. We compile some dummy files which will include xxhash.h with the correct
 *    flags. These will emit one symbol, called XXH3_dispatch_table_<number>,
 *    with <number> corresponding to the XXH_VECTOR value.
 *
 * The bulk of the xxHash implementation is found here by default, but it can
 * also be used with XXH_INLINE_ALL, where it will instead only emit the
 * dispatching code.
 */
#if !(defined(__x86_64__) || defined(__i386__) || defined(_M_IX86) || defined(_M_X64))
#  error "Dispatching is currently only supported on x86 and x86_64."
#endif

#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#ifndef XXH_DISPATCH
#  define XXH_DISPATCH
#endif
#define XXH_VECTOR XXH_SCALAR
#define XXHASH_DISPATCH_C

/*
 * This will, by default, be the host for xxHash's symbols. XXH_INLINE_ALL will
 * override this.
 */
#include "xxhash.h"

#ifdef XXH_DISPATCH_DEBUG
/* debug logging */
#  include <stdio.h>
#  define XXH_debugPrint(str) fprintf(stderr, "DEBUG: xxHash dispatch: %s\n", str)
#else
#  define XXH_debugPrint(str) ((void)0)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Declare the tables.
 *
 * Note that there is no harm in declaring a nonexistent table, so we declare
 * them all.
 */
extern const XXH3_dispatch_table_t *const XXH3_DISPATCH_MANGLE(XXH_SCALAR),
                                   *const XXH3_DISPATCH_MANGLE(XXH_SSE2),
                                   *const XXH3_DISPATCH_MANGLE(XXH_AVX2),
                                   *const XXH3_DISPATCH_MANGLE(XXH_AVX512);

const XXH3_dispatch_table_t* XXH3_getDispatchTable(void);

#ifdef XXH_DISPATCH_DEBUG
/* Public function on debug mode (needs a manually declared prototype) */
const XXH3_dispatch_table_t* XXH3_setDispatchTable(xxh_u32 dispatch_table_id);
#endif

#ifdef __cplusplus
}
#endif

/*
 * Modified version of Intel's guide
 * https://software.intel.com/en-us/articles/how-to-detect-new-instruction-support-in-the-4th-generation-intel-core-processor-family
 */
#if defined(_MSC_VER)
# include <intrin.h>
#endif

/*
 * Support both AT&T and Intel dialects
 *
 * GCC doesn't convert AT&T syntax to Intel syntax, and will error out if
 * compiled with -masm=intel. Instead, it supports dialect switching with
 * curly braces: { AT&T syntax | Intel syntax }
 *
 * Clang's integrated assembler automatically converts AT&T syntax to Intel if
 * needed, making the dialect switching useless (it isn't even supported).
 *
 * Note: Comments are written in the inline assembly itself.
 */
#ifdef __clang__
#  define I_ATT(intel, att) att "\n\t"
#else
#  define I_ATT(intel, att) "{" att "|" intel "}\n\t"
#endif


static void XXH_cpuid(xxh_u32 eax, xxh_u32 ecx, xxh_u32* abcd)
{
#if defined(_MSC_VER)
    __cpuidex(abcd, eax, ecx);
#else
    xxh_u32 ebx, edx;
# if defined(__i386__) && defined(__PIC__)
    __asm__(
        "# Call CPUID\n\t"
        "#\n\t"
        "# On 32-bit x86 with PIC enabled, we are not allowed to overwrite\n\t"
        "# EBX, so we use EDI instead.\n\t"
        I_ATT("mov     edi, ebx",   "movl    %%ebx, %%edi")
        I_ATT("cpuid",              "cpuid"               )
        I_ATT("xchg    edi, ebx",   "xchgl   %%ebx, %%edi")
        : "=D" (ebx),
# else
    __asm__(
        "# Call CPUID\n\t"
        I_ATT("cpuid",              "cpuid")
        : "=b" (ebx),
# endif
              "+a" (eax), "+c" (ecx), "=d" (edx));
    abcd[0] = eax;
    abcd[1] = ebx;
    abcd[2] = ecx;
    abcd[3] = edx;
#endif
}

#if defined(XXH_DISPATCH_AVX2) || defined(XXH_DISPATCH_AVX512)
/*
 * While the CPU may support AVX2, the operating system might not properly save
 * the full YMM/ZMM registers.
 *
 * xgetbv is used for detecting this: Any compliant operating system will define
 * a set of flags in the xcr0 register indicating how it saves the AVX registers.
 *
 * You can manually disable this flag on Windows by running, as admin:
 *
 *   bcdedit.exe /set xsavedisable 1
 *
 * and rebooting. Run the same command with 0 to re-enable it.
 */
static xxh_u64 XXH_xgetbv(void)
{
#if defined(_MSC_VER)
    return _xgetbv(0);  /* min VS2010 SP1 compiler is required */
#else
    xxh_u32 xcr0_lo, xcr0_hi;
    __asm__(
        "# Call XGETBV\n\t"
        "#\n\t"
        "# Older assemblers (e.g. macOS's ancient GAS version) don't support\n\t"
        "# the XGETBV opcode, so we encode it by hand instead.\n\t"
        "# See <https://github.com/asmjit/asmjit/issues/78> for details.\n\t"
        ".byte   0x0f, 0x01, 0xd0\n\t"
       : "=a" (xcr0_lo), "=d" (xcr0_hi) : "c" (0));
    return xcr0_lo | ((xxh_u64)xcr0_hi << 32);
#endif
}
#endif

#define SSE2_CPUID_MASK (1 << 26)
#define OSXSAVE_CPUID_MASK ((1 << 26) | (1 << 27))
#define AVX2_CPUID_MASK (1 << 5)
#define AVX2_XGETBV_MASK ((1 << 2) | (1 << 1))
#define AVX512F_CPUID_MASK (1 << 16)
#define AVX512F_XGETBV_MASK ((7 << 5) | (1 << 2) | (1 << 1))

/* Returns the best XXH3 implementation */
static xxh_u32 XXH_featureTest(void)
{
    xxh_u32 abcd[4];
    xxh_u32 max_leaves;
    xxh_u32 best = XXH_SCALAR;
#if defined(XXH_DISPATCH_AVX2) || defined(XXH_DISPATCH_AVX512)
    xxh_u64 xgetbv_val;
#endif
#if defined(__GNUC__) && defined(__i386__)
    xxh_u32 cpuid_supported;
    __asm__(
        "# For the sake of ruthless backwards compatibility, check if CPUID\n\t"
        "# is supported in the EFLAGS on i386.\n\t"
        "# This is not necessary on x86_64 - CPUID is mandatory.\n\t"
        "#   The ID flag (bit 21) in the EFLAGS register indicates support\n\t"
        "#   for the CPUID instruction. If a software procedure can set and\n\t"
        "#   clear this flag, the processor executing the procedure supports\n\t"
        "#   the CPUID instruction.\n\t"
        "#   <https://c9x.me/x86/html/file_module_x86_id_45.html>\n\t"
        "#\n\t"
        "# Routine is from <https://wiki.osdev.org/CPUID>.\n\t"

        "# Save EFLAGS\n\t"
        I_ATT("pushfd",                           "pushfl"                    )
        "# Store EFLAGS\n\t"
        I_ATT("pushfd",                           "pushfl"                    )
        "# Invert the ID bit in stored EFLAGS\n\t"
        I_ATT("xor     dword ptr[esp], 0x200000", "xorl    $0x200000, (%%esp)")
        "# Load stored EFLAGS (with ID bit inverted)\n\t"
        I_ATT("popfd",                            "popfl"                     )
        "# Store EFLAGS again (ID bit may or not be inverted)\n\t"
        I_ATT("pushfd",                           "pushfl"                    )
        "# eax = modified EFLAGS (ID bit may or may not be inverted)\n\t"
        I_ATT("pop     eax",                      "popl    %%eax"             )
        "# eax = whichever bits were changed\n\t"
        I_ATT("xor     eax, dword ptr[esp]",      "xorl    (%%esp), %%eax"    )
        "# Restore original EFLAGS\n\t"
        I_ATT("popfd",                            "popfl"                     )
        "# eax = zero if ID bit can't be changed, else non-zero\n\t"
        I_ATT("and     eax, 0x200000",            "andl    $0x200000, %%eax"  )
        : "=a" (cpuid_supported) :: "cc");

    if (XXH_unlikely(!cpuid_supported)) {
        XXH_debugPrint("CPUID support is not detected!");
        return best;
    }

#endif
    /* Check how many CPUID pages we have */
    XXH_cpuid(0, 0, abcd);
    max_leaves = abcd[0];

    /* Shouldn't happen on hardware, but happens on some QEMU configs. */
    if (XXH_unlikely(max_leaves == 0)) {
        XXH_debugPrint("Max CPUID leaves == 0!");
        return best;
    }

    /* Check for SSE2, OSXSAVE and xgetbv */
    XXH_cpuid(1, 0, abcd);

    /*
     * Test for SSE2. The check is redundant on x86_64, but it doesn't hurt.
     */
    if (XXH_unlikely((abcd[3] & SSE2_CPUID_MASK) != SSE2_CPUID_MASK))
        return best;

    XXH_debugPrint("SSE2 support detected.");

    best = XXH_SSE2;
#if defined(XXH_DISPATCH_AVX2) || defined(XXH_DISPATCH_AVX512)
    /* Make sure we have enough leaves */
    if (XXH_unlikely(max_leaves < 7))
        return best;

    /* Test for OSXSAVE and XGETBV */
    if ((abcd[2] & OSXSAVE_CPUID_MASK) != OSXSAVE_CPUID_MASK)
        return best;

    /* CPUID check for AVX features */
    XXH_cpuid(7, 0, abcd);

    xgetbv_val = XXH_xgetbv();
#if defined(XXH_DISPATCH_AVX2)
    /* Validate that AVX2 is supported by the CPU */
    if ((abcd[1] & AVX2_CPUID_MASK) != AVX2_CPUID_MASK)
        return best;

    /* Validate that the OS supports YMM registers */
    if ((xgetbv_val & AVX2_XGETBV_MASK) != AVX2_XGETBV_MASK) {
        XXH_debugPrint("AVX2 supported by the CPU, but not the OS.");
        return best;
    }

    /* AVX2 supported */
    XXH_debugPrint("AVX2 support detected.");
    best = XXH_AVX2;
#endif
#if defined(XXH_DISPATCH_AVX512)
    /* Validate that AVX512F is supported by the CPU */
    if ((abcd[1] & AVX512F_CPUID_MASK) != AVX512F_CPUID_MASK)
        return best;

    /* Validate that the OS supports ZMM registers */
    if ((xgetbv_val & AVX512F_XGETBV_MASK) != AVX512F_XGETBV_MASK) {
        XXH_debugPrint("AVX512F supported by the CPU, but not the OS.");
        return best;
    }

    /* AVX512F supported */
    XXH_debugPrint("AVX512F support detected.");
    best = XXH_AVX512;
#endif
#endif
    return best;
}

/* Cache variable */
static const XXH3_dispatch_table_t *s_xxh3_dispatch_table_cache = NULL;

/*
 * Sets the dispatch table and returns it. Call with the XXH_VECTOR value.
 *
 * With XXH_DISPATCH_DEBUG, this is a public function which can be used to
 * manually override the dispatch table.
 */
#ifndef XXH_DISPATCH_DEBUG
static
#endif
const XXH3_dispatch_table_t*
XXH3_setDispatchTable(xxh_u32 dispatch_table_id)
{
    switch (dispatch_table_id) {
#define XXH_DISPATCH_CASE(id)                                                 \
    case id:                                                                  \
        XXH_debugPrint("Setting dispatch table to " #id ".");                 \
        /* Assign and return in one line, avoid reloads */                    \
        return (s_xxh3_dispatch_table_cache = XXH3_DISPATCH_MANGLE(id))

    XXH_DISPATCH_CASE(XXH_SSE2);
#if defined(XXH_DISPATCH_AVX2)
    XXH_DISPATCH_CASE(XXH_AVX2);
#endif
#if defined(XXH_DISPATCH_AVX512)
    XXH_DISPATCH_CASE(XXH_AVX512);
#endif
    default:
        XXH_debugPrint("Unknown dispatch ID!");
        /* FALLTHROUGH */
    XXH_DISPATCH_CASE(XXH_SCALAR);
#undef XXH_DISPATCH_CASE
    }
}

/*
 * Public function.
 *
 * Returns a pointer to the best dispatch table for this target.
 *
 * This is cached just like the function pointer cache in XXH3's dispatch
 * macros.
 */
const XXH3_dispatch_table_t*
XXH3_getDispatchTable(void)
{
    if (XXH_unlikely(s_xxh3_dispatch_table_cache == NULL)) {
       return XXH3_setDispatchTable(XXH_featureTest());
    }
    return s_xxh3_dispatch_table_cache;
}

#undef XXH_debugPrint

