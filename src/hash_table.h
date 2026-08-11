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

#ifndef pjl_hash_table_H
#define pjl_hash_table_H

// local
#include "pjl_config.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>                     /* for max_align_t */
#include <stdint.h>

/// @endcond

/**
 * @defgroup hash-table-group Red-Black Tree
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

////////// typrdefs ///////////////////////////////////////////////////////////

typedef struct hash_table     hash_table_t;
typedef struct ht_entry       ht_entry_t;
typedef uint64_t              ht_hash_val_t;
typedef struct ht_insert_rv   ht_insert_rv_t;
typedef struct ht_iterator    ht_iterator_t;

/**
 * The signature for a function passed to ht_init() used to compare entry data.
 *
 * @param i_data A pointer to data.
 * @param j_data A pointer to data.
 * @return Returns an integer less than, equal to, or greater than 0, according
 * to whether the data pointed to by \a i_data is less than, equal to, or
 * greater than the data pointed to by \a j_data.
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
  hash_table_t *table;                  ///< Hash table being iterated over.
  ht_entry_t   *next;                   ///< Next entry, if any.
  unsigned      bucket_idx;             ///< Current bucket index.
  unsigned      n_buckets;              ///< Number of buckets.
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
inline bool ht_empty( hash_table_t const *table ) {
  return table->size == 0;
}

/**
 * Attempts to find \a data within a hash table.
 *
 * @param table The hash table to search.
 * @param data The data to search for.
 * @return Returns a pointer to the entry containing \a data or NULL if not
 * found.
 */
ht_entry_t* ht_find( hash_table_t const *table, void const *data );

/**
 * Initializes a hash table.
 *
 * @param table The hash table to initialize.
 * @param max_lf The maximum load factor.
 * @param est_size The estimated number of entries.
 * @param cmp_fn The comparison function to use.
 * @param hash_fn The hash function to use.
 * @sa ht_cleanup()
 */
void ht_init( hash_table_t *table, double max_lf, unsigned est_size,
              ht_cmp_fn_t cmp_fn, ht_hash_fn_t hash_fn );

/**
 * Attempts to insert \a data into \a table.
 *
 * @param table The hash table to insert into.
 * @param data The data to insert.
 * @param data_size The size of \a data.
 * @return Returns an \ref ht_insert_rv where its \ref ht_insert_rv::entry
 * "entry" points to either the newly inserted entry or the existing entry
 * having the same \ref ht_entry::data "data" and \ref ht_insert_rv::inserted
 * "inserted" is `true` only if \ref ht_entry::data "data" was inserted.
 *
 * @note If \ref ht_insert_rv::inserted "inserted" is `true`, only a new entry
 * was inserted; \a data was _not_ copied into \ref ht_entry::data "data" ---
 * that needs to be done by the caller.
 */
ht_insert_rv_t ht_insert( hash_table_t *table, void *data, size_t data_size );

/**
 * Initializes a hash table iterator.
 *
 * @param it The hash table iterator to initialize.
 * @param table The hash table to iterate over.
 *
 * @sa ht_iterator_next()
 */
void ht_iterator_init( ht_iterator_t *it, hash_table_t *table );

/**
 * Gets the nexy hash table entry, if any.
 *
 * @remarks The order entries are returned is in bucket order that is seemingly
 * arbitrary.
 *
 * @param it The hash table iterator.
 * @return Returns a pointer to the next entry or NULL if none.
 */
ht_entry_t* ht_iterator_next( ht_iterator_t *it );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* pjl_hash_table_H */
/* vim:set et sw=2 ts=2: */
