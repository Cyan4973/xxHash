/*
 * xxHash - Extremely Fast Hash algorithm
 * XXH3 LSX routines for long inputs (LoongArch)
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

#if (XXH_VECTOR == XXH_LSX)
#define _LSX_SHUFFLE(z, y, x, w) (((z) << 6) | ((y) << 4) | ((x) << 2) | (w))

XXH_FORCE_INLINE void
XXH3_accumulate_512_lsx( void* XXH_RESTRICT acc,
                    const void* XXH_RESTRICT input,
                    const void* XXH_RESTRICT secret)
{
    XXH_ASSERT((((size_t)acc) & 15) == 0);
    {
        __m128i* const xacc    =       (__m128i *) acc;
        const __m128i* const xinput  = (const __m128i *) input;
        const __m128i* const xsecret = (const __m128i *) secret;
        size_t i;

        for (i = 0; i < XXH_STRIPE_LEN / sizeof(__m128i); i++) {
            /* data_vec = xinput[i]; */
            __m128i const data_vec = __lsx_vld(xinput + i, 0);
            /* key_vec = xsecret[i]; */
            __m128i const key_vec = __lsx_vld(xsecret + i, 0);
            /* data_key = data_vec ^ key_vec; */
            __m128i const data_key = __lsx_vxor_v(data_vec, key_vec);
            /* data_key_lo = data_key >> 32; */
            __m128i const data_key_lo = __lsx_vsrli_d(data_key, 32);
            /* product = (data_key & 0xffffffff) * (data_key_lo & 0xffffffff); */
            __m128i const product = __lsx_vmulwev_d_wu(data_key, data_key_lo);
            /* xacc[i] += swap(data_vec); */
            __m128i const data_swap = __lsx_vshuf4i_w(data_vec, _LSX_SHUFFLE(1, 0, 3, 2));
            __m128i const sum = __lsx_vadd_d(xacc[i], data_swap);
            /* xacc[i] += product; */
            xacc[i] = __lsx_vadd_d(product, sum);
        }
    }
}
XXH_FORCE_INLINE XXH3_ACCUMULATE_TEMPLATE(lsx)

XXH_FORCE_INLINE void
XXH3_scrambleAcc_lsx(void* XXH_RESTRICT acc, const void* XXH_RESTRICT secret)
{
    XXH_ASSERT((((size_t)acc) & 15) == 0);
    {
        __m128i* const xacc = (__m128i*) acc;
        const __m128i* const xsecret = (const __m128i *) secret;
        const __m128i prime32 = __lsx_vreplgr2vr_d(XXH_PRIME32_1);
        size_t i;

        for (i = 0; i < XXH_STRIPE_LEN / sizeof(__m128i); i++) {
            /* xacc[i] ^= (xacc[i] >> 47) */
            __m128i const acc_vec = xacc[i];
            __m128i const shifted = __lsx_vsrli_d(acc_vec, 47);
            __m128i const data_vec = __lsx_vxor_v(acc_vec, shifted);
            /* xacc[i] ^= xsecret[i]; */
            __m128i const key_vec = __lsx_vld(xsecret + i, 0);
            __m128i const data_key = __lsx_vxor_v(data_vec, key_vec);

            /* xacc[i] *= XXH_PRIME32_1; */
            xacc[i] = __lsx_vmul_d(data_key, prime32);
        }
    }
}

#endif

#endif  /* !XXH_NO_XXH3 */
