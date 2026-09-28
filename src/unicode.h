/*
**      PJL Library
**      src/unicode.h
**
**      Copyright (C) 2015-2026  Paul J. Lucas
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

#ifndef pjl_unicode_h
#define pjl_unicode_h

/**
 * @file
 * Declares macros, types, and functions for working with Unicode characters.
 */

// local
#include "pjl_config.h"                 /* must go first */

/// @cond DOXYGEN_IGNORE

// standard
#include <stdbool.h>
#if defined(HAVE_CHAR8_T) || defined(HAVE_CHAR32_T)
# include <uchar.h>
#endif /* HAVE_CHAR8_T || HAVE_CHAR32_T */
#if !defined(HAVE_CHAR8_T) || !defined(HAVE_CHAR32_T)
# include <stdint.h>                    /* for uint*_t */
#endif /* HAVE_CHAR8_T || HAVE_CHAR32_T */

/// @endcond

/**
 * @defgroup unicode-group Unicode
 * Macros, types, and functions for working with Unicode characters.
 * @{
 */

////////// macros /////////////////////////////////////////////////////////////

#define CP_SURROGATE_HIGH_START   0x00D800u /**< Unicode surrogate high. */
#define CP_SURROGATE_LOW_END      0x00DFFFu /**< Unicode surrogate low. */
#define CP_VALID_MAX              0x10FFFFu /**< Maximum valid code-point. */
#define UTF8_CHAR_SIZE_MAX        4     /**< Bytes needed for UTF-8 char. */

////////// typedefs ///////////////////////////////////////////////////////////

#ifndef HAVE_CHAR8_T
typedef uint_least8_t char8_t;          /**< Borrowed from C++20. */
#endif /* HAVE_CHAR8_T */
#ifndef HAVE_CHAR32_T
typedef uint_least32_t char32_t;        /**< C11's `char32_t` */
#endif /* HAVE_CHAR32_T */

////////// extern functions ///////////////////////////////////////////////////

/**
 * Checks whether the given Unicode code-point is valid.
 *
 * @param cp The Unicode code-point to check.
 * @return Returns `true` only if \a cp is a valid code-point.
 */
NODISCARD
inline bool cp_is_valid( unsigned long long cp ) {
  return  cp < CP_SURROGATE_HIGH_START
      || (cp > CP_SURROGATE_LOW_END && cp <= CP_VALID_MAX);
}

/**
 * Encodes a Unicode code-point into UTF-8.
 *
 * @param cp The Unicode code-point to encode.
 * @param u8c A pointer to the start of a buffer to receive the UTF-8 bytes;
 * must be at least #UTF8_CHAR_SIZE_MAX long.  No NULL byte is appended.
 * @return Returns the number of bytes comprising \a u8c only if \a cp is valid
 * or 0 if invalid.
 */
NODISCARD
unsigned utf32c_8c( char32_t cp, char8_t u8c[static UTF8_CHAR_SIZE_MAX] );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* pjl_unicode_h */
/* vim:set et sw=2 ts=2: */
