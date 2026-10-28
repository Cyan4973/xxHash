/*
 * xxHash - Extremely Fast Hash algorithm
 * XXH3 LASX routines for long inputs (LoongArch)
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

#if (XXH_VECTOR == XXH_LASX)
#define XXH3_ROUTINES_LASX 1 /* see the selection of routines in xxh3_impl.h */
#define _LASX_SHUFFLE(z, y, x, w) (((z) << 6) | ((y) << 4) | ((x) << 2) | (w))

XXH_FORCE_INLINE void
XXH3_accumulate_512_lasx( void* XXH_RESTRICT acc,
                    const void* XXH_RESTRICT input,
                    const void* XXH_RESTRICT secret)
{
    XXH_ASSERT((((size_t)acc) & 31) == 0);
    {
        size_t i;
        __m256i* const xacc    =       (__m256i *) acc;
        const __m256i* const xinput  = (const __m256i *) input;
        const __m256i* const xsecret = (const __m256i *) secret;

        for (i = 0; i < XXH_STRIPE_LEN / sizeof(__m256i); i++) {
            /* data_vec = xinput[i]; */
            __m256i const data_vec = __lasx_xvld(xinput + i, 0);
            /* key_vec = xsecret[i]; */
            __m256i const key_vec = __lasx_xvld(xsecret + i, 0);
            /* data_key = data_vec ^ key_vec; */
            __m256i const data_key = __lasx_xvxor_v(data_vec, key_vec);
            /* data_key_lo = data_key >> 32; */
            __m256i const data_key_lo = __lasx_xvsrli_d(data_key, 32);
            /* product = (data_key & 0xffffffff) * (data_key_lo & 0xffffffff); */
            __m256i const product = __lasx_xvmulwev_d_wu(data_key, data_key_lo);
            /* xacc[i] += swap(data_vec); */
            __m256i const data_swap = __lasx_xvshuf4i_w(data_vec, _LASX_SHUFFLE(1, 0, 3, 2));
            __m256i const sum = __lasx_xvadd_d(xacc[i], data_swap);
            /* xacc[i] += product; */
            xacc[i] = __lasx_xvadd_d(product, sum);
        }
    }
}
XXH_FORCE_INLINE XXH3_ACCUMULATE_TEMPLATE(lasx)

XXH_FORCE_INLINE void
XXH3_scrambleAcc_lasx(void* XXH_RESTRICT acc, const void* XXH_RESTRICT secret)
{
    XXH_ASSERT((((size_t)acc) & 31) == 0);
    {
        __m256i* const xacc = (__m256i*) acc;
        const __m256i* const xsecret = (const __m256i *) secret;
        const __m256i prime32 = __lasx_xvreplgr2vr_d(XXH_PRIME32_1);
        size_t i;

        for (i = 0; i < XXH_STRIPE_LEN / sizeof(__m256i); i++) {
            /* xacc[i] ^= (xacc[i] >> 47) */
            __m256i const acc_vec = xacc[i];
            __m256i const shifted = __lasx_xvsrli_d(acc_vec, 47);
            __m256i const data_vec = __lasx_xvxor_v(acc_vec, shifted);
            /* xacc[i] ^= xsecret[i]; */
            __m256i const key_vec = __lasx_xvld(xsecret + i, 0);
            __m256i const data_key = __lasx_xvxor_v(data_vec, key_vec);

            /* xacc[i] *= XXH_PRIME32_1; */
            xacc[i] = __lasx_xvmul_d(data_key, prime32);
        }
    }
}

#endif

#endif  /* !XXH_NO_XXH3 */
