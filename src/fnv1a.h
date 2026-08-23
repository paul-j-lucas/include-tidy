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
 * Initialization value for Fowler-Noll-Vo hash function.
 *
 * @sa fnv1a_mem()
 */
#define FNV1A_INIT                14695981039346656037UL

////////// typedefs ///////////////////////////////////////////////////////////

/**
 * Result type for Fowler-Noll-Vo hash functions.
 *
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
