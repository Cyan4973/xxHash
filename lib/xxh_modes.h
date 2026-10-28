/*
 * xxHash - Extremely Fast Hash algorithm
 * Integration modes: XXH_INLINE_ALL, XXH_PRIVATE_API, XXH_NAMESPACE
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

/* ****************************
 *  INLINE mode
 ******************************/
#ifdef XXH_DOXYGEN
/*!
 * @brief Gives access to internal state declaration, required for static allocation.
 *
 * Incompatible with dynamic linking, due to risks of ABI changes.
 *
 * Usage:
 * @code{.c}
 *     #define XXH_STATIC_LINKING_ONLY
 *     #include "xxhash.h"
 * @endcode
 */
#  define XXH_STATIC_LINKING_ONLY
/* Do not undef XXH_STATIC_LINKING_ONLY for Doxygen */

/*!
 * @brief Gives access to internal definitions.
 *
 * Usage:
 * @code{.c}
 *     #define XXH_STATIC_LINKING_ONLY
 *     #define XXH_IMPLEMENTATION
 *     #include "xxhash.h"
 * @endcode
 */
#  define XXH_IMPLEMENTATION
/* Do not undef XXH_IMPLEMENTATION for Doxygen */

/*!
 * @brief Exposes the implementation and marks all functions as `inline`.
 *
 * Use these build macros to inline xxhash into the target unit.
 * Inlining improves performance on small inputs, especially when the length is
 * expressed as a compile-time constant:
 *
 *  https://fastcompression.blogspot.com/2018/03/xxhash-for-small-keys-impressive-power.html
 *
 * It also keeps xxHash symbols private to the unit, so they are not exported.
 *
 * Usage:
 * @code{.c}
 *     #define XXH_INLINE_ALL
 *     #include "xxhash.h"
 * @endcode
 * Do not compile and link xxhash.o as a separate object, as it is not useful.
 */
#  define XXH_INLINE_ALL
#  undef XXH_INLINE_ALL
/*!
 * @brief Exposes the implementation without marking functions as inline.
 */
#  define XXH_PRIVATE_API
#  undef XXH_PRIVATE_API
/*!
 * @brief Emulate a namespace by transparently prefixing all symbols.
 *
 * If you want to include _and expose_ xxHash functions from within your own
 * library, but also want to avoid symbol collisions with other libraries which
 * may also include xxHash, you can use @ref XXH_NAMESPACE to automatically prefix
 * any public symbol from xxhash library with the value of @ref XXH_NAMESPACE
 * (therefore, avoid empty or numeric values).
 *
 * Note that no change is required within the calling program as long as it
 * includes `xxhash.h`: Regular symbol names will be automatically translated
 * by this header.
 */
#  define XXH_NAMESPACE /* YOUR NAME HERE */
#  undef XXH_NAMESPACE
#endif

#define XXH_CAT(A,B) A##B
#define XXH_NAME2(A,B) XXH_CAT(A,B)
#define XXH_IPREF(Id) XXH_NAME2(XXH_NAMESPACE, Id)

#if (defined(XXH_INLINE_ALL) || defined(XXH_PRIVATE_API)) \
    && !defined(XXH_INLINE_ALL_31684351384)
   /* this section should be traversed only once */
#  define XXH_INLINE_ALL_31684351384
   /* give access to the advanced API, required to compile implementations */
#  undef XXH_STATIC_LINKING_ONLY   /* avoid macro redef */
#  define XXH_STATIC_LINKING_ONLY
   /* make all functions private */
#  undef XXH_PUBLIC_API
#  if defined(__GNUC__)
#    define XXH_PUBLIC_API static __inline __attribute__((__unused__))
#  elif defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
#    define XXH_PUBLIC_API static inline
#  elif defined(_MSC_VER)
#    define XXH_PUBLIC_API static __inline
#  else
     /* note: this version may generate warnings for unused static functions */
#    define XXH_PUBLIC_API static
#  endif

   /*
    * This part deals with the special case where a unit wants to inline xxHash,
    * but "xxhash.h" has previously been included without XXH_INLINE_ALL,
    * such as part of some previously included *.h header file.
    * Without further action, the new include would just be ignored,
    * and functions would effectively _not_ be inlined (silent failure).
    * The following macros solve this situation by prefixing all inlined names,
    * avoiding naming collision with previous inclusions.
    */
   /* Before that, we unconditionally #undef all symbols,
    * in case they were already defined with XXH_NAMESPACE.
    * They will then be redefined for XXH_INLINE_ALL.
    * Symbols of each hash family are handled at the beginning of its declarations.
    */
#  undef XXH_versionNumber
    /* Finally, free the namespace itself */
#  undef XXH_NAMESPACE

    /* employ the namespace for XXH_INLINE_ALL */
#  define XXH_NAMESPACE XXH_INLINE_
   /*
    * Some identifiers (enums, type names) are not symbols,
    * but they must nonetheless be renamed to avoid redeclaration.
    * Alternative solution: do not redeclare them.
    * However, this requires some #ifdefs, and has a more dispersed impact.
    * Meanwhile, renaming can be achieved in a single place.
    * Type names of each hash family are renamed at the beginning of its declarations.
    */
#  define XXH_OK XXH_IPREF(XXH_OK)
#  define XXH_ERROR XXH_IPREF(XXH_ERROR)
#  define XXH_errorcode XXH_IPREF(XXH_errorcode)
   /* Ensure the header is parsed again, even if it was previously included */
#  undef XXHASH_H_5627135585666179
#  undef XXHASH_H_STATIC_13879238742
#endif /* XXH_INLINE_ALL || XXH_PRIVATE_API */
