/**
 * Test derived from SWISH++.
 *
 * Copyright (C) 1998-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/swishxx
 */

#ifndef swishxx_pattern_map_hpp
#define swishxx_pattern_map_hpp

#include <map>
#include <string>

template<typename T>
struct pattern_map : std::map<std::string,T> {
  using base_type = std::map<std::string,T>;
  using value_type = base_type::value_type;
};

#endif /* swishxx_pattern_map_hpp */
/* vim:set et sw=2 ts=2: */
