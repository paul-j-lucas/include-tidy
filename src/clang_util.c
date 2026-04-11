/*
**      include-tidy -- #include tidier
**      src/includes.c
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
#include "clang_util.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

// libclang
#include <clang-c/Index.h>

/// @endcond

/**
 * @addtogroup util-group
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

/**
 * Creates a 128-bit `unsigned __int128` literal.
 *
 * @param UPPER The upper 64 bits.
 * @param LOWER The lower 64 bits.
 * @return Returns a 128-bit `unsigned __int128` literal.
 *
 * @note If \a UPPER or \a LOWER are integer literals, they _must_ have either
 * the `ULL` or `ull` suffix.
 */
#define UINT128LIT(UPPER,LOWER) \
  ((STATIC_CAST( unsigned __int128, (UPPER) ) << 64) | (LOWER))

////////// typedefs ///////////////////////////////////////////////////////////

/**
 * Result type for Fowler-Noll-Vo hash function.
 *
 * @sa fnv1a_s()
 */
#if HAVE_UNSIGNED_INT128
typedef unsigned __int128 fnv1a_t;
#else
typedef uint64_t fnv1a_t;
#endif /* HAVE_UNSIGNED_INT128 */

////////// local constants ////////////////////////////////////////////////////

#if HAVE_UNSIGNED_INT128
/**
 * Initialization value for Fowler-Noll-Vo hash function.
 *
 * @sa fnv1a_s()
 */
static fnv1a_t FNV1A_INIT =
  UINT128LIT( 0x6C62272E07BB0142ull, 0x62B821756295C58Dull );

/**
 * Prime value for Fowler-Noll-Vo hash function.
 *
 * @sa fnv1a_s()
 */
static fnv1a_t FNV1A_PRIME =
  UINT128LIT( 0x0000000001000000ull, 0x000000000000013Bull );

#else
static fnv1a_t FNV1A_INIT  = 14695981039346656037UL;
static fnv1a_t FNV1A_PRIME = 1099511628211UL;
#endif /* HAVE_UNSIGNED_INT128 */

////////// local functions ////////////////////////////////////////////////////

/**
 * Fowler-Noll-Vo hash function for a string.
 *
 * @param s The null-terminated string to calculate the hash of.
 * @return Returns said hash.
 *
 * @sa [The FNV Non-Cryptographic Hash Algorithm](https://datatracker.ietf.org/doc/html/draft-eastlake-fnv-17.html)
 */
static fnv1a_t fnv1a_s( char const *s ) {
  assert( s != NULL );

  fnv1a_t hash = FNV1A_INIT;
  while ( *s != '\0' )
    hash = FNV1A_PRIME * (hash ^ STATIC_CAST( uint8_t, *s++ ));
  return hash;
}

////////// extern functions ///////////////////////////////////////////////////

int tidy_CXFile_cmp( CXFile const *i_file, CXFile const *j_file ) {
  assert(  i_file != NULL );
  assert( *i_file != NULL );
  assert(  j_file != NULL );
  assert( *j_file != NULL );

  CXFileUniqueID const i_id = tidy_getFileUniqueID( *i_file );
  CXFileUniqueID const j_id = tidy_getFileUniqueID( *j_file );
  return tidy_CXFileUniqueID_cmp( &i_id, &j_id );
}

void tidy_CXFileUniqueID_fput( CXFileUniqueID const *id, FILE *out ) {
  assert( id != NULL );
  assert( out != NULL );

  static int const ID_HEX_WIDTH = (int)sizeof( id->data[0] ) * CHAR_BIT / 4;

  fprintf( out, "%0*llX-%0*llX-%0*llX",
    ID_HEX_WIDTH, id->data[0],
    ID_HEX_WIDTH, id->data[1],
    ID_HEX_WIDTH, id->data[2]
  );
}

CXString tidy_File_getRealPathName( CXFile file ) {
  assert( file != NULL );

  CXString          abs_path_cxs = clang_File_tryGetRealPathName( file );
  char const *const abs_path = clang_getCString( abs_path_cxs );

  if ( abs_path == NULL || abs_path[0] == '\0' ) {
    clang_disposeString( abs_path_cxs );
    abs_path_cxs = clang_getFileName( file );
  }

  return abs_path_cxs;
}

CXFileUniqueID tidy_getFileUniqueID( CXFile file ) {
  assert( file != NULL );

  CXFileUniqueID id;
  int const rv = clang_getFileUniqueID( file, &id );
  if ( unlikely( rv != 0 ) ) {
    CXString const    abs_path_cxs = tidy_File_getRealPathName( file );
    char const *const abs_path = clang_getCString( abs_path_cxs );
    fnv1a_t const     hash = fnv1a_s( abs_path );

    clang_disposeString( abs_path_cxs );
    id = (CXFileUniqueID){
      .data = {
#if HAVE_UNSIGNED_INT128
        STATIC_CAST( unsigned long long, hash >> 64 ),
#endif /* HAVE_UNSIGNED_INT128 */
        STATIC_CAST( unsigned long long, hash )
      }
    };
  }
  return id;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/// @cond DOXYGEN_IGNORE

extern inline int tidy_CXFileUniqueID_cmp( CXFileUniqueID const*, CXFileUniqueID const* );

/// @endcond

/* vim:set et sw=2 ts=2: */
