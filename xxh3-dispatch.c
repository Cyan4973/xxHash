#define XXH3_DISPATCH_C
#define XXH_INLINE_ALL
#define XXH3_DISPATCH

#include "xxhash.h"

#if !(defined(__x86_64__) || defined(__i386__) || defined(_M_IX86) || defined(_M_X64))
#  error "Dispatching is currently only supported on x86 and x86_64."
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern const XXH3_dispatch_table_t *XXH3_DISPATCH_MANGLE(XXH_SCALAR),
                                   *XXH3_DISPATCH_MANGLE(XXH_AVX2),
                                   *XXH3_DISPATCH_MANGLE(XXH_SSE2),
                                   *XXH3_DISPATCH_MANGLE(XXH_AVX512);
extern const XXH3_dispatch_table_t *XXH3_dispatch_table;

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

#define SSE2_CPUID_MASK (1 << 26)
#define OSXSAVE_CPUID_MASK ((1 << 26) | (1 << 27))
#define AVX2_CPUID_MASK (1 << 5)
#define AVX2_XGETBV_MASK ((1 << 2) | (1 << 1))
#define AVX512F_CPUID_MASK (1 << 16)
#define AVX512F_XGETBV_MASK ((7 << 5) | (1 << 2) | (1 << 1))
#include <stdio.h>

/* Returns the best XXH3 implementation */
static const XXH3_dispatch_table_t *XXH3_featureTest(void)
{
    xxh_u32 abcd[4];
    xxh_u64 xgetbv_val;
    xxh_u32 max_leaves;
    const XXH3_dispatch_table_t *best = XXH3_DISPATCH_MANGLE(XXH_SCALAR);
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
        I_ATT("pushfd",                           "pushfd"                    )
        "# Store EFLAGS\n\t"
        I_ATT("pushfd",                           "pushfd"                    )
        "# Invert the ID bit in stored EFLAGS\n\t"
        I_ATT("xor     dword ptr[esp], 0x200000", "xorl    $0x200000, (%%esp)")
        "# Load stored EFLAGS (with ID bit inverted)\n\t"
        I_ATT("popfd",                            "popfd"                     )
        "# Store EFLAGS again (ID bit may or not be inverted)\n\t"
        I_ATT("pushfd",                           "pushfd"                    )
        "# eax = modified EFLAGS (ID bit may or may not be inverted)\n\t"
        I_ATT("pop     eax",                      "popl    %%eax"             )
        "# eax = whichever bits were changed\n\t"
        I_ATT("xor     eax, dword ptr[esp]",      "xorl    (%%esp), %%eax"    )
        "# Restore original EFLAGS\n\t"
        I_ATT("popfd",                            "popfd"                     )
        "# eax = zero if ID bit can't be changed, else non-zero\n\t"
        I_ATT("and     eax, 0x200000",            "andl    $0x200000, %%eax"  )
        : "=a" (cpuid_supported) :: "cc");
    if (XXH_unlikely(!cpuid_supported))
        return best;
#endif
    /* Check how many CPUID pages we have */
    XXH_cpuid(0, 0, abcd);
    max_leaves = abcd[0];

    /* Sanity check: this shouldn't happen on any hardware. */
    if (XXH_unlikely(max_leaves == 0))
        return best;

    /* Check for SSE2, OSXSAVE and xgetbv */
    XXH_cpuid(1, 0, abcd);

    /* Test for SSE2. Basically every x86 CPU supports it, but
     * since we are already checking  */
    if (XXH_unlikely((abcd[3] & SSE2_CPUID_MASK) != SSE2_CPUID_MASK))
        return best;
    printf("SSE2 supported\n");
    best = XXH3_DISPATCH_MANGLE(XXH_SSE2);
#if defined(XXH3_DISPATCH_AVX2) || defined(XXH3_DISPATCH_AVX512)
    /* Make sure we have enough leaves */
    if (XXH_unlikely(max_leaves < 7))
        return best;

    /* Test for OSXSAVE and XGETBV */
    if ((abcd[2] & OSXSAVE_CPUID_MASK) != OSXSAVE_CPUID_MASK)
        return best;

    /* CPUID check for AVX features */
    XXH_cpuid(7, 0, abcd);

    xgetbv_val = XXH_xgetbv();
#if defined(XXH3_DISPATCH_AVX2)
    /* Validate that the OS supports YMM registers */
    if ((xgetbv_val & AVX2_XGETBV_MASK) != AVX2_XGETBV_MASK)
        return best;

    /* Validate that AVX2 is supported by the CPU */
    if ((abcd[1] & AVX2_CPUID_MASK) != AVX2_CPUID_MASK)
        return best;
    /* AVX2 supported */
    printf("AVX2 supported\n");
    best = XXH3_DISPATCH_MANGLE(XXH_AVX2);
#endif
#if defined(XXH3_DISPATCH_AVX512)
    /* Validate that the OS supports ZMM registers */
    if ((xgetbv_val & AVX512F_XGETBV_MASK) != AVX512F_XGETBV_MASK)
        return best;

    /* Validate that AVX512F is supported by the CPU */
    if ((abcd[1] & AVX512F_CPUID_MASK) != AVX512F_CPUID_MASK)
        return best;
    /* AVX512F supported */
    printf("AVX512F supported\n");
    best = XXH3_DISPATCH_MANGLE(XXH_AVX512);
#endif
#endif
    return best;
}

/* Wrappers which will select the correct dispatch_table. */
static XXH64_hash_t
XXH3_hashLong_64b_defaultSecret_dispatch(const xxh_u8 *input, size_t length)
{
    const XXH3_dispatch_table_t *dispatch_table = XXH3_featureTest();
    XXH3_dispatch_table = dispatch_table;
    return dispatch_table->hashLong_64b_defaultSecret(input, length);
}
static XXH64_hash_t
XXH3_hashLong_64b_withSeed_dispatch(const xxh_u8 *input, size_t length, XXH64_hash_t seed)
{
    const XXH3_dispatch_table_t *dispatch_table = XXH3_featureTest();
    XXH3_dispatch_table = dispatch_table;
    return dispatch_table->hashLong_64b_withSeed(input, length, seed);
}
static XXH64_hash_t
XXH3_hashLong_64b_withSecret_dispatch(const xxh_u8 *input, size_t length, const xxh_u8 *secret, size_t secretSize)
{
    const XXH3_dispatch_table_t *dispatch_table = XXH3_featureTest();
    XXH3_dispatch_table = dispatch_table;
    return dispatch_table->hashLong_64b_withSecret(input, length, secret, secretSize);
}
static XXH_errorcode
XXH3_update_64bits_internal_dispatch(XXH3_state_t* state, const xxh_u8* input, size_t len)
{
    const XXH3_dispatch_table_t *dispatch_table = XXH3_featureTest();
    XXH3_dispatch_table = dispatch_table;
    return dispatch_table->update_64bits_internal(state, input, len);
}

static XXH128_hash_t
XXH3_hashLong_128b_defaultSecret_dispatch(const xxh_u8 *input, size_t length)
{
    const XXH3_dispatch_table_t *dispatch_table = XXH3_featureTest();
    XXH3_dispatch_table = dispatch_table;
    return dispatch_table->hashLong_128b_defaultSecret(input, length);
}
static XXH128_hash_t
XXH3_hashLong_128b_withSeed_dispatch(const xxh_u8 *input, size_t length, XXH64_hash_t seed)
{
    const XXH3_dispatch_table_t *dispatch_table = XXH3_featureTest();
    XXH3_dispatch_table = dispatch_table;
    return dispatch_table->hashLong_128b_withSeed(input, length, seed);
}
static XXH128_hash_t
XXH3_hashLong_128b_withSecret_dispatch(const xxh_u8 *input, size_t length, const xxh_u8 *secret, size_t secretSize)
{
    const XXH3_dispatch_table_t *dispatch_table = XXH3_featureTest();
    XXH3_dispatch_table = dispatch_table;
    return dispatch_table->hashLong_128b_withSecret(input, length, secret, secretSize);
}
static XXH_errorcode
XXH3_update_128bits_internal_dispatch(XXH3_state_t* state, const xxh_u8* input, size_t len)
{
    const XXH3_dispatch_table_t *dispatch_table = XXH3_featureTest();
    XXH3_dispatch_table = dispatch_table;
    return dispatch_table->update_128bits_internal(state, input, len);
}

static const XXH3_dispatch_table_t kDispatchDispatcher = {
    &XXH3_hashLong_64b_defaultSecret_dispatch,
    &XXH3_hashLong_64b_withSeed_dispatch,
    &XXH3_hashLong_64b_withSecret_dispatch,
    &XXH3_update_64bits_internal_dispatch,
    &XXH3_hashLong_128b_defaultSecret_dispatch,
    &XXH3_hashLong_128b_withSeed_dispatch,
    &XXH3_hashLong_128b_withSecret_dispatch,
    &XXH3_update_64bits_internal_dispatch
};
const XXH3_dispatch_table_t *XXH3_dispatch_table = &kDispatchDispatcher;

