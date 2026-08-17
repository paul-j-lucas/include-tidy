/**
 * Test derived from SWISH++.
 *
 * Copyright (C) 1998-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/swishxx
 */

#ifndef entities_hpp
#define entities_hpp

#include "less.hpp"
#include <map>

class char_entity_map {
public:
  using key_type = char const*;
  using value_type = char;

  value_type operator[]( key_type key ) const {
    map_type::const_iterator const found = map_.find( key );
    return found != map_.end() ? found->second : ' ';
  }

private:
  using map_type = std::map<key_type,value_type>;
  map_type map_;
};

#endif /* entities_hpp */
/* vim:set et sw=2 ts=2: */
