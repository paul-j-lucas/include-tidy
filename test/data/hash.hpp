/**
 * Test derived from SWISH++.
 *
 * Copyright (C) 1998-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/swishxx
 */

#ifndef pjl_hash_hpp
#define pjl_hash_hpp

#include <cstddef>
#include <cstring>
#include <functional>
#include <unordered_set>

namespace PJL {

constexpr size_t Hash_Init = 14695981039346656037ul;

size_t hash_bytes( void const *p, size_t len, size_t init = Hash_Init );

template<typename T>
inline size_t hash_bytes( T const &value ) {
  return hash_bytes( &value, sizeof( value ) );
}

size_t hash_string( char const *s, size_t init = Hash_Init );

} // namespace PJL

namespace std {

template<>
inline size_t hash<char const*>::operator()( char const *s ) const noexcept {
  return PJL::hash_string( s );
}

} // namespace std

namespace PJL {

struct char_ptr_equal_to {
  using argument_type = char const*;
  using result_type = bool;

  result_type operator()( argument_type i, argument_type j ) const {
    return std::strcmp( i, j ) == 0;
  }
};

using unordered_char_ptr_set =
  std::unordered_set<char const*,std::hash<char const*>,char_ptr_equal_to>;

} // namespace PJL

#endif  /* pjl_hash_hpp */
/* vim:set et ts=2 sw=2: */
