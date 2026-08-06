/**
 * Test derived from SWISH++.
 *
 * Copyright (C) 1998-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/swishxx
 */

#ifndef swishxx_conf_string_hpp
#define swishxx_conf_string_hpp

#include "conf_var.hpp"
#include <string>

template<>
struct conf<std::string> : conf_var {
  conf( char const *name );
};

#endif /* swishxx_conf_string_hpp */
/* vim:set et sw=2 ts=2: */
