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

struct element {
  enum end_tag_type {
    et_forbidden,
    et_optional,
    et_required
  };

  end_tag_type const end_tag;

  explicit element( end_tag_type t ) : end_tag{ t } { }
};

struct element_map : std::map<char const*,element> {
  element_map();
};

#endif /* element_map_hpp */
/* vim:set et sw=2 ts=2: */
