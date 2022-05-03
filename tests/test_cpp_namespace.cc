/*
 * C++ namespace test program
 * Checks that the library can be compiled with the optional C++
 * namespace feature.
 *
 * Copyright (C) 2022 Yann Collet
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

#include <iostream>

#define XXH_CPP_NAMESPACE xxh
#define XXH_STATIC_LINKING_ONLY   /* access advanced declarations */
#define XXH_IMPLEMENTATION   /* access definitions */
#include "../xxhash.h"

int main() {
	xxh::XXH64_state_t state;
	xxh::XXH64_reset(&state, 763);
	xxh::XXH64_update(&state, "hello", 5);
	xxh::XXH64_update(&state, "world", 5);
	std::cout << XXH64_digest(&state) << '\n';
	return 0;
}
