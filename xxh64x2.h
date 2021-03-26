#if defined (__cplusplus)
extern "C" {
#endif

#ifndef XXH64X2_H
#define XXH64X2_H

/* ===   Dependencies   === */
#undef XXH_INLINE_ALL   /* avoid redefinition */
#define XXH_INLINE_ALL
#include "xxhash.h"

#ifdef XXH_NAMESPACE
#  define XXH64X2 XXH_NAME2(XXH_NAMESPACE, XXH64X2)
#endif

XXH_PUBLIC_API XXH128_hash_t XXH64X2(const void* input, size_t length, XXH64_hash_t seed);

XXH_FORCE_INLINE XXH128_hash_t XXH64X2_avalanche(xxh_u64 const h64, xxh_u64 const hi)
{
    XXH128_hash_t const r = { XXH64_avalanche(h64), XXH64_avalanche(hi) };
    return r;
}

static XXH128_hash_t
XXH64X2_finalize(xxh_u64 h64, xxh_u64 hi, const xxh_u8* ptr, size_t len, XXH_alignment align)
{
    len &= 31;
    while (len >= 8) {
        xxh_u64 const k1 = XXH64_round(0, XXH_get64bits(ptr));
        ptr += 8;
        h64 ^= k1;
        h64  = XXH_rotl64(h64,27) * XXH_PRIME64_1 + XXH_PRIME64_4;
        hi  ^= k1 + h64;
        len -= 8;
    }
    if (len >= 4) {
        xxh_u64 const d = (xxh_u64)(XXH_get32bits(ptr)) * XXH_PRIME64_1;
        ptr += 4;
        h64 ^= d;
        h64  = XXH_rotl64(h64, 23) * XXH_PRIME64_2 + XXH_PRIME64_3;
        hi  ^= d + h64;
        len -= 4;
    }
    while (len > 0) {
        xxh_u64 const d = (*ptr++) * XXH_PRIME64_5;
        h64 ^= d;
        h64 = XXH_rotl64(h64, 11) * XXH_PRIME64_1;
        hi  ^= d + h64;
        --len;
    }
    return  XXH64X2_avalanche(h64, hi);
}

XXH_FORCE_INLINE XXH128_hash_t
XXH64X2_endian_align(const xxh_u8* input, size_t len, xxh_u64 seed, XXH_alignment align)
{
    const xxh_u8* bEnd = input + len;
    xxh_u64 h64, hi;

#if defined(XXH_ACCEPT_NULL_INPUT_POINTER) && (XXH_ACCEPT_NULL_INPUT_POINTER>=1)
    if (input==NULL) {
        len=0;
        bEnd=input=(const xxh_u8*)(size_t)32;
    }
#endif

    if (len>=32) {
        const xxh_u8* const limit = bEnd - 32;
        xxh_u64 v1 = seed + XXH_PRIME64_1 + XXH_PRIME64_2;
        xxh_u64 v2 = seed + XXH_PRIME64_2;
        xxh_u64 v3 = seed + 0;
        xxh_u64 v4 = seed - XXH_PRIME64_1;

        do {
            v1 = XXH64_round(v1, XXH_get64bits(input)); input+=8;
            v2 = XXH64_round(v2, XXH_get64bits(input)); input+=8;
            v3 = XXH64_round(v3, XXH_get64bits(input)); input+=8;
            v4 = XXH64_round(v4, XXH_get64bits(input)); input+=8;
        } while (input<=limit);

        h64 = hi = XXH_rotl64(v1, 1) + XXH_rotl64(v2, 7) + XXH_rotl64(v3, 12) + XXH_rotl64(v4, 18);
        h64 = XXH64_mergeRound(h64, v1);
        h64 = XXH64_mergeRound(h64, v2);
        h64 = XXH64_mergeRound(h64, v3);
        h64 = XXH64_mergeRound(h64, v4);

        hi += (hi ^ v1 ^ v2 ^ v3 ^ v4) * XXH_PRIME64_1 + XXH_PRIME64_4;
    } else {
        h64  = seed + XXH_PRIME64_5;
        hi   = seed - XXH_PRIME64_3;
    }

    h64 += (xxh_u64) len;
    hi  -= (xxh_u64) len;

    return XXH64X2_finalize(h64, hi, input, len, align);
}

XXH_PUBLIC_API XXH128_hash_t XXH64X2(const void* input, size_t len, XXH64_hash_t seed)
{
    if (XXH_FORCE_ALIGN_CHECK) {
        if ((((size_t)input) & 7)==0) {  /* Input is aligned, let's leverage the speed advantage */
            return XXH64X2_endian_align((const xxh_u8*)input, len, seed, XXH_aligned);
    }   }

    return XXH64X2_endian_align((const xxh_u8*)input, len, seed, XXH_unaligned);
}


#endif /* XXH64X2_H */

#if defined (__cplusplus)
}
#endif
