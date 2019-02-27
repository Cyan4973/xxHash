#ifndef XXH3_H
#define XXH3_H


/* ===   Dependencies   === */

#undef XXH_INLINE_ALL   /* in case it's already defined */
#define XXH_INLINE_ALL
#include "xxhash.h"

#define NDEBUG
#include <assert.h>


/* ===   Compiler versions   === */

#if !(defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)   /* C99+ */
#  define restrict   /* disable */
#endif

#if defined(__GNUC__)
#  if defined(__SSE2__)
#    include <x86intrin.h>
#  endif
#  define ALIGN(n)      __attribute__ ((aligned(n)))
#elif defined(_MSC_VER)
#  include <intrin.h>
#  define ALIGN(n)      __declspec(align(n))
#else
#  define ALIGN(n)   /* disabled */
#endif



/* ==========================================
 * Vectorization detection
 * ========================================== */
#define XXH_SCALAR 0
#define XXH_SSE2   1
#define XXH_AVX2   2

#ifndef XXH_VECTOR    /* can be defined on command line */
#  if defined(__AVX2__)
#    define XXH_VECTOR XXH_AVX2
#  elif defined(__SSE2__)
#    define XXH_VECTOR XXH_SSE2
#  else
#    define XXH_VECTOR XXH_SCALAR
#  endif
#endif



/* ==========================================
 * Short keys
 * ========================================== */

static U64 XXH3_mixHigh(U64 val) {
  return val ^ (val >> 47);
}

static U64 XXH3_finalMerge_2u64(U64 ll1, U64 ll2, U64 mul)
{
    U64 const ll11 = XXH3_mixHigh((ll1 ^ ll2) * mul);
    U64 const ll21 = XXH3_mixHigh((ll2 ^ ll11) * mul);
    return ll21 * mul;
}

static U64 XXH3_finalMerge_4u64(U64 ll1, U64 ll2, U64 ll3, U64 ll4, U64 mul)
{
    U64 const ll11 = XXH_rotl64(ll1 + ll2, 43) + XXH_rotl64(ll3, 30) + ll4;
    U64 const ll12 = ll1 + XXH_rotl64(ll2, 18) + ll3 + PRIME64_3;

    return XXH3_finalMerge_2u64(ll11, ll12, mul);
}

static U64 XXH3_finalMerge_8u64(U64 ll1, U64 ll2, U64 ll3, U64 ll4,
                                U64 ll5, U64 ll6, U64 ll7, U64 ll8,
                                U64 mul)
{
    U64 const ll11 = XXH_rotl64(ll1 + ll7, 21) + (XXH_rotl64(ll2, 34) + ll3) * 9;
    U64 const ll12 = XXH_rotl64(((ll1 + ll2) ^ ll4), 17) + ll6 + PRIME64_5;
    U64 const ll13 = XXH_rotl64(ll5 * PRIME64_4 + ll6, 46) + ll3;
    U64 const ll14 = XXH_rotl64(ll8, 23) + XXH_rotl64(ll5 + ll7, 12);

    U64 const ll21 = (XXH_swap64((ll11 + ll12) * PRIME64_1) + ll13) * PRIME64_3 + ll8;
    U64 const ll22 = (XXH_swap64((ll12 + ll14) * PRIME64_2) + ll4) * mul;

    return XXH3_finalMerge_2u64(ll21, ll22, mul);
}


XXH_FORCE_INLINE U64 XXH3_len_1to3_64b(const void* data, size_t len)
{
    assert(data != NULL);
    assert(len > 0 && len <= 3);
    {   BYTE const c1 = ((const BYTE*)data)[0];
        BYTE const c2 = ((const BYTE*)data)[len >> 1];
        BYTE const c3 = ((const BYTE*)data)[len - 1];
        U32  const l1 = (U32)(c1) + ((U32)(c2) << 8);
        U32  const l2 = (U32)(len) + ((U32)(c3) << 2);
        U64  const ll3 = (l1 * PRIME64_2) ^ (l2 * PRIME64_1);
        return XXH3_mixHigh(ll3) * PRIME64_3;
    }
}


XXH_FORCE_INLINE U64 XXH3_len_4to8_64b(const void* data, size_t len)
{
    assert(data != NULL);
    assert(len >= 4 && len <= 8);
    {   U64 const mul = PRIME64_2 + (len * 2);  /* keep it odd */
        U64 const ll1 = XXH_read32(data);
        U64 const ll2 = XXH_read32((const BYTE*)data + len - 4) + PRIME64_1;
        return XXH3_finalMerge_2u64((len-1) + (ll1 << 3), ll2, mul);
    }
}

XXH_FORCE_INLINE U64 XXH3_len_9to16_64b(const void* data, size_t len)
{
    assert(data != NULL);
    assert(len >= 9 && len <= 16);
    {   U64 const ll1 = XXH_read64(data) + PRIME64_1;
        U64 const ll2 = XXH_read64((const BYTE*)data + len - 8);
        U64 const mul = PRIME64_2 + (len * 2);  /* keep it odd */
        U64 const ll11 = (ll1 * mul) + XXH_rotl64(ll2, 23);
        U64 const ll12 = (ll2 * mul) + XXH_rotl64(ll1, 37);
        return XXH3_finalMerge_2u64(ll11, ll12, mul);
    }
}

XXH_FORCE_INLINE U64 XXH3_len_1to16_64b(const void* data, size_t len)
{
    assert(data != NULL);
    assert(len > 0 && len <= 16);
    {   if (len > 8) return XXH3_len_9to16_64b(data, len);
        if (len >= 4) return XXH3_len_4to8_64b(data, len);
        return XXH3_len_1to3_64b(data, len);
    }
}


static U64 XXH3_len_17to32_64b(const void* data, size_t len)
{
    assert(data != NULL);
    assert(len > 16 && len <= 32);

    {   const BYTE* const p = (const BYTE*)data;

        U64 const mul = PRIME64_3 + len * 2;  /* keep it odd */
        U64 const ll1 = XXH_read64(p) * PRIME64_1;
        U64 const ll2 = XXH_read64(p + 8);
        U64 const ll3 = XXH_read64(p + len - 8) * mul;
        U64 const ll4 = XXH_read64(p + len - 16) * PRIME64_2;

        return XXH3_finalMerge_4u64(ll1, ll2, ll3, ll4, mul);
    }
}


static U64 XXH3_len_33to64_64b(const void* data, size_t len)
{
    assert(data != NULL);
    assert(len > 33 && len <= 64);

    {   const BYTE* const p = (const BYTE*)data;

        U64 const mul = PRIME64_2 + len * 2;   /* keep it odd */

        U64 const ll1 = XXH_read64(p);
        U64 const ll2 = XXH_read64(p + 8);
        U64 const ll3 = XXH_read64(p + 16);
        U64 const ll4 = XXH_read64(p + 24);
        U64 const ll5 = XXH_read64(p + len - 32);
        U64 const ll6 = XXH_read64(p + len - 24);
        U64 const ll7 = XXH_read64(p + len - 16);
        U64 const ll8 = XXH_read64(p + len - 8);

        return XXH3_finalMerge_8u64(ll1, ll2, ll3, ll4, ll5, ll6, ll7, ll8, mul);
    }
}


static U64 XXH3_len_65to96_64b(const void* data, size_t len)
{
    assert(data != NULL);
    assert(len > 64 && len <= 96);

    {   const BYTE* const p = (const BYTE*)data;

        U64 const ll1 = XXH3_len_33to64_64b(data, 64);
        U64 const ll2 = XXH3_len_17to32_64b(p + len - 32, 32);
        return XXH3_finalMerge_2u64(ll1, ll2, PRIME64_1 + 2*len);
    }
}

static U64 XXH3_len_97to128_64b(const void* data, size_t len)
{
    assert(data != NULL);
    assert(len > 96 && len <= 128);

    {   const BYTE* const p = (const BYTE*)data;

        U64 const ll1 = XXH3_len_33to64_64b(data, 64);
        U64 const ll2 = XXH3_len_33to64_64b(p + 64, len - 64);
        return XXH3_finalMerge_2u64(ll1, ll2, PRIME64_1 + 2*len);
    }
}



/* ==========================================
 * Long keys
 * ========================================== */

#define STRIPE_LEN 64
#define STRIPE_ELTS (STRIPE_LEN / sizeof(U32))
#define KEYSET_DEFAULT_SIZE 48   /* minimum 32 */


ALIGN(64) static const U32 kKey[KEYSET_DEFAULT_SIZE] = {
    0xb8fe6c39,0x23a44bbe,0x7c01812c,0xf721ad1c,
    0xded46de9,0x839097db,0x7240a4a4,0xb7b3671f,
    0xcb79e64e,0xccc0e578,0x825ad07d,0xccff7221,
    0xb8084674,0xf743248e,0xe03590e6,0x813a264c,
    0x3c2852bb,0x91c300cb,0x88d0658b,0x1b532ea3,
    0x71644897,0xa20df94e,0x3819ef46,0xa9deacd8,
    0xa8fa763f,0xe39c343f,0xf9dcbbc7,0xc70b4f1d,
    0x8a51e04b,0xcdb45931,0xc89f7ec9,0xd9787364,

    0xeac5ac83,0x34d3ebc3,0xc581a0ff,0xfa1363eb,
    0x170ddd51,0xb7f0da49,0xd3165526,0x29d4689e,
    0x2b16be58,0x7d47a1fc,0x8ff8b8d1,0x7ad031ce,
    0x45cb3a8f,0x95160428,0xafd7fbca,0xbb4b407e,
};

#define ACC_NB (STRIPE_LEN / sizeof(U64))

XXH_FORCE_INLINE void
XXH3_accumulate_512(void* acc, const void *restrict data, const void *restrict key)
{
#if (XXH_VECTOR == XXH_AVX2)

    assert(((size_t)acc) & 31 == 0);
    {                   __m256i* const xacc  =       (__m256i *) acc;
                  const __m256i* const xdata = (const __m256i *) data;
        ALIGN(32) const __m256i* const xkey  = (const __m256i *) key;

        for (size_t i=0; i < STRIPE_LEN/sizeof(__m256i); i++) {
            __m256i const d   = _mm256_loadu_si256 (xdata+i);
            __m256i const k   = _mm256_loadu_si256 (xkey+i);
            __m256i const dk  = _mm256_add_epi32 (d,k);                                  /* uint32 dk[8]  = {d0+k0, d1+k1, d2+k2, d3+k3, ...} */
            __m256i const res = _mm256_mul_epu32 (dk, _mm256_shuffle_epi32 (dk,0x31));   /* uint64 res[4] = {dk0*dk1, dk2*dk3, ...} */
            xacc[i]           = _mm256_add_epi64(res, xacc[i]);                          /* xacc must be aligned on 32 bytes boundaries */
        }
    }

#elif (XXH_VECTOR == XXH_SSE2)

    assert(((size_t)acc) & 15 == 0);
    {                   __m128i* const xacc  =       (__m128i *) acc;
                  const __m128i* const xdata = (const __m128i *) data;
        ALIGN(16) const __m128i* const xkey  = (const __m128i *) key;

        size_t i;
        for (i=0; i < STRIPE_LEN/sizeof(__m128i); i++) {
            __m128i const d   = _mm_loadu_si128 (xdata+i);
            __m128i const k   = _mm_loadu_si128 (xkey+i);
            __m128i const dk  = _mm_add_epi32 (d,k);                               /* uint32 dk[4]  = {d0+k0, d1+k1, d2+k2, d3+k3} */
            __m128i const res = _mm_mul_epu32 (dk, _mm_shuffle_epi32 (dk,0x31));   /* uint64 res[2] = {dk0*dk1,dk2*dk3} */
            xacc[i]           = _mm_add_epi64(res, xacc[i]);                       /* xacc must be aligned on 16 bytes boundaries */
        }
    }

#else   /* scalar variant */

          U64* const xacc  =       (U64*) acc;
    const U32* const xdata = (const U32*) data;
    const U32* const xkey  = (const U32*) key;

    int i;
    for (i=0; i < (int)ACC_NB; i++) {
        int const left = 2*i;
        int const right= 2*i + 1;
        xacc[i] += (xdata[left] + xkey[left]) * (U64)(xdata[right] + xkey[right]);
    }

#endif
}

static void XXH3_scrambleAcc(void* acc, const void* key)
{
#if (XXH_VECTOR == XXH_AVX2)

    assert(((size_t)acc) & 31 == 0);
    {   __m256i* const xacc = (__m256i*) acc;
        const __m256i* const xkey  = (const __m256i *) key;

        __m256i const xor_p5 = _mm256_set1_epi64x(PRIME64_5);

        for (size_t i=0; i < STRIPE_LEN/sizeof(__m256i); i++) {
            __m256i data = xacc[i];
            __m256i const shifted = _mm256_srli_epi64(data, 47);
            data = _mm256_xor_si256(data, shifted);
            data = _mm256_xor_si256(data, xor_p5);

            {   __m256i const k   = _mm256_loadu_si256 (xkey+i);
                __m256i const dk  = _mm256_mul_epu32 (data,k);          /* U32 dk[4]  = {d0+k0, d1+k1, d2+k2, d3+k3} */

                __m256i const d2  = _mm256_shuffle_epi32 (data,0x31);
                __m256i const k2  = _mm256_shuffle_epi32 (k,0x31);
                __m256i const dk2 = _mm256_mul_epu32 (d2,k2);           /* U32 dk[4]  = {d0+k0, d1+k1, d2+k2, d3+k3} */

                xacc[i] = _mm256_xor_si256(dk, dk2);
        }   }
    }

#elif (XXH_VECTOR == XXH_SSE2)

    assert(((size_t)acc) & 15 == 0);
    {   __m128i* const xacc = (__m128i*) acc;
        const __m128i* const xkey  = (const __m128i *) key;
        __m128i const xor_p5 = _mm_set1_epi64((__m64)PRIME64_5);

        size_t i;
        for (i=0; i < STRIPE_LEN/sizeof(__m128i); i++) {
            __m128i data = xacc[i];
            __m128i const shifted = _mm_srli_epi64(data, 47);
            data = _mm_xor_si128(data, shifted);
            data = _mm_xor_si128(data, xor_p5);

            {   __m128i const k   = _mm_loadu_si128 (xkey+i);
                __m128i const dk  = _mm_mul_epu32 (data,k);          /* U32 dk[4]  = {d0+k0, d1+k1, d2+k2, d3+k3} */

                __m128i const d2  = _mm_shuffle_epi32 (data,0x31);
                __m128i const k2  = _mm_shuffle_epi32 (k,0x31);
                __m128i const dk2 = _mm_mul_epu32 (d2,k2);           /* U32 dk[4]  = {d0+k0, d1+k1, d2+k2, d3+k3} */

                xacc[i] = _mm_xor_si128(dk, dk2);
        }   }
    }

#else   /* scalar variant */

          U64* const xacc =       (U64*) acc;
    const U32* const xkey = (const U32*) key;

    int i;
    for (i=0; i < (int)ACC_NB; i++) {
        int const left = 2*i;
        int const right= 2*i + 1;
        xacc[i] ^= xacc[i] >> 47;
        xacc[i] ^= PRIME64_5;

        {   U64 p1 = (xacc[i] >> 32) * xkey[left];
            U64 p2 = (xacc[i] & 0xFFFFFFFF) * xkey[right];
            xacc[i] = p1 ^ p2;
    }   }

#endif
}

static void XXH3_accumulate(U64* acc, const void* restrict data, const U32* restrict key, size_t nbStripes)
{
    size_t n;
    for (n = 0; n < nbStripes; n++ ) {
        XXH3_accumulate_512(acc, (const BYTE*)data + n*STRIPE_LEN, key);
        key += 2;
    }
}


__attribute__((noinline)) static U64    /* It seems better for XXH3_64b to have hashLong not inlined : may mess up the switch case ? */
XXH3_hashLong(const void* data, size_t len)
{
    ALIGN(64) U64 acc[ACC_NB] = { 0, PRIME64_1, PRIME64_2, PRIME64_3, PRIME64_4, PRIME64_5 };

    #define NB_KEYS ((KEYSET_DEFAULT_SIZE - STRIPE_ELTS) / 2)

    size_t const block_len = STRIPE_LEN * NB_KEYS;
    size_t const nb_blocks = len / block_len;

    size_t n;
    for (n = 0; n < nb_blocks; n++) {
        XXH3_accumulate(acc, (const BYTE*)data + n*block_len, kKey, NB_KEYS);
        XXH3_scrambleAcc(acc, kKey + (KEYSET_DEFAULT_SIZE - STRIPE_ELTS));
    }

    /* last partial block */
    assert(len > STRIPE_LEN);
    {   size_t const nbStripes = (len % block_len) / STRIPE_LEN;
        assert(nbStripes < NB_KEYS);
        XXH3_accumulate(acc, (const BYTE*)data + nb_blocks*block_len, kKey, nbStripes);

        /* last stripe */
        if (len & (STRIPE_LEN - 1)) {
            const BYTE* const p = (const BYTE*) data + len - STRIPE_LEN;
            XXH3_accumulate_512(acc, p, kKey + nbStripes*2);
    }   }

    /* converge into final hash */
    return XXH3_finalMerge_8u64(acc[0] + len, acc[1], acc[2], acc[3], acc[4], acc[5], acc[6], acc[7] - len, PRIME64_2 + len*2);
}



/* ==========================================
 * Public entry point
 * ========================================== */

XXH_PUBLIC_API XXH64_hash_t XXH3_64b(const void* data, size_t len)
{
    switch ((len-1) / 16) {  /* intentional underflow */
        case 0: return XXH3_len_1to16_64b(data, len);
        case 1: return XXH3_len_17to32_64b(data, len);
        case 2:
        case 3: return XXH3_len_33to64_64b(data, len);  /* 33-64 */
        default:;
    }
    if (len==0) return 0;
    if (len <= 96) return XXH3_len_65to96_64b(data, len);
    if (len <= 128) return XXH3_len_97to128_64b(data, len);
    return XXH3_hashLong(data, len);
}



#endif  /* XXH3_H */
