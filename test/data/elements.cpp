/**
 * Test derived from SWISH++.
 *
 * Copyright (C) 1998-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/swishxx
 */

#include "elements.hpp"

element_map::element_map() {
  static char const *const end_tag_table[] = {
    "a",    "/a",
    // ...
    nullptr
  };

  for ( auto p = end_tag_table; *p; ++p ) {
    //   rv = std::pair<iterator,bool>
    auto rv = insert( value_type{ *p++, 0 } );

    //   it = std::map<K,V>::iterator
    auto it = rv.first;

    auto pit = (&rv)->first;
  }
}

/* vim:set et sw=2 ts=2: */
