/*
 * xxHash - Extremely Fast Hash algorithm
 * XXH3 64-bit implementation (XXH3_64bits)
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

#endif  /* !XXH_NO_XXH3 */
