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
#include <stdlib.h>                     /* for bsearch, qsort */

/// @endcond

///
/// @defgroup array-group Dynamic Array
/// A type for a dynamic array and functions for manipulating said array.
///
/// @remarks
/// Functions without an `_nc` suffix do bounds-checking; functions with an
/// `_nc` (no-check) suffix don't.
///
/// @par Example
/// @parblock
/// An array of `int`:
///
///       array_t array_of_int;
///       array_init( &array_of_int, sizeof(int) );
///       *(int*)array_push_back( &array_of_int ) = 42;
///       // ...
///       array_cleanup( &array_of_int, /*free_fn=*/NULL );
///
/// Notice that \ref array_push_back _only_ makes space for a new element and
/// doesn't actually push it; instead, it returns a pointer to the new element
/// that we then cast to `int*` and dereference to assign a value to it.  In
/// general, for an array of type `T`, the pointer is of type `T*`.
///
/// Since the elements are just `int`s, no clean-up function is needed.
/// @endparblock
///
/// @par Example
/// @parblock
/// An array of `char*`:
///
///       array_t array_of_str;
///       array_init( &array_of_str, sizeof(char*) );
///       *(char**)array_push_back( &array_of_str ) = strdup( "hello" );
///       *(char**)array_push_back( &array_of_str ) = strdup( "world" );
///       // ...
///       array_cleanup( &array_of_str, &free_pptr );
///
/// Similar to the previous example, except now the element `T` is `char*`, so,
/// as before, the pointer is of type `T*` or `char**`.
///
/// Since the elements are owning `char*` pointers, a clean-up function is
/// needed.  Note that \ref array_cleanup passes a _pointer_ to each element to
/// be cleaned-up, so, in the case of an element of type `char*`, a _pointer_
/// to that pointer is passed, hence you can't use `free`, but instead need a
/// function that frees a pointer pointed to by a pointer such as:
///
///     void free_pptr( void *pptr ) {
///       if ( pptr != NULL )
///         free( *(void**)pptr );
///     }
/// @endparblock
/// @{
///

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
 * @param array The array to clean up.  If NULL, does nothing; otherwise,
 * reinitializes \a array upon completion.
 * @param free_fn A pointer to a function used to free each element or NULL if
 * unnecessary.
 *
 * @note If \a free_fn is NULL, this is an O(1) operation; otherwise O(N).
 *
 * @sa array_init()
 */
void array_cleanup( array_t *array, array_free_fn_t free_fn );

/**
 * Pushes the elements of \a src_array onto the back of \a dst_array.
 *
 * @param dst_array The \ref array to push onto.
 * @param src_array The \ref array to push the elements of.  It's \ref
 * array::len "length" is set to zero upon return.
 * @return Returns a pointer to the first pushed element in \a dst_array or
 * NULL if \a src_array is empty.
 *
 * @note Both arrays _must_ have the same \ref array::esize "element size".
 * @note This is an O(N) operation.
 *
 * @sa array_push_back()
 */
PJL_DISCARD
void* array_push_array_back( array_t *dst_array, array_t *src_array );

/**
 * Ensures at least \a res_len additional elements of capacity exist in \a
 * array.
 *
 * @param array A pointer to the \ref array to reserve \a res_len additional
 * elements for.
 * @param res_len The number of additional elements to reserve.
 * @return Returns `true` only if a memory reallocation was necessary.
 *
 * @note This is an O(1) best-case operation and O(N + log R) worst-case (where
 * R is \a res_len).
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
 * @note If the element type is a pointer, then this returns a _pointer to that
 * pointer_, i.e., `T**`.
 * @note This is an O(1) operation.
 *
 * @sa array_at()
 * @sa array_back()
 * @sa array_back_nc()
 * @sa array_front()
 * @sa array_front_nc()
 */
NODISCARD
inline void* array_at_nc( array_t const *array, size_t index ) {
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
 * @note If the element type is a pointer, then this returns a _pointer to that
 * pointer_, i.e., `T**`.
 * @note This is an O(1) operation.
 *
 * @sa array_at_nc()
 * @sa array_back()
 * @sa array_back_nc()
 * @sa array_front()
 * @sa array_front_nc()
 */
NODISCARD
inline void* array_at( array_t const *array, size_t index ) {
  return index < array->len ? array_at_nc( array, index ) : NULL;
}

/**
 * Gets a pointer to the element at the back of \a array.
 *
 * @param array A pointer to the \ref array.
 * @return Returns a pointer to the element at the back of \a array or NULL if
 * \a array is empty.
 *
 * @warning \a array is _not_ checked to ensure it's not empty.
 *
 * @note If the element type is a pointer, then this returns a _pointer to that
 * pointer_, i.e., `T**`.
 * @note This is an O(1) operation.
 *
 * @sa array_at()
 * @sa array_at_nc()
 * @sa array_back()
 * @sa array_front()
 * @sa array_front_nc()
 * @sa array_pop_back()
 * @sa array_pop_back_nc()
 */
NODISCARD
inline void* array_back_nc( array_t const *array ) {
  return array_at_nc( array, array->len - 1 );
}

/**
 * Gets a pointer to the element at the back of \a array.
 *
 * @param array A pointer to the \ref array.
 * @return Returns a pointer to the element at the back of \a array or NULL if
 * \a array is empty.
 *
 * @note If the element type is a pointer, then this returns a _pointer to that
 * pointer_, i.e., `T**`.
 * @note This is an O(1) operation.
 *
 * @sa array_at()
 * @sa array_at_nc()
 * @sa array_back_nc()
 * @sa array_front()
 * @sa array_front_nc()
 * @sa array_pop_back()
 * @sa array_pop_back_nc()
 */
NODISCARD
inline void* array_back( array_t const *array ) {
  return array->len > 0 ? array_back_nc( array ) : NULL;
}

/**
 * Calls **bsearch**(3) on \a array.
 *
 * @param array The array to search.
 * @param cmp_fn The comparison function to use.
 * @return Returns a pointer to the matching element or NULL if no match is
 * found.
 */
NODISCARD
inline void* array_bsearch( array_t *array, void const *key,
                            int (*cmp_fn)( void const*, void const*) ) {
  return bsearch( key, array->elements, array->len, array->esize, cmp_fn );
}

/**
 * Gets a pointer to the element at the front of \a array.
 *
 * @param array A pointer to the \ref array.
 * @return Returns a pointer to the element at the front of \a array.
 *
 * @warning \a array is _not_ checked to ensure it's not empty.
 *
 * @note If the element type is a pointer, then this returns a _pointer to that
 * pointer_, i.e., `T**`.
 * @note This is an O(1) operation.
 *
 * @sa array_at()
 * @sa array_at_nc()
 * @sa array_back()
 * @sa array_back_nc()
 * @sa array_front()
 */
NODISCARD
inline void* array_front_nc( array_t const *array ) {
  return array->elements;
}

/**
 * Gets a pointer to the element at the front of \a array.
 *
 * @param array A pointer to the \ref array.
 * @return Returns a pointer to the element at the front of \a array or NULL if
 * \a array is empty.
 *
 * @note If the element type is a pointer, then this returns a _pointer to that
 * pointer_, i.e., `T**`.
 * @note This is an O(1) operation.
 *
 * @sa array_at()
 * @sa array_at_nc()
 * @sa array_back()
 * @sa array_back_nc()
 * @sa array_front_nc()
 */
NODISCARD
inline void* array_front( array_t const *array ) {
  return array->len > 0 ? array_front_nc( array ) : NULL;
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
 * @warning \a array is _not_ checked to ensure it's not empty.
 *
 * @note If the element type is a pointer, then this returns a _pointer to that
 * pointer_, i.e., `T**`.
 * @note This is an O(1) operation.
 *
 * @sa array_at()
 * @sa array_at_nc()
 * @sa array_back()
 * @sa array_back_nc()
 * @sa array_pop_back()
 */
PJL_DISCARD
inline void* array_pop_back_nc( array_t *array ) {
  return array_at_nc( array, --array->len );
}

/**
 * Pops an element from the back of \a array.
 *
 * @param array The pointer to the \ref array.
 * @return Returns a pointer to the element at the back of \a array or NULL if
 * the array is empty.  The caller is responsible for freeing it if necessary.
 *
 * @note If the element type is a pointer, then this returns a _pointer to that
 * pointer_, i.e., `T**`.
 * @note This is an O(1) operation.
 *
 * @sa array_at()
 * @sa array_at_nc()
 * @sa array_back()
 * @sa array_back_nc()
 * @sa array_pop_back_nc()
 */
PJL_DISCARD
inline void* array_pop_back( array_t *array ) {
  return array->len > 0 ? array_pop_back_nc( array ) : NULL;
}

/**
 * Appends space for a new element onto the back of \a array.
 *
 * @param array The \ref array to push onto.
 * @return Returns a pointer to the new element.
 *
 * @note This is an O(1) amortized operation and O(N) worst-case.
 *
 * @sa array_push_array_back()
 */
NODISCARD
inline void* array_push_back( array_t *array ) {
  array_reserve( array, 1 );
  return array_at_nc( array, array->len++ );
}

/**
 * Calls **qsort**(3) on \a array.
 *
 * @param array The array to sort.
 * @param cmp_fn The comparison function to use.
 */
inline void array_qsort( array_t *array,
                         int (*cmp_fn)( void const*, void const* ) ) {
  qsort( array->elements, array->len, array->esize, cmp_fn );
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* pjl_array_H */
/* vim:set et sw=2 ts=2: */
