/*
**      include-tidy -- #include tidier
**      src/tidy_util.c
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

// local
#include "pjl_config.h"
#include "util.h"

// standard
#include <limits.h>                     /* for PATH_MAX */
#include <string.h>
#include <sysexits.h>
#include <unistd.h>                     /* for getcwd() */

////////// extern functions ///////////////////////////////////////////////////

void include_get_delims( char const *full_path, char delims[static 2] ) {
  static char   cwd_buf[ PATH_MAX + 1 ];
  static size_t cwd_len;

  if ( cwd_len == 0 ) {
    if ( getcwd( cwd_buf, PATH_MAX ) == NULL ) {
      fatal_error( EX_UNAVAILABLE,
        "could not get current working directory: %s\n", STRERROR()
      );
    }
    cwd_len = strlen( cwd_buf );
    if ( cwd_len > 0 && cwd_buf[ cwd_len - 1 ] != '/' ) {
      cwd_buf[   cwd_len ] = '/';
      cwd_buf[ ++cwd_len ] = '\0';
    }
  }

  if ( strncmp( full_path, cwd_buf, cwd_len ) == 0 ) {
    delims[0] = '"';
    delims[1] = '"';
  }
  else {
    delims[0] = '<';
    delims[1] = '>';
  }
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
