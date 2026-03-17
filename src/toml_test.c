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
#include <stdlib.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////

struct toml_test {
  toml_file   toml;
  toml_table  table;
};
typedef struct toml_test toml_test;

////////// local functions ////////////////////////////////////////////////////

static void strip_table_name( char *s ) {
  char *t = s;
  do {
    switch ( *s ) {
      case ' ':
      case '\n':
      case '[':
      case ']':
        continue;
      default:
        *t++ = *s;
    } // switch
  } while ( *s++ != '\0' );
}

static void toml_error_print( toml_file const *toml ) {
  assert( toml != NULL );
  if ( toml->error )
    EPRINTF( "%u:%u: %s\n", toml->line, toml->col, toml_error_msg( toml ) );
}

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

static bool test_valid_table_names( char const *const table_names[] ) {
  TEST_FUNC_BEGIN();

  for ( char const *const *table_name = table_names;
        *table_name != NULL; ++table_name ) {
    toml_test test;
    toml_test_init( &test, *table_name );
    if ( TEST( toml_table_next( &test.toml, &test.table ) ) &&
         TEST( test.table.name != NULL ) ) {
      char *const expected_name = strdup( *table_name );
      strip_table_name( expected_name );
      TEST( strcmp( test.table.name, expected_name ) == 0 );
      free( expected_name );
    }
    toml_error_print( &test.toml );
    toml_test_cleanup( &test );
  } // while

  TEST_FUNC_END();
}

static bool test_value_array( void ) {
  TEST_FUNC_BEGIN();

  static char const *const TOML =
    "[test-array]\n"
    "ab = [   \n"
    "  false, \n"
    "  true   \n"
    "]        \n";

  toml_test test;
  toml_test_init( &test, TOML );

  if ( TEST( toml_table_next( &test.toml, &test.table ) ) ) {
    toml_value const *value;

    value = toml_table_find( &test.table, "ab" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_ARRAY ) &&
      TEST( value->a.size == 2 ) &&
      TEST( value->a.values[0].type == TOML_BOOL ) &&
      TEST( value->a.values[0].b == false ) &&
      TEST( value->a.values[1].type == TOML_BOOL ) &&
      TEST( value->a.values[1].b == true );
  }

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_bool( void ) {
  TEST_FUNC_BEGIN();

  static char const *const TOML =
    "[test-bool]  \n"
    "bf = false   \n"
    "bt = true    \n";

  toml_test test;
  toml_test_init( &test, TOML );

  if ( TEST( toml_table_next( &test.toml, &test.table ) ) ) {
    toml_value const *value;

    value = toml_table_find( &test.table, "bf" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_BOOL ) &&
      TEST( value->b == false );

    value = toml_table_find( &test.table, "bt" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_BOOL ) &&
      TEST( value->b == true );
  }

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_int( void ) {
  TEST_FUNC_BEGIN();

  static char const *const TOML =
    "[test-int]     \n"
    "i2 = 0b101010  \n"
    "i8 = 0o52      \n"
    "i10 = 42       \n"
    "i16 = 0x2A     \n"
    "i10_us = 4_2   \n";

  toml_test test;
  toml_test_init( &test, TOML );

  if ( TEST( toml_table_next( &test.toml, &test.table ) ) ) {
    toml_value const *value;

    value = toml_table_find( &test.table, "i2" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_INT ) &&
      TEST( value->i == 42 );

    value = toml_table_find( &test.table, "i8" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_INT ) &&
      TEST( value->i == 42 );

    value = toml_table_find( &test.table, "i10" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_INT ) &&
      TEST( value->i == 42 );

    value = toml_table_find( &test.table, "i16" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_INT ) &&
      TEST( value->i == 42 );

    value = toml_table_find( &test.table, "i10_us" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_INT ) &&
      TEST( value->i == 42 );
  }

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_string( void ) {
  TEST_FUNC_BEGIN();
  static char const *const TOML =
    "[test-string]  \n"
    "s1 = \"ab\"    \n";

  toml_test test;
  toml_test_init( &test, TOML );

  if ( TEST( toml_table_next( &test.toml, &test.table ) ) ) {
    toml_value const *value;

    value = toml_table_find( &test.table, "s1" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_STRING ) &&
      TEST( strcmp( value->s, "ab" ) == 0 );
  }

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

////////// main ///////////////////////////////////////////////////////////////

int main( int argc, char const *const argv[] ) {
  test_prog_init( argc, argv );

  static char const *const VALID_TABLE_NAMES[] = {
    "[ab]",
    "[ ab ]",
    "[a.b]",
    "[a .b]",
    "[a  .b]",
    "[a . b]",
    "[a  .  b]",
    "[a. b]",
    "[a.  b]",
    NULL
  };
  test_valid_table_names( VALID_TABLE_NAMES );
  test_value_bool();
  test_value_int();
  test_value_string();
  if ( test_failures == 0 )
    test_value_array();
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
