/*
 * xxHash - Extremely Fast Hash algorithm
 * XXH3 NEON and WASM SIMD128 routines for long inputs (ARM, WebAssembly)
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

#if XXH_VECTOR == XXH_NEON

/*
 * UGLY HACK: While AArch64 GCC on Linux does not seem to care, on macOS, GCC -O3
 * optimizes out the entire hashLong loop because of the aliasing violation.
 *
 * However, GCC is also inefficient at load-store optimization with vld1q/vst1q,
 * so the only option is to mark it as aliasing.
 */
typedef uint64x2_t xxh_aliasing_uint64x2_t XXH_ALIASING;

/*!
 * @internal
 * @brief `vld1q_u64` but faster and alignment-safe.
 *
 * On AArch64, unaligned access is always safe, but on ARMv7-a, it is only
 * *conditionally* safe (`vld1` has an alignment bit like `movdq[ua]` in x86).
 *
 * GCC for AArch64 sees `vld1q_u8` as an intrinsic instead of a load, so it
 * prohibits load-store optimizations. Therefore, a direct dereference is used.
 *
 * Otherwise, `vld1q_u8` is used with `vreinterpretq_u8_u64` to do a safe
 * unaligned load.
 */
#if defined(__aarch64__) && defined(__GNUC__) && !defined(__clang__)
XXH_FORCE_INLINE uint64x2_t XXH_vld1q_u64(void const* ptr) /* silence -Wcast-align */
{
    return *(xxh_aliasing_uint64x2_t const *)ptr;
}
#else
XXH_FORCE_INLINE uint64x2_t XXH_vld1q_u64(void const* ptr)
{
    return vreinterpretq_u64_u8(vld1q_u8((uint8_t const*)ptr));
}
#endif

/*!
 * @internal
 * @brief `vmlal_u32` on low and high halves of a vector.
 *
 * This is a workaround for AArch64 GCC < 11 which implemented arm_neon.h with
 * inline assembly and were therefore incapable of merging the `vget_{low, high}_u32`
 * with `vmlal_u32`.
 */
#if defined(__aarch64__) && defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 11
XXH_FORCE_INLINE uint64x2_t
XXH_vmlal_low_u32(uint64x2_t acc, uint32x4_t lhs, uint32x4_t rhs)
{
    /* Inline assembly is the only way */
    __asm__("umlal   %0.2d, %1.2s, %2.2s" : "+w" (acc) : "w" (lhs), "w" (rhs));
    return acc;
}
XXH_FORCE_INLINE uint64x2_t
XXH_vmlal_high_u32(uint64x2_t acc, uint32x4_t lhs, uint32x4_t rhs)
{
    /* This intrinsic works as expected */
    return vmlal_high_u32(acc, lhs, rhs);
}
#else
/* Portable intrinsic versions */
XXH_FORCE_INLINE uint64x2_t
XXH_vmlal_low_u32(uint64x2_t acc, uint32x4_t lhs, uint32x4_t rhs)
{
    return vmlal_u32(acc, vget_low_u32(lhs), vget_low_u32(rhs));
}
/*! @copydoc XXH_vmlal_low_u32
 * Assume the compiler converts this to vmlal_high_u32 on aarch64 */
XXH_FORCE_INLINE uint64x2_t
XXH_vmlal_high_u32(uint64x2_t acc, uint32x4_t lhs, uint32x4_t rhs)
{
    return vmlal_u32(acc, vget_high_u32(lhs), vget_high_u32(rhs));
}
#endif

/*!
 * @ingroup tuning
 * @brief Controls the NEON to scalar ratio for XXH3
 *
 * This can be set to 2, 4, 6, or 8.
 *
 * ARM Cortex CPUs are _very_ sensitive to how their pipelines are used.
 *
 * For example, the Cortex-A73 can dispatch 3 micro-ops per cycle, but only 2 of those
 * can be NEON. If you are only using NEON instructions, you are only using 2/3 of the CPU
 * bandwidth.
 *
 * This is even more noticeable on the more advanced cores like the Cortex-A76 which
 * can dispatch 8 micro-ops per cycle, but still only 2 NEON micro-ops at once.
 *
 * Therefore, to make the most out of the pipeline, it is beneficial to run 6 NEON lanes
 * and 2 scalar lanes, which is chosen by default.
 *
 * This does not apply to Apple processors or 32-bit processors, which run better with
 * full NEON. These will default to 8. Additionally, size-optimized builds run 8 lanes.
 *
 * This change benefits CPUs with large micro-op buffers without negatively affecting
 * most other CPUs:
 *
 *  | Chipset               | Dispatch type       | NEON only | 6:2 hybrid | Diff. |
 *  |:----------------------|:--------------------|----------:|-----------:|------:|
 *  | Snapdragon 730 (A76)  | 2 NEON/8 micro-ops  |  8.8 GB/s |  10.1 GB/s |  ~16% |
 *  | Snapdragon 835 (A73)  | 2 NEON/3 micro-ops  |  5.1 GB/s |   5.3 GB/s |   ~5% |
 *  | Marvell PXA1928 (A53) | In-order dual-issue |  1.9 GB/s |   1.9 GB/s |    0% |
 *  | Apple M1              | 4 NEON/8 micro-ops  | 37.3 GB/s |  36.1 GB/s |  ~-3% |
 *
 * It also seems to fix some bad codegen on GCC, making it almost as fast as clang.
 *
 * When using WASM SIMD128, if this is 2 or 6, SIMDe will scalarize 2 of the lanes meaning
 * it effectively becomes worse 4.
 *
 * @see XXH3_accumulate_512_neon()
 */
# ifndef XXH3_NEON_LANES
#  if (defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64) || defined(_M_ARM64EC)) \
   && !defined(__APPLE__) && XXH_SIZE_OPT <= 0
#   define XXH3_NEON_LANES 6
#  else
#   define XXH3_NEON_LANES XXH_ACC_NB
#  endif
# endif

/* forward declarations for the scalar routines */
XXH_FORCE_INLINE void
XXH3_scalarRound(void* XXH_RESTRICT acc, void const* XXH_RESTRICT input,
                 void const* XXH_RESTRICT secret, size_t lane);

XXH_FORCE_INLINE void
XXH3_scalarScrambleRound(void* XXH_RESTRICT acc,
                         void const* XXH_RESTRICT secret, size_t lane);

/*!
 * @internal
 * @brief The bulk processing loop for NEON and WASM SIMD128.
 *
 * The NEON code path is actually partially scalar when running on AArch64. This
 * is to optimize the pipelining and can have up to 15% speedup depending on the
 * CPU, and it also mitigates some GCC codegen issues.
 *
 * @see XXH3_NEON_LANES for configuring this and details about this optimization.
 *
 * NEON's 32-bit to 64-bit long multiply takes a half vector of 32-bit
 * integers instead of the other platforms which mask full 64-bit vectors,
 * so the setup is more complicated than just shifting right.
 *
 * Additionally, there is an optimization for 4 lanes at once noted below.
 *
 * Since, as stated, the most optimal amount of lanes for Cortexes is 6,
 * there needs to be *three* versions of the accumulate operation used
 * for the remaining 2 lanes.
 *
 * WASM's SIMD128 uses SIMDe's arm_neon.h polyfill because the intrinsics overlap
 * nearly perfectly.
 */

XXH_FORCE_INLINE void
XXH3_accumulate_512_neon( void* XXH_RESTRICT acc,
                    const void* XXH_RESTRICT input,
                    const void* XXH_RESTRICT secret)
{
    XXH_ASSERT((((size_t)acc) & 15) == 0);
    XXH_STATIC_ASSERT(XXH3_NEON_LANES > 0 && XXH3_NEON_LANES <= XXH_ACC_NB && XXH3_NEON_LANES % 2 == 0);
    {   /* GCC for darwin arm64 does not like aliasing here */
        xxh_aliasing_uint64x2_t* const xacc = (xxh_aliasing_uint64x2_t*) acc;
        /* We don't use a uint32x4_t pointer because it causes bus errors on ARMv7. */
        uint8_t const* xinput = (const uint8_t *) input;
        uint8_t const* xsecret  = (const uint8_t *) secret;

        size_t i;
#ifdef __wasm_simd128__
        /*
         * On WASM SIMD128, Clang emits direct address loads when XXH3_kSecret
         * is constant propagated, which results in it converting it to this
         * inside the loop:
         *
         *    a = v128.load(XXH3_kSecret +  0 + $secret_offset, offset = 0)
         *    b = v128.load(XXH3_kSecret + 16 + $secret_offset, offset = 0)
         *    ...
         *
         * This requires a full 32-bit address immediate (and therefore a 6 byte
         * instruction) as well as an add for each offset.
         *
         * Putting an asm guard prevents it from folding (at the cost of losing
         * the alignment hint), and uses the free offset in `v128.load` instead
         * of adding secret_offset each time which overall reduces code size by
         * about a kilobyte and improves performance.
         */
        XXH_COMPILER_GUARD(xsecret);
#endif
        /* Scalar lanes use the normal scalarRound routine */
        for (i = XXH3_NEON_LANES; i < XXH_ACC_NB; i++) {
            XXH3_scalarRound(acc, input, secret, i);
        }
        i = 0;
        /* 4 NEON lanes at a time. */
        for (; i+1 < XXH3_NEON_LANES / 2; i+=2) {
            /* data_vec = xinput[i]; */
            uint64x2_t data_vec_1 = XXH_vld1q_u64(xinput  + (i * 16));
            uint64x2_t data_vec_2 = XXH_vld1q_u64(xinput  + ((i+1) * 16));
            /* key_vec  = xsecret[i];  */
            uint64x2_t key_vec_1  = XXH_vld1q_u64(xsecret + (i * 16));
            uint64x2_t key_vec_2  = XXH_vld1q_u64(xsecret + ((i+1) * 16));
            /* data_swap = swap(data_vec) */
            uint64x2_t data_swap_1 = vextq_u64(data_vec_1, data_vec_1, 1);
            uint64x2_t data_swap_2 = vextq_u64(data_vec_2, data_vec_2, 1);
            /* data_key = data_vec ^ key_vec; */
            uint64x2_t data_key_1 = veorq_u64(data_vec_1, key_vec_1);
            uint64x2_t data_key_2 = veorq_u64(data_vec_2, key_vec_2);

            /*
             * If we reinterpret the 64x2 vectors as 32x4 vectors, we can use a
             * de-interleave operation for 4 lanes in 1 step with `vuzpq_u32` to
             * get one vector with the low 32 bits of each lane, and one vector
             * with the high 32 bits of each lane.
             *
             * The intrinsic returns a double vector because the original ARMv7-a
             * instruction modified both arguments in place. AArch64 and SIMD128 emit
             * two instructions from this intrinsic.
             *
             *  [ dk11L | dk11H | dk12L | dk12H ] -> [ dk11L | dk12L | dk21L | dk22L ]
             *  [ dk21L | dk21H | dk22L | dk22H ] -> [ dk11H | dk12H | dk21H | dk22H ]
             */
            uint32x4x2_t unzipped = vuzpq_u32(
                vreinterpretq_u32_u64(data_key_1),
                vreinterpretq_u32_u64(data_key_2)
            );
            /* data_key_lo = data_key & 0xFFFFFFFF */
            uint32x4_t data_key_lo = unzipped.val[0];
            /* data_key_hi = data_key >> 32 */
            uint32x4_t data_key_hi = unzipped.val[1];
            /*
             * Then, we can split the vectors horizontally and multiply which, as for most
             * widening intrinsics, have a variant that works on both high half vectors
             * for free on AArch64. A similar instruction is available on SIMD128.
             *
             * sum = data_swap + (u64x2) data_key_lo * (u64x2) data_key_hi
             */
            uint64x2_t sum_1 = XXH_vmlal_low_u32(data_swap_1, data_key_lo, data_key_hi);
            uint64x2_t sum_2 = XXH_vmlal_high_u32(data_swap_2, data_key_lo, data_key_hi);
            /*
             * Clang reorders
             *    a += b * c;     // umlal   swap.2d, dkl.2s, dkh.2s
             *    c += a;         // add     acc.2d, acc.2d, swap.2d
             * to
             *    c += a;         // add     acc.2d, acc.2d, swap.2d
             *    c += b * c;     // umlal   acc.2d, dkl.2s, dkh.2s
             *
             * While it would make sense in theory since the addition is faster,
             * for reasons likely related to umlal being limited to certain NEON
             * pipelines, this is worse. A compiler guard fixes this.
             */
            XXH_COMPILER_GUARD_CLANG_NEON(sum_1);
            XXH_COMPILER_GUARD_CLANG_NEON(sum_2);
            /* xacc[i] = acc_vec + sum; */
            xacc[i]   = vaddq_u64(xacc[i], sum_1);
            xacc[i+1] = vaddq_u64(xacc[i+1], sum_2);
        }
        /* Operate on the remaining NEON lanes 2 at a time. */
        for (; i < XXH3_NEON_LANES / 2; i++) {
            /* data_vec = xinput[i]; */
            uint64x2_t data_vec = XXH_vld1q_u64(xinput  + (i * 16));
            /* key_vec  = xsecret[i];  */
            uint64x2_t key_vec  = XXH_vld1q_u64(xsecret + (i * 16));
            /* acc_vec_2 = swap(data_vec) */
            uint64x2_t data_swap = vextq_u64(data_vec, data_vec, 1);
            /* data_key = data_vec ^ key_vec; */
            uint64x2_t data_key = veorq_u64(data_vec, key_vec);
            /* For two lanes, just use VMOVN and VSHRN. */
            /* data_key_lo = data_key & 0xFFFFFFFF; */
            uint32x2_t data_key_lo = vmovn_u64(data_key);
            /* data_key_hi = data_key >> 32; */
            uint32x2_t data_key_hi = vshrn_n_u64(data_key, 32);
            /* sum = data_swap + (u64x2) data_key_lo * (u64x2) data_key_hi; */
            uint64x2_t sum = vmlal_u32(data_swap, data_key_lo, data_key_hi);
            /* Same Clang workaround as before */
            XXH_COMPILER_GUARD_CLANG_NEON(sum);
            /* xacc[i] = acc_vec + sum; */
            xacc[i] = vaddq_u64 (xacc[i], sum);
        }
    }
}
XXH_FORCE_INLINE XXH3_ACCUMULATE_TEMPLATE(neon)

XXH_FORCE_INLINE void
XXH3_scrambleAcc_neon(void* XXH_RESTRICT acc, const void* XXH_RESTRICT secret)
{
    XXH_ASSERT((((size_t)acc) & 15) == 0);

    {   xxh_aliasing_uint64x2_t* xacc       = (xxh_aliasing_uint64x2_t*) acc;
        uint8_t const* xsecret = (uint8_t const*) secret;

        size_t i;
        /* WASM uses operator overloads and doesn't need these. */
#ifndef __wasm_simd128__
        /* { prime32_1, prime32_1 } */
        uint32x2_t const kPrimeLo = vdup_n_u32(XXH_PRIME32_1);
        /* { 0, prime32_1, 0, prime32_1 } */
        uint32x4_t const kPrimeHi = vreinterpretq_u32_u64(vdupq_n_u64((xxh_u64)XXH_PRIME32_1 << 32));
#endif

        /* AArch64 uses both scalar and neon at the same time */
        for (i = XXH3_NEON_LANES; i < XXH_ACC_NB; i++) {
            XXH3_scalarScrambleRound(acc, secret, i);
        }
        for (i=0; i < XXH3_NEON_LANES / 2; i++) {
            /* xacc[i] ^= (xacc[i] >> 47); */
            uint64x2_t acc_vec  = xacc[i];
            uint64x2_t shifted  = vshrq_n_u64(acc_vec, 47);
            uint64x2_t data_vec = veorq_u64(acc_vec, shifted);

            /* xacc[i] ^= xsecret[i]; */
            uint64x2_t key_vec  = XXH_vld1q_u64(xsecret + (i * 16));
            uint64x2_t data_key = veorq_u64(data_vec, key_vec);
            /* xacc[i] *= XXH_PRIME32_1 */
#ifdef __wasm_simd128__
            /* SIMD128 has multiply by u64x2, use it instead of expanding and scalarizing */
            xacc[i] = data_key * XXH_PRIME32_1;
#else
            /*
             * Expanded version with portable NEON intrinsics
             *
             *    lo(x) * lo(y) + (hi(x) * lo(y) << 32)
             *
             * prod_hi = hi(data_key) * lo(prime) << 32
             *
             * Since we only need 32 bits of this multiply a trick can be used, reinterpreting the vector
             * as a uint32x4_t and multiplying by { 0, prime, 0, prime } to cancel out the unwanted bits
             * and avoid the shift.
             */
            uint32x4_t prod_hi = vmulq_u32 (vreinterpretq_u32_u64(data_key), kPrimeHi);
            /* Extract low bits for vmlal_u32  */
            uint32x2_t data_key_lo = vmovn_u64(data_key);
            /* xacc[i] = prod_hi + lo(data_key) * XXH_PRIME32_1; */
            xacc[i] = vmlal_u32(vreinterpretq_u64_u32(prod_hi), data_key_lo, kPrimeLo);
#endif
        }
    }
}

#endif  /* XXH_VECTOR == XXH_NEON */

#endif  /* !XXH_NO_XXH3 */
