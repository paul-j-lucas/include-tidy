/*
**      include-tidy -- #include tidier
**      src/file_ext.c
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
 * Defines constsnts and functions for supported C/C++ filename extensions.
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "file_ext.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <stddef.h>
#include <strings.h>

/// @endcond

/**
 * @addtogroup tidy-file-ext-group
 * @{
 */

////////// local constants ////////////////////////////////////////////////////

static char const LANG_C[]    = "c";    ///< C.
static char const LANG_CXX[]  = "c++";  ///< C++.

/**
 * Array of all common C/C++ filename extensions.
 */
static tidy_file_ext const FILE_EXT[] = {
  { "c",   LANG_C   },
  { "c++", LANG_CXX },
  { "cc",  LANG_CXX },
  { "cp",  LANG_CXX },
  { "cpp", LANG_CXX },
  { "cxx", LANG_CXX },
  { "h",   LANG_C   },
  { "h++", LANG_CXX },
  { "hh",  LANG_CXX },
  { "hp",  LANG_CXX },
  { "hpp", LANG_CXX },
  { "hxx", LANG_CXX },
};

////////// extern functions ///////////////////////////////////////////////////

tidy_file_ext const* file_ext_find( char const *ext ) {
  assert( ext != NULL );

  FOREACH_FILE_EXT( fe ) {
    if ( strcasecmp( ext, fe->ext ) == 0 )
      return fe;
  } // for

  return NULL;
}

tidy_file_ext const* file_ext_next( tidy_file_ext const *fe ) {
  return ARRAY_NEXT( FILE_EXT, fe );
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
