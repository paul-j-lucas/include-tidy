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
#include "fnv1a.h"
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

#define TEST_LIT(KEY,VAL)     (test_data){ .key = (KEY), .val = (VAL) }

#define TEST_INSERT(TABLE,KEY,VAL) \
  PJL_DISCARD_RV( ht_insert( (TABLE), &TEST_LIT( (KEY), (VAL) ), sizeof(test_data) ) )

static double const TEST_LOAD_FACTOR_MAX = 1.0;

////////// local functions ////////////////////////////////////////////////////

static int test_cmp( void const *iv, void const *jv ) {
  test_data const *const it = iv;
  test_data const *const jt = jv;
  return strcmp( it->key, jt->key );
}

static ht_hash_val_t test_hash( void const *v ) {
  test_data const *const t = v;
  return fnv1a64_mem( FNV1A_INIT, t->key, 1 );
}

static void test_ht_fill( hash_table_t *table ) {
  TEST_INSERT( table, "A", 1 );
  TEST_INSERT( table, "B", 2 );
  TEST_INSERT( table, "C", 3 );
  TEST_INSERT( table, "D", 4 );
  TEST_INSERT( table, "E", 5 );
}

static void test_ht_init( hash_table_t *table ) {
  ht_init( table, HT_DINT, TEST_LOAD_FACTOR_MAX, 0, &test_cmp, &test_hash );
}

static void test_ht_seen( hash_table_t *table, bool seen[] ) {
  memset( seen, 0, 128 * sizeof(bool) );

  ht_iterator_t it;
  ht_iterator_init( &it, table );

  for ( test_data const *data; (data = ht_iterator_next( &it )) != NULL; )
    seen[ (unsigned)data->key[0] ] = true;
}

////////// test functions /////////////////////////////////////////////////////

#include "unit_test.h"

static bool test_find_delete() {
  TEST_FUNC_BEGIN();

  hash_table_t table;
  test_ht_init( &table );
  test_ht_fill( &table );

  ht_entry_t *entry;
  entry = ht_find( &table, &TEST_LIT( "X", 0 ) );
  TEST( entry == NULL );

  entry = ht_find( &table, &TEST_LIT( "D", 0 ) );
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

  ht_insert_rv_t hti =
    ht_insert( &table, &TEST_LIT( "A", 0 ), sizeof(test_data) );
  if ( TEST( !ht_empty( &table ) ) && TEST( hti.inserted ) )
    goto end_test;
  test_data *t = HT_DINT( hti.entry );
  TEST( strcmp( t->key, "A" ) == 0 );
  TEST( t->val == 0 );

  hti = ht_insert( &table, &TEST_LIT( "A", 1 ), sizeof(test_data) );
  TEST( !hti.inserted );

  t = HT_DINT( hti.entry );
  TEST( strcmp( t->key, "A" ) == 0 );
  TEST( t->val == 0 );

  ht_delete( &table, hti.entry );
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

  (void)(test_insert_delete() &&
         test_iter() &&
         test_find_delete());
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
