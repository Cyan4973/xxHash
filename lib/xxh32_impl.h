/*
 * xxHash - Extremely Fast Hash algorithm
 * XXH32 implementation
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

#ifndef XXH_NO_XXH32

/* *******************************************************************
*  32-bit hash functions
*********************************************************************/
/*!
 * @}
 * @defgroup XXH32_impl XXH32 implementation
 * @ingroup impl
 *
 * Details on the XXH32 implementation.
 * @{
 */
 /* #define instead of static const, to be used as initializers */
#define XXH_PRIME32_1  0x9E3779B1U  /*!< 0b10011110001101110111100110110001 */
#define XXH_PRIME32_2  0x85EBCA77U  /*!< 0b10000101111010111100101001110111 */
#define XXH_PRIME32_3  0xC2B2AE3DU  /*!< 0b11000010101100101010111000111101 */
#define XXH_PRIME32_4  0x27D4EB2FU  /*!< 0b00100111110101001110101100101111 */
#define XXH_PRIME32_5  0x165667B1U  /*!< 0b00010110010101100110011110110001 */

#ifdef XXH_OLD_NAMES
#  define PRIME32_1 XXH_PRIME32_1
#  define PRIME32_2 XXH_PRIME32_2
#  define PRIME32_3 XXH_PRIME32_3
#  define PRIME32_4 XXH_PRIME32_4
#  define PRIME32_5 XXH_PRIME32_5
#endif

/*!
 * @ingroup tuning
 * @def XXH32_ENDJMP
 * @brief Whether to use a jump for `XXH32_finalize`.
 *
 * For performance, `XXH32_finalize` uses multiple branches in the finalizer.
 * This is generally preferable for performance,
 * but depending on exact architecture, a jmp may be preferable.
 *
 * This setting is only possibly making a difference for very small inputs.
 */
#ifndef XXH32_ENDJMP
/* generally preferable for performance */
#  define XXH32_ENDJMP 0
#endif

/*!
 * @internal
 * @brief Whether XXH32 code uses compiler fences against autovectorization.
 *
 * Autovectorization is detrimental to XXH32 on these targets,
 * see XXH32_round() and XXH32_mergeAccs().
 */
#if (defined(__SSE2__) || defined(__aarch64__) || defined(__wasm_simd128__)) && !defined(XXH_ENABLE_AUTOVECTORIZE)
#  define XXH32_PREVENT_AUTOVEC 1
#else
#  define XXH32_PREVENT_AUTOVEC 0
#endif

/*!
 * @internal
 * @brief Normal stripe processing routine.
 *
 * This shuffles the bits so that any bit from @p input impacts several bits in
 * @p acc.
 *
 * @param acc The accumulator lane.
 * @param input The stripe of input to mix.
 * @return The mixed accumulator lane.
 */
static xxh_u32 XXH32_round(xxh_u32 acc, xxh_u32 input)
{
    acc += input * XXH_PRIME32_2;
    acc  = XXH_rotl32(acc, 13);
    acc *= XXH_PRIME32_1;
#if XXH32_PREVENT_AUTOVEC
    /*
     * UGLY HACK:
     * A compiler fence is used to prevent GCC and Clang from
     * autovectorizing the XXH32 loop (pragmas and attributes don't work for some
     * reason) without globally disabling SSE4.1.
     *
     * The reason we want to avoid vectorization is because despite working on
     * 4 integers at a time, there are multiple factors slowing XXH32 down on
     * SSE4:
     * - There's a ridiculous amount of lag from pmulld (10 cycles of latency on
     *   newer chips!) making it slightly slower to multiply four integers at
     *   once compared to four integers independently. Even when pmulld was
     *   fastest, Sandy/Ivy Bridge, it is still not worth it to go into SSE
     *   just to multiply unless doing a long operation.
     *
     * - Four instructions are required to rotate,
     *      movqda tmp,  v // not required with VEX encoding
     *      pslld  tmp, 13 // tmp <<= 13
     *      psrld  v,   19 // x >>= 19
     *      por    v,  tmp // x |= tmp
     *   compared to one for scalar:
     *      roll   v, 13    // reliably fast across the board
     *      shldl  v, v, 13 // Sandy Bridge and later prefer this for some reason
     *
     * - Instruction level parallelism is actually more beneficial here because
     *   the SIMD actually serializes this operation: While v1 is rotating, v2
     *   can load data, while v3 can multiply. SSE forces them to operate
     *   together.
     *
     * This is also enabled on baseline SSE2 (no SSE4.1), where it matters even
     * more: without pmulld, each 32-bit vector multiply is emulated with
     * pmuludq + shuffles, or with long shift/add chains for constants.
     * GCC 16 does this on XXH32_update(), whose accumulators live in memory
     * (state->acc), making XXH32 streaming ~6x slower than single-shot.
     *
     * This is also enabled on AArch64, as Clang is *very aggressive* in vectorizing
     * the loop. NEON is only faster on the A53, and with the newer cores, it is less
     * than half the speed.
     *
     * Additionally, this is used on WASM SIMD128 because it JITs to the same
     * SIMD instructions and has the same issue.
     */
    XXH_COMPILER_GUARD(acc);
#endif
    return acc;
}

/*!
 * @internal
 * @brief Mixes all bits to finalize the hash.
 *
 * The final mix ensures that all input bits have a chance to impact any bit in
 * the output digest, resulting in an unbiased distribution.
 *
 * @param hash The hash to avalanche.
 * @return The avalanched hash.
 */
static xxh_u32 XXH32_avalanche(xxh_u32 hash)
{
    hash ^= hash >> 15;
    hash *= XXH_PRIME32_2;
    hash ^= hash >> 13;
    hash *= XXH_PRIME32_3;
    hash ^= hash >> 16;
    return hash;
}

/*!
 * @internal
 * @brief Sets up the initial accumulator state for XXH32().
 */
XXH_FORCE_INLINE void
XXH32_initAccs(xxh_u32 *acc, xxh_u32 seed)
{
    XXH_ASSERT(acc != NULL);
    acc[0] = seed + XXH_PRIME32_1 + XXH_PRIME32_2;
    acc[1] = seed + XXH_PRIME32_2;
    acc[2] = seed + 0;
    acc[3] = seed - XXH_PRIME32_1;
}

/*!
 * @internal
 * @brief Consumes a block of data for XXH32().
 *
 * @return the end input pointer.
 */
XXH_FORCE_INLINE const xxh_u8 *
XXH32_consumeLong(
    xxh_u32 *XXH_RESTRICT acc,
    xxh_u8 const *XXH_RESTRICT input,
    size_t len,
    XXH_alignment align
)
{
    const xxh_u8* const bEnd = input + len;
    const xxh_u8* const limit = bEnd - 15;
    XXH_ASSERT(acc != NULL);
    XXH_ASSERT(input != NULL);
    XXH_ASSERT(len >= 16);
    /*
     * Accumulators are copied into locals, so that they stay in registers
     * for the whole loop. When the loop works directly on @p acc,
     * which is state->acc in streaming mode, Clang stores them back
     * to memory on every iteration (XXH_COMPILER_GUARD prevents promotion),
     * costing ~10% on XXH32_update() and XXH64_update().
     */
    {   xxh_u32 a0 = acc[0], a1 = acc[1], a2 = acc[2], a3 = acc[3];
        do {
            a0 = XXH32_round(a0, XXH_get32bits(input)); input += 4;
            a1 = XXH32_round(a1, XXH_get32bits(input)); input += 4;
            a2 = XXH32_round(a2, XXH_get32bits(input)); input += 4;
            a3 = XXH32_round(a3, XXH_get32bits(input)); input += 4;
        } while (input < limit);
        acc[0] = a0; acc[1] = a1; acc[2] = a2; acc[3] = a3;
    }

    return input;
}

/*!
 * @internal
 * @brief Merges the accumulator lanes together for XXH32()
 */
XXH_FORCE_INLINE XXH_PUREF xxh_u32
XXH32_mergeAccs(const xxh_u32 *acc)
{
    XXH_ASSERT(acc != NULL);
    {   xxh_u32 a0 = acc[0], a1 = acc[1], a2 = acc[2], a3 = acc[3];
#if XXH32_PREVENT_AUTOVEC
        /*
         * Prevent vectorization of this merge (GCC and Clang do it with AVX2).
         * In XXH32_digest(), it results in a single 16-byte load of state->acc,
         * which was just written by 4-byte stores: store-to-load forwarding fails,
         * making XXH32 streaming of small inputs up to ~2x slower.
         */
        XXH_COMPILER_GUARD(a0); XXH_COMPILER_GUARD(a1);
        XXH_COMPILER_GUARD(a2); XXH_COMPILER_GUARD(a3);
#endif
        return XXH_rotl32(a0, 1)  + XXH_rotl32(a1, 7)
             + XXH_rotl32(a2, 12) + XXH_rotl32(a3, 18);
    }
}

/*!
 * @internal
 * @brief Processes the last 0-15 bytes of @p ptr.
 *
 * There may be up to 15 bytes remaining to consume from the input.
 * This final stage will digest them to ensure that all input bytes are present
 * in the final mix.
 *
 * @param hash The hash to finalize.
 * @param ptr The pointer to the remaining input.
 * @param len The remaining length, modulo 16.
 * @param align Whether @p ptr is aligned.
 * @return The finalized hash.
 * @see XXH64_finalize().
 */
static XXH_PUREF xxh_u32
XXH32_finalize(xxh_u32 hash, const xxh_u8* ptr, size_t len, XXH_alignment align)
{
#define XXH_PROCESS1 do {                             \
    hash += (*ptr++) * XXH_PRIME32_5;                 \
    hash = XXH_rotl32(hash, 11) * XXH_PRIME32_1;      \
} while (0)

#define XXH_PROCESS4 do {                             \
    hash += XXH_get32bits(ptr) * XXH_PRIME32_3;       \
    ptr += 4;                                         \
    hash  = XXH_rotl32(hash, 17) * XXH_PRIME32_4;     \
} while (0)

    if (ptr==NULL) XXH_ASSERT(len == 0);

    /* Compact rerolled version; generally faster */
    if (!XXH32_ENDJMP) {
        len &= 15;
        while (len >= 4) {
            XXH_PROCESS4;
            len -= 4;
        }
        while (len > 0) {
            XXH_PROCESS1;
            --len;
        }
        return XXH32_avalanche(hash);
    } else {
         switch(len&15) /* or switch(bEnd - p) */ {
           case 12:      XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 8:       XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 4:       XXH_PROCESS4;
                         return XXH32_avalanche(hash);

           case 13:      XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 9:       XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 5:       XXH_PROCESS4;
                         XXH_PROCESS1;
                         return XXH32_avalanche(hash);

           case 14:      XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 10:      XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 6:       XXH_PROCESS4;
                         XXH_PROCESS1;
                         XXH_PROCESS1;
                         return XXH32_avalanche(hash);

           case 15:      XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 11:      XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 7:       XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 3:       XXH_PROCESS1;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 2:       XXH_PROCESS1;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 1:       XXH_PROCESS1;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 0:       return XXH32_avalanche(hash);
        }
        XXH_ASSERT(0);
        return hash;   /* reaching this point is deemed impossible */
    }
}

#ifdef XXH_OLD_NAMES
#  define PROCESS1 XXH_PROCESS1
#  define PROCESS4 XXH_PROCESS4
#else
#  undef XXH_PROCESS1
#  undef XXH_PROCESS4
#endif

/*!
 * @internal
 * @brief The implementation for @ref XXH32().
 *
 * @param input , len , seed Directly passed from @ref XXH32().
 * @param align Whether @p input is aligned.
 * @return The calculated hash.
 */
#if defined(_MSC_VER)
#  pragma warning(push)
/* MSVC's LTCG reports valid branches eliminated for constant input lengths. */
#  pragma warning(disable : 4702)  /* C4702: unreachable code */
#endif
XXH_FORCE_INLINE XXH_PUREF xxh_u32
XXH32_endian_align(const xxh_u8* input, size_t len, xxh_u32 seed, XXH_alignment align)
{
    xxh_u32 h32;

    if (input==NULL) XXH_ASSERT(len == 0);

    if (len>=16) {
        xxh_u32 acc[4];
        XXH32_initAccs(acc, seed);

        input = XXH32_consumeLong(acc, input, len, align);

        h32 = XXH32_mergeAccs(acc);
    } else {
        h32  = seed + XXH_PRIME32_5;
    }

    h32 += (xxh_u32)len;

    return XXH32_finalize(h32, input, len&15, align);
}
#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

/*! @ingroup XXH32_family */
XXH_PUBLIC_API XXH32_hash_t XXH32 (const void* input, size_t len, XXH32_hash_t seed)
{
#if !defined(XXH_NO_STREAM) && XXH_SIZE_OPT >= 2
    /* Simple version, good for code maintenance, but unfortunately slow for small inputs */
    XXH32_state_t state;
    XXH32_reset(&state, seed);
    XXH32_update(&state, (const xxh_u8*)input, len);
    return XXH32_digest(&state);
#else
    if (XXH_FORCE_ALIGN_CHECK) {
        if ((((size_t)input) & 3) == 0) {   /* Input is 4-bytes aligned, leverage the speed benefit */
            return XXH32_endian_align((const xxh_u8*)input, len, seed, XXH_aligned);
    }   }

    return XXH32_endian_align((const xxh_u8*)input, len, seed, XXH_unaligned);
#endif
}



/*******   Canonical representation   *******/

/*!
 * @internal
 * @fn xxh_u32 XXH_readBE32(const void* ptr)
 * @brief Reads an unaligned 32-bit big endian integer from @p ptr.
 *
 * Affected by @ref XXH_FORCE_MEMORY_ACCESS.
 *
 * @param ptr The pointer to read from.
 * @return The 32-bit big endian integer from the bytes at @p ptr.
 */
/* XXH_FORCE_MEMORY_ACCESS==3 is an endian-independent byteshift load. */
#if (defined(XXH_FORCE_MEMORY_ACCESS) && (XXH_FORCE_MEMORY_ACCESS==3))
XXH_FORCE_INLINE xxh_u32 XXH_readBE32(const void* memPtr)
{
    const xxh_u8* bytePtr = (const xxh_u8 *)memPtr;
    return bytePtr[3]
         | ((xxh_u32)bytePtr[2] << 8)
         | ((xxh_u32)bytePtr[1] << 16)
         | ((xxh_u32)bytePtr[0] << 24);
}
#else
static xxh_u32 XXH_readBE32(const void* ptr)
{
    return XXH_CPU_LITTLE_ENDIAN ? XXH_swap32(XXH_read32(ptr)) : XXH_read32(ptr);
}
#endif

/*! @ingroup XXH32_family */
XXH_PUBLIC_API void XXH32_canonicalFromHash(XXH32_canonical_t* dst, XXH32_hash_t hash)
{
    XXH_STATIC_ASSERT(sizeof(XXH32_canonical_t) == sizeof(XXH32_hash_t));
    if (XXH_CPU_LITTLE_ENDIAN) hash = XXH_swap32(hash);
    XXH_memcpy(dst, &hash, sizeof(*dst));
}
/*! @ingroup XXH32_family */
XXH_PUBLIC_API XXH32_hash_t XXH32_hashFromCanonical(const XXH32_canonical_t* src)
{
    return XXH_readBE32(src);
}

#endif  /* XXH_NO_XXH32 */
