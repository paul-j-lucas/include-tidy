/**
 * Test derived from SWISH++.
 *
 * Copyright (C) 1998-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/swishxx
 */

#include "elements.hpp"

element_map::element_map() {
  using cpcc = char const *const;

  cpcc R = reinterpret_cast<char const*>( element::et_required  );

  static char const *const END_TAG_TABLE[] = {
    "a",  R,  "/a",
    // ...
    nullptr
  };

  for ( auto p = END_TAG_TABLE; *p; ++p ) {
    auto const v = (element::end_tag_type const)(long const)(p[1]);
    auto &e = insert( value_type{ *p++, element{ v } } ).first->second;

    //   rv = std::pair<iterator,bool>
    auto rv = insert( value_type{ *p++, element{ v } } );

    //   it = std::map<K,V>::iterator
    auto it = rv.first;

    auto pit = (&rv)->first;
  }
}

/* vim:set et sw=2 ts=2: */
