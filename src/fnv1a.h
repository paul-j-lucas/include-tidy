/*
**      PJL Library
**      src/fnv1a.h
**
**      Copyright (C) 2026  Paul J. Lucas
**
**      This program is free software: you can redistribute it and/or modify
**      it under the terms of the GNU General Public License as published by
**      the Free Software Foundation, either version 3 of the License, or
**      (at your option) any later version.
**
**      This program is distributed in the hope that it will be useful,
**      but WITHOUT ANY WARRANTY; without even the implied warranty of
**      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**      GNU General Public License for more details.
**
**      You should have received a copy of the GNU General Public License
**      along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef pjl_fnv1a_h
#define pjl_fnv1a_h

/**
 * @file
 * Declares constants, macros, and functions for Fowler-Noll-Vo hashing.
 *
 * @sa [The FNV Non-Cryptographic Hash Algorithm](https://datatracker.ietf.org/doc/html/draft-eastlake-fnv-17.html)
 */

// local
#include "pjl_config.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <stddef.h>
#include <stdint.h>

/// @endcond

/**
 * @defgroup fnv1a-group Fowler-Noll-Vo Macros & Functions
 * Constants, macros, and functions for Fowler-Noll-Vo hashing.
 *
 * @sa [The FNV Non-Cryptographic Hash Algorithm](https://datatracker.ietf.org/doc/html/draft-eastlake-fnv-17.html)
 * @{
 */

////////// macros /////////////////////////////////////////////////////////////

/**
 * Creates an appropriate integer literal of \a N for fnv1a_t.
 *
 * @param N The integer literal to use.
 * @return Returns \a N of type fnv1a_t.
 */
#define FNV1A_C(N)                UINT64_C(N)

/**
 * Initialization value for Fowler-Noll-Vo hash function.
 *
 * @sa fnv1a_mem()
 * @sa #FNV1A_PRIME
 * @sa fnv1a_s()
 */
#define FNV1A_INIT                FNV1A_C(14695981039346656037)

/**
 * Prime value for Fowler-Noll-Vo hash function.
 *
 * @sa #FNV1A_INIT
 * @sa fnv1a_mem()
 * @sa fnv1a_s()
 */
#define FNV1A_PRIME               FNV1A_C(1099511628211)

////////// typedefs ///////////////////////////////////////////////////////////

/**
 * Result type for Fowler-Noll-Vo hash functions.
 *
 * @sa #FNV1A_C()
 * @sa fnv1a_mem()
 * @sa fnv1a_s()
 */
typedef uint64_t fnv1a_t;

////////// extern functions ///////////////////////////////////////////////////

/**
 * Fowler-Noll-Vo hash function for memory.
 *
 * @param hash The current hash.  Use #FNV1A_INIT to start.
 * @param data The data to calculate the hash of.
 * @param n The size of \a data.
 * @return Returns said hash.
 *
 * @sa fnv1a64_s()
 * @sa [The FNV Non-Cryptographic Hash Algorithm](https://datatracker.ietf.org/doc/html/draft-eastlake-fnv-17.html)
 */
NODISCARD
fnv1a_t fnv1a64_mem( fnv1a_t hash, void const *data, size_t n );

/**
 * Fowler-Noll-Vo hash function for a string.
 *
 * @param s The null-terminated string to calculate the hash of.
 * @return Returns said hash.
 *
 * @sa fnv1a64_mem()
 * @sa [The FNV Non-Cryptographic Hash Algorithm](https://datatracker.ietf.org/doc/html/draft-eastlake-fnv-17.html)
 */
NODISCARD
fnv1a_t fnv1a_s( char const *s );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* pjl_fnv1a_h */
/* vim:set et sw=2 ts=2: */
