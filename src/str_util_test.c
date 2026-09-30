/*
**      PJL Library
**      src/str_util_test.c
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
#include "unit_test.h"
#include "str_util.h"
#include "util.h"

// standard
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

////////// local functions ////////////////////////////////////////////////////

static bool trim_equal( char const *before, char const *after ) {
  char *const dup = check_strdup( before );
  bool const is_equal = strcmp( str_trim( dup, strlen( dup ) ), after ) == 0;
  free( dup );
  return is_equal;
}

////////// test functions /////////////////////////////////////////////////////

static bool test_str_is_any( void ) {
  TEST_FUNC_BEGIN();

  static char const *const TEST_STRINGS[] = {
    "apple", "banana", "cherry", NULL
  };

  TEST( str_is_any( "apple", TEST_STRINGS ) );
  TEST( str_is_any( "banana", TEST_STRINGS ) );
  TEST( str_is_any( "cherry", TEST_STRINGS ) );

  TEST( !str_is_any( "app", TEST_STRINGS ) );
  TEST( !str_is_any( "orange", TEST_STRINGS ) );
  TEST( !str_is_any( "", TEST_STRINGS ) );

  static char const *const EMPTY_STRINGS[] = { NULL };
  TEST( !str_is_any( "apple", EMPTY_STRINGS ) );

  TEST_FUNC_END();
}

static bool test_str_trim( void ) {
  TEST_FUNC_BEGIN();

  TEST( trim_equal( "x", "x" ) );

  TEST( trim_equal( " x", "x" ) );
  TEST( trim_equal( "x ", "x" ) );
  TEST( trim_equal( " x ", "x" ) );

  TEST( trim_equal( "  x", "x" ) );
  TEST( trim_equal( "x  ", "x" ) );
  TEST( trim_equal( "  x  ", "x" ) );

  TEST( trim_equal( "\tx", "x" ) );
  TEST( trim_equal( "x\t", "x" ) );
  TEST( trim_equal( "\tx\t", "x" ) );

  TEST( trim_equal( "\t\tx", "x" ) );
  TEST( trim_equal( "x\t\t", "x" ) );
  TEST( trim_equal( "\t\tx\t\t", "x" ) );

  TEST_FUNC_END();
}

////////// main ///////////////////////////////////////////////////////////////

int main( int argc, char const *const argv[] ) {
  test_prog_init( argc, argv );

  test_str_is_any();
  test_str_trim();

  return test_exit_status;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
