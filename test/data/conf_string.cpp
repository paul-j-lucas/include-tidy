/**
 * Test derived from SWISH++.
 *
 * Copyright (C) 1998-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/swishxx
 */

#include "conf_string.hpp"
#include <string>

conf<std::string>::conf( char const *name ) : conf_var{ name } {
}

/* vim:set et sw=2 ts=2: */
