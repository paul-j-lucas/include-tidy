/**
 * Test derived from SWISH++.
 *
 * Copyright (C) 1998-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/swishxx
 */

#include "derived_conf_string.hpp"
#include <cerrno>

void derived_conf_string::f() {
  int x = errno;
}
