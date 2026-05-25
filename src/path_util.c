/*
**      include-tidy -- #include tidier
**      src/path_util.c
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

/**
 * @file
 * Defines path utility functions.
 */

// local
#include "pjl_config.h"
#include "path_util.h"
#include "array.h"
#include "strbuf.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <limits.h>                     /* for PATH_MAX */
#include <stdbool.h>
#include <stdlib.h>                     /* for malloc(), ... */
#include <string.h>
#include <sysexits.h>
#include <unistd.h>                     /* for close(2), getpid(3) */

/// @endcond

/**
 * @addtogroup path-util-group
 * @{
 */

////////// extern functions ///////////////////////////////////////////////////

char const* path_basename( char const *path_name ) {
  assert( path_name != NULL );
  char const *const slash = strrchr( path_name, '/' );
  if ( slash != NULL )
    return slash[1] ? slash + 1 : path_name;
  return path_name;
}

char const* path_cwd( size_t *plen ) {
  static char   cwd_path_buf[ PATH_MAX ];
  static size_t cwd_path_len;

  if ( cwd_path_len == 0 ) {
    if ( getcwd( cwd_path_buf, sizeof cwd_path_buf - 1 ) == NULL ) {
      fatal_error( EX_UNAVAILABLE,
        "could not get current working directory: %s\n", STRERROR()
      );
    }
    cwd_path_len = strlen( cwd_path_buf );
    assert( cwd_path_len > 0 );
    if ( cwd_path_buf[ cwd_path_len - 1 ] != '/' )
      strcpy( cwd_path_buf + cwd_path_len++, "/" );
  }

  if ( plen != NULL )
    *plen = cwd_path_len;
  return cwd_path_buf;
}

bool path_ends_with( char const *abs_path, char const *rel_path,
                     size_t rel_path_len ) {
  assert( rel_path != NULL );
  assert( abs_path != NULL );

  size_t const abs_path_len = strlen( abs_path );
  if ( rel_path_len > abs_path_len )
    return false;
  char const *const suffix = abs_path + (abs_path_len - rel_path_len);
  return  strcmp( rel_path, suffix ) == 0 &&
          (suffix == abs_path || suffix[-1] == '/');
}

char const* path_ext( char const *path ) {
  assert( path != NULL );
  // Do path_basename() first for a case like "a.b/c".
  char const *const file_name = path_basename( path );
  char const *const dot = strrchr( file_name, '.' );
  return dot != NULL && dot[1] != '\0' ? dot + 1 : NULL;
}

bool path_is_local( char const *abs_path ) {
  assert( abs_path != NULL );
  assert( path_is_absolute( abs_path ) );

  size_t cwd_path_len;
  char const *const cwd_path = path_cwd( &cwd_path_len );
  return strncmp( abs_path, cwd_path, cwd_path_len ) == 0;
}

char const* path_no_dot_slash( char const *path ) {
  assert( path != NULL );
  while ( STRNCMPLIT( path, "./" ) == 0 )
    path += STRLITLEN( "./" );
  return path;
}

char const* path_no_ext( char const *path, char path_buf[static PATH_MAX] ) {
  assert( path != NULL );

  ssize_t last_dot = -1, last_slash = -1;

  for ( ssize_t i = 0; path[i] != '\0'; ++i ) {
    switch ( path[i] ) {
      case '.':
        last_dot = i;
        break;
      case '/':
        last_slash = i;
        break;
    } // switch
  }

  if ( last_dot == -1 ||                // "foo"
       last_dot < last_slash ||         // "fo.o/bar"
       last_dot == last_slash + 1 ) {   // ".foo" or "foo/.bar"
    return path;
  }

  size_t const len = last_dot < PATH_MAX ?
    STATIC_CAST( size_t, last_dot ) : PATH_MAX - 1;
  return strncpy_0( path_buf, path, len );
}

char* path_normalize( char const *path ) {
  assert( path != NULL );

  strbuf_t in_path;
  strbuf_init( &in_path );

  if ( path_is_relative( path ) ) {
    path = path_no_dot_slash( path );
    if ( strstr( path, "../" ) != NULL ) {
      size_t cwd_path_len;
      char const *const cwd_path = path_cwd( &cwd_path_len );
      strbuf_putsn( &in_path, cwd_path, cwd_path_len );
    }
  }
  strbuf_paths( &in_path, path );

  array_t comp_stack;
  array_init( &comp_stack, sizeof(char*) );

  for ( char const *comp = strtok( in_path.str, "/" ); comp != NULL;
        comp = strtok( NULL, "/" ) ) {
    if ( strcmp( comp, ".." ) == 0 )
      array_pop_back( &comp_stack );
    else if ( strcmp( comp, "." ) != 0 )
      *(char const**)array_push_back( &comp_stack ) = comp;
  } // for

  strbuf_t out_path;
  strbuf_init( &out_path );

  if ( path_is_absolute( in_path.str ) )
    strbuf_putc( &out_path, '/' );
  for ( size_t i = 0; i < comp_stack.len; ++i ) {
    char const *const comp = *(char const**)array_at_nc( &comp_stack, i );
    strbuf_paths( &out_path, comp );
  }

  strbuf_cleanup( &in_path );
  array_cleanup( &comp_stack, /*free_fn=*/NULL );
  return strbuf_take( &out_path );
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/// @cond DOXYGEN_IGNORE

extern inline bool path_is_absolute( char const* );
extern inline bool path_is_relative( char const* );

/// @endcond

/* vim:set et sw=2 ts=2: */
