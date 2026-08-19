/**
 * Test derived from SWISH++.
 *
 * Copyright (C) 1998-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/swishxx
 */

#ifndef element_map_hpp
#define element_map_hpp

#include <map>
#include <string>

struct element_map : std::map<std::string,int> {
  element_map();
};

#endif /* element_map_hpp */
/* vim:set et sw=2 ts=2: */
