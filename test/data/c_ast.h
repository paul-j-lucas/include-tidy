/**
 * Test derived from cdecl.
 *
 * Copyright (C) 2017-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/cdecl
 */

#ifndef cdecl_c_ast_H
#define cdecl_c_ast_H

#include "c_type.h"
#include "types.h"

struct c_ast {
  // ...
  c_type_t type;
  // ...
};

#endif /* cdecl_c_ast_H */
/* vim:set et sw=2 ts=2: */
