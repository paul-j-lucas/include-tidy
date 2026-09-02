/*
**      PJL Library
**      src/hash_table.h
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

#ifndef pjl_hash_table_h
#define pjl_hash_table_h

// local
#include "pjl_config.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>                     /* for max_align_t */
#include <stdint.h>

#ifndef NODISCARD
# define NODISCARD                      /* nothing */
#endif /* NODISCARD */

/// @endcond

/**
 * @defgroup hash-table-group Hash Table
 * Types for defining and functions for manipulating hash tables.
 *
 * @sa [Hash Table](https://en.wikipedia.org/wiki/Hash_table)
 *
 * @{
 */

////////// macros /////////////////////////////////////////////////////////////

/**
 * Gets a pointer to the internal data of \a ENTRY.
 *
 * @param ENTRY The ht_entry to get a pointer to the data of.
 * @return Returns a pointer to the data internal to \a ENTRY.
 *
 * @warning The entry's \ref ht_entry::data "data" _must not_ be modified if
 * that would change the entry's hash value according to its \ref
 * hash_table::hash_fn "hash_fn".
 *
 * @sa #HT_DPTR()
 */
#define HT_DINT(ENTRY)            ( (void*)(ENTRY)->data )

/**
 * Gets an lvalue reference to a pointer to the external data of \a ENTRY.  As
 * an lvalue reference, `HT_DPTR` can appear on the left-hand side of an `=`
 * and be assigned to.
 *
 * @param ENTRY The ht_entry to get a pointer to the data of.
 * @return Returns a pointer to the data \a ENTRY points to.
 *
 * @warning The entry's \ref ht_entry::data "data" _must not_ be modified if
 * that would change the entry's hash value according to its \ref
 * hash_table::hash_fn "hash_fn".
 *
 * @sa #HT_DINT()
 */
#define HT_DPTR(ENTRY)            ( *(void**)HT_DINT( (ENTRY) ) )

////////// enums //////////////////////////////////////////////////////////////

/**
 * Hash table entry data location.
 */
enum ht_dloc {
  /**
   * Entries contain data internally.  The advantages are:
   *
   *  + No separate call to `malloc()` is needed to allocate the data and no
   *    separate call to `free()` is needed to deallocate it.
   *  + The data can be accessed faster since it's already been loaded with the
   *    entry (cache hit).
   *
   * The disadvantages are:
   *
   *  - Inserting data into a tree requires copying it into an entry.
   *  - If you want to delete an entry from the tree but keep its data, you
   *    have to copy it out first.
   */
  HT_DINT,

  /**
   * Entries contain a pointer to the data.  The advantages are:
   *
   *  + Inserting data into a tree requires copying only a pointer into an
   *    entry.
   *  + Hence, the lifetime of data is separate from the tree.
   *
   * The disadvantages are:
   *
   *  - A separate call to `malloc()` is needed to allocate the data and a
   *    separate call to `free()` is needed to deallocate it.
   *  - Accessing the data is slower since it's in a different memory location
   *    than the entry (cache miss).
   */
  HT_DPTR
};

////////// typrdefs ///////////////////////////////////////////////////////////

typedef struct hash_table     hash_table_t;
typedef enum   ht_dloc        ht_dloc_t;
typedef struct ht_entry       ht_entry_t;
typedef uint64_t              ht_hash_val_t;
typedef struct ht_insert_rv   ht_insert_rv_t;
typedef struct ht_iterator    ht_iterator_t;

/**
 * The signature for a function passed to ht_init() used to compare entry data.
 *
 * @remarks This function need only compare for equality; neither less nor
 * greater than comparisons are necessary.
 *
 * @param i_data A pointer to data.
 * @param j_data A pointer to data.
 * @return Returns 0 only if the data pointed to by \a i_data equals that
 * pointed to by \a j_data; non-zero otherwise.
 *
 * @note The return value of 0 meaning equal allows a 3-way comparison function
 * like **strcmp**(3) to be used directly when the data are strings.
 */
typedef int (*ht_cmp_fn_t)( void const *i_data, void const *j_data );

/**
 * The signature for a function passed to ht_cleanup() used to free data
 * associated with each entry (if necessary).
 *
 * @param data A pointer to the data to free.
 */
typedef void (*ht_free_fn_t)( void *data );

/**
 * The signature for a function pass to ht_init() used to hash entry data.
 *
 * @param data A pointer to the data to hash.
 * @return Returns a hash value for \a data.
 */
typedef ht_hash_val_t (*ht_hash_fn_t)( void const *data );

////////// structures /////////////////////////////////////////////////////////

/**
 * A hash table.
 */
struct hash_table {
  ht_entry_t   *buckets;                ///< Buckets.
  ht_cmp_fn_t   cmp_fn;                 ///< Comparison function.
  ht_hash_fn_t  hash_fn;                ///< Hash function.
  double        max_lf;                 ///< Maximum load factor.
  ht_dloc_t     dloc;                   ///< Entry data location.
  unsigned      size;                   ///< Number of entries.
  unsigned      prime_idx;              ///< Index into HT_PRIME.
};

/**
 * A hash table entry.
 *
 * @remarks Once created, `ht_entry` objects don't move even if the hash table
 * grows, so pointers to them remain valid until either deleted or the hash
 * table is cleaned up.
 */
struct ht_entry {
  ht_entry_t   *next;                   ///< Next entry, if any.
  ht_entry_t   *prev;                   ///< Previous entry, if any.
  ht_hash_val_t hash;                   ///< Entry hash.
  alignas(max_align_t) char data[];     ///< Entry data.
};

/**
 * The return value of ht_insert().
 */
struct ht_insert_rv {
  /**
   * The \ref ht_entry "entry" either found or inserted.  Use \ref inserted to
   * know which.
   *
   * @warning Even though this is a pointer to a non-`const` \ref ht_entry, the
   * entry's \ref ht_entry::data "data" _must not_ be modified if that would
   * change the entry's hash value according to the table's \ref
   * hash_table::hash_fn "hash_fn".
   */
  ht_entry_t *entry;

  /**
   * If `true`, \ref entry refers to the newly inserted entry; if `false`, \ref
   * entry refers to the existing entry having the same \ref ht_entry::data
   * "data" according to the table's \ref hash_table::cmp_fn "cmp_fn".
   */
  bool inserted;
};

/**
 * An iterator for a hash_table.
 */
struct ht_iterator {
  hash_table_t const *table;            ///< Hash table being iterated over.
  ht_entry_t         *next;             ///< Next entry, if any.
  unsigned            bucket_idx;       ///< Current bucket index.
  unsigned            n_buckets;        ///< Number of buckets.
};

////////// extern functions ///////////////////////////////////////////////////

/**
 * Cleans-up a hash table.
 *
 * @param table The hash table to clean up.  If NULL, does nothing.
 * @param free_fn A pointer to a function used to free data associated with
 * each entry or NULL if unnecessary.
 *
 * @sa ht_init()
 */
void ht_cleanup( hash_table_t *table, ht_free_fn_t free_fn );

/**
 * Deletes an entry from a hash table.
 *
 * @remarks
 * @parblock
 * This function deletes _only_ the entry from \a table.  If \ref
 * ht_entry::data "data" also needs to be deleted because \ref hash_table::dloc
 * "dloc" is \ref ht_dloc::HT_DPTR "RB_DPTR", the caller must delete it
 * explicitly.  For example, for some type `T` that is an entry's \ref
 * ht_entry::data "data":
 *
 *      ht_entry_t *const entry = ht_find( table, find_data );
 *      if ( entry != NULL ) {
 *        T *const found_t = ht_entry_data( entry );
 *        T_cleanup( found_t );         // if necessary
 *        free( found_t );
 *        ht_delete( table, entry );
 *      }
 * @endparblock
 *
 * @param table The hash table to delete from.
 * @param entry The entry to delete.
 */
void ht_delete( hash_table_t *table, ht_entry_t *entry );

/**
 * Gets whether a hash table is empty.
 *
 * @param table The hash table to check.
 * @return Returns `true` only if \a table is empty.
 */
NODISCARD
inline bool ht_empty( hash_table_t const *table ) {
  return table->size == 0;
}

/**
 * Gets a pointer to an \a entry's data.
 *
 * @param table A pointer to the hash_table of \a entry.
 * @param entry The ht_entry to get the data of.
 * @return Returns said data.
 *
 * @note Normally, either #HT_DINT or #HT_DPTR is used to get a pointer to an
 * entry's data.  This function would only be used in code that should work
 * with a table using either data location.
 *
 * @sa #HT_DINT
 * @sa #HT_DPTR
 */
NODISCARD
inline void* ht_entry_data( hash_table_t const *table,
                            ht_entry_t const *entry ) {
  return table->dloc == HT_DINT ? HT_DINT( entry ) : HT_DPTR( entry );
}

/**
 * Attempts to find \a data within a hash table.
 *
 * @param table The hash table to search.
 * @param data The data to search for.
 * @return Returns a pointer to the entry containing \a data or NULL if not
 * found.
 */
NODISCARD
ht_entry_t* ht_find( hash_table_t const *table, void const *data );

/**
 * Initializes a hash table.
 *
 * @param table The hash table to initialize.
 * @param dloc Where data for each entry is stored.
 * @param max_lf The maximum load factor.
 * @param est_size The estimated number of entries.
 * @param cmp_fn The comparison function to use.
 * @param hash_fn The hash function to use.
 * @sa ht_cleanup()
 */
void ht_init( hash_table_t *table, ht_dloc_t dloc, double max_lf,
              unsigned est_size, ht_cmp_fn_t cmp_fn, ht_hash_fn_t hash_fn );

/**
 * Attempts to insert \a data into \a table.
 *
 * @param table The hash table to insert into.
 * @param data The data to insert.
 * @param data_size If \a table's \ref hash_table::dloc "dloc" is:
 *  + #HT_DINT: The size of \a data.  If an entry is inserted, then this number
 *    of bytes are copied from \a data into the new entry's \ref ht_entry::data
 *    "data".
 *  + #HT_DPTR: Not used.  If an entry is inserted, then the pointer value of
 *    \a data itself is copied into the new entry's \ref ht_entry::data "data".
 *
 * @return Returns an \ref ht_insert_rv where its \ref ht_insert_rv::entry
 * "entry" points to either the newly inserted entry or the existing entry
 * having the same \ref ht_entry::data "data" and \ref ht_insert_rv::inserted
 * "inserted" is `true` only if \ref ht_entry::data "data" was inserted.
 */
NODISCARD
ht_insert_rv_t ht_insert( hash_table_t *table, void *data, size_t data_size );

/**
 * Initializes a hash table iterator.
 *
 * @param it The hash table iterator to initialize.
 * @param table The hash table to iterate over.
 *
 * @sa ht_iterator_next()
 */
void ht_iterator_init( ht_iterator_t *it, hash_table_t const *table );

/**
 * Gets the next hash table entry, if any.
 *
 * @remarks The order entries are returned is in bucket order that is seemingly
 * arbitrary.
 *
 * @param it The hash table iterator.
 * @return Returns a pointer to the data of the next entry or NULL if none.
 */
NODISCARD
void* ht_iterator_next( ht_iterator_t *it );

/**
 * Calculates the current load factor of \a table.
 *
 * @param table The hash table to calculate the load factor of.
 * @return Returns the load factor of \a table.
 */
NODISCARD
inline double ht_load_factor( hash_table_t const *table ) {
  extern unsigned const HT_PRIME[];
  return (double)table->size / HT_PRIME[ table->prime_idx ];
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* pjl_hash_table_h */
/* vim:set et sw=2 ts=2: */
