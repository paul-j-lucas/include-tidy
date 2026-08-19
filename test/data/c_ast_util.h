/**
 * Test derived from cdecl.
 *
 * Copyright (C) 2017-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/cdecl
 */

#ifndef cdecl_c_ast_util_H
#define cdecl_c_ast_util_H

#include "c_operator.h"
#include "types.h"
#include <stdbool.h>

inline bool c_ast_op_mbr_matches( c_ast_t const *ast, c_operator_t const *op ) {
  (void)ast;
  return op->overload  != 0;
}

#endif /* cdecl_c_ast_util_H */
/* vim:set et sw=2 ts=2: */
