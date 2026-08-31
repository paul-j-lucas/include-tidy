/*
**      PJL Library
**      src/hash_table.c
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
#include "pjl_config.h"
#include "hash_table.h"
#include "util.h"

// standard
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/**
 * @addtogroup hash-table-group
 * @{
 */

////////// extern constants ///////////////////////////////////////////////////

/**
 * A table of prime numbers for the number of buckets a hash table should have
 * as the number of entries increases.
 *
 * @remarks
 * @parblock
 * If the number of buckets, _m_, isn't prime and the set of keys are not
 * uniformly distributed, _h(k)_ `%` _m_ produces more collisions at bucket
 * index _i_ where it’s a factor of _m_.
 *
 * Additionally, _m_ should be as far away from a power of 2 as possibe. If _m_
 * &asymp; 2<sup>_b_</sup>, then all but the lower _b_ bits of _h(k)_ are
 * discarded.
 * @endparblock
 */
unsigned const HT_PRIME[] = {
      11,      23,      53,      97,      193,      389,    769,   1543,
    3079,    6151,   12289,   24593,    49157,    98317, 196613, 393241,
  786433, 1572869, 3145739, 6291469, 12582917, 25165843
};

////////// local functions ////////////////////////////////////////////////////

/**
 * Grows a hash table.
 *
 * @param table The hash table to grow.
 * @return Returns `true` only if the table was grown (very likely).
 */
NODISCARD
static bool ht_grow( hash_table_t *table ) {
  assert( table != NULL );

  if ( unlikely( table->prime_idx >= ARRAY_SIZE( HT_PRIME ) - 1 ) )
    return false;

  unsigned const old_n_buckets = HT_PRIME[ table->prime_idx ];
  unsigned const new_n_buckets = HT_PRIME[ ++table->prime_idx ];
  ht_entry_t *const new_buckets = calloc( new_n_buckets, sizeof(ht_entry_t) );

  for ( unsigned b = 0; b < old_n_buckets; ++b ) {
    for ( ht_entry_t *entry = table->buckets[b].next, *next;
          entry != NULL; entry = next ) {
      ht_hash_val_t const hash = entry->hash;
      ht_entry_t *const new_head = &new_buckets[ hash % new_n_buckets ];

      next = entry->next;
      entry->next = new_head->next;
      entry->prev = new_head;

      if ( new_head->next != NULL )
        new_head->next->prev = entry;
      new_head->next = entry;
    } // for
  } // for

  free( table->buckets );
  table->buckets = new_buckets;
  return true;
}

////////// extern functions ///////////////////////////////////////////////////

void ht_cleanup( hash_table_t *table, ht_free_fn_t free_fn ) {
  if ( table == NULL || table->buckets == NULL )
    return;

  for ( unsigned b = 0; b < HT_PRIME[ table->prime_idx ]; ++b ) {
    for ( ht_entry_t *entry = table->buckets[b].next, *next;
          entry != NULL; entry = next ) {
      if ( free_fn != NULL )
        (*free_fn)( ht_entry_data( table, entry ) );
      next = entry->next;
      free( entry );
    }
  } // for

  free( table->buckets );
  *table = (hash_table_t){ 0 };
}

void ht_delete( hash_table_t *table, ht_entry_t *entry ) {
  assert( table != NULL );
  assert( entry != NULL );

  entry->prev->next = entry->next;
  if ( entry->next != NULL )
    entry->next->prev = entry->prev;
  free( entry );
  --table->size;
}

ht_entry_t* ht_find( hash_table_t const *table, void const *data ) {
  assert( table != NULL );
  assert( data != NULL );

  ht_hash_val_t const b =
    (*table->hash_fn)( data ) % HT_PRIME[ table->prime_idx ];
  for ( ht_entry_t *entry = table->buckets[b].next; entry != NULL;
        entry = entry->next ) {
    if ( (*table->cmp_fn)( data, ht_entry_data( table, entry ) ) == 0 )
      return entry;
  } // for

  return NULL;
}

void ht_init( hash_table_t *table, ht_dloc_t dloc, double max_lf,
              unsigned est_size, ht_cmp_fn_t cmp_fn, ht_hash_fn_t hash_fn ) {
  assert( table != NULL );
  assert( max_lf > 0.0 );
  assert( cmp_fn != NULL );
  assert( hash_fn != NULL );

  unsigned prime_idx = 0;
  for ( ; prime_idx < ARRAY_SIZE( HT_PRIME ) - 1; ++prime_idx ) {
    if ( HT_PRIME[ prime_idx ] * max_lf >= est_size )
      break;
  } // for

  *table = (hash_table_t){
    .buckets = calloc( HT_PRIME[ prime_idx ], sizeof(ht_entry_t) ),
    .cmp_fn = cmp_fn,
    .dloc = dloc,
    .hash_fn = hash_fn,
    .max_lf = max_lf,
    .prime_idx = prime_idx
  };
}

ht_insert_rv_t ht_insert( hash_table_t *table, void *data, size_t data_size ) {
  assert( table != NULL );
  assert( data != NULL );
  assert( table->dloc == HT_DPTR || data_size > 0 );

  unsigned n_buckets = HT_PRIME[ table->prime_idx ];
  ht_hash_val_t const hash = (*table->hash_fn)( data );
  ht_hash_val_t b = hash % n_buckets;
  ht_entry_t *head = &table->buckets[b], *entry;

  for ( entry = head->next; entry != NULL; entry = entry->next ) {
    if ( (*table->cmp_fn)( data, ht_entry_data( table, entry ) ) == 0 )
      return (ht_insert_rv_t){ entry, .inserted = false };
  } // for

  ++table->size;
  double const lf = ht_load_factor( table );
  if ( lf >= table->max_lf && likely( ht_grow( table ) ) ) {
    n_buckets = HT_PRIME[ table->prime_idx ];
    b = hash % n_buckets;
    head = &table->buckets[b];
  }

  if ( table->dloc == HT_DINT ) {
    entry = malloc( sizeof *entry + data_size );
    memcpy( HT_DINT( entry ), data, data_size );
  }
  else {
    entry = malloc( sizeof *entry + sizeof( void* ) );
    HT_DPTR( entry ) = data;
  }

  *entry = (ht_entry_t){ .next = head->next, .prev = head, .hash = hash };
  if ( head->next != NULL )
    head->next->prev = entry;
  head->next = entry;

  return (ht_insert_rv_t){ entry, .inserted = true };
}

void ht_iterator_init( ht_iterator_t *it, hash_table_t const *table ) {
  assert( it != NULL );
  assert( table != NULL );

  *it = (ht_iterator_t){
    .table = table,
    // Initialize to -1 so the first time ht_iterator_next() is called, it will
    // be incremented and wrap around to 0.
    .bucket_idx = (unsigned)-1,
    .n_buckets = HT_PRIME[ table->prime_idx ]
  };
}

void* ht_iterator_next( ht_iterator_t *it ) {
  assert( it != NULL );
  assert( it->n_buckets == HT_PRIME[ it->table->prime_idx ] );

  for (;;) {
    if ( it->next != NULL ) {
      ht_entry_t *const entry = it->next;
      it->next = it->next->next;
      return ht_entry_data( it->table, entry );
    }
    if ( ++it->bucket_idx == it->n_buckets )
      return NULL;
    it->next = it->table->buckets[ it->bucket_idx ].next;
  } // for
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/// @cond DOXYGEN_IGNORE

extern inline bool ht_empty( hash_table_t const* );
extern inline void* ht_entry_data( hash_table_t const*, ht_entry_t const* );
extern inline double ht_load_factor( hash_table_t const* );

/// @endcond

/* vim:set et sw=2 ts=2: */
