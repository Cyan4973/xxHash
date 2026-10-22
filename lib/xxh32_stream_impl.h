/*
 * xxHash - Extremely Fast Hash algorithm
 * XXH32 streaming implementation
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

#if !defined(XXH_NO_XXH32) && !defined(XXH_NO_STREAM)

/*******   Hash streaming   *******/
/*! @ingroup XXH32_family */
XXH_PUBLIC_API XXH32_state_t* XXH32_createState(void)
{
    return (XXH32_state_t*)XXH_malloc(sizeof(XXH32_state_t));
}
/*! @ingroup XXH32_family */
XXH_PUBLIC_API XXH_errorcode XXH32_freeState(XXH32_state_t* statePtr)
{
    XXH_free(statePtr);
    return XXH_OK;
}

/*! @ingroup XXH32_family */
XXH_PUBLIC_API void XXH32_copyState(XXH32_state_t* dstState, const XXH32_state_t* srcState)
{
    XXH_memcpy(dstState, srcState, sizeof(*dstState));
}

/*! @ingroup XXH32_family */
XXH_PUBLIC_API XXH_errorcode XXH32_reset(XXH32_state_t* statePtr, XXH32_hash_t seed)
{
    XXH_ASSERT(statePtr != NULL);
    XXH_memset(statePtr, 0, sizeof(*statePtr));
    XXH32_initAccs(statePtr->acc, seed);
    return XXH_OK;
}


/*! @ingroup XXH32_family */
XXH_PUBLIC_API XXH_errorcode
XXH32_update(XXH32_state_t* state, const void* input, size_t len)
{
    if (input==NULL) {
        XXH_ASSERT(len == 0);
        return XXH_OK;
    }

    state->total_len_32 += (XXH32_hash_t)len;
    state->large_len |= (XXH32_hash_t)((len>=16) | (state->total_len_32>=16));

    XXH_ASSERT(state->bufferedSize < sizeof(state->buffer));
    if (len < sizeof(state->buffer) - state->bufferedSize)  {   /* fill in tmp buffer */
        XXH_memcpy(state->buffer + state->bufferedSize, input, len);
        state->bufferedSize += (XXH32_hash_t)len;
        return XXH_OK;
    }

    {   const xxh_u8* xinput = (const xxh_u8*)input;
        const xxh_u8* const bEnd = xinput + len;

        if (state->bufferedSize) {   /* non-empty buffer: complete first */
            XXH_memcpy(state->buffer + state->bufferedSize, xinput, sizeof(state->buffer) - state->bufferedSize);
            xinput += sizeof(state->buffer) - state->bufferedSize;
            /* then process one round */
            (void)XXH32_consumeLong(state->acc, state->buffer, sizeof(state->buffer), XXH_aligned);
            state->bufferedSize = 0;
        }

        XXH_ASSERT(xinput <= bEnd);
        if ((size_t)(bEnd - xinput) >= sizeof(state->buffer)) {
            /* Process the remaining data */
            xinput = XXH32_consumeLong(state->acc, xinput, (size_t)(bEnd - xinput), XXH_unaligned);
        }

        if (xinput < bEnd) {
            /* Copy the leftover to the tmp buffer */
            XXH_memcpy(state->buffer, xinput, (size_t)(bEnd-xinput));
            state->bufferedSize = (unsigned)(bEnd-xinput);
        }
    }

    return XXH_OK;
}


/*! @ingroup XXH32_family */
XXH_PUBLIC_API XXH32_hash_t XXH32_digest(const XXH32_state_t* state)
{
    xxh_u32 h32;

    if (state->large_len) {
        h32 = XXH32_mergeAccs(state->acc);
    } else {
        h32 = state->acc[2] /* == seed */ + XXH_PRIME32_5;
    }

    h32 += state->total_len_32;

    return XXH32_finalize(h32, state->buffer, state->bufferedSize, XXH_aligned);
}


#endif  /* !XXH_NO_XXH32 && !XXH_NO_STREAM */
