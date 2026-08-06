/**
 * Test derived from SWISH++.
 *
 * Copyright (C) 1998-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/swishxx
 */

#ifndef swishxx_derived_conf_string_hpp
#define swishxx_derived_conf_string_hpp

#include "conf_string.hpp"
#include "conf_var.hpp"

#include <string>

struct derived_conf_string : conf<std::string> {
  void f();
};

#endif /* swishxx_derived_conf_string_hpp */
/* vim:set et sw=2 ts=2: */
