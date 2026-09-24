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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/// @endcond

/**
 * @addtogroup array-group
 * @{
 */

#define ARRAY_CAP_MIN             4     /**< Minimum array capacity. */

////////// extern functions ///////////////////////////////////////////////////

void array_cleanup( array_t *restrict array, array_free_fn_t free_fn ) {
  if ( array == NULL || array->elements == NULL )
    return;

  // Force hoist of array-> out of loop.
  char *const   elements = array->elements;
  size_t const  esize = array->esize;

  if ( free_fn != NULL ) {
    char const *const end = elements + array->len * esize;
    for ( char *element = elements; element < end; element += esize )
      (*free_fn)( element );
  }

  free( elements );
  array_init( array, esize );
}

void* array_push_array_back( array_t *restrict dst_array,
                             array_t *restrict src_array ) {
  assert( dst_array != NULL );
  assert( src_array != NULL );
  assert( dst_array != src_array );
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

bool array_reserve( array_t *restrict array, size_t res_len ) {
  assert( array != NULL );

  if ( res_len <= array->cap - array->len )
    return false;

  assert( res_len <= SIZE_MAX - array->len );
  size_t const min_cap = array->len + res_len;

  size_t const max_cap = SIZE_MAX / array->esize;
  assert( min_cap <= max_cap );

  if ( array->cap == 0 )
    array->cap = ARRAY_CAP_MIN;

  //
  // Why not grow by 2x?  The problem is that the size of each new allocation
  // is always > the sum of all previous allocations combined, which means
  // malloc can't reuse even a block coalesced from previous allocations.
  //
  // For example, given the previous allocations of 4, 8, and 16 (summing to
  // 28), the next allocation will be 32, but 32 > 28, so malloc can't reuse
  // that block.
  //
  // In contrast, growing by 1.5x yields allocations 4, 6, and 9 (summing to
  // 19), and the next allocation will be 13, and 13 <= 19, so malloc can reuse
  // that block.
  //
  while ( array->cap < min_cap ) {
    size_t const delta = array->cap >> 1; // grow by ~1.5x
    if ( unlikely( array->cap > max_cap - delta ) ) {
      // LCOV_EXCL_START
      array->cap = min_cap;
      break;
      // LCOV_EXCL_STOP
    }
    array->cap += delta;
  }
  array->elements = check_realloc( array->elements, array->cap * array->esize );
  return true;
}

void array_unique( array_t *restrict array, array_cmp_fn_t cmp_fn,
                   array_free_fn_t free_fn ) {
  assert( array != NULL );
  assert( cmp_fn != NULL );

  if ( array->len < 2 )
    return;

  void             *dst = array_front_nc( array );
  char const *const end = array_at_nc( array, array->len );
  size_t const      esize = array->esize;
  size_t            new_len = 1;

  for ( char *src = array_at_nc( array, 1 ); src < end; src += esize ) {
    if ( (*cmp_fn)( dst, src ) == 0 ) {
      if ( free_fn != NULL )
        (*free_fn)( src );
    }
    else {
      dst = array_at_nc( array, new_len++ );
      if ( dst != src )
        memcpy( dst, src, esize );
    }
  } // for

  array->len = new_len;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/// @cond DOXYGEN_IGNORE

// See comment for NONCONST_OVERLOAD regarding ().
extern inline void const* (array_at)( array_t const*, size_t );
extern inline void const* (array_at_nc)( array_t const*, size_t );
extern inline void const* (array_back)( array_t const* );
extern inline void const* (array_back_nc)( array_t const* );

extern inline void* array_bsearch( array_t*, void const*, array_cmp_fn_t );

// See comment for NONCONST_OVERLOAD regarding ().
extern inline void const* (array_front)( array_t const* );
extern inline void const* (array_front_nc)( array_t const* );

extern inline void array_init( array_t*, size_t );
extern inline void* array_pop_back( array_t* );
extern inline void* array_pop_back_nc( array_t* );
extern inline void* array_push_back( array_t* );
extern inline void array_qsort( array_t*, array_cmp_fn_t );

extern inline void* nonconst_array_at( array_t*, size_t );
extern inline void* nonconst_array_at_nc( array_t*, size_t );
extern inline void* nonconst_array_back( array_t* );
extern inline void* nonconst_array_back_nc( array_t* );
extern inline void* nonconst_array_front( array_t* );
extern inline void* nonconst_array_front_nc( array_t* );

/// @endcond

/* vim:set et sw=2 ts=2: */
