/**
 * Test derived from cdecl.
 *
 * Copyright (C) 2017-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/cdecl
 */

#ifndef cdecl_c_operator_H
#define cdecl_c_operator_H

#include "types.h"

enum c_op_overload {
  C_OVERLOAD_NONE       = 0,
  C_OVERLOAD_MEMBER     = C_FUNC_MEMBER,
  C_OVERLOAD_NON_MEMBER = C_FUNC_NON_MEMBER,
};
typedef enum c_op_overload c_op_overload_t;

struct c_operator {
  // ...
  c_op_overload_t overload;
  // ...
};

#endif /* cdecl_c_operator_H */
/* vim:set et sw=2 ts=2: */
