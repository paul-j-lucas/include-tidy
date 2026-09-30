/*
**      PJL Library
**      src/str_util.c
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
 * Defines string utility functions.
 */

// local
#include "pjl_config.h"
#include "str_util.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/// @endcond

/**
 * @addtogroup str-util-group
 * @{
 */

////////// extern functions ///////////////////////////////////////////////////

bool str_is_any( char const *s, char const *const strings[static 1] ) {
  assert( s != NULL );

  for (;;) {
    char const *const next = *strings++;
    if ( next == NULL )
      return false;
    if ( strcmp( s, next ) == 0 )
      return true;
  } // for
}

char* str_trim( char *s ) {
  assert( s != NULL );
  SKIP_WS( s );
  for ( size_t len = strlen( s ); len > 0 && isspace( s[ --len ] ); )
    s[ len ] = '\0';
  return s;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/// @cond DOXYGEN_IGNORE

extern inline bool str_ends_with( char const*, char const*, size_t );

/// @endcond

/* vim:set et sw=2 ts=2: */
