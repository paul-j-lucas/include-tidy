/*
**      PJL Library
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
#include "str_util.h"
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
#include <unistd.h>                     /* for getcwd() */

/// @endcond

/**
 * @addtogroup path-util-group
 * @{
 */

////////// extern functions ///////////////////////////////////////////////////

char const* path_basename( char const *path ) {
  assert( path != NULL );
  char const *const slash = strrchr( path, '/' );
  if ( slash != NULL )
    return slash[1] ? slash + 1 : path;
  return path;
}

char const* path_cwd( size_t *rv_len ) {
  static char   cwd_path_buf[ PATH_MAX + 1 ];
  static size_t cwd_path_len;

  if ( cwd_path_len == 0 ) {
    if ( getcwd( cwd_path_buf, sizeof cwd_path_buf - 1 ) == NULL ) {
      // LCOV_EXCL_START
      fatal_error( EX_UNAVAILABLE,
        "could not get current working directory: %s\n", STRERROR()
      );
      // LCOV_EXCL_STOP
    }
    cwd_path_len = strlen( cwd_path_buf );
    assert( cwd_path_len > 0 );
    if ( cwd_path_buf[ cwd_path_len - 1 ] != '/' )
      strcpy( cwd_path_buf + cwd_path_len++, "/" );
  }

  if ( rv_len != NULL )
    *rv_len = cwd_path_len;
  return cwd_path_buf;
}

char* path_dirname( char const *path ) {
  assert( path != NULL );

  char const *const slash = strrchr( path, '/' );
  if ( slash == NULL )
    return strdup( "." );

  size_t const dir_len = STATIC_CAST( size_t, slash - path );
  if ( dir_len == 0 )
    return strdup( "/" );

  return strndup( path, dir_len );
}

bool path_ends_with( char const *path, size_t path_len, char const *end_path,
                     size_t end_path_len ) {
  assert( path != NULL );
  assert( end_path != NULL );

  if ( end_path_len > path_len )
    return false;
  char const *const suffix = path + (path_len - end_path_len);
  return  strcmp( end_path, suffix ) == 0 &&
          (suffix == path || suffix[-1] == '/');
}

bool path_equal( char const *i_path, char const *j_path ) {
  assert( i_path != NULL );
  assert( j_path != NULL );

  if ( i_path == j_path )
    return true;

  size_t i_len = strlen( i_path );
  while ( i_len > 1 && i_path[ i_len - 1 ] == '/' )
    --i_len;

  size_t j_len = strlen( j_path );
  while ( j_len > 1 && j_path[ j_len - 1 ] == '/' )
    --j_len;

  return i_len == j_len && memcmp( i_path, j_path, i_len ) == 0;
}

char const* path_ext( char const *path ) {
  assert( path != NULL );
  // Do path_basename() first for a case like "a.b/c".
  char const *const base_name = path_basename( path );
  char const *const dot = strrchr( base_name, '.' );
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

char* path_no_ext( char const *path ) {
  assert( path != NULL );

  ssize_t last_dot = -1, last_slash = -1;
  ssize_t i;

  for ( i = 0; path[i] != '\0'; ++i ) {
    switch ( path[i] ) {
      case '.':
        last_dot = i;
        break;
      case '/':
        last_slash = i;
        break;
    } // switch
  } // for

  if ( last_dot == -1 ||                // "foo"
       last_dot == i - 1 ||             // "foo."
       last_dot < last_slash ||         // "fo.o/bar"
       last_dot == last_slash + 1 ) {   // ".foo" or "foo/.bar"
    return NULL;
  }

  size_t const no_ext_len = STATIC_CAST( size_t, last_dot );
  return strndup( path, no_ext_len );
}

char* path_normalize( char const *path ) {
  assert( path != NULL );

  if ( path[0] == '.' && path[1] == '\0' )
    return strdup_or_exit( path_cwd( /*len=*/NULL ) );

  char       *comp_save = NULL;
  array_t     comp_stack = ARRAY_INIT( sizeof(char*) );
  char       *cwd_copy = NULL;
  bool        is_absolute = path_is_absolute( path );
  char *const path_copy = strdup_or_exit( path );

  for ( char const *path_comp = strtok_r( path_copy, "/", &comp_save );
        path_comp != NULL; path_comp = strtok_r( NULL, "/", &comp_save ) ) {

    if ( strcmp( path_comp, ".." ) == 0 ) {
      if ( comp_stack.len > 0 ) {
        array_pop_back( &comp_stack );
      }
      else if ( !is_absolute ) {
        // Path has escaped its relative root: anchor to CWD
        cwd_copy = strdup_or_exit( path_cwd( /*len=*/NULL ) );
        is_absolute = true;

        char *cwd_save = NULL;
        for ( char const *cwd_comp = strtok_r( cwd_copy, "/", &cwd_save );
              cwd_comp != NULL; cwd_comp = strtok_r( NULL, "/", &cwd_save ) ) {
          *(char const**)array_push_back( &comp_stack ) = cwd_comp;
        }

        // Pop one directory level for the '..' that triggered the escape
        array_pop_back( &comp_stack );
      }
    }
    else if ( strcmp( path_comp, "." ) != 0 ) {
      *(char const**)array_push_back( &comp_stack ) = path_comp;
    }
  } // for

  strbuf_t out_path = STRBUF_INIT();

  if ( is_absolute )
    strbuf_putc( &out_path, '/' );
  for ( size_t i = 0; i < comp_stack.len; ++i ) {
    char const *const comp = *(char const**)array_at_nc( &comp_stack, i );
    strbuf_paths( &out_path, comp );
  } // for

  array_cleanup( &comp_stack, /*free_fn=*/NULL );
  free( cwd_copy );
  free( path_copy );
  return strbuf_take( &out_path );
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/// @cond DOXYGEN_IGNORE

extern inline bool path_is_absolute( char const* );
extern inline bool path_is_filename( char const* );
extern inline bool path_is_relative( char const* );

/// @endcond

/* vim:set et sw=2 ts=2: */
