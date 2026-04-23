/*
**      PJL Library
**      src/array.h
**
**      Copyright (C) 2017-2026  Paul J. Lucas, et al.
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

#ifndef pjl_array_H
#define pjl_array_H

// local
#include "pjl_config.h"                 /* must go first */

// standard
#include <stdbool.h>
#include <stddef.h>

////////// typedefs ///////////////////////////////////////////////////////////

typedef struct array array_t;

/**
 * The signature for a function passed to array_cleanup() used to free data
 * associated with element (if necessary).
 *
 * @param data A pointer to the data to free.
 */
typedef void (*array_free_fn_t)( void *data );

////////// structures /////////////////////////////////////////////////////////

/**
 * An array.
 */
struct array {
  void   *elements;                     ///< Pointer to array of elements.
  size_t  esize;                        ///< Size of an element.
  size_t  len;                          ///< Length of array.
  size_t  cap;                          ///< Capacity of array.
};

////////// extern functions ///////////////////////////////////////////////////

/**
 * Cleans-up all memory associated with \a array but does _not_ free \a array
 * itself.
 *
 * @param array The array tree to clean up.  If NULL, does nothing; otherwise,
 * reinitializes \a array upon completion.
 * @param free_fn A pointer to a function used to free data associated with
 * each element or NULL if unnecessary.
 *
 * @sa array_init()
 */
void array_cleanup( array_t *array, array_free_fn_t free_fn );

/**
 * Appends \a data onto the back of \a list.
 *
 * @param array The \ref array to push onto.
 * @return Returns a pointer to the new element.
 */
NODISCARD
void* array_push_back( array_t *array );

/**
 * Ensures at least \a res_len additional elements of capacity exist in \a
 * array.
 *
 * @param array A pointer to the \ref array to reserve \a res_len additional
 * elements for.
 * @param res_len The number of additional elements to reserve.
 * @return Returns `true` only if a memory reallocation was necessary.
 */
PJL_DISCARD
bool array_reserve( array_t *array, size_t res_len );

////////// inline functions ///////////////////////////////////////////////////

/**
 * Peeks at the element's data at \a offset of \a array.
 *
 * @param array A pointer to the \ref array.
 * @param index The index (starting at 0) of the element's data to get.
 * @return Returns the element's data at \a index.
 *
 * @warning \a index is _not_ checked to ensure it's &lt; the array's length.
 * A value &ge; the array's length results in undefined behavior.
 *
 * @note This function isn't normally called directly; use array_at() instead.
 * @note This is an O(1) operation.
 *
 * @sa array_at()
 * @sa array_back()
 * @sa array_front()
 */
NODISCARD
inline void* array_at_nocheck( array_t const *array, size_t index ) {
  return (char*)array->elements + index * array->esize;
}

/**
 * Peeks at the element's data at \a index of \a array.
 *
 * @param array A pointer to the \ref array.
 * @param index The index (starting at 0) of the element's data to get.
 * @return Returns the element's data at \a index or NULL if \a index &ge;
 * \ref array::len "len".
 *
 * @note This is an O(1) operation.
 *
 * @sa array_at_nocheck()
 * @sa array_back()
 * @sa array_front()
 */
NODISCARD
inline void* array_at( array_t const *array, size_t index ) {
  return index < array->len ? array_at_nocheck( array, index ) : NULL;
}

/**
 * Peeks at the element's data at the back of \a array.
 *
 * @param array A pointer to the \ref array.
 * @return Returns the element's data at the back of \a array or NULL if \a
 * array is empty.
 *
 * @note This is an O(1) operation.
 *
 * @sa array_at_nocheck()
 * @sa array_at()
 * @sa array_front()
 */
NODISCARD
inline void* array_back( array_t const *array ) {
  return array->len > 0 ? array_at_nocheck( array, array->len - 1 ) : NULL;
}

/**
 * Peeks at the element's data at the front of \a array.
 *
 * @param array A pointer to the \ref array.
 * @return Returns the element's data at the front of \a array or NULL if \a
 * array is empty.
 *
 * @note This is an O(1) operation.
 *
 * @sa array_at_nocheck()
 * @sa array_at()
 * @sa array_back()
 */
NODISCARD
inline void* array_front( array_t const *array ) {
  return array->len > 0 ? array->elements : NULL;
}

/**
 * Initializes \a array.
 *
 * @param array A pointer to the \ref array to initialize.
 * @param esize The element size.
 *
 * @sa array_cleanup()
 */
inline void array_init( array_t *array, size_t esize ) {
  *array = (array_t){ .esize = esize };
}

///////////////////////////////////////////////////////////////////////////////

#endif /* pjl_array_H */
/* vim:set et sw=2 ts=2: */
