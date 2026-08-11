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
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>                     /* for uint8_t */
#include <stdio.h>
#include <stdlib.h>                     /* for malloc(), ... */
#include <string.h>
#include <sysexits.h>

/// @endcond

/**
 * @addtogroup util-group
 * @{
 */

////////// local constants ////////////////////////////////////////////////////

/**
 * Prime value for Fowler-Noll-Vo hash function.
 *
 * @sa #FNV1A_INIT
 * @sa fnv1a_mem()
 * @sa fnv1a_s()
 */
static fnv1a_t const FNV1A_PRIME = 1099511628211UL;

////////// extern variables ///////////////////////////////////////////////////

/// @cond DOXYGEN_IGNORE
/// Otherwise Doxygen generates two entries.

char const WS_CHARS[] =           " \n\t\r\f\v";

/// @endcond

////////// extern functions ///////////////////////////////////////////////////

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
  p = realloc( p, size );
  PERROR_EXIT_IF( p == NULL, EX_OSERR );
  return p;
}

char* check_strdup( char const *s ) {
  assert( s != NULL );
  char *const dup = strdup( s );
  PERROR_EXIT_IF( dup == NULL, EX_OSERR );
  return dup;
}

void fatal_error( int status, char const *format, ... ) {
  EPRINTF( "%s: error: ", prog_name );
  va_list args;
  va_start( args, format );
  vfprintf( stderr, format, args );
  va_end( args );
  _Exit( status );
}

fnv1a_t fnv1a64_mem( fnv1a_t hash, void const *data, size_t n ) {
  for ( size_t i = 0; i < n; ++i )
    hash = FNV1A_PRIME * (hash ^ STATIC_CAST( uint8_t const*, data )[i]);
  return hash;
}

fnv1a_t fnv1a_s( char const *s ) {
  fnv1a_t hash = FNV1A_INIT;
  for ( ; *s != '\0'; ++s )
    hash = FNV1A_PRIME * (hash ^ STATIC_CAST( uint8_t, *s ));
  return hash;
}

void fputs_quoted( char const *s, char quote, FILE *fout ) {
  assert( quote == '\'' || quote == '"' );
  assert( fout != NULL );

  if ( s == NULL ) {
    FPUTS( "null", fout );
    return;
  }

  bool in_quote = false;
  char const other_quote = quote == '\'' ? '"' : '\'';

  FPUTC( quote, fout );
  for ( char prev = '\0'; *s != '\0'; prev = *s++ ) {
    switch ( *s ) {
      case '\b': FPUTS( "\\b", fout ); continue;
      case '\f': FPUTS( "\\f", fout ); continue;
      case '\n': FPUTS( "\\n", fout ); continue;
      case '\r': FPUTS( "\\r", fout ); continue;
      case '\t': FPUTS( "\\t", fout ); continue;
      case '\v': FPUTS( "\\v", fout ); continue;
      case '\\':
        if ( in_quote ) {
          if ( prev != '\\' )
            FPUTS( "\\\\", fout );
          continue;
        }
        break;
    } // switch

    if ( prev != '\\' ) {
      if ( *s == quote ) {
        FPUTC( '\\', fout );
        in_quote = !in_quote;
      }
      else if ( *s == other_quote ) {
        in_quote = !in_quote;
      }
    }

    FPUTC( *s, fout );
  } // for
  FPUTC( quote, fout );
}

void free_pptr( void *pptr ) {
  if ( pptr != NULL )
    free( *POINTER_CAST( void**, pptr ) );
}

#ifdef NEED_II_MATRIX                   /* See comment above ii_matrix def. */
void** matrix2d_new( size_t esize, size_t ealign, size_t idim, size_t jdim ) {
  // ensure &elements[0] is suitably aligned
  size_t const ptrs_size = round_up_pow_2( sizeof(void*) * idim, ealign );
  size_t const row_size = esize * jdim;
  // allocate the row pointers followed by the elements
  void **const rows = MALLOC( char, ptrs_size + idim * row_size );
  char *const elements = POINTER_CAST( char*, rows ) + ptrs_size;
  for ( size_t i = 0; i < idim; ++i )
    rows[i] = &elements[ i * row_size ];
  return rows;
}
#endif /* NEED_II_MATRIX */

void perror_exit( int status ) {
  perror( prog_name );
  exit( status );
}

char const* strchr_nul( char const *s, int c ) {
  assert( s != NULL );
  while ( *s != '\0' && *s != (char)c )
    ++s;
  return s;
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

// See comment for NONCONST_OVERLOAD regarding ().
extern inline char const* (empty_if_null)( char const* );

extern inline bool false_set( bool* );
extern inline char* nonconst_null_if_empty( char* );
extern inline char* nonconst_empty_if_null( char* );

// See comment for NONCONST_OVERLOAD regarding ().
extern inline char const* (null_if_empty)( char const* );

#ifdef NEED_II_MATRIX                   /* See comment above ii_matrix def. */
extern inline size_t round_up_pow_2( size_t, size_t );
#endif /* NEED_II_MATRIX */
extern inline char* strncpy_0( char*, char const*, size_t );
extern inline bool true_or_set( bool* );
extern inline bool true_clear( bool* );

/// @endcond

/* vim:set et sw=2 ts=2: */
