/*
**      PJL Library
**      src/array.c
**
**      Copyright (C) 2017-2026  Paul J. Lucas
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
#include "util.h"

// standard
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

////////// extern functions ///////////////////////////////////////////////////

void array_cleanup( array_t *array, array_free_fn_t free_fn ) {
  assert( array != NULL );

  if ( free_fn != NULL ) {
    char *element = POINTER_CAST( char*, array->elements );
    for ( size_t i = 0; i < array->len; ++i ) {
      (*free_fn)( element );
      element += array->esize;
    } // for
  }

  free( array->elements );
  array_init( array, array->esize );
}

void* array_push_back( array_t *array ) {
  assert( array != NULL );
  size_t const new_len = array->len + 1;
  array_reserve( array, new_len );
  void *const rv = array_at_nocheck( array, array->len );
  array->len = new_len;
  return rv;
}

bool array_reserve( array_t *array, size_t res_len ) {
  assert( array != NULL );
  if ( res_len < array->cap - array->len )
    return false;
  if ( array->cap == 0 )
    array->cap = 2;
  size_t const new_len = array->len + res_len;
  while ( array->cap <= new_len )
    array->cap <<= 1;
  array->elements = check_realloc( array->elements, array->cap * array->esize );
  return true;
}

///////////////////////////////////////////////////////////////////////////////

/// @cond DOXYGEN_IGNORE

extern inline void* array_at( array_t const*, size_t );
extern inline void* array_at_nocheck( array_t const*, size_t );
extern inline void* array_back( array_t const* );
extern inline void* array_front( array_t const* );
extern inline void array_init( array_t*, size_t );
extern inline void* array_pop_back( array_t* );

/// @endcond

/* vim:set et sw=2 ts=2: */
