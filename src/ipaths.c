/*
**      include-tidy -- #include tidier
**      src/ipaths.c
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
**      along with this program.  If not, see <http
*/

/**
 * @file
 * Defines types, global variables, and functions for `-I` paths.
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "ipaths.h"
#include "array.h"
#include "path_util.h"
#include "strbuf.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <limits.h>                     /* for PATH_MAX */
#include <stdbool.h>
#include <stdlib.h>                     /* for exit() */
#include <string.h>                     /* for str...() */
#include <unistd.h>                     /* for access() */

/// @endcond

/**
 * @addtogroup tidy-ipaths-group
 * @{
 */

////////// local variables ////////////////////////////////////////////////////

/**
 * Array of `-I` paths.
 */
static array_t ipaths = ARRAY_INIT( sizeof(tidy_ipath) );

/////////// local functions ///////////////////////////////////////////////////

/**
 * Cleans-up a tidy_ipath.
 *
 * @param ipath The tidy_ipath to clean up.  If NULL, does nothing.
 */
static void ipath_cleanup( tidy_ipath *ipath ) {
  if ( ipath == NULL )
    return;
  free( ipath->abs_path );
}

/**
 * Cleans-up options.
 *
 * @sa options_init()
 */
static void ipaths_cleanup( void ) {
  array_cleanup( &ipaths, POINTER_CAST( array_free_fn_t, &ipath_cleanup ) );
}

////////// extern functions ///////////////////////////////////////////////////

void ipath_add( char const *path ) {
  assert( path != NULL );

  if ( access( path, X_OK ) != 0 )
    return;

  size_t path_len;
  char path_buf[ PATH_MAX ];
  if ( realpath( path, path_buf ) == NULL ) {
    //
    // Upon success, realpath() never includes a trailing '/' on directories;
    // upon failure, fall back to using the given path, but ensure it doesn't
    // include a trailing '/' either.
    //
    strncpy_0( path_buf, path, PATH_MAX-1 );
    path_len = strlen( path_buf );
    while ( path_len > 1 && path_buf[ path_len - 1 ] == '/' )
      path_buf[ --path_len ] = '\0';
  }
  else {
    path_len = strlen( path_buf );
  }

  for ( size_t i = 0; i < ipaths.len; ++i ) {
    tidy_ipath const *const ipath = array_at_nc( &ipaths, i );
    if ( strcmp( path_buf, ipath->abs_path ) == 0 )
      return;
  } // for
  *(tidy_ipath*)array_push_back( &ipaths ) = (tidy_ipath){
    .abs_path = check_strdup( path_buf ),
    .abs_path_len = path_len
  };
}

bool ipath_find( char const *rel_path, char abs_path[static PATH_MAX] ) {
  assert( rel_path != NULL );
  assert( path_is_relative( rel_path ) );

  bool is_found = false;
  strbuf_t sbuf;
  strbuf_init( &sbuf );

  for ( size_t i = 0; i < ipaths.len; ++i ) {
    tidy_ipath const *const ipath = array_at_nc( &ipaths, i );
    strbuf_putsn( &sbuf, ipath->abs_path, ipath->abs_path_len );
    strbuf_paths( &sbuf, rel_path );
    if ( access( sbuf.str, F_OK ) == 0 ) {
      strncpy_0( abs_path, sbuf.str, PATH_MAX );
      is_found = true;
      break;
    }
    strbuf_reset( &sbuf );
  } // for

  strbuf_cleanup( &sbuf );
  return is_found;
}

char const* ipath_relativize( char const *abs_path ) {
  assert( abs_path != NULL );

  size_t      longest_include_path_len = 0;
  char const *shortest_include_path = abs_path;

  for ( size_t i = 0; i < ipaths.len; ++i ) {
    tidy_ipath const *const ipath = array_at_nc( &ipaths, i );

    if ( ipath->abs_path_len > longest_include_path_len &&
         strncmp( abs_path, ipath->abs_path, ipath->abs_path_len ) == 0 ) {
      longest_include_path_len = ipath->abs_path_len;
      shortest_include_path = abs_path + ipath->abs_path_len;

      if ( shortest_include_path[0] == '/' )
        ++shortest_include_path;
    }
  } // for

  return path_no_dot_slash( shortest_include_path );
}

void ipaths_init( void ) {
  ASSERT_RUN_ONCE();
  ATEXIT( &ipaths_cleanup );
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
