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

// standard
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

// libclang
#include <clang-c/Index.h>

static uint64_t FNV1A64_INIT  = 14695981039346656037UL;
static uint64_t FNV1A64_PRIME = 1099511628211UL;

////////// local functions ////////////////////////////////////////////////////

/**
 * Fowler-Noll-Vo hash function for a string.
 *
 * @param str The null-terminated string to calculate the hash of.
 * @return Returns said hash.
 *
 * @sa [The FNV Non-Cryptographic Hash Algorithm](https://datatracker.ietf.org/doc/html/draft-eastlake-fnv-17.html)
 */
static uint64_t fnv1a64_str( char const *str ) {
  uint64_t hash = FNV1A64_INIT;
  for ( char const *c = str; *c != '\0'; ++c )
    hash = FNV1A64_PRIME * (hash ^ (uint8_t)*c);
  return hash;
}

////////// extern functions ///////////////////////////////////////////////////

void tidy_CXFileUniqueID_fput( CXFileUniqueID const *id, FILE *out ) {
  static int const ID_HEX_WIDTH = (int)sizeof( id->data[0] ) * CHAR_BIT / 4;

  fprintf( out, "%0*llX%0*llX",
    ID_HEX_WIDTH, id->data[0],
    ID_HEX_WIDTH, id->data[1]
  );
}

NODISCARD
CXString tidy_File_getRealPathName( CXFile file ) {
  CXString          file_cxs  = clang_File_tryGetRealPathName( file );
  char const *const file_cs   = clang_getCString( file_cxs );

  if ( file_cs == NULL || file_cs[0] == '\0' ) {
    clang_disposeString( file_cxs );
    file_cxs = clang_getFileName( file );
  }

  return file_cxs;
}

NODISCARD
CXFileUniqueID tidy_getFileUniqueID( CXFile file ) {
  CXFileUniqueID id;
  int const rv = clang_getFileUniqueID( file, &id );
  if ( unlikely( rv != 0 ) ) {
    CXString          abs_path_cxs = tidy_File_getRealPathName( file );
    char const *const abs_path_cs = clang_getCString( abs_path_cxs );

    uint64_t const hash = fnv1a64_str( abs_path_cs );
    clang_disposeString( abs_path_cxs );
    id = (CXFileUniqueID){ .data = { hash } };
  }
  return id;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
