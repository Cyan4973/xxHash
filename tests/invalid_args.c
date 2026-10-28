/*
 * Invalid arguments test program
 * Validates that public functions report invalid arguments with XXH_ERROR,
 * whatever the optimization level and XXH_DEBUGLEVEL
 *
 * Copyright (C) 2026 Yann Collet
 *
 * GPL v2 License
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * You can contact the author at:
 *   - xxHash homepage: https://www.xxhash.com
 *   - xxHash source repository: https://github.com/Cyan4973/xxHash
 */

#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "../xxhash.h"
#include <stdio.h>

static int nbErrors = 0;

#define EXPECT_ERROR(call)                                          \
    if ((call) != XXH_ERROR) {                                      \
        printf("invalid arguments not reported: %s \n", #call);     \
        nbErrors++;                                                 \
    }

int main(int argc, const char* argv[])
{
    /* sizes unknown at compile time, so that checks cannot be constant-folded */
    size_t const tooSmall = (size_t)argc * (XXH3_SECRET_SIZE_MIN - 1);
    size_t const valid = (size_t)argc * XXH3_SECRET_SIZE_MIN;
    unsigned char secret[XXH3_SECRET_SIZE_MIN] = { 0 };
    XXH3_state_t state;
    (void)argv;

    EXPECT_ERROR(XXH3_64bits_reset_withSecret(&state, secret, tooSmall));
    EXPECT_ERROR(XXH3_64bits_reset_withSecret(&state, NULL, valid));
    EXPECT_ERROR(XXH3_64bits_reset_withSecret(NULL, secret, valid));
    EXPECT_ERROR(XXH3_128bits_reset_withSecret(&state, secret, tooSmall));
    EXPECT_ERROR(XXH3_64bits_reset_withSecretandSeed(&state, secret, tooSmall, 1));
    EXPECT_ERROR(XXH3_128bits_reset_withSecretandSeed(&state, NULL, valid, 1));
    EXPECT_ERROR(XXH3_generateSecret(secret, tooSmall, "seed", 4));
    EXPECT_ERROR(XXH3_generateSecret(NULL, valid, "seed", 4));

    if (nbErrors) return 1;
    printf("invalid arguments test: OK \n");
    return 0;
}
