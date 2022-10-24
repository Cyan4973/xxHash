xxHash fast digest algorithm
======================

### Notices

Copyright (c) Yann Collet

Permission is granted to copy and distribute this document
for any purpose and without charge,
including translations into other languages
and incorporation into compilations,
provided that the copyright notice and this notice are preserved,
and that any substantive changes or deletions from the original
are clearly marked.
Distribution of this document is unlimited.

### Version

0.1.1 (10/10/18)


Table of Contents
---------------------
- [Introduction](#introduction)
- [XXH32 algorithm description](#xxh32-algorithm-description)
- [XXH64 algorithm description](#xxh64-algorithm-description)
- [XXH3 algorithm description](#xxh3-algorithm-overview)
   - [Small inputs](#xxh3-algorithm-description-for-small-inputs)
   - [Medium inputs](#xxh3-algorithm-description-for-medium-inputs)
   - [Large inputs](#xxh3-algorithm-description-for-large-inputs)
- [Performance considerations](#performance-considerations)
- [Reference Implementation](#reference-implementation)


Introduction
----------------

This document describes the xxHash digest algorithm for both 32-bit and 64-bit variants, named `XXH32` and `XXH64`. The algorithm takes an input a message of arbitrary length and an optional seed value, then produces an output of 32 or 64-bit as "fingerprint" or "digest".

xxHash is primarily designed for speed. It is labeled non-cryptographic, and is not meant to avoid intentional collisions (same digest for 2 different messages), or to prevent producing a message with a predefined digest.

XXH32 is designed to be fast on 32-bit machines.
XXH64 is designed to be fast on 64-bit machines.
Both variants produce different output.
However, a given variant shall produce exactly the same output, irrespective of the cpu / os used. In particular, the result remains identical whatever the endianness and width of the cpu is.

### Operation notations

All operations are performed modulo {32,64} bits. Arithmetic overflows are expected.
`XXH32` uses 32-bit modular operations. `XXH64` uses 64-bit modular operations.

- `+`: denotes modular addition
- `*`: denotes modular multiplication
- `X <<< s`: denotes the value obtained by circularly shifting (rotating) `X` left by `s` bit positions.
- `X >> s`: denotes the value obtained by shifting `X` right by s bit positions. Upper `s` bits become `0`.
- `X xor Y`: denotes the bit-wise XOR of `X` and `Y` (same width).


XXH32 Algorithm Description
-------------------------------------

### Overview

We begin by supposing that we have a message of any length `L` as input, and that we wish to find its digest. Here `L` is an arbitrary nonnegative integer; `L` may be zero. The following steps are performed to compute the digest of the message.

The algorithm collect and transform input in _stripes_ of 16 bytes. The transforms are stored inside 4 "accumulators", each one storing an unsigned 32-bit value. Each accumulator can be processed independently in parallel, speeding up processing for cpu with multiple execution units.

The algorithm uses 32-bits addition, multiplication, rotate, shift and xor operations. Many operations require some 32-bits prime number constants, all defined below:

```c
  static const u32 PRIME32_1 = 0x9E3779B1U;  // 0b10011110001101110111100110110001
  static const u32 PRIME32_2 = 0x85EBCA77U;  // 0b10000101111010111100101001110111
  static const u32 PRIME32_3 = 0xC2B2AE3DU;  // 0b11000010101100101010111000111101
  static const u32 PRIME32_4 = 0x27D4EB2FU;  // 0b00100111110101001110101100101111
  static const u32 PRIME32_5 = 0x165667B1U;  // 0b00010110010101100110011110110001
```

These constants are prime numbers, and feature a good mix of bits 1 and 0, neither too regular, nor too dissymmetric. These properties help dispersion capabilities.

### Step 1. Initialize internal accumulators

Each accumulator gets an initial value based on optional `seed` input. Since the `seed` is optional, it can be `0`.

```c
  u32 acc1 = seed + PRIME32_1 + PRIME32_2;
  u32 acc2 = seed + PRIME32_2;
  u32 acc3 = seed + 0;
  u32 acc4 = seed - PRIME32_1;
```

#### Special case: input is less than 16 bytes

When the input is too small (< 16 bytes), the algorithm will not process any stripes. Consequently, it will not make use of parallel accumulators.

In this case, a simplified initialization is performed, using a single accumulator:

```c
  u32 acc  = seed + PRIME32_5;
```

The algorithm then proceeds directly to step 4.

### Step 2. Process stripes

A stripe is a contiguous segment of 16 bytes.
It is evenly divided into 4 _lanes_, of 4 bytes each.
The first lane is used to update accumulator 1, the second lane is used to update accumulator 2, and so on.

Each lane read its associated 32-bit value using __little-endian__ convention.

For each {lane, accumulator}, the update process is called a _round_, and applies the following formula:

```c
  accN = accN + (laneN * PRIME32_2);
  accN = accN <<< 13;
  accN = accN * PRIME32_1;
```

This shuffles the bits so that any bit from input _lane_ impacts several bits in output _accumulator_. All operations are performed modulo 2^32.

Input is consumed one full stripe at a time. Step 2 is looped as many times as necessary to consume the whole input, except for the last remaining bytes which cannot form a stripe (< 16 bytes).
When that happens, move to step 3.

### Step 3. Accumulator convergence

All 4 lane accumulators from the previous steps are merged to produce a single remaining accumulator of the same width (32-bit). The associated formula is as follows:

```c
  acc = (acc1 <<< 1) + (acc2 <<< 7) + (acc3 <<< 12) + (acc4 <<< 18);
```

### Step 4. Add input length

The input total length is presumed known at this stage. This step is just about adding the length to accumulator, so that it participates to final mixing.

```c
  acc = acc + (u32)inputLength;
```

Note that, if input length is so large that it requires more than 32-bits, only the lower 32-bits are added to the accumulator.

### Step 5. Consume remaining input

There may be up to 15 bytes remaining to consume from the input.
The final stage will digest them according to following pseudo-code:

```c
  while (remainingLength >= 4) {
      lane = read_32bit_little_endian(input_ptr);
      acc = acc + lane * PRIME32_3;
      acc = (acc <<< 17) * PRIME32_4;
      input_ptr += 4; remainingLength -= 4;
  }

  while (remainingLength >= 1) {
      lane = read_byte(input_ptr);
      acc = acc + lane * PRIME32_5;
      acc = (acc <<< 11) * PRIME32_1;
      input_ptr += 1; remainingLength -= 1;
  }
```

This process ensures that all input bytes are present in the final mix.

### Step 6. Final mix (avalanche)

The final mix ensures that all input bits have a chance to impact any bit in the output digest, resulting in an unbiased distribution. This is also called avalanche effect.

```c
  acc = acc xor (acc >> 15);
  acc = acc * PRIME32_2;
  acc = acc xor (acc >> 13);
  acc = acc * PRIME32_3;
  acc = acc xor (acc >> 16);
```

### Step 7. Output

The `XXH32()` function produces an unsigned 32-bit value as output.

For systems which require to store and/or display the result in binary or hexadecimal format, the canonical format is defined to reproduce the same value as the natural decimal format, hence follows __big-endian__ convention (most significant byte first).


XXH64 Algorithm Description
-------------------------------------

### Overview

`XXH64`'s algorithm structure is very similar to `XXH32` one. The major difference is that `XXH64` uses 64-bit arithmetic, speeding up memory transfer for 64-bit compliant systems, but also relying on cpu capability to efficiently perform 64-bit operations.

The algorithm collects and transforms input in _stripes_ of 32 bytes. The transforms are stored inside 4 "accumulators", each one storing an unsigned 64-bit value. Each accumulator can be processed independently in parallel, speeding up processing for cpu with multiple execution units.

The algorithm uses 64-bit addition, multiplication, rotate, shift and xor operations. Many operations require some 64-bit prime number constants, all defined below:

```c
  static const u64 PRIME64_1 = 0x9E3779B185EBCA87ULL;  // 0b1001111000110111011110011011000110000101111010111100101010000111
  static const u64 PRIME64_2 = 0xC2B2AE3D27D4EB4FULL;  // 0b1100001010110010101011100011110100100111110101001110101101001111
  static const u64 PRIME64_3 = 0x165667B19E3779F9ULL;  // 0b0001011001010110011001111011000110011110001101110111100111111001
  static const u64 PRIME64_4 = 0x85EBCA77C2B2AE63ULL;  // 0b1000010111101011110010100111011111000010101100101010111001100011
  static const u64 PRIME64_5 = 0x27D4EB2F165667C5ULL;  // 0b0010011111010100111010110010111100010110010101100110011111000101
```

These constants are prime numbers, and feature a good mix of bits 1 and 0, neither too regular, nor too dissymmetric. These properties help dispersion capabilities.

### Step 1. Initialize internal accumulators

Each accumulator gets an initial value based on optional `seed` input. Since the `seed` is optional, it can be `0`.

```c
  u64 acc1 = seed + PRIME64_1 + PRIME64_2;
  u64 acc2 = seed + PRIME64_2;
  u64 acc3 = seed + 0;
  u64 acc4 = seed - PRIME64_1;
```

#### Special case: input is less than 32 bytes

When the input is too small (< 32 bytes), the algorithm will not process any stripes. Consequently, it will not make use of parallel accumulators.

In this case, a simplified initialization is performed, using a single accumulator:

```c
  u64 acc  = seed + PRIME64_5;
```

The algorithm then proceeds directly to step 4.

### Step 2. Process stripes

A stripe is a contiguous segment of 32 bytes.
It is evenly divided into 4 _lanes_, of 8 bytes each.
The first lane is used to update accumulator 1, the second lane is used to update accumulator 2, and so on.

Each lane read its associated 64-bit value using __little-endian__ convention.

For each {lane, accumulator}, the update process is called a _round_, and applies the following formula:

```c
round(accN,laneN):
  accN = accN + (laneN * PRIME64_2);
  accN = accN <<< 31;
  return accN * PRIME64_1;
```

This shuffles the bits so that any bit from input _lane_ impacts several bits in output _accumulator_. All operations are performed modulo 2^64.

Input is consumed one full stripe at a time. Step 2 is looped as many times as necessary to consume the whole input, except for the last remaining bytes which cannot form a stripe (< 32 bytes).
When that happens, move to step 3.

### Step 3. Accumulator convergence

All 4 lane accumulators from previous steps are merged to produce a single remaining accumulator of same width (64-bit). The associated formula is as follows.

Note that accumulator convergence is more complex than 32-bit variant, and requires to define another function called _mergeAccumulator()_:

```c
mergeAccumulator(acc,accN):
  acc  = acc xor round(0, accN);
  acc  = acc * PRIME64_1;
  return acc + PRIME64_4;
```

which is then used in the convergence formula:

```c
  acc = (acc1 <<< 1) + (acc2 <<< 7) + (acc3 <<< 12) + (acc4 <<< 18);
  acc = mergeAccumulator(acc, acc1);
  acc = mergeAccumulator(acc, acc2);
  acc = mergeAccumulator(acc, acc3);
  acc = mergeAccumulator(acc, acc4);
```

### Step 4. Add input length

The input total length is presumed known at this stage. This step is just about adding the length to accumulator, so that it participates to final mixing.

```c
  acc = acc + inputLength;
```

### Step 5. Consume remaining input

There may be up to 31 bytes remaining to consume from the input.
The final stage will digest them according to following pseudo-code:

```c
  while (remainingLength >= 8) {
      lane = read_64bit_little_endian(input_ptr);
      acc = acc xor round(0, lane);
      acc = (acc <<< 27) * PRIME64_1;
      acc = acc + PRIME64_4;
      input_ptr += 8; remainingLength -= 8;
  }

  if (remainingLength >= 4) {
      lane = read_32bit_little_endian(input_ptr);
      acc = acc xor (lane * PRIME64_1);
      acc = (acc <<< 23) * PRIME64_2;
      acc = acc + PRIME64_3;
      input_ptr += 4; remainingLength -= 4;
  }

  while (remainingLength >= 1) {
      lane = read_byte(input_ptr);
      acc = acc xor (lane * PRIME64_5);
      acc = (acc <<< 11) * PRIME64_1;
      input_ptr += 1; remainingLength -= 1;
  }
```

This process ensures that all input bytes are present in the final mix.

### Step 6. Final mix (avalanche)

The final mix ensures that all input bits have a chance to impact any bit in the output digest, resulting in an unbiased distribution. This is also called avalanche effect.

```c
  acc = acc xor (acc >> 33);
  acc = acc * PRIME64_2;
  acc = acc xor (acc >> 29);
  acc = acc * PRIME64_3;
  acc = acc xor (acc >> 32);
```

### Step 7. Output

The `XXH64()` function produces an unsigned 64-bit value as output.

For systems which require to store and/or display the result in binary or hexadecimal format, the canonical format is defined to reproduce the same value as the natural decimal format, hence follows __big-endian__ convention (most significant byte first).

XXH3 Algorithm Overview
-------------------------------------

XXH3 comes in two different versions: XXH3-64 and XXH3-128 (or XXH128), producing 64 and 128 bytes of output, respectively.

XXH3 uses different algorithms for small (0-16 bytes), medium (17-240 bytes), and large (241+ bytes) inputs. The algorithms for small and medium inputs are optimized for performance. The three algorithms are described in the following sections.

Many operations require some 64-bit prime number constants, which are the same constants used in XXH32 and XXH64, all defined below:

```c
  static const u64 PRIME32_1 = 0x9E3779B1U;  // 0b10011110001101110111100110110001
  static const u64 PRIME32_2 = 0x85EBCA77U;  // 0b10000101111010111100101001110111
  static const u64 PRIME32_3 = 0xC2B2AE3DU;  // 0b11000010101100101010111000111101
  static const u64 PRIME64_1 = 0x9E3779B185EBCA87ULL;  // 0b1001111000110111011110011011000110000101111010111100101010000111
  static const u64 PRIME64_2 = 0xC2B2AE3D27D4EB4FULL;  // 0b1100001010110010101011100011110100100111110101001110101101001111
  static const u64 PRIME64_3 = 0x165667B19E3779F9ULL;  // 0b0001011001010110011001111011000110011110001101110111100111111001
  static const u64 PRIME64_4 = 0x85EBCA77C2B2AE63ULL;  // 0b1000010111101011110010100111011111000010101100101010111001100011
  static const u64 PRIME64_5 = 0x27D4EB2F165667C5ULL;  // 0b0010011111010100111010110010111100010110010101100110011111000101
```

The `XXH3_64bits()` function produces an unsigned 64-bit value.  
The `XXH3_128bits()` function produces a `XXH128_hash_t` struct containing `low64` and `high64` - the lower and higher 64-bit half values of the result, respectively.

For systems requiring storing and/or displaying the result in binary or hexadecimal format, the canonical format is defined to reproduce the same value as the natural decimal format, hence following **big-endian** convention (most significant byte first).

### Seed and Secret

XXH3 provides seeded hashing by introducing two configurable constants used in the hashing process: the seed and the secret. The seed is an unsigned 64-bit value, and the secret is an array of bytes that is at least 136 bytes in size. The default seed is 0, and the default secret is the following value:

```c
static const u8 defaultSecret[192] = {
  0xb8, 0xfe, 0x6c, 0x39, 0x23, 0xa4, 0x4b, 0xbe, 0x7c, 0x01, 0x81, 0x2c, 0xf7, 0x21, 0xad, 0x1c,
  0xde, 0xd4, 0x6d, 0xe9, 0x83, 0x90, 0x97, 0xdb, 0x72, 0x40, 0xa4, 0xa4, 0xb7, 0xb3, 0x67, 0x1f,
  0xcb, 0x79, 0xe6, 0x4e, 0xcc, 0xc0, 0xe5, 0x78, 0x82, 0x5a, 0xd0, 0x7d, 0xcc, 0xff, 0x72, 0x21,
  0xb8, 0x08, 0x46, 0x74, 0xf7, 0x43, 0x24, 0x8e, 0xe0, 0x35, 0x90, 0xe6, 0x81, 0x3a, 0x26, 0x4c,
  0x3c, 0x28, 0x52, 0xbb, 0x91, 0xc3, 0x00, 0xcb, 0x88, 0xd0, 0x65, 0x8b, 0x1b, 0x53, 0x2e, 0xa3,
  0x71, 0x64, 0x48, 0x97, 0xa2, 0x0d, 0xf9, 0x4e, 0x38, 0x19, 0xef, 0x46, 0xa9, 0xde, 0xac, 0xd8,
  0xa8, 0xfa, 0x76, 0x3f, 0xe3, 0x9c, 0x34, 0x3f, 0xf9, 0xdc, 0xbb, 0xc7, 0xc7, 0x0b, 0x4f, 0x1d,
  0x8a, 0x51, 0xe0, 0x4b, 0xcd, 0xb4, 0x59, 0x31, 0xc8, 0x9f, 0x7e, 0xc9, 0xd9, 0x78, 0x73, 0x64,
  0xea, 0xc5, 0xac, 0x83, 0x34, 0xd3, 0xeb, 0xc3, 0xc5, 0x81, 0xa0, 0xff, 0xfa, 0x13, 0x63, 0xeb,
  0x17, 0x0d, 0xdd, 0x51, 0xb7, 0xf0, 0xda, 0x49, 0xd3, 0x16, 0x55, 0x26, 0x29, 0xd4, 0x68, 0x9e,
  0x2b, 0x16, 0xbe, 0x58, 0x7d, 0x47, 0xa1, 0xfc, 0x8f, 0xf8, 0xb8, 0xd1, 0x7a, 0xd0, 0x31, 0xce,
  0x45, 0xcb, 0x3a, 0x8f, 0x95, 0x16, 0x04, 0x28, 0xaf, 0xd7, 0xfb, 0xca, 0xbb, 0x4b, 0x40, 0x7e,
};
```

The seed and the secret can be specified using the `*_withSecret` and `*_withSeed` versions of the hash function.

The seed and the secret cannot be specified simultaneously (`*_withSecretAndSeed` is just `*_withSeed` for inputs less than or equal to 240 bytes and `*_withSecret` otherwise). When one is specified, the other one uses the default value. There is one exception, though: if the input is larger than 240 bytes and the seed is given, the secret is derived from the seed value and the default secret using the following procedure:

```c
deriveSecret(u64 seed):
  u64 derivedSecret[24] = defaultSecret[0:192];
  for (i = 0; i < 12; i++) {
    derivedSecret[i*2] += seed;
    derivedSecret[i*2+1] -= seed;
  }
  return derivedSecret; // convert to u8[192] (little-endian)
```

The derivation treats the secrets as 24 64-bit values. In XXH3 algorithms, the secret is always read similarly by treating a contiguous segment of the array (whose size is a multiple of 8 bytes) as one or more 64-bit values. **The secret values are always read using little-endian convention**.


XXH3 Algorithm Description (for small inputs)
-------------------------------------

*TODO*


XXH3 Algorithm Description (for medium inputs)
-------------------------------------

*TODO*


XXH3 Algorithm Description (for large inputs)
-------------------------------------

For inputs larger than 240 bytes, XXH3-64 and XXH3-128 use the same algorithm except for the finalizing step.

The internal hash state is stored inside 8 "accumulators", each one storing an unsigned 64-bit value.

### Step 1. Initialize internal accumulators

The accumulators are initialized to fixed constants:

```c
u64 acc[8] = {
  PRIME32_3, PRIME64_1, PRIME64_2, PRIME64_3,
  PRIME64_4, PRIME32_2, PRIME64_5, PRIME32_1};
```

### Step 2. Process blocks

The input is consumed and processed one full block at a time. The size of the block depends on the length of the secret. Specifically, a block consists of several 64-byte stripes. The number of stripes per block is `floor((secretLen-64)/8)` . For the default 192-byte secret, there are 16 stripes in a block, and thus the block size is 1024 bytes.

```c
secretLen = lengthInBytes(secret);    // default 192; at least 136
stripesPerBlock = (secretLen-64) / 8; // default 16; at least 9
blockSize = 64 * stripesPerBlock;     // default 1024; at least 576
```

The process of processing a full block is called a *round*. It consists of the following two sub-steps:

#### Step 2-1. Process stripes in the block

A stripe is evenly divided into 8 lanes, of 8 bytes each. In an accumulation step, one stripe and a 64-byte contiguous segment of the secret are used to update the accumulators. Each lane reads its associated 64-bit value using little-endian convention.

The accumulation step applies the following procedure:

```c
accumulate(u64 stripe[8], size secretOffset):
  u64 secretWords[8] = secret[secretOffset:secretOffset+64];
  for (i = 0; i < 8; i++) {
    u64 dataKey = stripe[i] xor secretWords[i];
    acc[i xor 1] += stripe[i];
    acc[i] += (u64)lowerHalf(dataKey) * (u64)higherHalf(dataKey);
              // (data_key and 0xFFFFFFFF) * (data_key >> 32)
  }
```

The accumulation step is repeated for all stripes in a block, using different segments of the secret, starting from the first 64 bytes for the first stripe, and offset by 8 bytes for each following round:

```c
round_accumulate(u8 block[blockSize]):
  for (n = 0; n < stripesPerBlock; n++) {
    u64 stripe[8] = block[n*64:n*64+64]; // 64 bytes = 8 u64s
    accumulate(stripe, n*8);
  }
```

#### Step 2-2. Scramble accumulators

After the accumulation steps are finished for all stripes in the block, the accumulators are scrambled using the last 64 bytes of the secret.

```c
round_scramble():
  u64 secretWords[8] = secret[secretSize-64:secretSize];
  for (i = 0; i < 8; i++) {
    acc[i] ^= acc[i] >> 47;
    acc[i] ^= secretWords[i];
    acc[i] *= PRIME32_1;
  }
```

A round is thus a `round_accumulate` followed by a `round_scramble`:

```c
round(u8 block[blockSize]):
  round_accumulate(block);
  round_scramble();
```

Step 2 is looped to consume the input until there are less than or equal to `blockSize` bytes of input left. Note that we leave the last block to the next step even if it is a full block.

### Step 3. Process the last block and the last 64 bytes

Accumulation steps are run for the stripes in the last block, except for the last stripe (whether it is full or not). After that, run a final accumulation step by treating the last 64 bytes as a stripe. Note that the last 64 bytes might overlap with the second-to-last block.

```c
// len is the size of the last block (1 <= len <= blockSize)
lastRound(u8 block[], size len, u64 lastStripe[8]):
  size nFullStripes = (len-1)/64;
  for (n = 0; n < nFullStripes; n++) {
    u64 stripe[8] = block[n*64:n*64+64];
    accumulate(stripe, n * 8);
  }
  accumulate(lastStripe, secretSize - 71);
```

### Step 4. Finalization

In the finalization step, a merging procedure is used to extract a single 64-bit value from the accumulators, using an initial seed value and a 64-byte segment of the secret.

```c
finalMerge(u64 initValue, size secretOffset):
  u64 secretWords[8] = secret[secretOffset:secretOffset+64];
  u64 result = initValue;
  for (i = 0; i < 4; i++) {
    // 64-bit by 64-bit multiplication to 128-bit full result
    u128 mulResult = (u128)(acc[i*2] xor secretWords[i*2]) *
                     (u128)(acc[i*2+1] xor secretWords[i*2+1]);
    result += lowerHalf(mulResult) xor higherHalf(mulResult);
              // (mulResult and 0xFFFFFFFFFFFFFFFF) xor (mulResult >> 64)
  }
  // final mix (avalanche)
  result ^= result >> 37;
  result *= PRIME64_3;
  result ^= result >> 32;
  return result;
```

#### XXH3-128

XXH3-128 runs the merging procedure twice for the two halves of the result, using different secret segments and different initial values derived from the total input length:

```c
finalize128():
  return {finalMerge((u64)inputLength * PRIME64_1, 11), // lower half
          finalMerge(~((u64)inputLength * PRIME64_2), secretSize - 75)}; // higher half
```

#### XXH3-64

The XXH3-64 result is just the lower half of the XXH3-128 result:

```c
finalize64():
  return finalMerge((u64)inputLength * PRIME64_1, 11);
```


Performance considerations
----------------------------------

The xxHash algorithms are simple and compact to implement. They provide a system independent "fingerprint" or digest of a message of arbitrary length.

The algorithm allows input to be streamed and processed in multiple steps. In such case, an internal buffer is needed to ensure data is presented to the algorithm in full stripes.

On 64-bit systems, the 64-bit variant `XXH64` is generally faster to compute, so it is a recommended variant, even when only 32-bit are needed.

On 32-bit systems though, positions are reversed: `XXH64` performance is reduced, due to its usage of 64-bit arithmetic. `XXH32` becomes a faster variant.


Reference Implementation
----------------------------------------

A reference library written in C is available at https://www.xxhash.com.
The web page also links to multiple other implementations written in many different languages.
It links to the [github project page](https://github.com/Cyan4973/xxHash) where an [issue board](https://github.com/Cyan4973/xxHash/issues) can be used for further public discussions on the topic.


Version changes
--------------------
v0.7.3: Minor fixes
v0.1.1: added a note on rationale for selection of constants
v0.1.0: initial release
