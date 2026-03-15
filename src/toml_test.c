/*
**      PJL Library
**      src/toml_test.c
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
#include "toml_lite.h"
#include "util.h"
#include "unit_test.h"

// standard
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////

struct toml_test {
  toml_file   toml;
  toml_table  table;
};
typedef struct toml_test toml_test;

////////// local functions ////////////////////////////////////////////////////

static void toml_test_cleanup( toml_test *test ) {
  if ( test == NULL )
    return;
  toml_table_cleanup( &test->table );
  toml_close( &test->toml );
}

static void toml_test_init( toml_test *test, char const *buf ) {
  assert( test != NULL );
  assert( buf != NULL );

  FILE *const file = fmemopen( CONST_CAST( void*, buf ), strlen( buf ), "r" );
  if ( file == NULL )
    fatal_error( EX_SOFTWARE, "%s\n", STRERROR() );
  toml_init( &test->toml, file );
  toml_table_init( &test->table );
}

////////// test functions /////////////////////////////////////////////////////

static bool test_table_names( char const *toml_str ) {
  TEST_FUNC_BEGIN();
  toml_test test;
  toml_test_init( &test, toml_str );

  if ( TEST( toml_table_next( &test.toml, &test.table ) ) ) {
    if ( TEST( test.table.name != NULL ) )
      TEST( strcmp( test.table.name, "table-1" ) == 0 );
  }
    
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

////////// main ///////////////////////////////////////////////////////////////

int main( int argc, char const *const argv[] ) {
  test_prog_init( argc, argv );

  char const TOML_1[] =
    "[table-1]\n"
    "bare-bool = true\n";

  test_table_names( TOML_1 );
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
