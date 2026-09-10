/*
**      PJL Library
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
#include "unit_test.h"
#include "util.h"

// standard
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#pragma GCC diagnostic ignored "-Wunused-value"

////////// local functions ////////////////////////////////////////////////////

NODISCARD
static FILE* check_fmemopen( char *buf, size_t buf_size,
                             char const *mode ) {
  assert( buf != NULL );
  assert( mode != NULL );

  FILE *const file = fmemopen( buf, buf_size, mode );
  if ( file == NULL )
    fatal_error( EX_SOFTWARE, "%s\n", STRERROR() );
  return file;
}

static void buf_puts_quoted( char *buf, size_t buf_size, char const *s,
                             char quote ) {
  assert( buf != NULL );
  FILE *const fbuf = check_fmemopen( buf, buf_size, "w" );
  fputs_quoted( s, quote, fbuf );
  fclose( fbuf );
}

static bool trim_equal( char const *before, char const *after ) {
  char *const dup = check_strdup( before );
  bool const is_equal = strcmp( str_trim( dup ), after ) == 0;
  free( dup );
  return is_equal;
}

////////// test functions /////////////////////////////////////////////////////

static bool test_fputs_quoted( void ) {
  TEST_FUNC_BEGIN();

  char buf[ 80 ] = { 0 };

  buf_puts_quoted( buf, sizeof( buf ), "hello \"\b\f\n\r\t\v\\\" world", '"' );
  TEST( strcmp( buf, "\"hello \\\"\\b\\f\\n\\r\\t\\v\\\\\\\" world\"" ) == 0 );

  buf_puts_quoted( buf, sizeof( buf ), "hello \"\b\f\n\r\t\v\\\" world", '\'' );
  TEST( strcmp( buf, "'hello \"\\b\\f\\n\\r\\t\\v\\\\\" world'" ) == 0 );

  buf_puts_quoted( buf, sizeof( buf ), NULL, '"' );
  TEST( strcmp( buf, "null" ) == 0 );

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

  test_fputs_quoted();
  test_str_trim();

  return test_exit_status;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
