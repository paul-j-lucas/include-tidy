/*
**      PJL Library
**      src/fnv1a.c
**
**      Copyright (C) 2013-2026  Paul J. Lucas
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

/**
 * @file
 * Defines constants and functions for Fowler-Noll-Vo hashing.
 *
 * @sa [The FNV Non-Cryptographic Hash Algorithm](https://datatracker.ietf.org/doc/html/draft-eastlake-fnv-17.html)
 */

// local
#include "pjl_config.h"
#include "fnv1a.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

/// @endcond

/**
 * @addtogroup fnv1a-group
 * @{
 */

////////// local constants ////////////////////////////////////////////////////

/**
 * Prime value for Fowler-Noll-Vo hash function.
 *
 * @sa #FNV1A_INIT
 * @sa fnv1a_mem()
 * @sa fnv1a_s()
 */
static fnv1a_t const FNV1A_PRIME = 1099511628211UL;

////////// extern functions ///////////////////////////////////////////////////

fnv1a_t fnv1a64_mem( fnv1a_t hash, void const *data, size_t n ) {
  assert( data != NULL );

  for ( size_t i = 0; i < n; ++i )
    hash = FNV1A_PRIME * (hash ^ STATIC_CAST( uint8_t const*, data )[i]);
  return hash;
}

fnv1a_t fnv1a_s( char const *s ) {
  assert( s != NULL );

  fnv1a_t hash = FNV1A_INIT;
  for ( ; *s != '\0'; ++s )
    hash = FNV1A_PRIME * (hash ^ STATIC_CAST( uint8_t, *s ));
  return hash;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
