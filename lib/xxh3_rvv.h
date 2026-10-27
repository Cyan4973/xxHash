/*
 * xxHash - Extremely Fast Hash algorithm
 * XXH3 RVV routines for long inputs (RISC-V)
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

#if (XXH_VECTOR == XXH_RVV)
    #define XXH_CONCAT2(X, Y) X ## Y
    #define XXH_CONCAT(X, Y) XXH_CONCAT2(X, Y)
#if ((defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 13) || \
        (defined(__clang__) && __clang_major__ < 16))
    #define XXH_RVOP(op) op
    #define XXH_RVCAST(op) XXH_CONCAT(vreinterpret_v_, op)
#else
    #define XXH_RVOP(op) XXH_CONCAT(__riscv_, op)
    #define XXH_RVCAST(op) XXH_CONCAT(__riscv_vreinterpret_v_, op)
#endif
XXH_FORCE_INLINE void
XXH3_accumulate_512_rvv(  void* XXH_RESTRICT acc,
                    const void* XXH_RESTRICT input,
                    const void* XXH_RESTRICT secret)
{
    XXH_ASSERT((((size_t)acc) & 63) == 0);
    {
        /* Try to set vector length to 512 bits.
         * If this length is unavailable, then maximum available will be used */
        size_t vl = XXH_RVOP(vsetvl_e64m2)(8);

        uint64_t*       xacc    = (uint64_t*) acc;
        const uint64_t* xinput  = (const uint64_t*) input;
        const uint64_t* xsecret = (const uint64_t*) secret;
        static const uint64_t swap_mask[16] = {1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14};
        vuint64m2_t xswap_mask = XXH_RVOP(vle64_v_u64m2)(swap_mask, vl);

        size_t i;
        for (i = 0; i < XXH_STRIPE_LEN/8; i += vl) {
            /* data_vec = xinput[i]; */
            vuint64m2_t data_vec = XXH_RVCAST(u8m2_u64m2)(XXH_RVOP(vle8_v_u8m2)((const uint8_t*)(xinput + i), vl * 8));
            /* key_vec = xsecret[i]; */
            vuint64m2_t key_vec = XXH_RVCAST(u8m2_u64m2)(XXH_RVOP(vle8_v_u8m2)((const uint8_t*)(xsecret + i), vl * 8));
            /* acc_vec = xacc[i]; */
            vuint64m2_t acc_vec = XXH_RVOP(vle64_v_u64m2)(xacc + i, vl);
            /* data_key = data_vec ^ key_vec; */
            vuint64m2_t data_key = XXH_RVOP(vxor_vv_u64m2)(data_vec, key_vec, vl);
            /* data_key_hi = data_key >> 32; */
            vuint64m2_t data_key_hi = XXH_RVOP(vsrl_vx_u64m2)(data_key, 32, vl);
            /* data_key_lo = data_key & 0xffffffff; */
            vuint64m2_t data_key_lo = XXH_RVOP(vand_vx_u64m2)(data_key, 0xffffffff, vl);
            /* swap high and low halves */
            vuint64m2_t data_swap = XXH_RVOP(vrgather_vv_u64m2)(data_vec, xswap_mask, vl);
            /* acc_vec += data_key_lo * data_key_hi; */
            acc_vec = XXH_RVOP(vmacc_vv_u64m2)(acc_vec, data_key_lo, data_key_hi, vl);
            /* acc_vec += data_swap; */
            acc_vec = XXH_RVOP(vadd_vv_u64m2)(acc_vec, data_swap, vl);
            /* xacc[i] = acc_vec; */
            XXH_RVOP(vse64_v_u64m2)(xacc + i, acc_vec, vl);
        }
    }
}

XXH_FORCE_INLINE XXH3_ACCUMULATE_TEMPLATE(rvv)

XXH_FORCE_INLINE void
XXH3_scrambleAcc_rvv(void* XXH_RESTRICT acc, const void* XXH_RESTRICT secret)
{
    XXH_ASSERT((((size_t)acc) & 15) == 0);
    {
        size_t count = XXH_STRIPE_LEN/8;
        uint64_t* xacc = (uint64_t*)acc;
        const uint8_t* xsecret = (const uint8_t *)secret;
        size_t vl;
        for (; count > 0; count -= vl, xacc += vl, xsecret += vl*8) {
            vl = XXH_RVOP(vsetvl_e64m2)(count);
            {
                /* key_vec = xsecret[i]; */
                vuint64m2_t key_vec = XXH_RVCAST(u8m2_u64m2)(XXH_RVOP(vle8_v_u8m2)(xsecret, vl*8));
                /* acc_vec = xacc[i]; */
                vuint64m2_t acc_vec = XXH_RVOP(vle64_v_u64m2)(xacc, vl);
                /* acc_vec ^= acc_vec >> 47; */
                vuint64m2_t vsrl = XXH_RVOP(vsrl_vx_u64m2)(acc_vec, 47, vl);
                acc_vec = XXH_RVOP(vxor_vv_u64m2)(acc_vec, vsrl, vl);
                /* acc_vec ^= key_vec; */
                acc_vec = XXH_RVOP(vxor_vv_u64m2)(acc_vec, key_vec, vl);
                /* acc_vec *= XXH_PRIME32_1; */
                acc_vec = XXH_RVOP(vmul_vx_u64m2)(acc_vec, XXH_PRIME32_1, vl);
                /* xacc[i] *= acc_vec; */
                XXH_RVOP(vse64_v_u64m2)(xacc, acc_vec, vl);
            }
        }
    }
}

XXH_FORCE_INLINE void
XXH3_initCustomSecret_rvv(void* XXH_RESTRICT customSecret, xxh_u64 seed64)
{
    XXH_STATIC_ASSERT(XXH_SEC_ALIGN >= 8);
    XXH_ASSERT(((size_t)customSecret & 7) == 0);
    (void)(&XXH_writeLE64);
    {
        size_t count = XXH_SECRET_DEFAULT_SIZE/8;
        size_t vl;
        size_t VLMAX = XXH_RVOP(vsetvlmax_e64m2)();
        int64_t* cSecret = (int64_t*)customSecret;
        const int64_t* kSecret = (const int64_t*)(const void*)XXH3_kSecret;

#if __riscv_v_intrinsic >= 1000000
        /* ratified v1.0 intrinsics version */
        vbool32_t mneg = XXH_RVCAST(u8m1_b32)(
                         XXH_RVOP(vmv_v_x_u8m1)(0xaa, XXH_RVOP(vsetvlmax_e8m1)()));
#else
        /* support pre-ratification intrinsics, which lack mask to vector casts */
        size_t vlmax = XXH_RVOP(vsetvlmax_e8m1)();
        vbool32_t mneg = XXH_RVOP(vmseq_vx_u8mf4_b32)(
                         XXH_RVOP(vand_vx_u8mf4)(
                         XXH_RVOP(vid_v_u8mf4)(vlmax), 1, vlmax), 1, vlmax);
#endif
        vint64m2_t seed = XXH_RVOP(vmv_v_x_i64m2)((int64_t)seed64, VLMAX);
        seed = XXH_RVOP(vneg_v_i64m2_mu)(mneg, seed, seed, VLMAX);

        for (; count > 0; count -= vl, cSecret += vl, kSecret += vl) {
            /* make sure vl=VLMAX until last iteration */
            vl = XXH_RVOP(vsetvl_e64m2)(count < VLMAX ? count : VLMAX);
            {
                vint64m2_t src = XXH_RVOP(vle64_v_i64m2)(kSecret, vl);
                vint64m2_t res = XXH_RVOP(vadd_vv_i64m2)(src, seed, vl);
                XXH_RVOP(vse64_v_i64m2)(cSecret, res, vl);
            }
        }
    }
}
#endif

#endif  /* !XXH_NO_XXH3 */
