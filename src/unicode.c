/*
**      PJL Library
**      src/unicode.c
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

/**
 * @file
 * Defines functions for working with Unicode characters.
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "unicode.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <stdbool.h>

/// @endcond

/**
 * @addtogroup unicode-group
 * @{
 */

////////// extern functions ///////////////////////////////////////////////////

unsigned utf32c_8c( char32_t cp, char8_t u8c[static UTF8_CHAR_SIZE_MAX] ) {
  static unsigned const MASK_1 = 0x80;
  static unsigned const MASK_2 = 0xC0;
  static unsigned const MASK_3 = 0xE0;
  static unsigned const MASK_4 = 0xF0;

  if ( cp < 0x80u ) {                   // 0xxxxxxx
    u8c[0] = STATIC_CAST( char8_t, cp );
    return 1;
  }

  if ( cp < 0x800u ) {                  // 110xxxxx 10xxxxxx
    u8c[0] = STATIC_CAST( char8_t, MASK_2 |  (cp >>  6)         );
    u8c[1] = STATIC_CAST( char8_t, MASK_1 | ( cp        & 0x3F) );
    return 2;
  }

  if ( cp < 0x10000u ) {                // 1110xxxx 10xxxxxx 10xxxxxx
    u8c[0] = STATIC_CAST( char8_t, MASK_3 |  (cp >> 12)         );
    u8c[1] = STATIC_CAST( char8_t, MASK_1 | ((cp >>  6) & 0x3F) );
    u8c[2] = STATIC_CAST( char8_t, MASK_1 | ( cp        & 0x3F) );
    return 3;
  }

  if ( cp <= 0x10FFFFu ) {              // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    u8c[0] = STATIC_CAST( char8_t, MASK_4 |  (cp >> 18)         );
    u8c[1] = STATIC_CAST( char8_t, MASK_1 | ((cp >> 12) & 0x3F) );
    u8c[2] = STATIC_CAST( char8_t, MASK_1 | ((cp >>  6) & 0x3F) );
    u8c[3] = STATIC_CAST( char8_t, MASK_1 | ( cp        & 0x3F) );
    return 4;
  }

  return 0;
}

///////////////////////////////////////////////////////////////////////////////

extern inline bool cp_is_valid( unsigned long long );

/** @} */

/* vim:set et sw=2 ts=2: */
