/*
**      include-tidy -- #include tidier
**      src/util.c
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
 * Defines utility data structures, variables, and functions.
 */

// local
#include "pjl_config.h"
#include "util.h"
#include "include-tidy.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <ctype.h>
#include <limits.h>                     /* for PATH_MAX */
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>                     /* for malloc(), ... */
#include <string.h>
#include <sysexits.h>
#include <unistd.h>                     /* for close(2), getpid(3) */

/// @endcond

/**
 * @addtogroup util-group
 * @{
 */

////////// extern variables ///////////////////////////////////////////////////

/// @cond DOXYGEN_IGNORE
/// Otherwise Doxygen generates two entries.

char const WS_CHARS[] =           " \n\t\r\f\v";

/// @endcond

////////// local functions ////////////////////////////////////////////////////

/**
 * Rounds \a n up to a multiple of \a multiple.
 *
 * @param n The number to round up.  Must be &gt; 0.
 * @param multiple The multiple to round up to.  It _must_ be a power of 2.
 * @return Returns \a n rounded up to a multiple of \a multiple.
 */
NODISCARD
static inline size_t round_up_pow_2( size_t n, size_t multiple ) {
  return (n + multiple - 1) & ~(multiple - 1);
}

/**
 * Checks whether \a s is any one of \a matches, case-insensitive.
 *
 * @param s The null-terminated string to check or null.  May be NULL.
 * @param matches The null-terminated array of values to check against.
 * @return Returns `true` only if \a s is among \a matches.
 */
NODISCARD
static bool str_is_any( char const *s, char const *const matches[static 2] ) {
  if ( s != NULL ) {
    for ( char const *const *match = matches; *match != NULL; ++match ) {
      if ( strcasecmp( s, *match ) == 0 )
        return true;
    } // for
  }
  return false;
}

////////// extern functions ///////////////////////////////////////////////////

char const* base_name( char const *path_name ) {
  assert( path_name != NULL );
  char const *const slash = strrchr( path_name, '/' );
  if ( slash != NULL )
    return slash[1] ? slash + 1 : path_name;
  return path_name;
}

unsigned check_asprintf( char **ps, char const *format, ... ) {
  assert( ps != NULL );
  assert( format != NULL );

  va_list args;
  va_start( args, format );
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
  int const raw_len = vasprintf( ps, format, args );
#pragma GCC diagnostic pop
  va_end( args );

  PERROR_EXIT_IF( raw_len < 0, EX_OSERR );
  return STATIC_CAST( unsigned, raw_len );
}

void* check_realloc( void *p, size_t size ) {
  assert( size > 0 );
  p = p != NULL ? realloc( p, size ) : malloc( size );
  PERROR_EXIT_IF( p == NULL, EX_OSERR );
  return p;
}

void check_snprintf( char *buf, size_t buf_size, char const *format, ... ) {
  assert( buf != NULL );
  assert( format != NULL );

  va_list args;
  va_start( args, format );
  int const raw_len = vsnprintf( buf, buf_size, format, args );
  va_end( args );

  PERROR_EXIT_IF( raw_len < 0, EX_OSERR );
  PERROR_EXIT_IF( STATIC_CAST( size_t, raw_len ) >= buf_size, EX_SOFTWARE );
}

char* check_strdup( char const *s ) {
  assert( s != NULL );
  char *const dup = strdup( s );
  PERROR_EXIT_IF( dup == NULL, EX_OSERR );
  return dup;
}

void fatal_error( int status, char const *format, ... ) {
  EPRINTF( "%s: ", prog_name );
  va_list args;
  va_start( args, format );
  vfprintf( stderr, format, args );
  va_end( args );
  _Exit( status );
}

char const* get_cwd( size_t *plen ) {
  static char   cwd_path_buf[ PATH_MAX ];
  static size_t cwd_path_len;

  if ( cwd_path_len == 0 ) {
    if ( getcwd( cwd_path_buf, sizeof cwd_path_buf ) == NULL ) {
      fatal_error( EX_UNAVAILABLE,
        "could not get current working directory: %s\n", STRERROR()
      );
    }
    cwd_path_len = strlen( cwd_path_buf );
    if ( cwd_path_len > 0 && cwd_path_buf[ cwd_path_len - 1 ] != '/' )
      strcpy( cwd_path_buf + cwd_path_len++, "/" );
  }

  if ( plen != NULL )
    *plen = cwd_path_len;
  return cwd_path_buf;
}

void** matrix2d_new( size_t esize, size_t ealign, size_t idim, size_t jdim,
                     bool zero ) {
  // ensure &elements[0] is suitably aligned
  size_t const ptrs_size = round_up_pow_2( sizeof(void*) * idim, ealign );
  size_t const rows_size = esize * jdim;
  size_t const data_size = idim * rows_size;
  // allocate the row pointers followed by the elements
  void **const rows = MALLOC( char, ptrs_size + data_size );
  char *const elements = POINTER_CAST( char*, rows ) + ptrs_size;
  if ( zero )
    memset( elements, 0, data_size );
  for ( size_t i = 0; i < idim; ++i )
    rows[i] = &elements[ i * rows_size ];
  return rows;
}

int path_base_name_cmp( char const *i_path, char const *j_path ) {
  assert( i_path != NULL );
  assert( j_path != NULL );

  i_path = base_name( i_path );
  j_path = base_name( j_path );
  return strcmp( i_path, j_path );
}

char const* path_ext( char const *path ) {
  assert( path != NULL );
  // Do base_name() first for a case like "a.b/c".
  char const *const file_name = base_name( path );
  char const *const dot = strrchr( file_name, '.' );
  return dot != NULL && dot[1] != '\0' ? dot + 1 : NULL;
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

void perror_exit( int status ) {
  perror( prog_name );
  exit( status );
}

#ifndef NDEBUG
bool str_is_affirmative( char const *s ) {
  static char const *const AFFIRMATIVES[] = {
    "1",
    "t",
    "true",
    "y",
    "yes",
    NULL
  };
  return str_is_any( s, AFFIRMATIVES );
}
#endif /* NDEBUG */

int str_ptr_cmp( char const **i_ps, char const **j_ps ) {
  return strcmp( *i_ps, *j_ps );
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

extern inline char const* empty_if_null( char const* );
extern inline bool false_set( bool* );
extern inline char const* null_if_empty( char const* );
extern inline bool path_is_relative( char const* );
extern inline char const* path_no_dot_slash( char const* );
extern inline char* strncpy_0( char*, char const*, size_t );
extern inline bool true_or_set( bool* );
extern inline bool true_clear( bool* );

/// @endcond

/* vim:set et sw=2 ts=2: */
