/*
 * xxHash - Extremely Fast Hash algorithm
 * XXH3 AVX512 routines for long inputs (x86)
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

#if (XXH_VECTOR == XXH_AVX512) \
     || (defined(XXH_DISPATCH_AVX512) && XXH_DISPATCH_AVX512 != 0)

#ifndef XXH_TARGET_AVX512
# define XXH_TARGET_AVX512  /* disable attribute target */
#endif

XXH_FORCE_INLINE XXH_TARGET_AVX512 void
XXH3_accumulate_512_avx512(void* XXH_RESTRICT acc,
                     const void* XXH_RESTRICT input,
                     const void* XXH_RESTRICT secret)
{
    __m512i* const xacc = (__m512i *) acc;
    XXH_ASSERT((((size_t)acc) & 63) == 0);
    XXH_STATIC_ASSERT(XXH_STRIPE_LEN == sizeof(__m512i));

    {
        /* data_vec    = input[0]; */
        __m512i const data_vec    = _mm512_loadu_si512   (input);
        /* key_vec     = secret[0]; */
        __m512i const key_vec     = _mm512_loadu_si512   (secret);
        /* data_key    = data_vec ^ key_vec; */
        __m512i const data_key    = _mm512_xor_si512     (data_vec, key_vec);
        /* data_key_lo = data_key >> 32; */
        __m512i const data_key_lo = _mm512_srli_epi64 (data_key, 32);
        /* product     = (data_key & 0xffffffff) * (data_key_lo & 0xffffffff); */
        __m512i const product     = _mm512_mul_epu32     (data_key, data_key_lo);
        /* xacc[0] += swap(data_vec); */
        __m512i const data_swap = _mm512_shuffle_epi32(data_vec, (_MM_PERM_ENUM)_MM_SHUFFLE(1, 0, 3, 2));
        __m512i const sum       = _mm512_add_epi64(*xacc, data_swap);
        /* xacc[0] += product; */
        *xacc = _mm512_add_epi64(product, sum);
    }
}
XXH_FORCE_INLINE XXH_TARGET_AVX512 XXH3_ACCUMULATE_TEMPLATE(avx512)

/*
 * XXH3_scrambleAcc: Scrambles the accumulators to improve mixing.
 *
 * Multiplication isn't perfect, as explained by Google in HighwayHash:
 *
 *  // Multiplication mixes/scrambles bytes 0-7 of the 64-bit result to
 *  // varying degrees. In descending order of goodness, bytes
 *  // 3 4 2 5 1 6 0 7 have quality 228 224 164 160 100 96 36 32.
 *  // As expected, the upper and lower bytes are much worse.
 *
 * Source: https://github.com/google/highwayhash/blob/0aaf66b/highwayhash/hh_avx2.h#L291
 *
 * Since our algorithm uses a pseudorandom secret to add some variance into the
 * mix, we don't need to (or want to) mix as often or as much as HighwayHash does.
 *
 * This isn't as tight as XXH3_accumulate, but still written in SIMD to avoid
 * extraction.
 *
 * Both XXH3_64bits and XXH3_128bits use this subroutine.
 */

XXH_FORCE_INLINE XXH_TARGET_AVX512 void
XXH3_scrambleAcc_avx512(void* XXH_RESTRICT acc, const void* XXH_RESTRICT secret)
{
    XXH_ASSERT((((size_t)acc) & 63) == 0);
    XXH_STATIC_ASSERT(XXH_STRIPE_LEN == sizeof(__m512i));
    {   __m512i* const xacc = (__m512i*) acc;
        const __m512i prime32 = _mm512_set1_epi32((int)XXH_PRIME32_1);

        /* xacc[0] ^= (xacc[0] >> 47) */
        __m512i const acc_vec     = *xacc;
        __m512i const shifted     = _mm512_srli_epi64    (acc_vec, 47);
        /* xacc[0] ^= secret; */
        __m512i const key_vec     = _mm512_loadu_si512   (secret);
        __m512i const data_key    = _mm512_ternarylogic_epi32(key_vec, acc_vec, shifted, 0x96 /* key_vec ^ acc_vec ^ shifted */);

        /* xacc[0] *= XXH_PRIME32_1; */
        __m512i const data_key_hi = _mm512_srli_epi64 (data_key, 32);
        __m512i const prod_lo     = _mm512_mul_epu32     (data_key, prime32);
        __m512i const prod_hi     = _mm512_mul_epu32     (data_key_hi, prime32);
        *xacc = _mm512_add_epi64(prod_lo, _mm512_slli_epi64(prod_hi, 32));
    }
}

XXH_FORCE_INLINE XXH_TARGET_AVX512 void
XXH3_initCustomSecret_avx512(void* XXH_RESTRICT customSecret, xxh_u64 seed64)
{
    XXH_STATIC_ASSERT((XXH_SECRET_DEFAULT_SIZE & 63) == 0);
    XXH_STATIC_ASSERT(XXH_SEC_ALIGN == 64);
    XXH_ASSERT(((size_t)customSecret & 63) == 0);
    (void)(&XXH_writeLE64);
    {   int const nbRounds = XXH_SECRET_DEFAULT_SIZE / sizeof(__m512i);
        __m512i const seed_pos = _mm512_set1_epi64((xxh_i64)seed64);
        __m512i const seed     = _mm512_mask_sub_epi64(seed_pos, 0xAA, _mm512_setzero_si512(), seed_pos);

        const __m512i* const src  = (const __m512i*) ((const void*) XXH3_kSecret);
              __m512i* const dest = (      __m512i*) customSecret;
        int i;
        XXH_ASSERT(((size_t)src & 63) == 0); /* control alignment */
        XXH_ASSERT(((size_t)dest & 63) == 0);
        for (i=0; i < nbRounds; ++i) {
            dest[i] = _mm512_add_epi64(_mm512_load_si512(src + i), seed);
    }   }
}

#endif

#endif  /* !XXH_NO_XXH3 */
