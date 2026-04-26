/*
**      include-tidy -- #include tidier
**      src/util_test.c
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
#include "unit_test.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <limits.h>
#include <stdbool.h>
#include <string.h>

#pragma GCC diagnostic ignored "-Wunused-value"

/// @endcond

////////// test functions /////////////////////////////////////////////////////

static bool test_path_ext( void ) {
  TEST_FUNC_BEGIN();

  char const *ext;

  TEST( path_ext( "" ) == NULL );
  TEST( path_ext( "a" ) == NULL );
  TEST( path_ext( "a." ) == NULL );
  TEST( (ext = path_ext( "a.b" )) != NULL )
    && TEST( strcmp( ext, "b" ) == 0 );
  TEST( path_ext( "a.b/c" ) == NULL );

  TEST_FUNC_END();
}

static bool test_path_no_ext( void ) {
  TEST_FUNC_BEGIN();

  char const *path;
  char path_buf[ PATH_MAX ];

  TEST( (path = path_no_ext( "a", path_buf )) != NULL )
    && TEST( strcmp( path, "a" ) == 0 );
  TEST( (path = path_no_ext( "a.", path_buf )) != NULL )
    && TEST( strcmp( path, "a" ) == 0 );
  TEST( (path = path_no_ext( "a.b", path_buf )) != NULL )
    && TEST( strcmp( path, "a" ) == 0 );
  TEST( (path = path_no_ext( "a.b/c", path_buf )) != NULL )
    && TEST( strcmp( path, "a.b/c" ) == 0 );
  TEST( (path = path_no_ext( ".a", path_buf )) != NULL )
    && TEST( strcmp( path, ".a" ) == 0 );
  TEST( (path = path_no_ext( "a/.b", path_buf )) != NULL )
    && TEST( strcmp( path, "a/.b" ) == 0 );

  TEST_FUNC_END();
}

////////// main ///////////////////////////////////////////////////////////////

int main( int argc, char const *const argv[] ) {
  test_prog_init( argc, argv );

  test_path_ext();
  test_path_no_ext();
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
