/**
 * Test derived from SWISH++.
 *
 * Copyright (C) 1998-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/swishxx
 */

#ifndef swishxx_conf_filter_hpp
#define swishxx_conf_filter_hpp

#include "pattern_map.hpp"
#include <string>

struct conf_filter {
  using map_type = pattern_map<std::string>;

  void f();
};

#endif /* swishxx_conf_filter_hpp */
/* vim:set et sw=2 ts=2: */
