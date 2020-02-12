/*
*  test_multiinclude
*  test program, just to validate including multiple times in any order
*
*  Copyright (C) Yann Collet 2013-present
*
*  GPL v2 License
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License along
*  with this program; if not, write to the Free Software Foundation, Inc.,
*  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*
*  You can contact the author at :
*  - xxHash homepage : http://www.xxhash.com
*  - xxHash source repository : https://github.com/Cyan4973/xxHash
*/

#include <stdio.h>  // printf

/* normal include, gives access to public symbols */
#include "../xxhash.h"

/* advanced include, gives access to experimental symbols */
/* note : without specific effort, experimental symbols might be unavailable */
#define XXH_STATIC_LINKING_ONLY
#include "../xxhash.h"

/* inlining : re-define all symbols, keep them private to the unit.
 * note : without specific efforts, symbol names will collide
 * note 2 : no linking to xxhash.o, so that link stage will fail if inline is ignored */
#define XXH_INLINE_ALL
#include "../xxhash.h"


int main(void)
{
    XXH3_state_t state;   /* part of experimental */

    XXH3_64bits_reset(&state);
    const char input[] = "Hello World !";

    XXH3_64bits_update(&state, input, sizeof(input));

    XXH64_hash_t const h = XXH3_64bits_digest(&state);
    printf("hash '%s' : %0llx \n", input, (unsigned long long)h);

    return 0;
}
