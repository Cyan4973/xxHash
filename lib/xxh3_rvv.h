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
#define XXH3_ROUTINES_RVV 1 /* see the selection of routines in xxh3_impl.h */
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

/*
 * The whole loop of long inputs, with the accumulators kept in 4 vector
 * registers across all stripes and blocks, instead of being loaded and stored
 * at each stripe. It requires VLEN >= 128, like the rest of this unit.
 * RVV types cannot be members of structs or arrays, hence a single function.
 */
XXH_FORCE_INLINE void
XXH3_hashLong_internal_loop_rvv(xxh_u64* XXH_RESTRICT acc,
                      const xxh_u8* XXH_RESTRICT input, size_t len,
                      const xxh_u8* XXH_RESTRICT secret, size_t secretSize)
{
    size_t const nbStripesPerBlock = (secretSize - XXH_STRIPE_LEN) / XXH_SECRET_CONSUME_RATE;
    size_t const block_len = XXH_STRIPE_LEN * nbStripesPerBlock;
    size_t const nb_blocks = (len - 1) / block_len;

    size_t vl = XXH_RVOP(vsetvl_e64m1)(2);
    static const uint64_t swap_mask[2] = {1, 0};
    vuint64m1_t xswap_mask = XXH_RVOP(vle64_v_u64m1)(swap_mask, vl);
    const xxh_u64* xacc = (const xxh_u64*)acc;
    size_t n;

    /* accumulators: 4 x (e64m1, vl=2) */
    vuint64m1_t r0 = XXH_RVOP(vle64_v_u64m1)(xacc + 0, vl);
    vuint64m1_t r1 = XXH_RVOP(vle64_v_u64m1)(xacc + 2, vl);
    vuint64m1_t r2 = XXH_RVOP(vle64_v_u64m1)(xacc + 4, vl);
    vuint64m1_t r3 = XXH_RVOP(vle64_v_u64m1)(xacc + 6, vl);

#define XXH_RVV_ACC_LANE(R, XI, XS)                                   \
    {                                                                  \
        vuint64m1_t d_ = XXH_RVCAST(u8m1_u64m1)(XXH_RVOP(vle8_v_u8m1)((const uint8_t*)(XI), 16)); \
        vuint64m1_t k_ = XXH_RVCAST(u8m1_u64m1)(XXH_RVOP(vle8_v_u8m1)((const uint8_t*)(XS), 16)); \
        vuint64m1_t dk_ = XXH_RVOP(vxor_vv_u64m1)(d_, k_, vl);        \
        vuint64m1_t hi_ = XXH_RVOP(vsrl_vx_u64m1)(dk_, 32, vl);       \
        vuint64m1_t lo_ = XXH_RVOP(vand_vx_u64m1)(dk_, 0xffffffff, vl); \
        vuint64m1_t sw_ = XXH_RVOP(vrgather_vv_u64m1)(d_, xswap_mask, vl); \
        R = XXH_RVOP(vmacc_vv_u64m1)(R, lo_, hi_, vl);                \
        R = XXH_RVOP(vadd_vv_u64m1)(R, sw_, vl);                       \
    }

#define XXH_RVV_SCRAMBLE_LANE(R, XS)                                  \
    {                                                                  \
        vuint64m1_t k_ = XXH_RVCAST(u8m1_u64m1)(XXH_RVOP(vle8_v_u8m1)((const uint8_t*)(XS), 16)); \
        vuint64m1_t s_ = XXH_RVOP(vsrl_vx_u64m1)(R, 47, vl);          \
        R = XXH_RVOP(vxor_vv_u64m1)(R, s_, vl);                        \
        R = XXH_RVOP(vxor_vv_u64m1)(R, k_, vl);                        \
        R = XXH_RVOP(vmul_vx_u64m1)(R, XXH_PRIME32_1, vl);             \
    }

    for (n = 0; n < nb_blocks; n++) {
        const xxh_u8* xin  = input + n*block_len;
        size_t s;
        for (s = 0; s < nbStripesPerBlock; s++) {
            const xxh_u8* si = xin + s*XXH_STRIPE_LEN;
            const xxh_u8* sk = secret + s*XXH_SECRET_CONSUME_RATE;
            /* 4 independent chains per stripe */
            XXH_RVV_ACC_LANE(r0, si + 0, sk + 0)
            XXH_RVV_ACC_LANE(r1, si + 16, sk + 16)
            XXH_RVV_ACC_LANE(r2, si + 32, sk + 32)
            XXH_RVV_ACC_LANE(r3, si + 48, sk + 48)
        }
        {   const uint8_t* sc = secret + secretSize - XXH_STRIPE_LEN;
            XXH_RVV_SCRAMBLE_LANE(r0, sc + 0)
            XXH_RVV_SCRAMBLE_LANE(r1, sc + 16)
            XXH_RVV_SCRAMBLE_LANE(r2, sc + 32)
            XXH_RVV_SCRAMBLE_LANE(r3, sc + 48)
        }
    }

    /* last partial block */
    {   size_t const nbStripes = ((len - 1) - (block_len * nb_blocks)) / XXH_STRIPE_LEN;
        const xxh_u8* xin = input + nb_blocks*block_len;
        size_t s;
        for (s = 0; s < nbStripes; s++) {
            const xxh_u8* si = xin + s*XXH_STRIPE_LEN;
            const xxh_u8* sk = secret + s*XXH_SECRET_CONSUME_RATE;
            XXH_RVV_ACC_LANE(r0, si + 0, sk + 0)
            XXH_RVV_ACC_LANE(r1, si + 16, sk + 16)
            XXH_RVV_ACC_LANE(r2, si + 32, sk + 32)
            XXH_RVV_ACC_LANE(r3, si + 48, sk + 48)
        }
        /* last stripe */
        {   const xxh_u8* si = input + len - XXH_STRIPE_LEN;
            const xxh_u8* sk = secret + secretSize - XXH_STRIPE_LEN - XXH_SECRET_LASTACC_START;
            XXH_RVV_ACC_LANE(r0, si + 0, sk + 0)
            XXH_RVV_ACC_LANE(r1, si + 16, sk + 16)
            XXH_RVV_ACC_LANE(r2, si + 32, sk + 32)
            XXH_RVV_ACC_LANE(r3, si + 48, sk + 48)
        }
    }

#undef XXH_RVV_SCRAMBLE_LANE
#undef XXH_RVV_ACC_LANE

    XXH_RVOP(vse64_v_u64m1)(acc + 0, r0, vl);
    XXH_RVOP(vse64_v_u64m1)(acc + 2, r1, vl);
    XXH_RVOP(vse64_v_u64m1)(acc + 4, r2, vl);
    XXH_RVOP(vse64_v_u64m1)(acc + 6, r3, vl);
}
/* replaces the generic loop of long inputs, in xxh3_impl.h */
#define XXH3_hashLong_internal_loop_simd XXH3_hashLong_internal_loop_rvv
#endif

#endif  /* !XXH_NO_XXH3 */
