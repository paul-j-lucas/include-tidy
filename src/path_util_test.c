/*
**      include-tidy -- #include tidier
**      src/path_util_test.c
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
#include "path_util.h"
#include "unit_test.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#pragma GCC diagnostic ignored "-Wunused-value"

#define STR_STRLEN(LIT)           (LIT), STRLITLEN(LIT)

/// @endcond

////////// test functions /////////////////////////////////////////////////////

static bool test_path_dirname( void ) {
  TEST_FUNC_BEGIN();

  char buf[ PATH_MAX + 1 ];

  // Standard multi-level paths
  TEST( strcmp( path_dirname( "/usr/local/bin", buf ), "/usr/local" ) == 0 );
  TEST( strcmp( path_dirname( "foo/bar/baz.txt", buf ), "foo/bar" ) == 0 );
  TEST( strcmp( path_dirname( "foo/bar", buf ), "foo" ) == 0 );

  // Paths with no directory component
  TEST( strcmp( path_dirname( "file.txt", buf ), "." ) == 0 );
  TEST( strcmp( path_dirname( "", buf ), "." ) == 0 );

  // Root and single-level absolute paths
  TEST( strcmp( path_dirname( "/", buf ), "/" ) == 0 );
  TEST( strcmp( path_dirname( "/etc", buf ), "/" ) == 0 );

  // Paths ending with a slash
  TEST( strcmp( path_dirname( "dir/", buf ), "dir" ) == 0 );
  TEST( strcmp( path_dirname( "/var/log/", buf ), "/var/log" ) == 0 );

  TEST_FUNC_END();
}

static bool test_path_ends_with( void ) {
  TEST_FUNC_BEGIN();

  TEST(  path_ends_with( "/var/bar/x.log", STR_STRLEN( "bar/x.log" ) ) );
  TEST( !path_ends_with( "/var/bar/x.log", STR_STRLEN( "foobar/x.log" ) ) );
  TEST( !path_ends_with( "/var/bar/x.log", STR_STRLEN( "foobarbaz/x.log" ) ) );

  TEST_FUNC_END();
}

static bool test_path_ext( void ) {
  TEST_FUNC_BEGIN();

  char const *ext;

  TEST( path_ext( "" ) == NULL );
  TEST( path_ext( "a" ) == NULL );
  TEST( path_ext( "a." ) == NULL );
  TEST( (ext = path_ext( "a.b" )) != NULL )
    && TEST( strcmp( ext, "b" ) == 0 );
  TEST( (ext = path_ext( "a.b.c" )) != NULL )
    && TEST( strcmp( ext, "c" ) == 0 );
  TEST( path_ext( "a.b/c" ) == NULL );

  TEST_FUNC_END();
}

static bool test_path_no_dot_slash( void ) {
  TEST_FUNC_BEGIN();

  TEST( strcmp( path_no_dot_slash( "a" ), "a" ) == 0 );
  TEST( strcmp( path_no_dot_slash( ".a" ), ".a" ) == 0 );
  TEST( strcmp( path_no_dot_slash( "./a" ), "a" ) == 0 );
  TEST( strcmp( path_no_dot_slash( "././a" ), "a" ) == 0 );

  TEST_FUNC_END();
}

static bool test_path_no_ext( void ) {
  TEST_FUNC_BEGIN();

  char const *path;
  char path_buf[ PATH_MAX + 1 ];

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

static bool test_path_normalize( void ) {
  TEST_FUNC_BEGIN();

  char *path;

  if ( TEST( (path = path_normalize( "/a" )) != NULL ) ) {
    TEST( strcmp( path, "/a" ) == 0 );
    free( path );
  }
  if ( TEST( (path = path_normalize( "/a/./b" )) != NULL ) ) {
    TEST( strcmp( path, "/a/b" ) == 0 );
    free( path );
  }
  if ( TEST( (path = path_normalize( "/a/b/." )) != NULL ) ) {
    TEST( strcmp( path, "/a/b" ) == 0 );
    free( path );
  }
  if ( TEST( (path = path_normalize( "/a/../b" )) != NULL ) ) {
    TEST( strcmp( path, "/b" ) == 0 );
    free( path );
  }
  if ( TEST( (path = path_normalize( "/a/.././b" )) != NULL ) ) {
    TEST( strcmp( path, "/b" ) == 0 );
    free( path );
  }
  if ( TEST( (path = path_normalize( "/a/b/../../c" )) != NULL ) ) {
    TEST( strcmp( path, "/c" ) == 0 );
    free( path );
  }
  if ( TEST( (path = path_normalize( "/a/b/.././../c" )) != NULL ) ) {
    TEST( strcmp( path, "/c" ) == 0 );
    free( path );
  }

  TEST_FUNC_END();
}

////////// main ///////////////////////////////////////////////////////////////

int main( int argc, char const *const argv[] ) {
  test_prog_init( argc, argv );

  test_path_dirname();
  test_path_ends_with();
  test_path_ext();
  test_path_no_dot_slash();
  test_path_no_ext();
  test_path_normalize();

  return test_exit_status;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
