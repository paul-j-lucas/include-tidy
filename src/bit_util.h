/*
**      PJL Library
**      src/bit_util.h
**
**      Copyright (C) 2017-2026  Paul J. Lucas
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

#ifndef pjl_bit_util_H
#define pjl_bit_util_H

/**
 * @file
 * Declares bit utility macros and functions.
 */

// local
#include "pjl_config.h"                 /* must go first */

/// @cond DOXYGEN_IGNORE

// standard
#include <stdbool.h>
#include <stdint.h>                     /* for uint*_t */

/// @endcond

/**
 * @defgroup bitutil-group Bit Utility Macros & Functions
 * Bit utility macros and functions.
 * @{
 */

////////// inline functions ///////////////////////////////////////////////////

/**
 * Checks whether \a n has either 0 or 1 bits set.
 *
 * @param n The number to check.
 * @return Returns `true` only if \a n has either 0 or 1 bits set.
 *
 * @sa is_0n_bit_only_in_set()
 * @sa is_1_bit()
 */
NODISCARD
inline bool is_01_bit( uint64_t n ) {
  return (n & (n - 1)) == 0;
}

/**
 * Checks whether there are 0 or more bits set in \a n that are only among the
 * bits set in \a set.
 *
 * @param n The bits to check.
 * @param set The bits to check against.
 * @return Returns `true` only if there are 0 or more bits set in \a n that are
 * only among the bits set in \a set.
 *
 * @sa is_01_bit()
 * @sa is_1_bit()
 */
NODISCARD
inline bool is_0n_bit_only_in_set( uint64_t n, uint64_t set ) {
  return (n & set) == n;
}

/**
 * Checks whether \a n has exactly 1 bit is set.
 *
 * @param n The number to check.
 * @return Returns `true` only if \a n has exactly 1 bit set.
 *
 * @sa is_01_bit()
 * @sa is_0n_bit_only_in_set()
 */
NODISCARD
inline bool is_1_bit( uint64_t n ) {
  return n != 0 && is_01_bit( n );
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* pjl_bit_util_H */
/* vim:set et sw=2 ts=2: */
