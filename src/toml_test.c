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

////////// typedefs ///////////////////////////////////////////////////////////

typedef struct toml_test toml_test;

////////// structs ////////////////////////////////////////////////////////////

struct toml_test {
  toml_file  toml;
  toml_table table;
};

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
  if ( toml->error ) {
    EPRINTF( "%u:%u: %s\n",
      toml->loc.line, toml->loc.col, toml_error_msg( toml )
    );
  }
}

static void toml_test_cleanup( toml_test *test ) {
  if ( test != NULL ) {
    toml_table_cleanup( &test->table );
    fclose( test->toml.file );
    toml_file_cleanup( &test->toml );
  }
}

static void toml_test_init( toml_test *test, char const *buf ) {
  assert( test != NULL );
  assert( buf != NULL );

  FILE *const file = fmemopen( CONST_CAST( void*, buf ), strlen( buf ), "r" );
  if ( file == NULL )
    fatal_error( EX_SOFTWARE, "%s\n", STRERROR() );
  toml_file_init( &test->toml, file );
  toml_table_init( &test->table );
}

////////// test functions /////////////////////////////////////////////////////

static bool test_comments( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "#1             \n"
    " #2            \n"
    "[test]     #3  \n"
    "bf = false #4  \n"
    "ab = [     #5  \n"
    "  false,   #6  \n"
    "  true     #7  \n"
    "]          #8  \n"
  );

  if ( TEST( toml_table_next( &test.toml, &test.table ) ) ) {
    toml_value const *value;

    value = toml_table_find( &test.table, "bf" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_BOOL ) &&
      TEST( value->b == false );

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

static bool test_key_bad_leading_dot( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]       \n"
    ".key = false \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_KEY )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 1 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_key_bad_string( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]             \n"
    "\"k\x01\" = false  \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_CHAR )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 3 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_key_bad_trailing_dot( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]       \n"
    "key. = false \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_KEY )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 4 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_key_duplicate( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]       \n"
    "key = false  \n"
    "key = true   \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_DUPLICATE_KEY )
    && TEST( test.toml.loc.line == 3 )
    && TEST( test.toml.loc.col == 1 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_key_empty( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[ ]          \n"
    "key = false  \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_KEY )
    && TEST( test.toml.loc.line == 1 )
    && TEST( test.toml.loc.col == 2 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_key_invalid_char( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]\n"
    "k\x01"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_CHAR )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 2 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_key_invalid_char2( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]\n"
    "k \x01"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_CHAR )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 3 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_invalid_char( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]\033"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_CHAR )
    && TEST( test.toml.loc.line == 1 )
    && TEST( test.toml.loc.col == 7 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_invalid_char_in_array( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]     \n"
    "a = [\033  \n"
    " 1         \n"
    "]          \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_CHAR )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col == 6 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_table_name_duplicate( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]       \n"
    "key = false  \n"
    "[test]       \n"
    "key = false  \n"
  );

  TEST( toml_table_next( &test.toml, &test.table ) )
    && TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_DUPLICATE_TABLE )
    && TEST( test.toml.loc.line == 3 )
    && TEST( test.toml.loc.col == 2 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_table_name_eof( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "["
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_UNEX_EOF )
    && TEST( test.toml.loc.line == 1 )
    && TEST( test.toml.loc.col == 1 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_table_name_invalid_char( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[ \x01test]\n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_CHAR )
    && TEST( test.toml.loc.line == 1 )
    && TEST( test.toml.loc.col  == 3 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_table_name_invalid_char2( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "\x01[test]\n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_CHAR )
    && TEST( test.toml.loc.line == 1 )
    && TEST( test.toml.loc.col  == 1 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_table_name_invalid_char3( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[\"test\" \x01]\n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_CHAR )
    && TEST( test.toml.loc.line == 1 )
    && TEST( test.toml.loc.col  == 9 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_table_name_valid( void ) {
  TEST_FUNC_BEGIN();

  static char const *const VALID_TABLE_NAMES[] = {
    "[ab]",
    "[ ab ]",
    "[a.b]",
    "[a .b]",
    "[a  .b]",
    "[a . b]",
    "[a  .  b]",
    "[a. b]",
    "[a.  b]"
  };

  FOREACH_ARRAY_ELEMENT( char const*, ptable_name, VALID_TABLE_NAMES ) {
    toml_test test;
    toml_test_init( &test, *ptable_name );
    if ( TEST( toml_table_next( &test.toml, &test.table ) ) &&
         TEST( test.table.key.name != NULL ) ) {
      char *const expected_name = strdup( *ptable_name );
      strip_table_name( expected_name );
      TEST( strcmp( test.table.key.name, expected_name ) == 0 );
      free( expected_name );
    }
    toml_error_print( &test.toml );
    toml_test_cleanup( &test );
  } // for

  TEST_FUNC_END();
}

static bool test_table_second( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[table-1]  \n"
    "b = false  \n"
    "i = 42     \n"
    "[table-2]  \n"
    "b = true   \n"
  );

  if ( TEST( toml_table_next( &test.toml, &test.table ) ) &&
       TEST( toml_table_next( &test.toml, &test.table ) ) ) {
    toml_value const *value;

    value = toml_table_find( &test.table, "b" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_BOOL ) &&
      TEST( value->b == true );

    value = toml_table_find( &test.table, "i" );
    TEST( value == NULL );

    TEST( !toml_table_next( &test.toml, &test.table ) );
  }

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_unex_char( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]\n"
    "value 42\n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_UNEX_CHAR )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 7 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_unex_eof( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]\n"
    "value"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_UNEX_EOF )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 5 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_array( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]     \n"
    "ab = [     \n"
    "  false,   \n"
    "  true #1  \n"
    "]          \n"
  );

  if ( TEST( toml_table_next( &test.toml, &test.table ) ) ) {
    toml_value const *const value = toml_table_find( &test.table, "ab" );
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

static bool test_value_array_bad_comma( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test] \n"
    "ab = [ \n"
    "  ,    \n"
    "]      \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_UNEX_CHAR )
    && TEST( test.toml.loc.line == 3 )
    && TEST( test.toml.loc.col  == 3 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_array_bad_value( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]   \n"
    "ab = [   \n"
    "  truex  \n"
    "]        \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_UNEX_CHAR )
    && TEST( test.toml.loc.line == 3 )
    && TEST( test.toml.loc.col  == 3 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_array_missing_comma( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]     \n"
    "array = [  \n"
    "  1        \n"
    "  2        \n"
    "]          \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_UNEX_CHAR )
    && TEST( test.toml.loc.line == 4 )
    && TEST( test.toml.loc.col  == 3 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_array_unex_eof( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test] \n"
    "ab = [ \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_UNEX_EOF )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 8 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_bool( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]     \n"
    "bf = false \n"
    "bt = true  \n"
  );

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

static bool test_value_bool_bad_value( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]     \n"
    "b = fALSE  \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_UNEX_CHAR )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 5 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_bool_extra_chars( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]       \n"
    "bool = truest\n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_UNEX_CHAR )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 8 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_int( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]         \n"
    "i2 = 0b101010  \n"
    "i8 = 0o52      \n"
    "i10 = +42      \n"
    "i16 = 0x2A     \n"
    "i10_us = 4_2   \n"
    "n10 = -42      \n"
    "z = 0#         \n"
  );

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

    value = toml_table_find( &test.table, "n10" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_INT ) &&
      TEST( value->i == -42 );

    value = toml_table_find( &test.table, "z" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_INT ) &&
      TEST( value->i == 0 );
  }

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_int_bad_base( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test] \n"
    "i = 0a \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_INT )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 6 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_int_bad_binary( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]   \n"
    "i = 0b2  \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_INT )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 7 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_int_bad_decimal( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]   \n"
    "i = 42a  \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_INT )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 7 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_int_bad_hexadecimal( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]   \n"
    "i = 0xg  \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_INT )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 7 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_int_bad_octal( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]   \n"
    "i = 0o8  \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_INT )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 7 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_int_bad_underscore( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test] \n"
    "i = 1_ \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_INT )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 6 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_int_bad_underscore2( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]   \n"
    "i = 1__0 \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_INT )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 7 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_int_invalid_char( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]     \n"
    "i = 1\x01  \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_CHAR )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 6 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_int_too_many_digits( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]                   \n"
    "i = 12345678901234567890 \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_INT )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 4 + MAX_DEC_INT_DIGITS( long ) );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_string( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]               \n"
    "s1 = \"ab\"          \n"
    "s2 = \"\\\"ab\\\"\"  \n"
    "sb = \"x\\by\"       \n"
    "se = \"x\\ey\"       \n"
    "sf = \"x\\fy\"       \n"
    "sn = \"x\\ny\"       \n"
    "sr = \"x\\ry\"       \n"
    "st = \"x\\ty\"       \n"
    "ss = \"x\\\\y\"      \n"
  );

  if ( TEST( toml_table_next( &test.toml, &test.table ) ) ) {
    toml_value const *value;

    value = toml_table_find( &test.table, "s1" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_STRING ) &&
      TEST( strcmp( value->s, "ab" ) == 0 );

    value = toml_table_find( &test.table, "s2" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_STRING ) &&
      TEST( strcmp( value->s, "\"ab\"" ) == 0 );

    value = toml_table_find( &test.table, "sb" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_STRING ) &&
      TEST( strcmp( value->s, "x\by" ) == 0 );

    value = toml_table_find( &test.table, "se" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_STRING ) &&
      TEST( value->s[0] == 'x' ) &&
      TEST( value->s[1] == 0x1B ) &&
      TEST( value->s[2] == 'y' ) &&
      TEST( value->s[3] == '\0' );

    value = toml_table_find( &test.table, "sf" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_STRING ) &&
      TEST( strcmp( value->s, "x\fy" ) == 0 );

    value = toml_table_find( &test.table, "sn" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_STRING ) &&
      TEST( strcmp( value->s, "x\ny" ) == 0 );

    value = toml_table_find( &test.table, "sr" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_STRING ) &&
      TEST( strcmp( value->s, "x\ry" ) == 0 );

    value = toml_table_find( &test.table, "st" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_STRING ) &&
      TEST( strcmp( value->s, "x\ty" ) == 0 );

    value = toml_table_find( &test.table, "ss" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_STRING ) &&
      TEST( strcmp( value->s, "x\\y" ) == 0 );
  }

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_string_bad_escape( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]         \n"
    "s = \"a\\xb\"  \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_STRING )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 8 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_string_bad_escape_eof( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]     \n"
    "s = \"a\\"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_UNEX_EOF )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 7 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_string_bad_escape_invalid_char( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]           \n"
    "s = \"a\\\x01\"  \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_CHAR )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 8 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_string_eof( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]   \n"
    "s = \"a"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_UNEX_EOF )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 6 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_string_invalid_char( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]         \n"
    "k = \"a\x01b\" \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_CHAR )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 7 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_string_unterminated( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]   \n"
    "s = \"a  \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_INVALID_STRING )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 9 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_unexpected_char( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test] \n"
    "k = x  \n"
  );

  TEST( !toml_table_next( &test.toml, &test.table ) )
    && TEST( test.toml.error == TOML_ERR_UNEX_CHAR )
    && TEST( test.toml.loc.line == 2 )
    && TEST( test.toml.loc.col  == 5 );

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

static bool test_value_whitespace( void ) {
  TEST_FUNC_BEGIN();

  toml_test test;
  toml_test_init( &test,
    "[test]   \n"
    "b1 =     \n"
    "false    \n"
    "b2       \n"
    "= true   \n"
    "i1 = #1  \n"
    "1        \n"
    "i2   #2  \n"
    "= 2      \n"
    "s1 =     \n"
    "\"a\"    \n"
    "s2       \n"
    "= \"b\"  \n"
  );

  if ( TEST( toml_table_next( &test.toml, &test.table ) ) ) {
    toml_value const *value;

    value = toml_table_find( &test.table, "b1" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_BOOL ) &&
      TEST( value->b == false );

    value = toml_table_find( &test.table, "b2" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_BOOL ) &&
      TEST( value->b == true );

    value = toml_table_find( &test.table, "i1" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_INT ) &&
      TEST( value->i == 1 );

    value = toml_table_find( &test.table, "i2" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_INT ) &&
      TEST( value->i == 2 );

    value = toml_table_find( &test.table, "s1" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_STRING ) &&
      TEST( strcmp( value->s, "a" ) == 0 );

    value = toml_table_find( &test.table, "s2" );
    TEST( value != NULL ) &&
      TEST( value->type == TOML_STRING ) &&
      TEST( strcmp( value->s, "b" ) == 0 );
  }

  toml_error_print( &test.toml );
  toml_test_cleanup( &test );
  TEST_FUNC_END();
}

////////// main ///////////////////////////////////////////////////////////////

int main( int argc, char const *const argv[] ) {
  test_prog_init( argc, argv );

  test_comments();

  test_value_bool();
  test_value_int();
  test_value_string();
  test_value_whitespace();

  test_table_second();

  if ( test_failures == 0 ) {
    test_key_bad_leading_dot();
    test_key_bad_string();
    test_key_bad_trailing_dot();
    test_key_duplicate();
    test_key_empty();
    test_key_invalid_char();
    test_key_invalid_char2();

    test_table_name_duplicate();
    test_table_name_eof();
    test_table_name_invalid_char();
    test_table_name_invalid_char2();
    test_table_name_invalid_char3();
    test_table_name_valid();

    test_invalid_char();
    test_invalid_char_in_array();
    test_unex_char();
    test_unex_eof();

    test_value_array();
    test_value_array_bad_comma();
    test_value_array_bad_value();
    test_value_array_missing_comma();
    test_value_array_unex_eof();

    test_value_bool_bad_value();
    test_value_bool_extra_chars();

    test_value_int_bad_base();
    test_value_int_bad_binary();
    test_value_int_bad_decimal();
    test_value_int_bad_hexadecimal();
    test_value_int_bad_octal();
    test_value_int_bad_underscore();
    test_value_int_bad_underscore2();
    test_value_int_invalid_char();
    test_value_int_too_many_digits();

    test_value_string_bad_escape();
    test_value_string_bad_escape_eof();
    test_value_string_bad_escape_invalid_char();
    test_value_string_eof();
    test_value_string_invalid_char();
    test_value_string_unterminated();

    test_value_unexpected_char();
  }

  return test_exit_status;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
