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

/**
 * @file
 * Defines functions for manipulating dynamic arrays.
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "array.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/// @endcond

/**
 * @addtogroup array-group
 * @{
 */

#define ARRAY_CAP_MIN             4     /**< Minimum array capacity. */

////////// extern functions ///////////////////////////////////////////////////

void array_cleanup( array_t *array, array_free_fn_t free_fn ) {
  if ( array == NULL )
    return;

  if ( free_fn != NULL ) {
    char *element = array->elements;
    for ( size_t i = 0; i < array->len; ++i ) {
      (*free_fn)( element );
      element += array->esize;
    } // for
  }

  free( array->elements );
  array_init( array, array->esize );
}

void* array_push_array_back( array_t *dst_array, array_t *src_array ) {
  assert( dst_array != NULL );
  assert( src_array != NULL );
  assert( dst_array->esize == src_array->esize );

  if ( src_array->len == 0 )
    return NULL;

  array_reserve( dst_array, src_array->len );
  void *const dst_end = array_at_nc( dst_array, dst_array->len );
  memcpy( dst_end, src_array->elements, src_array->len * src_array->esize );
  dst_array->len += src_array->len;
  src_array->len = 0;
  return dst_end;
}

bool array_reserve( array_t *array, size_t res_len ) {
  assert( array != NULL );
  if ( res_len <= array->cap - array->len )
    return false;
  if ( array->cap == 0 )
    array->cap = ARRAY_CAP_MIN;
  size_t const min_cap = array->len + res_len;
  //
  // Why not grow by 2x?  The problem is that the size of each new allocation
  // is always > the sum of all previous allocations combined, which means
  // malloc can't reuse even a block coalesced from previous allocations.
  //
  // For example, given the previous allocations of 4, 8, and 16 (summing to
  // 28), the next allocation will be 32, but 32 > 28, so malloc can't reuse
  // that block.
  //
  // In contast, growing by 1.5x yields allocations 4, 6, and 9 (summing to
  // 19), and the next allocation will be 13, and 13 <= 19, so malloc can reuse
  // that block.
  //
  while ( array->cap < min_cap )
    array->cap += array->cap >> 1;      // grow by ~1.5x
  array->elements = check_realloc( array->elements, array->cap * array->esize );
  return true;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/// @cond DOXYGEN_IGNORE

extern inline void* array_at( array_t const*, size_t );
extern inline void* array_at_nc( array_t const*, size_t );
extern inline void* array_back( array_t const* );
extern inline void* array_back_nc( array_t const* );
extern inline void* array_bsearch( array_t*, void const*,
                                   int (*)( void const*, void const* ) );
extern inline void* array_front( array_t const* );
extern inline void* array_front_nc( array_t const* );
extern inline void array_init( array_t*, size_t );
extern inline void* array_pop_back( array_t* );
extern inline void* array_pop_back_nc( array_t* );
extern inline void* array_push_back( array_t* );
extern inline void array_qsort( array_t*, int (*)( void const*, void const* ) );

/// @endcond

/* vim:set et sw=2 ts=2: */
