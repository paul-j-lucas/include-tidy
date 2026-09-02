/*
**      PJL Library
**      src/array_test.c
**
**      Copyright (C) 2025-2026  Paul J. Lucas
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
#include "array.h"
#include "unit_test.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <stdbool.h>
#include <stdio.h>

/// @endcond

////////// local variables ////////////////////////////////////////////////////

static unsigned test_cleanup_called;

////////// local functions ////////////////////////////////////////////////////

static void test_cleanup( void *element ) {
  (void)element;
  ++test_cleanup_called;
}

static int test_int_cmp( void const *i_element, void const *j_element ) {
  int const *const pi = i_element;
  int const *const pj = j_element;
  return *pi - *pj;
}

////////// test functions /////////////////////////////////////////////////////

static bool test_array_basics( void ) {
  TEST_FUNC_BEGIN();

  array_t a = ARRAY_INIT( sizeof(int) );
  TEST( a.elements == NULL );
  TEST( a.esize == sizeof(int) );
  TEST( a.len == 0 );
  TEST( a.cap == 0 );

  void *e = array_push_back( &a );
  TEST( a.elements != NULL );
  TEST( a.len == 1 );
  TEST( a.cap >= a.len );
  if ( TEST( e != NULL ) )
    *(int*)e = 42;

  e = array_front( &a );
  if ( TEST( e != NULL ) )
    TEST( *(int*)e == 42 );

  e = array_at( &a, 1 );
  TEST( e == NULL );

  if ( TEST( array_reserve( &a, 10 ) ) ) {
    TEST( a.elements != NULL );
    TEST( a.len == 1 );
    TEST( a.cap >= 10 );
  }

  e = array_pop_back( &a );
  if ( TEST( e != NULL ) )
    TEST( *(int*)e == 42 );
  TEST( a.elements != NULL );
  TEST( a.len == 0 );

  array_cleanup( &a, NULL );
  TEST( a.elements == NULL );
  TEST( a.esize == sizeof(int) );
  TEST( a.len == 0 );
  TEST( a.cap == 0 );

  TEST_FUNC_END();
}

static bool test_array_cleanup( void ) {
  TEST_FUNC_BEGIN();

  array_t a = ARRAY_INIT( sizeof(int) );
  *(int*)array_push_back( &a ) = 0;
  *(int*)array_push_back( &a ) = 1;
  test_cleanup_called = 0;
  array_cleanup( &a, &test_cleanup );
  TEST( test_cleanup_called == 2 );

  TEST_FUNC_END();
}

static bool test_array_grow( void ) {
  TEST_FUNC_BEGIN();

  array_t a = ARRAY_INIT( sizeof(int) );
  array_cleanup( &a, NULL );

  *(int*)array_push_back( &a ) = 0;
  *(int*)array_push_back( &a ) = 1;
  *(int*)array_push_back( &a ) = 2;
  *(int*)array_push_back( &a ) = 3;
  TEST( a.elements != NULL );
  TEST( a.len == 4 );
  TEST( a.cap == a.len );

  *(int*)array_push_back( &a ) = 4;
  TEST( a.elements != NULL );
  TEST( a.len == 5 );
  TEST( a.cap > a.len );

  void *e;
  if ( TEST( (e = array_at_nc( &a, 0 )) != NULL ) )
    TEST( *(int*)e == 0 );
  if ( TEST( (e = array_at_nc( &a, 1 )) != NULL ) )
    TEST( *(int*)e == 1 );
  if ( TEST( (e = array_at_nc( &a, 2 )) != NULL ) )
    TEST( *(int*)e == 2 );
  if ( TEST( (e = array_at_nc( &a, 3 )) != NULL ) )
    TEST( *(int*)e == 3 );
  if ( TEST( (e = array_at_nc( &a, 4 )) != NULL ) )
    TEST( *(int*)e == 4 );

  array_cleanup( &a, NULL );
  TEST_FUNC_END();
}

static bool test_array_push_array_back( void ) {
  TEST_FUNC_BEGIN();

  array_t a = ARRAY_INIT( sizeof(int) );
  *(int*)array_push_back( &a ) = 0;

  array_t b = ARRAY_INIT( sizeof(int) );
  array_push_array_back( &a, &b );
  TEST( a.len == 1 );
  TEST( b.len == 0 );

  *(int*)array_push_back( &b ) = 1;
  array_push_array_back( &a, &b );
  TEST( a.len == 2 );
  void *e;
  if ( TEST( (e = array_at_nc( &a, 0 )) != NULL ) )
    TEST( *(int*)e == 0 );
  if ( TEST( (e = array_at_nc( &a, 1 )) != NULL ) )
    TEST( *(int*)e == 1 );
  TEST( b.len == 0 );

  array_cleanup( &a, NULL );
  TEST_FUNC_END();
}

static bool test_array_sort_dedup_bsearch( void ) {
  TEST_FUNC_BEGIN();

  array_t a = ARRAY_INIT( sizeof(int) );
  *(int*)array_push_back( &a ) = 3;
  *(int*)array_push_back( &a ) = 4;
  *(int*)array_push_back( &a ) = 1;
  *(int*)array_push_back( &a ) = 4;
  *(int*)array_push_back( &a ) = 2;
  *(int*)array_push_back( &a ) = 0;
  *(int*)array_push_back( &a ) = 2;

  array_qsort( &a, &test_int_cmp );
  TEST( a.len == 7 );
  void *e;

  if ( TEST( (e = array_at_nc( &a, 0 )) != NULL ) )
    TEST( *(int*)e == 0 );
  if ( TEST( (e = array_at_nc( &a, 1 )) != NULL ) )
    TEST( *(int*)e == 1 );
  if ( TEST( (e = array_at_nc( &a, 2 )) != NULL ) )
    TEST( *(int*)e == 2 );
  if ( TEST( (e = array_at_nc( &a, 3 )) != NULL ) )
    TEST( *(int*)e == 2 );
  if ( TEST( (e = array_at_nc( &a, 4 )) != NULL ) )
    TEST( *(int*)e == 3 );
  if ( TEST( (e = array_at_nc( &a, 5 )) != NULL ) )
    TEST( *(int*)e == 4 );
  if ( TEST( (e = array_at_nc( &a, 6 )) != NULL ) )
    TEST( *(int*)e == 4 );

  test_cleanup_called = 0;
  array_dedup( &a, &test_int_cmp, &test_cleanup );
  TEST( test_cleanup_called == 2 );
  TEST( a.elements != NULL );
  TEST( a.len == 5 );
  if ( TEST( (e = array_at_nc( &a, 0 )) != NULL ) )
    TEST( *(int*)e == 0 );
  if ( TEST( (e = array_at_nc( &a, 1 )) != NULL ) )
    TEST( *(int*)e == 1 );
  if ( TEST( (e = array_at_nc( &a, 2 )) != NULL ) )
    TEST( *(int*)e == 2 );
  if ( TEST( (e = array_at_nc( &a, 3 )) != NULL ) )
    TEST( *(int*)e == 3 );
  if ( TEST( (e = array_at_nc( &a, 4 )) != NULL ) )
    TEST( *(int*)e == 4 );

  e = array_bsearch( &a, (int const[]){ 3 }, &test_int_cmp );
  if ( TEST( e != NULL ) )
    TEST( *(int*)e == 3 );

  array_cleanup( &a, NULL );
  TEST_FUNC_END();
}

////////// main ///////////////////////////////////////////////////////////////

int main( int argc, char const *const argv[] ) {
  test_prog_init( argc, argv );

  if ( test_array_basics() ) {
    test_array_cleanup();
    test_array_grow();
    test_array_push_array_back();
    test_array_sort_dedup_bsearch();
  }

  return test_exit_status;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
