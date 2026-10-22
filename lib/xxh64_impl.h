/*
 * xxHash - Extremely Fast Hash algorithm
 * XXH64 implementation
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

#ifndef XXH_NO_LONG_LONG

/* *******************************************************************
*  64-bit hash functions
*********************************************************************/
/*!
 * @}
 * @ingroup impl
 * @{
 */
#if defined(XXH_INLINE_ALL)
#  define XXH_STATIC XXH_FORCE_INLINE
#else
#  define XXH_STATIC static
#endif

/*******   Memory access   *******/

typedef XXH64_hash_t xxh_u64;

#ifdef XXH_OLD_NAMES
#  define U64 xxh_u64
#endif

#if (defined(XXH_FORCE_MEMORY_ACCESS) && (XXH_FORCE_MEMORY_ACCESS==3))
/*
 * Manual byteshift. Best for old compilers which don't inline memcpy.
 * We actually directly use XXH_readLE64 and XXH_readBE64.
 */
#elif (defined(XXH_FORCE_MEMORY_ACCESS) && (XXH_FORCE_MEMORY_ACCESS==2))

/* Force direct memory access. Only works on CPU which support unaligned memory access in hardware */
static xxh_u64 XXH_read64(const void* memPtr)
{
    return *(const xxh_u64*) memPtr;
}

#elif (defined(XXH_FORCE_MEMORY_ACCESS) && (XXH_FORCE_MEMORY_ACCESS==1))

/*
 * __attribute__((aligned(1))) is supported by gcc and clang. Originally the
 * documentation claimed that it only increased the alignment, but actually it
 * can decrease it on gcc, clang, and icc:
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=69502,
 * https://gcc.godbolt.org/z/xYez1j67Y.
 */
#ifdef XXH_OLD_NAMES
typedef union { xxh_u32 u32; xxh_u64 u64; } __attribute__((__packed__)) unalign64;
#endif
static xxh_u64 XXH_read64(const void* ptr)
{
    typedef __attribute__((__aligned__(1))) __attribute__((__may_alias__)) xxh_u64 xxh_unalign64;
    return *((const xxh_unalign64*)ptr);
}

#else

/*
 * Portable and safe solution. Generally efficient.
 * see: https://fastcompression.blogspot.com/2015/08/accessing-unaligned-memory.html
 */
static xxh_u64 XXH_read64(const void* memPtr)
{
    xxh_u64 val;
    XXH_memcpy(&val, memPtr, sizeof(val));
    return val;
}

#endif   /* XXH_FORCE_DIRECT_MEMORY_ACCESS */

#if defined(_MSC_VER)     /* Visual Studio */
#  define XXH_swap64 _byteswap_uint64
#elif XXH_GCC_VERSION >= 403
#  define XXH_swap64 __builtin_bswap64
#else
static xxh_u64 XXH_swap64(xxh_u64 x)
{
    return  ((x << 56) & 0xff00000000000000ULL) |
            ((x << 40) & 0x00ff000000000000ULL) |
            ((x << 24) & 0x0000ff0000000000ULL) |
            ((x << 8)  & 0x000000ff00000000ULL) |
            ((x >> 8)  & 0x00000000ff000000ULL) |
            ((x >> 24) & 0x0000000000ff0000ULL) |
            ((x >> 40) & 0x000000000000ff00ULL) |
            ((x >> 56) & 0x00000000000000ffULL);
}
#endif


/* XXH_FORCE_MEMORY_ACCESS==3 is an endian-independent byteshift load. */
#if (defined(XXH_FORCE_MEMORY_ACCESS) && (XXH_FORCE_MEMORY_ACCESS==3))

XXH_FORCE_INLINE xxh_u64 XXH_readLE64(const void* memPtr)
{
    const xxh_u8* bytePtr = (const xxh_u8 *)memPtr;
    return bytePtr[0]
         | ((xxh_u64)bytePtr[1] << 8)
         | ((xxh_u64)bytePtr[2] << 16)
         | ((xxh_u64)bytePtr[3] << 24)
         | ((xxh_u64)bytePtr[4] << 32)
         | ((xxh_u64)bytePtr[5] << 40)
         | ((xxh_u64)bytePtr[6] << 48)
         | ((xxh_u64)bytePtr[7] << 56);
}

XXH_FORCE_INLINE xxh_u64 XXH_readBE64(const void* memPtr)
{
    const xxh_u8* bytePtr = (const xxh_u8 *)memPtr;
    return bytePtr[7]
         | ((xxh_u64)bytePtr[6] << 8)
         | ((xxh_u64)bytePtr[5] << 16)
         | ((xxh_u64)bytePtr[4] << 24)
         | ((xxh_u64)bytePtr[3] << 32)
         | ((xxh_u64)bytePtr[2] << 40)
         | ((xxh_u64)bytePtr[1] << 48)
         | ((xxh_u64)bytePtr[0] << 56);
}

#else
XXH_FORCE_INLINE xxh_u64 XXH_readLE64(const void* ptr)
{
    return XXH_CPU_LITTLE_ENDIAN ? XXH_read64(ptr) : XXH_swap64(XXH_read64(ptr));
}

static xxh_u64 XXH_readBE64(const void* ptr)
{
    return XXH_CPU_LITTLE_ENDIAN ? XXH_swap64(XXH_read64(ptr)) : XXH_read64(ptr);
}
#endif

XXH_FORCE_INLINE xxh_u64
XXH_readLE64_align(const void* ptr, XXH_alignment align)
{
    if (align==XXH_unaligned)
        return XXH_readLE64(ptr);
    else
        return XXH_CPU_LITTLE_ENDIAN ? *(const xxh_u64*)ptr : XXH_swap64(*(const xxh_u64*)ptr);
}


/*******   xxh64   *******/
/*!
 * @}
 * @defgroup XXH64_impl XXH64 implementation
 * @ingroup impl
 *
 * Details on the XXH64 implementation.
 * @{
 */
/* #define rather that static const, to be used as initializers */
#define XXH_PRIME64_1  0x9E3779B185EBCA87ULL  /*!< 0b1001111000110111011110011011000110000101111010111100101010000111 */
#define XXH_PRIME64_2  0xC2B2AE3D27D4EB4FULL  /*!< 0b1100001010110010101011100011110100100111110101001110101101001111 */
#define XXH_PRIME64_3  0x165667B19E3779F9ULL  /*!< 0b0001011001010110011001111011000110011110001101110111100111111001 */
#define XXH_PRIME64_4  0x85EBCA77C2B2AE63ULL  /*!< 0b1000010111101011110010100111011111000010101100101010111001100011 */
#define XXH_PRIME64_5  0x27D4EB2F165667C5ULL  /*!< 0b0010011111010100111010110010111100010110010101100110011111000101 */

#ifdef XXH_OLD_NAMES
#  define PRIME64_1 XXH_PRIME64_1
#  define PRIME64_2 XXH_PRIME64_2
#  define PRIME64_3 XXH_PRIME64_3
#  define PRIME64_4 XXH_PRIME64_4
#  define PRIME64_5 XXH_PRIME64_5
#endif

/*! @copydoc XXH32_round */
static xxh_u64 XXH64_round(xxh_u64 acc, xxh_u64 input)
{
    acc += input * XXH_PRIME64_2;
    acc  = XXH_rotl64(acc, 31);
    acc *= XXH_PRIME64_1;
#if (defined(__SSE2__)) && !defined(XXH_ENABLE_AUTOVECTORIZE)
    /*
     * DISABLE AUTOVECTORIZATION:
     * A compiler fence is used to prevent GCC and Clang from
     * autovectorizing the XXH64 loop (pragmas and attributes don't work for some
     * reason) without globally disabling AVX512.
     *
     * Autovectorization of XXH64 tends to be detrimental,
     * though the exact outcome may change depending on exact cpu and compiler version.
     * For information, it has been reported as detrimental for Skylake-X and Zen5,
     * but possibly beneficial for Zen4.
     *
     * This is also enabled below AVX512: without vpmullq,
     * each 64-bit vector multiply is emulated with 3x pmuludq + shifts.
     * Clang 22 vectorizes XXH64_update() this way with AVX2,
     * making XXH64 streaming ~3x slower.
     *
     * The default is to disable auto-vectorization,
     * but you can select to enable it instead using `XXH_ENABLE_AUTOVECTORIZE` build variable.
     */
    XXH_COMPILER_GUARD(acc);
#endif
    return acc;
}

static xxh_u64 XXH64_mergeRound(xxh_u64 acc, xxh_u64 val)
{
    val  = XXH64_round(0, val);
    acc ^= val;
    acc  = acc * XXH_PRIME64_1 + XXH_PRIME64_4;
    return acc;
}

/*! @copydoc XXH32_avalanche */
static xxh_u64 XXH64_avalanche(xxh_u64 hash)
{
    hash ^= hash >> 33;
    hash *= XXH_PRIME64_2;
    hash ^= hash >> 29;
    hash *= XXH_PRIME64_3;
    hash ^= hash >> 32;
    return hash;
}


#define XXH_get64bits(p) XXH_readLE64_align(p, align)

/*!
 * @internal
 * @brief Sets up the initial accumulator state for XXH64().
 */
XXH_FORCE_INLINE void
XXH64_initAccs(xxh_u64 *acc, xxh_u64 seed)
{
    XXH_ASSERT(acc != NULL);
    acc[0] = seed + XXH_PRIME64_1 + XXH_PRIME64_2;
    acc[1] = seed + XXH_PRIME64_2;
    acc[2] = seed + 0;
    acc[3] = seed - XXH_PRIME64_1;
}

/*!
 * @internal
 * @brief Consumes a block of data for XXH64().
 *
 * @return the end input pointer.
 */
XXH_FORCE_INLINE const xxh_u8 *
XXH64_consumeLong(
    xxh_u64 *XXH_RESTRICT acc,
    xxh_u8 const *XXH_RESTRICT input,
    size_t len,
    XXH_alignment align
)
{
    const xxh_u8* const bEnd = input + len;
    const xxh_u8* const limit = bEnd - 31;
    XXH_ASSERT(acc != NULL);
    XXH_ASSERT(input != NULL);
    XXH_ASSERT(len >= 32);
    if (sizeof(void *) < sizeof(xxh_u64)) {
        /* reroll on 32-bit */
        do {
            size_t i;
            for (i = 0; i < 4; i++) {
                acc[i] = XXH64_round(acc[i], XXH_get64bits(input));
                input += 8;
            }
        } while (input < limit);
    } else {
        /* see XXH32_consumeLong() */
        xxh_u64 a0 = acc[0], a1 = acc[1], a2 = acc[2], a3 = acc[3];
        do {
            a0 = XXH64_round(a0, XXH_get64bits(input)); input += 8;
            a1 = XXH64_round(a1, XXH_get64bits(input)); input += 8;
            a2 = XXH64_round(a2, XXH_get64bits(input)); input += 8;
            a3 = XXH64_round(a3, XXH_get64bits(input)); input += 8;
        } while (input < limit);
        acc[0] = a0; acc[1] = a1; acc[2] = a2; acc[3] = a3;
    }

    return input;
}

/*!
 * @internal
 * @brief Merges the accumulator lanes together for XXH64()
 */
XXH_FORCE_INLINE XXH_PUREF xxh_u64
XXH64_mergeAccs(const xxh_u64 *acc)
{
    XXH_ASSERT(acc != NULL);
    {
        xxh_u64 h64 = XXH_rotl64(acc[0], 1) + XXH_rotl64(acc[1], 7)
                    + XXH_rotl64(acc[2], 12) + XXH_rotl64(acc[3], 18);
        /* reroll on 32-bit */
        if (sizeof(void *) < sizeof(xxh_u64)) {
            size_t i;
            for (i = 0; i < 4; i++) {
                h64 = XXH64_mergeRound(h64, acc[i]);
            }
        } else {
            h64 = XXH64_mergeRound(h64, acc[0]);
            h64 = XXH64_mergeRound(h64, acc[1]);
            h64 = XXH64_mergeRound(h64, acc[2]);
            h64 = XXH64_mergeRound(h64, acc[3]);
        }
        return h64;
    }
}

/*!
 * @internal
 * @brief Processes the last 0-31 bytes of @p ptr.
 *
 * There may be up to 31 bytes remaining to consume from the input.
 * This final stage will digest them to ensure that all input bytes are present
 * in the final mix.
 *
 * @param hash The hash to finalize.
 * @param ptr The pointer to the remaining input.
 * @param len The remaining length, modulo 32.
 * @param align Whether @p ptr is aligned.
 * @return The finalized hash
 * @see XXH32_finalize().
 */
XXH_STATIC XXH_PUREF xxh_u64
XXH64_finalize(xxh_u64 hash, const xxh_u8* ptr, size_t len, XXH_alignment align)
{
    if (ptr==NULL) XXH_ASSERT(len == 0);
    len &= 31;
    while (len >= 8) {
        xxh_u64 const k1 = XXH64_round(0, XXH_get64bits(ptr));
        ptr += 8;
        hash ^= k1;
        hash  = XXH_rotl64(hash,27) * XXH_PRIME64_1 + XXH_PRIME64_4;
        len -= 8;
    }
    if (len >= 4) {
        hash ^= (xxh_u64)(XXH_get32bits(ptr)) * XXH_PRIME64_1;
        ptr += 4;
        hash = XXH_rotl64(hash, 23) * XXH_PRIME64_2 + XXH_PRIME64_3;
        len -= 4;
    }
    while (len > 0) {
        hash ^= (*ptr++) * XXH_PRIME64_5;
        hash = XXH_rotl64(hash, 11) * XXH_PRIME64_1;
        --len;
    }
    return  XXH64_avalanche(hash);
}

#ifdef XXH_OLD_NAMES
#  define PROCESS1_64 XXH_PROCESS1_64
#  define PROCESS4_64 XXH_PROCESS4_64
#  define PROCESS8_64 XXH_PROCESS8_64
#else
#  undef XXH_PROCESS1_64
#  undef XXH_PROCESS4_64
#  undef XXH_PROCESS8_64
#endif

/*!
 * @internal
 * @brief The implementation for @ref XXH64().
 *
 * @param input , len , seed Directly passed from @ref XXH64().
 * @param align Whether @p input is aligned.
 * @return The calculated hash.
 */
XXH_FORCE_INLINE XXH_PUREF xxh_u64
XXH64_endian_align(const xxh_u8* input, size_t len, xxh_u64 seed, XXH_alignment align)
{
    xxh_u64 h64;
    if (input==NULL) XXH_ASSERT(len == 0);

    if (len>=32) {  /* Process a large block of data */
        xxh_u64 acc[4];
        XXH64_initAccs(acc, seed);

        input = XXH64_consumeLong(acc, input, len, align);

        h64 = XXH64_mergeAccs(acc);
    } else {
        h64  = seed + XXH_PRIME64_5;
    }

    h64 += (xxh_u64) len;

    return XXH64_finalize(h64, input, len, align);
}


/*! @ingroup XXH64_family */
XXH_PUBLIC_API XXH64_hash_t XXH64 (XXH_NOESCAPE const void* input, size_t len, XXH64_hash_t seed)
{
#if !defined(XXH_NO_STREAM) && XXH_SIZE_OPT >= 2
    /* Simple version, good for code maintenance, but unfortunately slow for small inputs */
    XXH64_state_t state;
    XXH64_reset(&state, seed);
    XXH64_update(&state, (const xxh_u8*)input, len);
    return XXH64_digest(&state);
#else
    if (XXH_FORCE_ALIGN_CHECK) {
        if ((((size_t)input) & 7)==0) {  /* Input is aligned, let's leverage the speed advantage */
            return XXH64_endian_align((const xxh_u8*)input, len, seed, XXH_aligned);
    }   }

    return XXH64_endian_align((const xxh_u8*)input, len, seed, XXH_unaligned);

#endif
}

/******* Canonical representation   *******/

/*! @ingroup XXH64_family */
XXH_PUBLIC_API void XXH64_canonicalFromHash(XXH_NOESCAPE XXH64_canonical_t* dst, XXH64_hash_t hash)
{
    XXH_STATIC_ASSERT(sizeof(XXH64_canonical_t) == sizeof(XXH64_hash_t));
    if (XXH_CPU_LITTLE_ENDIAN) hash = XXH_swap64(hash);
    XXH_memcpy(dst, &hash, sizeof(*dst));
}

/*! @ingroup XXH64_family */
XXH_PUBLIC_API XXH64_hash_t XXH64_hashFromCanonical(XXH_NOESCAPE const XXH64_canonical_t* src)
{
    return XXH_readBE64(src);
}

#endif  /* XXH_NO_LONG_LONG */
