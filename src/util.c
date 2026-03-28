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
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>                     /* for malloc(), ... */
#include <string.h>
#include <strings.h>                    /* for strcasecmp(3) */
#include <sysexits.h>
#include <unistd.h>                     /* for close(2), getpid(3) */

char const WS_CHARS[] =           " \n\t\r\f\v";

/// @endcond

/**
 * @addtogroup util-group
 * @{
 */

////////// local functions ////////////////////////////////////////////////////

/**
 * Helper function for fput_list() that, given a pointer to a pointer to an
 * array of pointer to `char`, returns the pointer to the associated string.
 *
 * @param ppelt A pointer to the pointer to the element to get the string of.
 * On return, it is incremented by the size of the element.
 * @return Returns said string or NULL if none.
 */
NODISCARD
static char const* fput_list_apc_gets( void const **ppelt ) {
  char const *const *const ps = *ppelt;
  *ppelt = ps + 1;
  return *ps;
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

void fput_list( FILE *out, void const *elt,
                char const* (*gets)( void const** ) ) {
  assert( out != NULL );
  assert( elt != NULL );

  if ( gets == NULL )
    gets = &fput_list_apc_gets;

  char const *s = (*gets)( &elt );
  for ( size_t i = 0; s != NULL; ++i ) {
    char const *const next_s = (*gets)( &elt );
    if ( i > 0 )
      FPUTS( next_s != NULL ? ", " : i > 1 ? ", or " : " or ", out );
    FPUTS( s, out );
    s = next_s;
  } // for
}

#ifndef NDEBUG
bool is_affirmative( char const *s ) {
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

bool is_any( char const *s, char const *const matches[const static 2] ) {
  if ( s != NULL ) {
    for ( char const *const *match = matches; *match != NULL; ++match ) {
      if ( strcasecmp( s, *match ) == 0 )
        return true;
    } // for
  }
  return false;
}

void perror_exit( int status ) {
  perror( prog_name );
  exit( status );
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

extern inline char const* empty_if_null( char const* );
extern inline bool is_digits( char const* );
extern inline char const* null_if_empty( char const* );
extern inline char const* path_no_dot_slash( char const* );
extern inline char* strncpy_0( char*, char const*, size_t );
extern inline bool true_or_set( bool* );
extern inline bool true_clear( bool* );

/* vim:set et sw=2 ts=2: */
