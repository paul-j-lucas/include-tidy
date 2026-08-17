/**
 * Test derived from SWISH++.
 *
 * Copyright (C) 1998-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/swishxx
 */

#ifndef less_hpp
#define less_hpp

#include <cstddef>
#include <cstring>
#include <functional>

namespace std {

template<>
struct less<char const*> {
  using first_argument_type = char const*;
  using second_argument_type = char const*;
  using result_type = bool;

  less() { }

  result_type operator()( first_argument_type i,
                          second_argument_type j ) const {
    return std::strcmp( i, j ) < 0;
  }
};

} // namespace std

#endif /* less_hpp */
/* vim:set et sw=2 ts=2: */
