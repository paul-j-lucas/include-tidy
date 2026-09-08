/**
 * Test derived from SWISH++.
 *
 * Copyright (C) 1998-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/swishxx
 */

#ifndef pjl_hash_H
#define pjl_hash_H

#include <cstddef>

constexpr size_t Hash_Init = 14695981039346656037ul;

size_t hash_bytes( void const *p, size_t len, size_t init = Hash_Init );

template<typename T>
inline size_t hash_bytes( T const &value ) {
  return hash_bytes( &value, sizeof( value ) );
}

#endif /* pjl_hash_H */
/* vim:set et ts=2 sw=2: */
