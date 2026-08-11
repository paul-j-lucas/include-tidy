/*
**      PJL Library
**      src/hash_table_test.c
**
**      Copyright (C) 2021-2026  Paul J. Lucas
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
#include "pjl_config.h"                 /* must go first */
#include "hash_table.h"
#include "util.h"

// standard
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////

typedef struct test_data test_data;

struct test_data {
  char  key[2];
  int   val;
};

#define TEST_DINT(ENTRY)      (test_data*)HT_DINT( (ENTRY) )
#define TEST_INSERT(HT,KEY)   ht_insert( (HT), (void*)(KEY), sizeof(test_data) )
#define TEST_LIT(KEY,VAL)     (test_data){ .key = (KEY), .val = (VAL) }

#define TEST_ASSIGN(ENTRY,KEY,VAL) \
  *TEST_DINT( (ENTRY) ) = TEST_LIT( (KEY), (VAL) )

#define TEST_INSERT_ASSIGN(HT,KEY,VAL) \
  TEST_ASSIGN( TEST_INSERT( (HT), (KEY) ).entry, (KEY), (VAL) )

static double const TEST_LOAD_FACTOR_MAX = 1.0;

////////// local functions ////////////////////////////////////////////////////

static ht_hash_val_t test_fnv1a( void const *data ) {
  test_data const *const test_data = data;
  return fnv1a64_mem( FNV1A_INIT, test_data->key, 1 );
}

static void test_ht_fill( hash_table_t *table ) {
  TEST_INSERT_ASSIGN( table, "A", 1 );
  TEST_INSERT_ASSIGN( table, "B", 2 );
  TEST_INSERT_ASSIGN( table, "C", 3 );
  TEST_INSERT_ASSIGN( table, "D", 4 );
  TEST_INSERT_ASSIGN( table, "E", 5 );
}

static void test_ht_init( hash_table_t *table ) {
  ht_init(
    table, TEST_LOAD_FACTOR_MAX, 0, POINTER_CAST( ht_cmp_fn_t, &strcmp ),
    &test_fnv1a
  );
}

static void test_ht_seen( hash_table_t *table, bool seen[] ) {
  memset( seen, 0, 128 * sizeof(bool) );

  ht_iterator_t it;
  ht_iterator_init( &it, table );

  for ( ht_entry_t const *entry; (entry = ht_iterator_next( &it )) != NULL; )
    seen[ (unsigned)((test_data*)HT_DINT( entry ))->key[0] ] = true;
}

////////// test functions /////////////////////////////////////////////////////

#include "unit_test.h"

static bool test_find_delete() {
  TEST_FUNC_BEGIN();

  hash_table_t table;
  test_ht_init( &table );
  test_ht_fill( &table );

  ht_entry_t *entry;
  entry = ht_find( &table, "X" );
  TEST( entry == NULL );

  entry = ht_find( &table, "D" );
  if ( !TEST( entry != NULL ) )
    goto end_test;

  ht_delete( &table, entry );

  bool seen[ 128 ];
  test_ht_seen( &table, seen );

  TEST(  seen[ 'A' ] );
  TEST(  seen[ 'B' ] );
  TEST(  seen[ 'C' ] );
  TEST( !seen[ 'D' ] );
  TEST(  seen[ 'E' ] );

end_test:
  ht_cleanup( &table, /*free_fn=*/NULL );
  TEST_FUNC_END();
}

static bool test_insert_delete() {
  TEST_FUNC_BEGIN();

  hash_table_t table;
  test_ht_init( &table );

  ht_insert_rv_t hti_rv = ht_insert( &table, (void*)"A", sizeof(test_data) );
  if ( TEST( !ht_empty( &table ) ) && TEST( hti_rv.inserted ) )
    goto end_test;
  TEST_ASSIGN( hti_rv.entry, "A", 1 );

  test_data const *data = TEST_DINT( hti_rv.entry );
  TEST( strcmp( data->key, "A" ) == 0 );
  TEST( data->val == 1 );

  hti_rv = ht_insert( &table, (void*)"A", sizeof(test_data) );
  TEST( !hti_rv.inserted );

  ht_delete( &table, hti_rv.entry );
  TEST( ht_empty( &table ) );

end_test:
  ht_cleanup( &table, /*free_fn=*/NULL );
  TEST_FUNC_END();
}

static bool test_iter() {
  TEST_FUNC_BEGIN();

  hash_table_t table;
  test_ht_init( &table );
  test_ht_fill( &table );

  bool seen[ 128 ];
  test_ht_seen( &table, seen );

  TEST( seen[ 'A' ] );
  TEST( seen[ 'B' ] );
  TEST( seen[ 'C' ] );
  TEST( seen[ 'D' ] );
  TEST( seen[ 'E' ] );

  ht_cleanup( &table, /*free_fn=*/NULL );
  TEST_FUNC_END();
}

////////// main ///////////////////////////////////////////////////////////////

int main( int argc, char const *const argv[] ) {
  test_prog_init( argc, argv );

  test_insert_delete() &&
  test_iter() &&
  test_find_delete();
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
