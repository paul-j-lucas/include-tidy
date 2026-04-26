/*
**      PJL Library
**      src/array.h
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

#ifndef pjl_array_H
#define pjl_array_H

/**
 * @file
 * Declares a type to represent a dynamic array as well as functions for
 * manipulating said arrays.
 */

// local
#include "pjl_config.h"                 /* must go first */

/// @cond DOXYGEN_IGNORE

// standard
#include <stdbool.h>
#include <stddef.h>                     /* for size_t */

/// @endcond

/**
 * @defgroup array-group Dynamic Array
 * A type for a dynamic array functions for manipulating said array.
 * @{
 */

////////// typedefs ///////////////////////////////////////////////////////////

typedef struct array array_t;

/**
 * The signature for a function passed to array_cleanup() used to free an
 * element (if necessary).
 *
 * @param element A pointer to the element to free.
 */
typedef void (*array_free_fn_t)( void *element );

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
 * @param free_fn A pointer to a function used to free each element or NULL if
 * unnecessary.
 *
 * @sa array_init()
 */
void array_cleanup( array_t *array, array_free_fn_t free_fn );

/**
 * Appends space for a new element onto the back of \a array.
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
 * Gets a pointer to the element at \a offset of \a array.
 *
 * @param array A pointer to the \ref array.
 * @param index The index (starting at 0) of the element to get.
 * @return Returns a pointer to the element at \a index.
 *
 * @warning \a index is _not_ checked to ensure it's &lt; the array's length.
 * A value &ge; the array's length results in undefined behavior.
 *
 * @note If the type of element is a pointer, then this returns a _pointer to
 * that pointer_, i.e., `T**`.
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
 * Gets a pointer to the element at \a offset of \a array.
 *
 * @param array A pointer to the \ref array.
 * @param index The index (starting at 0) of the element to get.
 * @return Returns a pointer to the element at \a index or NULL if \a index
 * &ge; \ref array::len "len".
 *
 * @note If the type of element is a pointer, then this returns a _pointer to
 * that pointer_, i.e., `T**`.
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
 * Gets a pointer to the element at the back of \a array.
 *
 * @param array A pointer to the \ref array.
 * @return Returns a pointer to the element at the back of \a array or NULL if
 * \a array is empty.
 *
 * @note If the type of element is a pointer, then this returns a _pointer to
 * that pointer_, i.e., `T**`.
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
 * Gets a pointer to the element at the front of \a array.
 *
 * @param array A pointer to the \ref array.
 * @return Returns a pointer to the element at the front of \a array or NULL if
 * \a array is empty.
 *
 * @note If the type of element is a pointer, then this returns a _pointer to
 * that pointer_, i.e., `T**`.
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

/**
 * Pops an element from the back of \a array.
 *
 * @param array The pointer to the \ref array.
 * @return Returns a pointer to the element at the back of \a array.  The
 * caller is responsible for freeing it if necessary.
 *
 * @note If the type of element is a pointer, then this returns a _pointer to
 * that pointer_, i.e., `T**`.
 * @note This is an O(1) operation.
 */
PJL_DISCARD
inline void* array_pop_back( array_t *array ) {
  return array->len > 0 ? array_at_nocheck( array, --array->len ) : NULL;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* pjl_array_H */
/* vim:set et sw=2 ts=2: */
