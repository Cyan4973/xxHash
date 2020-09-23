/*
 * xxhsum - Command line interface for xxhash algorithms
 * Copyright (C) 2013-2020 Yann Collet
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

#ifndef XSUM_BENCH_H
#define XSUM_BENCH_H

#include "xsum_config.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Runs the internal benchmark, a.k.a. xxhsum -b.
 */
XSUM_API int XSUM_benchInternal(size_t keySize);

/*
 * Runs the internal benchmark on a list of files.
 */
XSUM_API int XSUM_benchFiles(char* const* fileNamesTable, int nbFiles);

/*
 * Marks the hash function for benchmarking.
 *
 * If fill != 0, fills the table with the defaults if (id == 0) or with all
 * hashes if id > XSUM_NB_HASHES or == 99
 */
XSUM_API void XSUM_setBenchID(XSUM_U32 id, int fill);

/*
 * Sets the number of iterations of the benchmark.
 */
XSUM_API void XSUM_setBenchIter(XSUM_U32 iter);

#ifdef __cplusplus
}
#endif

#endif /* XSUM_BENCH_H */
